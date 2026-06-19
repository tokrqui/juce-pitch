#include "RotarySlider.h"
#include "LookAndFeel.h"

PitchRotarySlider::PitchRotarySlider (PitchLookAndFeel& laf)
    : lookAndFeel (laf)
{
    slider.setLookAndFeel (&lookAndFeel);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                                juce::MathConstants<float>::pi * 2.8f,
                                true);
    slider.setDoubleClickReturnValue (true, 0.0);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (juce::FontOptions (22.0f).withStyle ("Bold"));
    valueLabel.setColour (juce::Label::textColourId, juce::Colour (0xffe0e0e0));
    valueLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (slider);
    addAndMakeVisible (valueLabel);

    slider.onValueChange = [this]
    {
        setSemitoneValue (static_cast<float> (slider.getValue()));
    };
}

void PitchRotarySlider::setSemitoneValue (float semitones)
{
    valueLabel.setText (formatSemitones (semitones), juce::dontSendNotification);
}

void PitchRotarySlider::resized()
{
    auto area = getLocalBounds();

    valueLabel.setBounds (area.removeFromBottom (36));
    slider.setBounds (area.reduced (8));
}

juce::String PitchRotarySlider::formatSemitones (float semitones)
{
    const auto sign = semitones > 0.0f ? "+" : "";
    return sign + juce::String (semitones, 1) + " st";
}
