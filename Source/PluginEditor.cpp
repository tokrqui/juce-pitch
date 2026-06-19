#include "PluginEditor.h"
#include "PluginProcessor.h"

#include "MainEditor.h"

JucePitchAudioProcessorEditor::JucePitchAudioProcessorEditor (JucePitchAudioProcessor& processor)
    : AudioProcessorEditor (&processor)
{
    mainEditor = std::make_unique<PitchMainEditor> (processor.getAPVTS());
    addAndMakeVisible (*mainEditor);

    setSize (360, 420);
    setResizable (false, false);
}

JucePitchAudioProcessorEditor::~JucePitchAudioProcessorEditor() = default;

void JucePitchAudioProcessorEditor::resized()
{
    mainEditor->setBounds (getLocalBounds());
}
