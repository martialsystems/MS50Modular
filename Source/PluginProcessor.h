// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Modular/ExtIn.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"

#include <juce_audio_processors/juce_audio_processors.h>

class MS50ModularAudioProcessor : public juce::AudioProcessor
{
public:
    MS50ModularAudioProcessor();
    ~MS50ModularAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Graph indices from addModule. Panel jack ids live in PanelGeometry.inc.
    int extInGraphIndex() const noexcept { return extModuleIndex_; }
    int outputGraphIndex() const noexcept { return outputModuleIndex_; }
    int noiseGraphIndex() const noexcept { return noiseModuleIndex_; }

    // Copies the snapshot process() reads. No allocation. Editor thread only.
    int copyPublishedCables (Cable* dest, int capacity) const;

    // Message thread only. The audio callback does not call these.
    PatchGraph::ConnectResult connectJacks (int sourceModule, int sourcePort, int destModule, int destPort);
    void disconnectJacks (int sourceModule, int sourcePort, int destModule, int destPort);
    void setOutputMix (float zeroToOne);

private:
    PatchGraph graph;
    ExtIn extIn;
    OutputModule output;
    NoiseModule noise;
    int extModuleIndex_ = -1;
    int outputModuleIndex_ = -1;
    int noiseModuleIndex_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS50ModularAudioProcessor)
};
