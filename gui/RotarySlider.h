#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class PitchLookAndFeel;

//==============================================================================
// Large rotary pitch control with a dedicated value readout.
// Wraps juce::Slider — no custom rendering frameworks.
//==============================================================================
class PitchRotarySlider final : public juce::Component
{
public:
    explicit PitchRotarySlider (PitchLookAndFeel& lookAndFeel);

    juce::Slider& getSlider() noexcept { return slider; }

    void setSemitoneValue (float semitones);

    void resized() override;

private:
    PitchLookAndFeel& lookAndFeel;
    juce::Slider slider;
    juce::Label valueLabel;

    static juce::String formatSemitones (float semitones);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchRotarySlider)
};
