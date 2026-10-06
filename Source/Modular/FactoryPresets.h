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

inline constexpr int kFactoryPresetCount = 6;
inline constexpr int kDefaultFactoryPreset = 2;

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

inline constexpr FactoryPreset kFactoryPresets[kFactoryPresetCount] = {
    { "Dry", false, kPresetDry, 2 },
    { "Noise to mixer", true, kPresetNoiseToMixer, 2 },
    { "Voice", true, kPresetVoice, 8 },
    { "Ring", true, kPresetRing, 3 },
    { "S&H", true, kPresetSampleHold, 4 },
    { "Feedback", true, kPresetFeedback, 4 },
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
    return true;
}
