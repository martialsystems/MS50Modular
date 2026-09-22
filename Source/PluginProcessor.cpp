// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginProcessor.h"
#include "PluginEditor.h"

MS50ModularAudioProcessor::MS50ModularAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

MS50ModularAudioProcessor::~MS50ModularAudioProcessor() = default;

void MS50ModularAudioProcessor::prepareToPlay (double, int)
{
}

void MS50ModularAudioProcessor::releaseResources()
{
}

bool MS50ModularAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1)
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MS50ModularAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numInputs = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();

    for (int channel = 0; channel < numOutputs; ++channel)
    {
        if (channel < numInputs)
        {
            const float* input = buffer.getReadPointer (channel);
            float* output = buffer.getWritePointer (channel);

            // VST3 presents one buffer. A self-copy would be an overlapping memcpy.
            if (output != input)
                juce::FloatVectorOperations::copy (output, input, numSamples);
        }
        else
        {
            buffer.clear (channel, 0, numSamples);
        }
    }
}

juce::AudioProcessorEditor* MS50ModularAudioProcessor::createEditor()
{
    return new MS50ModularAudioProcessorEditor (*this);
}

bool MS50ModularAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String MS50ModularAudioProcessor::getName() const
{
    return "MS-50 Modular";
}

bool MS50ModularAudioProcessor::acceptsMidi() const
{
    return false;
}

bool MS50ModularAudioProcessor::producesMidi() const
{
    return false;
}

bool MS50ModularAudioProcessor::isMidiEffect() const
{
    return false;
}

double MS50ModularAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MS50ModularAudioProcessor::getNumPrograms()
{
    // Hosts that divide by the program count reject a plugin that reports zero.
    return 1;
}

int MS50ModularAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MS50ModularAudioProcessor::setCurrentProgram (int)
{
}

const juce::String MS50ModularAudioProcessor::getProgramName (int)
{
    return {};
}

void MS50ModularAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void MS50ModularAudioProcessor::getStateInformation (juce::MemoryBlock&)
{
}

void MS50ModularAudioProcessor::setStateInformation (const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MS50ModularAudioProcessor();
}
