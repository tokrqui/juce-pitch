#include "PitchShifterDSP.h"

#include <algorithm>

namespace
{
    constexpr float kHalfPi = 1.5707963267948966f;

    // 4-point Hermite interpolation — chosen over linear because live pitch
    // shifts expose interpolation zipper noise when the read pointer moves
    // at non-integer speeds. Hermite adds negligible latency vs. linear.
    float hermite (float y0, float y1, float y2, float y3, float frac) noexcept
    {
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + y1;
    }

    float semitonesToRatio (float semitones) noexcept
    {
        return std::pow (2.0f, semitones / 12.0f);
    }
}

//==============================================================================
void PitchShifterDSP::prepare (double sampleRate) noexcept
{
    // Target low-latency operation: aim for total algorithmic latency <= 5 ms.
    // Choose a small overlap (a few ms) but not too small to avoid audible
    // crossfade ripple. On 48kHz, 128 samples ≈ 2.67 ms, overlap*2 => ~5.3 ms.
    const double targetLatencySec = 0.005; // 5 ms target
    const int minOverlap = 32; // don't go below 32 samples to avoid very short windows
    overlapSamples = std::max (minOverlap, static_cast<int> (sampleRate * 0.002)); // ~2ms base

    // Keep a small safety margin: minDelay = 2 * overlap, cap maxDelay to targetLatencySec
    minDelaySamples = overlapSamples * 2;
    const int latencyCapSamples = static_cast<int> (std::ceil (sampleRate * targetLatencySec));
    // Ensure maxDelaySamples at least minDelaySamples but no more than latencyCapSamples.
    maxDelaySamples = std::min (std::max (minDelaySamples, latencyCapSamples), kBufferSize / 2);

    crossfadeIncrement = 1.0 / static_cast<double> (overlapSamples);

    // Shorter smoothing for fast response in live use while avoiding zippering.
    const auto smoothingTimeSec = 0.005; // 5 ms smoothing time constant
    pitchSmoothingCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate * smoothingTimeSec)));

    reset();
}

void PitchShifterDSP::reset() noexcept
{
    buffer.fill (0.0f);
    writeIndex = 0;

    // Initialize read heads close behind the write head to keep latency low.
    readPosition[0] = static_cast<double> (wrapIndex (writeIndex - minDelaySamples));
    readPosition[1] = static_cast<double> (wrapIndex (writeIndex - (minDelaySamples + overlapSamples)));

    crossfadePhase = 0.0;
    headToResetOnCycle = 0;
    targetSemitones = 0.0f;
    smoothedSemitones = 0.0f;
}

void PitchShifterDSP::setTargetSemitones (float semitones) noexcept
{
    targetSemitones = std::clamp (semitones, kMinSemitones, kMaxSemitones);
}

float PitchShifterDSP::readInterpolated (double position) const noexcept
{
    const auto index = static_cast<int> (std::floor (position));
    const auto frac  = static_cast<float> (position - static_cast<double> (index));

    const auto y0 = buffer[static_cast<size_t> (wrapIndex (index - 1))];
    const auto y1 = buffer[static_cast<size_t> (wrapIndex (index))];
    const auto y2 = buffer[static_cast<size_t> (wrapIndex (index + 1))];
    const auto y3 = buffer[static_cast<size_t> (wrapIndex (index + 2))];

    return hermite (y0, y1, y2, y3, frac);
}

void PitchShifterDSP::wrapReadPosition (double& position) const noexcept
{
    while (position >= kBufferSizeD)
        position -= kBufferSizeD;

    while (position < 0.0)
        position += kBufferSizeD;
}

double PitchShifterDSP::lagBehindWrite (double readPos) const noexcept
{
    auto lag = static_cast<double> (writeIndex) - readPos;

    if (lag < 0.0)
        lag += kBufferSizeD;

    return lag;
}

void PitchShifterDSP::repositionHead (int headIndex) noexcept
{
    readPosition[headIndex] = static_cast<double> (wrapIndex (writeIndex - maxDelaySamples));
    wrapReadPosition (readPosition[headIndex]);
}

float PitchShifterDSP::currentPitchRatio() const noexcept
{
    return semitonesToRatio (smoothedSemitones);
}

void PitchShifterDSP::updateSmoothedPitch() noexcept
{
    smoothedSemitones += (targetSemitones - smoothedSemitones) * pitchSmoothingCoeff;
}

float PitchShifterDSP::processSample (float inputSample) noexcept
{
    buffer[static_cast<size_t> (writeIndex)] = inputSample;
    writeIndex = wrapIndex (writeIndex + 1);

    updateSmoothedPitch();

    // Unity-pitch bypass — dual-head crossfade would impose ~1/overlap Hz
    // amplitude modulation even when ratio == 1. Live performance requires
    // a transparent zero-shift path with no added latency.
    if (std::abs (smoothedSemitones) < 0.01f)
        return inputSample;

    const auto sample0 = readInterpolated (readPosition[0]);
    const auto sample1 = readInterpolated (readPosition[1]);

    // Equal-power crossfade — sum of squares stays ~1, avoiding level dip
    // in the middle of the overlap (critical for live monitoring levels).
    const auto phase = static_cast<float> (crossfadePhase);
    const auto gain0 = std::cos (phase * kHalfPi);
    const auto gain1 = std::sin (phase * kHalfPi);
    const auto output = sample0 * gain0 + sample1 * gain1;

    const auto ratio = static_cast<double> (currentPitchRatio());

    // Variable read pointer speed implements time-domain pitch shift:
    // ratio > 1 → read head consumes buffer faster → higher pitch.
    // ratio < 1 → read head slows down → lower pitch.
    readPosition[0] += ratio;
    readPosition[1] += ratio;

    wrapReadPosition (readPosition[0]);
    wrapReadPosition (readPosition[1]);

    crossfadePhase += crossfadeIncrement;

    if (crossfadePhase >= 1.0)
    {
        crossfadePhase -= 1.0;

        // Head 0 fades out over each cycle (cosine weight). When the cycle
        // completes, snap it to the far end of the buffer and let head 1 carry
        // the audio. Roles alternate every overlap window.
        repositionHead (headToResetOnCycle);
        headToResetOnCycle = 1 - headToResetOnCycle;
    }

    // Safety constraint — if a head drifts too close to the write pointer
    // (e.g. extreme pitch or host block-size change), force a reposition
    // without waiting for the crossfade cycle.
    for (int head = 0; head < 2; ++head)
    {
        if (lagBehindWrite (readPosition[head]) < static_cast<double> (minDelaySamples))
            repositionHead (head);
    }

    return output;
}
