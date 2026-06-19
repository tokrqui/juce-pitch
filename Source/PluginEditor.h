#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class JucePitchAudioProcessor;

//==============================================================================
// Thin editor shell — delegates all layout to PitchMainEditor.
//==============================================================================
class JucePitchAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit JucePitchAudioProcessorEditor (JucePitchAudioProcessor&);
    ~JucePitchAudioProcessorEditor() override;

    void resized() override;

private:
    std::unique_ptr<juce::Component> mainEditor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JucePitchAudioProcessorEditor)
};
