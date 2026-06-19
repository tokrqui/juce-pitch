#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
// Minimal custom look-and-feel for the live-performance pitch control.
// Inherits LookAndFeel_V4 — no OpenGL, no external GUI libraries.
//==============================================================================
class PitchLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PitchLookAndFeel();

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

private:
    juce::Colour accentColour;
    juce::Colour trackColour;
    juce::Colour textColour;
};
