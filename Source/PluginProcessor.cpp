#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { JucePitchAudioProcessor::kPitchParamId, 1 },
            "Pitch",
            juce::NormalisableRange<float> (PitchShifterDSP::kMinSemitones,
                                            PitchShifterDSP::kMaxSemitones,
                                            PitchShifterDSP::kSemitoneStep),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("st")));

        return layout;
    }
}

JucePitchAudioProcessor::JucePitchAudioProcessor()
    : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
 #if ! JucePlugin_IsSynth
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
 #endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                      ),
      apvts (*this, nullptr, "Parameters", createLayout())
{
}

JucePitchAudioProcessor::~JucePitchAudioProcessor() = default;

const juce::String JucePitchAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JucePitchAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool JucePitchAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool JucePitchAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double JucePitchAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int JucePitchAudioProcessor::getNumPrograms()
{
    return 1;
}

int JucePitchAudioProcessor::getCurrentProgram()
{
    return 0;
}

void JucePitchAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String JucePitchAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void JucePitchAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void JucePitchAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    for (auto& shifter : pitchShifters)
        shifter.prepare (sampleRate);
}

void JucePitchAudioProcessor::releaseResources()
{
}

bool JucePitchAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono()
        && out != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != out)
        return false;
#endif

    return true;
#endif
}

void JucePitchAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    const auto pitchSemitones = apvts.getRawParameterValue (kPitchParamId)->load();

    for (auto channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto& shifter = pitchShifters[static_cast<size_t> (channel)];
        shifter.setTargetSemitones (pitchSemitones);

        auto* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = shifter.processSample (channelData[sample]);
    }
}

bool JucePitchAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JucePitchAudioProcessor::createEditor()
{
    return new JucePitchAudioProcessorEditor (*this);
}

void JucePitchAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void JucePitchAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JucePitchAudioProcessor();
}
