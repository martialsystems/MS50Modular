// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Eg1.h"
#include "Mg.h"
#include "Mixer.h"
#include "Noise.h"
#include "PatchGraph.h"
#include "Ring.h"
#include "SampleHold.h"
#include "Vca1.h"
#include "Vcf.h"
#include "Vco.h"

// Module numbers are the processor's addModule order.
namespace FactoryModule {
inline constexpr int Ext = 0;
inline constexpr int Output = 1;
inline constexpr int Noise = 2;
inline constexpr int Vcf = 3;
inline constexpr int Vca1 = 4;
inline constexpr int Eg1 = 6;
inline constexpr int Mg = 7;
inline constexpr int Vco = 8;
inline constexpr int Ring = 10;
inline constexpr int Mixer = 14;
inline constexpr int SampleHold = 15;
}

inline constexpr int kFactoryPresetCount = 7;
inline constexpr int kDefaultFactoryPreset = 2;
inline constexpr int kFeedbackPreset = 5;
inline constexpr int kHoldPreset = 6;
inline constexpr float kFeedbackVca1Initial = 0.70f;

// Hold knob travels. Attack 0.10 is a short fade. VCO range sits in the 8' footage, C3 at 130.8 Hz.
inline constexpr float kHoldEg1Attack = 0.10f;
inline constexpr float kHoldEg1Decay = 0.30f;
inline constexpr float kHoldEg1Sustain = 0.70f;
inline constexpr float kHoldEg1Release = 0.40f;
inline constexpr float kHoldVcfCutoff = 0.45f;
inline constexpr float kHoldVcfPeak = 0.20f;
inline constexpr float kHoldVcoRange = 2.0f / 3.0f;

inline float factoryVca1Initial (int index) noexcept
{
    return index == kFeedbackPreset ? kFeedbackVca1Initial : 0.0f;
}

// Host knobs restored when a program loads. Voice and Feedback use the shared row.
// Hold replaces the envelope, filter, and VCO range on that row.
struct FactoryProgramKnobs {
    float vcfCutoff;
    float vcfPeak;
    float eg1Attack;
    float eg1Decay;
    float eg1Sustain;
    float eg1Release;
    float vcoRange;
};

inline FactoryProgramKnobs factoryProgramKnobs (int index) noexcept
{
    FactoryProgramKnobs knobs { 0.50f, 0.30f, 0.50f, 0.30f, 0.68f, 0.42f, 0.50f };
    if (index == kHoldPreset)
    {
        knobs.vcfCutoff = kHoldVcfCutoff;
        knobs.vcfPeak = kHoldVcfPeak;
        knobs.eg1Attack = kHoldEg1Attack;
        knobs.eg1Decay = kHoldEg1Decay;
        knobs.eg1Sustain = kHoldEg1Sustain;
        knobs.eg1Release = kHoldEg1Release;
        knobs.vcoRange = kHoldVcoRange;
    }
    return knobs;
}

struct FactoryCable {
    int sourceModule;
    int sourcePort;
    int destModule;
    int destPort;
};

struct FactoryPreset {
    const char* name;
    bool effectOn;
    const FactoryCable* cables;
    int cableCount;
};

inline constexpr FactoryCable kPresetDry[] = {
    { FactoryModule::Ext, 0, FactoryModule::Output, 0 },
    { FactoryModule::Ext, 1, FactoryModule::Output, 1 },
};

inline constexpr FactoryCable kPresetNoiseToMixer[] = {
    { FactoryModule::Noise, NoiseModule::kWhite, FactoryModule::Mixer, Mixer::kIn1 },
    { FactoryModule::Mixer, Mixer::kOut, FactoryModule::Output, 2 },
};

