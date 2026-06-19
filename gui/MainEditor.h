#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "LookAndFeel.h"
#include "RotarySlider.h"

//==============================================================================
// Plugin editor UI — lives entirely on the message thread.
// Communicates with DSP only through APVTS parameters (lock-free).
//==============================================================================
class PitchMainEditor final : public juce::Component
{
public:
    PitchMainEditor (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    PitchLookAndFeel lookAndFeel;
    PitchRotarySlider pitchControl;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchMainEditor)
};
