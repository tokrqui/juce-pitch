#include "PitchShifterDSP.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    float semitonesToRatio (float semitones) noexcept
    {
        return std::pow (2.0f, semitones / 12.0f);
    }

    // 4-point Hermite (Catmull-Rom) interpolation. Needed because grain read
    // pointers move at a non-integer sample rate whenever pitch != 0; plain
    // linear interpolation is audibly duller / introduces more high-frequency
    // smearing on voiced consonants and sibilants than Hermite, for a
    // negligible CPU cost difference.
    float hermite (float y0, float y1, float y2, float y3, float frac) noexcept
    {
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + y1;
    }
}

//==============================================================================
void PitchShifterDSP::prepare (double sampleRate) noexcept
{
    sampleRateHz = sampleRate;

    // Grain length is the single knob controlling latency vs. quality.
    // It must be longer than one voice pitch period, or the granular
    // technique has no coherent waveform cycle to work with and produces
    // comb-filtering / buzz instead of an audible pitch shift. Low male
    // voices go down to ~90-100 Hz (a ~10-11 ms period), so 12 ms gives a
    // safe margin while staying well under a ~25 ms total latency budget.
    constexpr double grainLengthSeconds = 0.012;
    grainLengthSamples = std::max (32, static_cast<int> (std::round (sampleRate * grainLengthSeconds)));
    phaseIncrement = 1.0 / static_cast<double> (grainLengthSamples);

    // Pitch parameter smoothing: fast enough to feel responsive for live
    // performance, slow enough to avoid zipper artifacts on the read-speed
    // ramp. 5 ms time constant is a reasonable middle ground.
    constexpr double smoothingTimeSec = 0.005;
    pitchSmoothingCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate * smoothingTimeSec)));

    reset();
}

void PitchShifterDSP::reset() noexcept
{
    buffer.fill (0.0f);
    writeIndex = 0;
    targetSemitones = 0.0f;
    smoothedSemitones = 0.0f;

    // Stagger grains evenly across the lifetime cycle (phase) and across
    // read position (lag behind the write head). With kNumGrains grains,
    // staggering by 1/kNumGrains of a grain length means at any instant
    // exactly one grain is near its window peak and overlap is smooth even
    // on large pitch ratios, where 2-grain schemes tend to show audible
    // "double-tracking" beating.
    const int lagTarget = grainLengthSamples + 4; // +4 sample margin for Hermite taps

    for (int i = 0; i < kNumGrains; ++i)
    {
        const double staggerFraction = static_cast<double> (i) / static_cast<double> (kNumGrains);

        grains[static_cast<size_t> (i)].phase = staggerFraction;
        grains[static_cast<size_t> (i)].readPos =
            static_cast<double> (wrapIndex (writeIndex - lagTarget - static_cast<int> (staggerFraction * grainLengthSamples)));
        grains[static_cast<size_t> (i)].primed = true;
    }
}

void PitchShifterDSP::setTargetSemitones (float semitones) noexcept
{
    targetSemitones = std::clamp (semitones, kMinSemitones, kMaxSemitones);
}

float PitchShifterDSP::currentPitchRatio() const noexcept
{
    return semitonesToRatio (smoothedSemitones);
}

float PitchShifterDSP::grainWindow (double phase) const noexcept
{
    // Half-sine (raised sine) envelope: 0 at phase==0, 1 at phase==0.5,
    // 0 at phase==1. Cheap, click-free at the boundaries, and well-behaved
    // under the energy normalization applied in processSample().
    return std::sin (static_cast<float> (phase) * kPi);
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

void PitchShifterDSP::wrapPosition (double& position) const noexcept
{
    while (position >= kBufferSizeD)
        position -= kBufferSizeD;

    while (position < 0.0)
        position += kBufferSizeD;
}

void PitchShifterDSP::scheduleGrain (int grainIndex) noexcept
{
    // Re-anchor this grain just behind the current write head. Because this
    // is only ever called at the instant this grain's own phase has wrapped
    // (see processSample), its window gain is guaranteed to be 0 right now —
    // the reposition is therefore inherently inaudible, with no dependency
    // on any other grain's state.
    const int lagTarget = grainLengthSamples + 4;
    auto& grain = grains[static_cast<size_t> (grainIndex)];
    grain.readPos = static_cast<double> (wrapIndex (writeIndex - lagTarget));
    wrapPosition (grain.readPos);
}

float PitchShifterDSP::processSample (float inputSample) noexcept
{
    buffer[static_cast<size_t> (writeIndex)] = inputSample;
    writeIndex = wrapIndex (writeIndex + 1);

    smoothedSemitones += (targetSemitones - smoothedSemitones) * pitchSmoothingCoeff;

    // Unity-pitch bypass: fully transparent path when no shift is active,
    // avoids any grain-window coloration / added noise floor at ratio == 1.
    if (std::abs (smoothedSemitones) < 0.01f)
        return inputSample;

    const auto ratio = static_cast<double> (currentPitchRatio());

    float output = 0.0f;
    float energy = 0.0f; // sum of gain^2, used for equal-power normalization

    for (int i = 0; i < kNumGrains; ++i)
    {
        auto& grain = grains[static_cast<size_t> (i)];

        const float gain = grainWindow (grain.phase);
        output += readInterpolated (grain.readPos) * gain;
        energy += gain * gain;

        grain.readPos += ratio;
        wrapPosition (grain.readPos);

        grain.phase += phaseIncrement;
        if (grain.phase >= 1.0)
        {
            grain.phase -= 1.0;
            scheduleGrain (i);
        }
    }

    // Normalize by actual instantaneous energy rather than relying on the
    // window shapes summing to an exact analytic constant. This keeps
    // output level stable even if grain count / stagger / window shape are
    // changed later, without having to re-derive the overlap math by hand.
    constexpr float kEpsilon = 1.0e-6f;
    const float normalization = 1.0f / std::sqrt (std::max (energy, kEpsilon));

    return output * normalization;
}
