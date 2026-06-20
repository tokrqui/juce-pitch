#pragma once

#include <array>
#include <cmath>
#include <cstdint>

//==============================================================================
// Realtime time-domain pitch shifter for live performance.
//
// Algorithm: dual read heads on a circular delay line with equal-power
// crossfading and fractional (Hermite) interpolation. No FFT or spectral
// processing is used anywhere in this class.
//
// Each input sample is written once; two read pointers traverse the buffer at
// a rate proportional to the pitch ratio. When a pointer approaches the write
// head, it is repositioned to a safe lag while the other pointer carries the
// output through a crossfade, eliminating discontinuity clicks.
//==============================================================================
class PitchShifterDSP
{
public:
    static constexpr float kMinSemitones = -12.0f;
    static constexpr float kMaxSemitones =  12.0f;
    static constexpr float kSemitoneStep   =   0.5f;

    PitchShifterDSP() noexcept = default;

    // Called on the message thread or during prepareToPlay — never allocates.
    void prepare (double sampleRate) noexcept;

    // Clears buffer state; safe to call when transport stops.
    void reset() noexcept;

    // Sets the target pitch in semitones. Smoothing happens inside processSample().
    void setTargetSemitones (float semitones) noexcept;

    // Processes one sample and returns the pitch-shifted result.
    [[nodiscard]] float processSample (float inputSample) noexcept;

private:
    static constexpr int   kBufferSize  = 16384;
    static constexpr int   kBufferMask  = kBufferSize - 1;
    static constexpr double kBufferSizeD = static_cast<double> (kBufferSize);

    // Delay line — fixed storage, allocated once with the object.
    std::array<float, static_cast<size_t> (kBufferSize)> buffer {};

    int   writeIndex = 0;
    double readPosition[2] { 0.0, 0.0 };
    double headPhase[2] { 0.0, 0.5 };

    float targetSemitones  = 0.0f;
    float smoothedSemitones = 0.0f;
    float pitchSmoothingCoeff = 0.01f;

    // Tunable buffer/delay sizes. Defaults were large causing ~100ms latency on
    // typical sample rates. We cap the practical delay to keep monitoring
    // latency low (<= 5 ms) while keeping overlap long enough for smooth
    // crossfades.
    int overlapSamples = 128; // small default (~2.7ms @48kHz)
    // Adaptive overlap settings
    int minOverlapSamples = 32;
    int maxOverlapSamples = 1024;
    int overlapPerSemitone = 24; // samples added per semitone of shift
    int minDelaySamples = 256; // default (~5.3ms @48kHz)
    int maxDelaySamples = 256; // hard cap to enforce low-latency behavior
    double crossfadeIncrement = 1.0 / 480.0;

    // Prevent repeated forced repositions: after a reposition occur, wait this
    // many samples before allowing another safety-driven reposition.
    int repositionCooldownSamples = 0;
    int lastRepositionWriteIndex = -kBufferSize;

    void updateOverlap() noexcept;

    // Hermite interpolation — four-point, low CPU, good enough for live pitch.
    [[nodiscard]] float readInterpolated (double position) const noexcept;

    [[nodiscard]] int wrapIndex (int index) const noexcept
    {
        return index & kBufferMask;
    }

    void wrapReadPosition (double& position) const noexcept;

    // Circular distance from read position to write index (in samples behind write).
    [[nodiscard]] double lagBehindWrite (double readPos) const noexcept;

    void repositionHead (int headIndex) noexcept;

    [[nodiscard]] float currentPitchRatio() const noexcept;

    void updateSmoothedPitch() noexcept;
};
