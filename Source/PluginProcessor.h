// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Modular/Eg1.h"
#include "Modular/ExtIn.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "UI/FaceKnobs.h"

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
    int vcfGraphIndex() const noexcept { return vcfModuleIndex_; }
    int vca1GraphIndex() const noexcept { return vca1ModuleIndex_; }
    int vca2GraphIndex() const noexcept { return vca2ModuleIndex_; }
    int eg1GraphIndex() const noexcept { return eg1ModuleIndex_; }

    // Null when that column is still a picture.
    juce::AudioProcessorParameter* parameterForPanelKnob (const char* section, const char* label) const;
    juce::AudioProcessorParameter* effectParameter() noexcept { return effectOn_; }
    bool effectIsOn() const noexcept;

    // Copies the snapshot process() reads. No allocation. Editor thread only.
    int copyPublishedCables (Cable* dest, int capacity) const;

    // Message thread only. The audio callback does not call these.
    PatchGraph::ConnectResult connectJacks (int sourceModule, int sourcePort, int destModule, int destPort);
    void disconnectJacks (int sourceModule, int sourcePort, int destModule, int destPort);
    void setOutputMix (float zeroToOne);

private:
    void addKnobParameter (const FaceKnobBinding& binding);
    void applyHostControls();
    juce::AudioParameterFloat* floatParameter (FaceKnob knob) const noexcept;

    PatchGraph graph;
    ExtIn extIn;
    OutputModule output;
    NoiseModule noise;
    Vcf vcf;
    Vca1 vca1;
    Vca2 vca2;
    Eg1 eg1;
    int extModuleIndex_ = -1;
    int outputModuleIndex_ = -1;
    int noiseModuleIndex_ = -1;
    int vcfModuleIndex_ = -1;
    int vca1ModuleIndex_ = -1;
    int vca2ModuleIndex_ = -1;
    int eg1ModuleIndex_ = -1;

    juce::AudioParameterBool* effectOn_ = nullptr;
    juce::AudioParameterFloat* vcfCutoff_ = nullptr;
    juce::AudioParameterFloat* vcfPeak_ = nullptr;
    juce::AudioParameterFloat* vcfAmount_ = nullptr;
    juce::AudioParameterFloat* vca1LowCut_ = nullptr;
    juce::AudioParameterFloat* eg1Attack_ = nullptr;
    juce::AudioParameterFloat* eg1Decay_ = nullptr;
    juce::AudioParameterFloat* eg1Sustain_ = nullptr;
    juce::AudioParameterFloat* eg1Release_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS50ModularAudioProcessor)
};
