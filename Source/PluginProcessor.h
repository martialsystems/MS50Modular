// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Modular/Divider.h"
#include "Modular/FactoryPresets.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/ExtIn.h"
#include "Modular/Integrator.h"
#include "Modular/Inverter.h"
#include "Modular/Mg.h"
#include "Modular/Mixer.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/Ring.h"
#include "Modular/SampleHold.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "Modular/Vco.h"
#include "UI/FaceKnobs.h"
#include "UI/Meter.h"

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
    const juce::String& presetError() const noexcept { return presetError_; }

    // Graph indices from addModule. Panel jack ids live in PanelGeometry.inc.
    int extInGraphIndex() const noexcept { return extModuleIndex_; }
    int outputGraphIndex() const noexcept { return outputModuleIndex_; }
    int noiseGraphIndex() const noexcept { return noiseModuleIndex_; }
    int vcfGraphIndex() const noexcept { return vcfModuleIndex_; }
    int vca1GraphIndex() const noexcept { return vca1ModuleIndex_; }
    int vca2GraphIndex() const noexcept { return vca2ModuleIndex_; }
    int eg1GraphIndex() const noexcept { return eg1ModuleIndex_; }
    int mgGraphIndex() const noexcept { return mgModuleIndex_; }
    int vcoGraphIndex() const noexcept { return vcoModuleIndex_; }
    int eg2GraphIndex() const noexcept { return eg2ModuleIndex_; }
    int ringGraphIndex() const noexcept { return ringModuleIndex_; }
    int dividerGraphIndex() const noexcept { return dividerModuleIndex_; }
    int inverterGraphIndex() const noexcept { return inverterModuleIndex_; }
    int integratorGraphIndex() const noexcept { return integratorModuleIndex_; }
    int mixerGraphIndex() const noexcept { return mixerModuleIndex_; }
    int sampleHoldGraphIndex() const noexcept { return sampleHoldModuleIndex_; }

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
    void setExtInButtonHeld (bool held);

    // Output Wet until a jack has been selected. Selecting does not patch.
    Meter& meter() noexcept { return meter_; }
    float meterVolts() const noexcept;

private:
    void addKnobParameter (const FaceKnobBinding& binding);
    void applyHostControls();
    void applyProgramParameters (int index);
    juce::AudioParameterFloat* floatParameter (FaceKnob knob) const noexcept;

    PatchGraph graph;
    ExtIn extIn;
    OutputModule output;
    NoiseModule noise;
    Vcf vcf;
    Vca1 vca1;
    Vca2 vca2;
    Eg1 eg1;
    MgModule mg;
    Vco vco;
    Eg2 eg2;
    Ring ring;
    Divider divider;
    Inverter inverter;
    Integrator integrator;
    Mixer mixer;
    SampleHold sampleHold;
    int extModuleIndex_ = -1;
    int outputModuleIndex_ = -1;
    int noiseModuleIndex_ = -1;
    int vcfModuleIndex_ = -1;
    int vca1ModuleIndex_ = -1;
    int vca2ModuleIndex_ = -1;
    int eg1ModuleIndex_ = -1;
    int mgModuleIndex_ = -1;
    int vcoModuleIndex_ = -1;
    int eg2ModuleIndex_ = -1;
    int ringModuleIndex_ = -1;
    int dividerModuleIndex_ = -1;
    int inverterModuleIndex_ = -1;
    int integratorModuleIndex_ = -1;
    int mixerModuleIndex_ = -1;
    int sampleHoldModuleIndex_ = -1;

    juce::AudioParameterBool* effectOn_ = nullptr;
    juce::AudioParameterFloat* vcfCutoff_ = nullptr;
    juce::AudioParameterFloat* vcfPeak_ = nullptr;
    juce::AudioParameterFloat* vcfAmount_ = nullptr;
    juce::AudioParameterFloat* vca1LowCut_ = nullptr;
    juce::AudioParameterFloat* eg1Attack_ = nullptr;
    juce::AudioParameterFloat* eg1Decay_ = nullptr;
    juce::AudioParameterFloat* eg1Sustain_ = nullptr;
    juce::AudioParameterFloat* eg1Release_ = nullptr;
    juce::AudioParameterFloat* mgRate_ = nullptr;
    juce::AudioParameterFloat* mgPw_ = nullptr;
    juce::AudioParameterFloat* vcoRange_ = nullptr;
    juce::AudioParameterFloat* vcoFine_ = nullptr;
    juce::AudioParameterFloat* vcoPw_ = nullptr;
    juce::AudioParameterFloat* vcoFm1_ = nullptr;
    juce::AudioParameterFloat* vcoFm2_ = nullptr;
    juce::AudioParameterFloat* eg2Hold_ = nullptr;
    juce::AudioParameterFloat* eg2Delay_ = nullptr;
    juce::AudioParameterFloat* eg2Attack_ = nullptr;
    juce::AudioParameterFloat* eg2Release_ = nullptr;
    juce::AudioParameterFloat* integratorTime_ = nullptr;
    juce::AudioParameterFloat* mixerLevel1_ = nullptr;
    juce::AudioParameterFloat* mixerLevel2_ = nullptr;
    juce::AudioParameterFloat* mixerLevel3_ = nullptr;
    juce::AudioParameterFloat* sampleHoldRate_ = nullptr;
    Meter meter_;
    int currentProgram_ = kDefaultFactoryPreset;
    juce::String presetError_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS50ModularAudioProcessor)
};
