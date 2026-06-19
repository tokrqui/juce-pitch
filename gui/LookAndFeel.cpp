#include "LookAndFeel.h"

PitchLookAndFeel::PitchLookAndFeel()
    : accentColour  (juce::Colour (0xff4fc3f7)),
      trackColour   (juce::Colour (0xff2a2a2a)),
      textColour    (juce::Colour (0xffe0e0e0))
{
    setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    setColour (juce::Slider::rotarySliderOutlineColourId, trackColour);
    setColour (juce::Slider::thumbColourId, accentColour);
    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void PitchLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x),
                                                static_cast<float> (y),
                                                static_cast<float> (width),
                                                static_cast<float> (height)).reduced (6.0f);

    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto arcThickness = radius * 0.14f;

    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    g.setColour (trackColour);
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, arcThickness);

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y,
                       radius - arcThickness * 0.5f,
                       radius - arcThickness * 0.5f,
                       0.0f,
                       rotaryStartAngle,
                       angle,
                       true);

    g.setColour (accentColour);
    g.strokePath (arc, juce::PathStrokeType (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto thumbRadius = arcThickness * 0.9f;
    const auto thumbPoint = centre.getPointOnCircumference (radius - arcThickness * 1.2f, angle);

    g.setColour (accentColour.brighter (0.25f));
    g.fillEllipse (thumbPoint.x - thumbRadius,
                   thumbPoint.y - thumbRadius,
                   thumbRadius * 2.0f,
                   thumbRadius * 2.0f);
}
