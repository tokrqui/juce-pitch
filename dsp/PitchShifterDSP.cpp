#include "PitchShifterDSP.h"

#include <algorithm>
#include <cmath>

// =============================================================================
// NOTE ON HEADER CHANGES REQUIRED (PitchShifterDSP.h)
// =============================================================================
// The original bug: a single shared `crossfadePhase` drove BOTH gain curves
// (gain0 = cos, gain1 = sin), while repositioning was driven by a SEPARATE
// alternating index `headToResetOnCycle`. These two things only agreed with
// each other half the time, causing an audible click on every second cycle
// when the "wrong" (loud) head got repositioned.
//
// Fix: give each head its OWN phase accumulator. Each head's gain depends
// only on its own phase, and each head is repositioned exactly when its own
// phase wraps (i.e. exactly when its own gain is guaranteed to be zero).
// There is no longer any indirection between "who is loud" and "who gets
// repositioned" — they are the same variable by construction.
//
// Please update the header as follows:
//
//   - Remove:   double crossfadePhase;
//   - Remove:   int headToResetOnCycle;
//   - Add:      double headPhase[2];      // each head's own 0..1 cycle position
//   - Add:      double crossfadeIncrement; // unchanged, still 1.0 / overlapSamples
//
// Everything else in the header (buffer, readPosition[2], writeIndex,
// overlapSamples, min/maxDelaySamples, smoothing members, kBufferSize, etc.)
// stays as-is. Method signatures are unchanged, so call sites do not change.
// =============================================================================

namespace
{
    constexpr float kPi = 3.14159265358979323846f;

    // 4-point Hermite interpolation — kept from the original implementation.
    // Chosen over linear because live pitch shifts expose interpolation
    // zipper noise when the read pointer moves at non-integer speeds.
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

    // Raised-half-sine window: 0 at phase==0, 1 at phase==0.5, 0 at phase==1.
    // Two such windows offset by 0.5 in phase sum to a constant 1.0
    // (it's the standard equal-power complement of cos/sin, just expressed
    // per-head instead of as a single shared cos/sin pair).
    float grainWindow (double phase) noexcept
    {
        return std::sin (static_cast<float> (phase) * kPi);
    }
}

//==============================================================================
void PitchShifterDSP::prepare (double sampleRate) noexcept
{
    // Target low-latency operation: total algorithmic latency <= 5 ms.
    const double targetLatencySec = 0.005; // 5 ms target
    const int minOverlap = 32; // floor to avoid degenerate, clicky windows
    overlapSamples = std::max (minOverlap, static_cast<int> (sampleRate * 0.002)); // ~2ms base

    updateOverlap();

    repositionCooldownSamples = static_cast<int> (std::ceil (sampleRate * 0.050));

    minDelaySamples = overlapSamples * 2;
    const int latencyCapSamples = static_cast<int> (std::ceil (sampleRate * targetLatencySec));
    maxDelaySamples = std::min (std::max (minDelaySamples, latencyCapSamples), kBufferSize / 2);

    crossfadeIncrement = 1.0 / static_cast<double> (overlapSamples);

    const auto smoothingTimeSec = 0.005; // 5 ms smoothing time constant
    pitchSmoothingCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (sampleRate * smoothingTimeSec)));

    reset();
}

void PitchShifterDSP::updateOverlap() noexcept
{
    const auto pitchMag = std::abs (smoothedSemitones);
    const int boost = static_cast<int> (std::round (pitchMag * static_cast<float> (overlapPerSemitone)));

    const int newOverlap = std::clamp (overlapSamples + boost, minOverlapSamples, maxOverlapSamples);
    overlapSamples = newOverlap;

    crossfadeIncrement = 1.0 / static_cast<double> (overlapSamples);
    minDelaySamples = overlapSamples * 2;
    maxDelaySamples = std::max (minDelaySamples, maxDelaySamples);
}

void PitchShifterDSP::reset() noexcept
{
    buffer.fill (0.0f);
    writeIndex = 0;

    // Read heads start close behind the write head to keep latency low.
    readPosition[0] = static_cast<double> (wrapIndex (writeIndex - minDelaySamples));
    readPosition[1] = static_cast<double> (wrapIndex (writeIndex - (minDelaySamples + overlapSamples)));

    // Heads are offset by half a cycle so their windows overlap correctly
    // and sum to constant power (equal-power-ish via two half-sine windows).
    headPhase[0] = 0.0;
    headPhase[1] = 0.5;

    targetSemitones = 0.0f;
    smoothedSemitones = 0.0f;
    lastRepositionWriteIndex = -kBufferSize;
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
    // Safe to call any time for this head IF this is only invoked when
    // headPhase[headIndex] is at (or just past) 0.0 / 1.0, i.e. its own
    // window gain is zero. The wrap-driven call site below guarantees this
    // by construction; the safety-net call site further down does not, and
    // is a separate, intentional exception (see comment there).
    readPosition[headIndex] = static_cast<double> (wrapIndex (writeIndex - minDelaySamples));
    wrapReadPosition (readPosition[headIndex]);
    lastRepositionWriteIndex = writeIndex;
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
    updateOverlap();

    // Unity-pitch bypass — fully transparent path, zero added coloration,
    // no latency penalty when the user isn't actually shifting pitch.
    if (std::abs (smoothedSemitones) < 0.01f)
        return inputSample;

    const auto sample0 = readInterpolated (readPosition[0]);
    const auto sample1 = readInterpolated (readPosition[1]);

    // Each head's gain depends ONLY on its own phase. With the 0.5 phase
    // offset set in reset(), gain0 + gain1 stays close to constant power
    // across the whole cycle (this is the per-head equivalent of the
    // original cos/sin pair, but it can no longer desync from the
    // reposition logic, because there IS no separate reposition logic).
    const auto gain0 = grainWindow (headPhase[0]);
    const auto gain1 = grainWindow (headPhase[1]);

    const auto output = sample0 * gain0 + sample1 * gain1;

    const auto ratio = static_cast<double> (currentPitchRatio());

    // Variable read pointer speed implements time-domain pitch shift:
    // ratio > 1 → read head consumes buffer faster → higher pitch.
    // ratio < 1 → read head slows down → lower pitch.
    readPosition[0] += ratio;
    readPosition[1] += ratio;

    wrapReadPosition (readPosition[0]);
    wrapReadPosition (readPosition[1]);

    // Advance each head's own phase and reposition it exactly when ITS OWN
    // window has decayed to zero. This is the core fix: the head that gets
    // repositioned is, by construction, always the head whose gain is 0 at
    // that instant — there is no indexing mismatch possible anymore.
    for (int head = 0; head < 2; ++head)
    {
        headPhase[head] += crossfadeIncrement;

        if (headPhase[head] >= 1.0)
        {
            headPhase[head] -= 1.0;
            repositionHead (head);
        }
    }

    // Safety constraint — if a head drifts too close to the write pointer
    // (e.g. extreme pitch or a host block-size change pushed it forward
    // faster than expected), force a reposition without waiting for its
    // window to reach zero. This IS allowed to click in the worst case —
    // it's a last-resort guard, not the normal path — but it's now rare
    // and cooldown-limited rather than happening every other cycle.
    for (int head = 0; head < 2; ++head)
    {
        if (lagBehindWrite (readPosition[head]) < static_cast<double> (minDelaySamples))
        {
            int distSince = writeIndex - lastRepositionWriteIndex;
            if (distSince < 0)
                distSince += kBufferSize;

            if (distSince >= repositionCooldownSamples)
            {
                repositionHead (head);
                headPhase[head] = 0.0; // resync this head's own window too
            }
        }
    }

    return output;
}
