// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginProcessor.h"
#include "PluginEditor.h"

MS50ModularAudioProcessor::MS50ModularAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    extModuleIndex_ = graph.addModule (extIn);
    outputModuleIndex_ = graph.addModule (output);
    graph.connect (extModuleIndex_, 0, outputModuleIndex_, 0);
    graph.connect (extModuleIndex_, 1, outputModuleIndex_, 1);
}

int MS50ModularAudioProcessor::copyPublishedCables (Cable* dest, int capacity) const
{
    return graph.copyPublishedCables (dest, capacity);
}

namespace {

bool onMessageThread()
{
    auto* messages = juce::MessageManager::getInstanceWithoutCreating();
    return messages != nullptr && messages->isThisTheMessageThread();
}

}

PatchGraph::ConnectResult MS50ModularAudioProcessor::connectJacks (int sourceModule, int sourcePort,
                                                                  int destModule, int destPort)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return PatchGraph::ConnectResult::Rejected;
    return graph.attemptConnect (sourceModule, sourcePort, destModule, destPort);
}

void MS50ModularAudioProcessor::disconnectJacks (int sourceModule, int sourcePort, int destModule, int destPort)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return;
    graph.disconnect (sourceModule, sourcePort, destModule, destPort);
}

void MS50ModularAudioProcessor::setOutputMix (float zeroToOne)
{
    jassert (onMessageThread());
    if (! onMessageThread())
        return;
    output.setMix (zeroToOne);
}

MS50ModularAudioProcessor::~MS50ModularAudioProcessor() = default;

void MS50ModularAudioProcessor::prepareToPlay (double sampleRate, int)
{
    graph.prepare (sampleRate);
    extIn.prepare (sampleRate);
    output.prepare (sampleRate);
    setLatencySamples (0);
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

    for (int channel = numInputs; channel < numOutputs; ++channel)
        buffer.clear (channel, 0, numSamples);

    if (numInputs < 2 || numOutputs < 2)
        return;

    float* left = buffer.getWritePointer (0);
    float* right = buffer.getWritePointer (1);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inLeft = left[i];
        const float inRight = right[i];
        extIn.setHostSample (inLeft, inRight);
        graph.process();
        left[i] = output.hostLeft();
        right[i] = output.hostRight();
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
