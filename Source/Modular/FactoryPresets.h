// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Eg1.h"
#include "Integrator.h"
#include "Mg.h"
#include "Mixer.h"
#include "Noise.h"
#include "PanelDefaults.h"
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
inline constexpr int Integrator = 13;
inline constexpr int Mixer = 14;
inline constexpr int SampleHold = 15;
}

// The factory bank is cleared for now and will be rewritten. It holds one program, INIT: the fresh-instance patch.
// INIT is EXT IN Mono through the VCF and VCA 1 to Output Wet, the dry L and R cables, and EG 1 on the Ext In gate
// opening VCA 1 and moving the cutoff. Effect on, every knob on the PanelDefaults.h table, VCA 1 Initial 0.
// TODO(init-triangle): new patches should start the VCO on a true triangle. The VCO has one Tri output today
// (the integrated saw, a parabola; Vco.cpp) and no shape setting, so INIT cannot choose it yet.
inline constexpr int kFactoryPresetCount = 1;
inline constexpr int kDefaultFactoryPreset = 0;
inline constexpr int kInitPreset = 0;

// Factory loads set VCA 1 Initial. INIT leaves it at the default table value.
inline float factoryVca1Initial (int) noexcept
{
    return PanelDefault::kVca1Initial;
}

// Host knobs restored when a program loads. INIT uses the shared row from PanelDefaults.h.
// MG rate, S&H rate, and integrator time are separate restores.
struct FactoryProgramKnobs {
    float vcfCutoff;
    float vcfPeak;
    float eg1Attack;
    float eg1Decay;
    float eg1Sustain;
    float eg1Release;
    float vcoRange;
};

inline FactoryProgramKnobs factoryProgramKnobs (int) noexcept
{
    return { PanelDefault::kVcfCutoff, PanelDefault::kVcfPeak, PanelDefault::kEg1Attack,
             PanelDefault::kEg1Decay,  PanelDefault::kEg1Sustain, PanelDefault::kEg1Release,
             PanelDefault::kVcoRange };
}

inline float factoryMgRate (int) noexcept { return PanelDefault::kMgRate; }
inline float factorySampleHoldRate (int) noexcept { return PanelDefault::kSampleHoldRate; }
inline float factoryIntegratorTime (int) noexcept { return PanelDefault::kIntegratorTime; }

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

// Same eight cables, in the same order, as connectFactoryCables in DefaultPatch.h.
inline constexpr FactoryCable kPresetInit[] = {
    { FactoryModule::Ext, 2, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Ext, 0, FactoryModule::Output, 0 },
    { FactoryModule::Ext, 1, FactoryModule::Output, 1 },
    { FactoryModule::Ext, 3, FactoryModule::Eg1, Eg1::kTrig },
    { FactoryModule::Eg1, Eg1::kOutA, FactoryModule::Vca1, Vca1::kEnv },
    { FactoryModule::Eg1, Eg1::kOutA, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryPreset kFactoryPresets[kFactoryPresetCount] = {
    { "INIT", true, kPresetInit, 8 },
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

// Graph knobs a program writes beyond the mixer and S&H restores in loadFactoryPreset. INIT writes none.
inline bool writeFactoryProgramKnobs (PatchGraph&, int)
{
    return true;
}

// Replaces cables. Mixer levels return to 0.8 and the sample-and-hold rate to 0.5.
// Each module's applyFactoryPreset runs; VCA 1 sets Initial from factoryVca1Initial. The graph blob does not store that knob.
inline bool loadFactoryPreset (PatchGraph& graph, int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
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
        graph.writePresetKnob (FactoryModule::Mixer, Mixer::kKnobLevel1, PanelDefault::kMixerLevel);
        graph.writePresetKnob (FactoryModule::Mixer, Mixer::kKnobLevel2, PanelDefault::kMixerLevel);
        graph.writePresetKnob (FactoryModule::Mixer, Mixer::kKnobLevel3, PanelDefault::kMixerLevel);
    }

    if (graph.moduleCount() > FactoryModule::SampleHold)
        graph.writePresetKnob (FactoryModule::SampleHold, SampleHold::kKnobRate, PanelDefault::kSampleHoldRate);

    for (int module = 0; module < graph.moduleCount(); ++module)
        if (Module* item = graph.moduleAt (module))
            item->applyFactoryPreset (index);

    return writeFactoryProgramKnobs (graph, index);
}