inline constexpr FactoryCable kPresetVoice[] = {
    { FactoryModule::Ext, 2, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Ext, 0, FactoryModule::Output, 0 },
    { FactoryModule::Ext, 1, FactoryModule::Output, 1 },
    { FactoryModule::Ext, 3, FactoryModule::Eg1, Eg1::kTrig },
    { FactoryModule::Eg1, Eg1::kOutA, FactoryModule::Vca1, Vca1::kEnv },
    { FactoryModule::Eg1, Eg1::kOutA, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryCable kPresetRing[] = {
    { FactoryModule::Noise, NoiseModule::kWhite, FactoryModule::Ring, Ring::kA },
    { FactoryModule::Mg, MgModule::kTri, FactoryModule::Ring, Ring::kB },
    { FactoryModule::Ring, Ring::kOut, FactoryModule::Output, 2 },
};

inline constexpr FactoryCable kPresetSampleHold[] = {
    { FactoryModule::Noise, NoiseModule::kWhite, FactoryModule::SampleHold, SampleHold::kIn },
    { FactoryModule::SampleHold, SampleHold::kOut, FactoryModule::Vcf, Vcf::kCutoff },
    { FactoryModule::Ext, 2, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Output, 2 },
};

inline constexpr FactoryCable kPresetFeedback[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryCable kPresetHold[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Ext, 3, FactoryModule::Eg1, Eg1::kTrig },
    { FactoryModule::Eg1, Eg1::kOutA, FactoryModule::Vca1, Vca1::kEnv },
    { FactoryModule::Eg1, Eg1::kOutA, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryPreset kFactoryPresets[kFactoryPresetCount] = {
    { "Dry", false, kPresetDry, 2 },
    { "Noise to mixer", true, kPresetNoiseToMixer, 2 },
    { "Voice", true, kPresetVoice, 8 },
    { "Ring", true, kPresetRing, 3 },
    { "S&H", true, kPresetSampleHold, 4 },
    { "Feedback", true, kPresetFeedback, 4 },
    { "Hold", true, kPresetHold, 6 },
};

inline const char* factoryPresetName (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return "";
    return kFactoryPresets[index].name;
}

inline bool factoryPresetEffect (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return false;
    return kFactoryPresets[index].effectOn;
}

// Replaces cables. Mixer levels return to 0.8 and the sample-and-hold rate to 0.5 when those modules are present.
// Feedback sets VCA 1 Initial to 0.7. Every other preset clears it. The graph blob does not store that knob.
// Hold also writes its envelope, filter, and VCO range. Those knobs are already in the blob.
inline bool loadFactoryPreset (PatchGraph& graph, int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return false;
    if (index == 1 && graph.moduleCount() <= FactoryModule::Mixer)
        return false;

    const FactoryPreset& preset = kFactoryPresets[index];
    Cable cables[PatchGraph::kMaxCables] {};
    if (preset.cableCount < 0 || preset.cableCount > PatchGraph::kMaxCables)
        return false;
    for (int i = 0; i < preset.cableCount; ++i)
    {
        cables[i].sourceModule = preset.cables[i].sourceModule;
        cables[i].sourcePort = preset.cables[i].sourcePort;
        cables[i].destModule = preset.cables[i].destModule;
        cables[i].destPort = preset.cables[i].destPort;
    }
    if (! graph.setCables (cables, preset.cableCount))
        return false;

    if (graph.moduleCount() > FactoryModule::Mixer)
    {
        graph.writePresetKnob (FactoryModule::Mixer, Mixer::kKnobLevel1, 0.8f);
        graph.writePresetKnob (FactoryModule::Mixer, Mixer::kKnobLevel2, 0.8f);
        graph.writePresetKnob (FactoryModule::Mixer, Mixer::kKnobLevel3, 0.8f);
    }

    if (graph.moduleCount() > FactoryModule::SampleHold)
        graph.writePresetKnob (FactoryModule::SampleHold, SampleHold::kKnobRate, 0.50f);

    for (int module = 0; module < graph.moduleCount(); ++module)
        if (Module* item = graph.moduleAt (module))
            item->applyFactoryPreset (index);

    if (index == kHoldPreset)
    {
        const FactoryProgramKnobs knobs = factoryProgramKnobs (index);
        const bool wrote = graph.writePresetKnob (FactoryModule::Vcf, Vcf::kKnobCutoff, knobs.vcfCutoff)
                        && graph.writePresetKnob (FactoryModule::Vcf, Vcf::kKnobPeak, knobs.vcfPeak)
                        && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobAttack, knobs.eg1Attack)
                        && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobDecay, knobs.eg1Decay)
                        && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobSustain, knobs.eg1Sustain)
                        && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobRelease, knobs.eg1Release)
                        && graph.writePresetKnob (FactoryModule::Vco, Vco::kKnobScale, knobs.vcoRange);
        if (! wrote)
            return false;
    }
    return true;
}
