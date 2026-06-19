#include "MainEditor.h"

namespace
{
    constexpr const char* kPitchParamId = "pitchSemitones";
}

PitchMainEditor::PitchMainEditor (juce::AudioProcessorValueTreeState& apvts)
    : pitchControl (lookAndFeel)
{
    setOpaque (true);

    pitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, kPitchParamId, pitchControl.getSlider());

    addAndMakeVisible (pitchControl);

    pitchControl.setSemitoneValue (static_cast<float> (pitchControl.getSlider().getValue()));
}

void PitchMainEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1a));
}

void PitchMainEditor::resized()
{
    auto area = getLocalBounds().reduced (24);
    const auto controlSize = juce::jmin (area.getWidth(), area.getHeight() - 40);

    pitchControl.setBounds (area.withSizeKeepingCentre (controlSize, controlSize + 36));
}
