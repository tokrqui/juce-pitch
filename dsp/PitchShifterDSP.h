#pragma once

#include <array>
#include <cstddef>

// =============================================================================
// PitchShifterDSP
// -----------------------------------------------------------------------------
// Real-time, low-latency time-domain pitch shifter for voice.
// No pitch detection, no FFT — granular delay-line approach.
//
// Public contract (drop-in compatible with the previous implementation):
//   prepare(sampleRate)        — call once before use / on sample rate change
//   reset()                    — clear state, call on transport stop / bypass
//   setTargetSemitones(float)  — set desired shift, range [-12, +12]
//   processSample(float) -> float — process one sample, returns shifted sample
//
// Latency: bounded by (2 * grain length), target <= 5ms total.
// =============================================================================
class PitchShifterDSP
{
public:
    PitchShifterDSP() = default;

    // Exposed constants used by the host-facing code (APVTS ranges, UI, etc.)
    // Make these public so other translation units (e.g. PluginProcessor.cpp)
    // can reference them when creating parameter ranges.
    static constexpr float kMinSemitones = -12.0f;
    static constexpr float kMaxSemitones =  12.0f;
    // UI step / granularity for the pitch parameter (in semitones).
    static constexpr float kSemitoneStep = 0.1f;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void setTargetSemitones (float semitones) noexcept;

    float processSample (float inputSample) noexcept;

    // Optional but useful for host PDC (plugin delay compensation) reporting.
    int getLatencySamples() const noexcept { return grainLengthSamples; }

private:

    // Circular write buffer. Sized generously relative to expected grain
    // length range so read pointers always have safe room to roam.
    static constexpr int kBufferSize = 8192;
    static constexpr double kBufferSizeD = static_cast<double> (kBufferSize);

    std::array<float, kBufferSize> buffer {};
    int writeIndex = 0;

    // --- Grain engine -------------------------------------------------------
    // N overlapping grains (heads). Each has its own read position (in the
    // circular buffer, fractional) and its own lifetime phase in [0, 1).
    // A grain's gain is a pure function of ITS OWN phase, and a grain is only
    // ever rescheduled when ITS OWN phase wraps (i.e. its own gain is 0).
    // This makes "who is audible" and "who gets rescheduled" the same
    // variable by construction — there is no separate index to desync.
    static constexpr int kNumGrains = 4; // 4 grains, 25% phase stagger -> smoother than 2 on big shifts

    struct Grain
    {
        double readPos = 0.0;   // fractional position in circular buffer
        double phase   = 0.0;   // 0..1 lifetime position
        bool   primed  = false; // false until first scheduled (avoids garbage at start)
    };

    std::array<Grain, kNumGrains> grains {};

    int grainLengthSamples = 0;     // length of one grain, in samples
    double phaseIncrement  = 0.0;   // 1.0 / grainLengthSamples

    // --- Pitch smoothing ------------------------------------------------------
    float targetSemitones   = 0.0f;
    float smoothedSemitones = 0.0f;
    float pitchSmoothingCoeff = 0.0f;

    double sampleRateHz = 44100.0;

    // --- Helpers ---------------------------------------------------------------
    void   scheduleGrain (int grainIndex) noexcept;
    float  grainWindow (double phase) const noexcept;
    float  readInterpolated (double position) const noexcept;
    void   wrapPosition (double& position) const noexcept;
    float  currentPitchRatio() const noexcept;

    static int wrapIndex (int index) noexcept
    {
        index %= kBufferSize;
        if (index < 0)
            index += kBufferSize;
        return index;
    }
};
