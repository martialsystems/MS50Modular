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

inline constexpr int kFactoryPresetCount = 13;
inline constexpr int kDefaultFactoryPreset = 2;
inline constexpr int kFeedbackPreset = 5;
inline constexpr int kHoldPreset = 6;
inline constexpr int kFilterLoopPreset = 7;
inline constexpr int kMgFilterPreset = 8;
inline constexpr int kSteppedCutoffPreset = 9;
inline constexpr int kRingDronePreset = 10;
inline constexpr int kDelayedBouncePreset = 11;
inline constexpr int kSelfRingPreset = 12;
inline constexpr float kFeedbackVca1Initial = 0.70f;

// Hold knob travels. Attack 0.10 is a short fade. VCO range sits in the 8' footage, C3 at 130.8 Hz.
inline constexpr float kHoldEg1Attack = 0.10f;
inline constexpr float kHoldEg1Decay = 0.30f;
inline constexpr float kHoldEg1Sustain = 0.70f;
inline constexpr float kHoldEg1Release = 0.40f;
inline constexpr float kHoldVcfCutoff = 0.45f;
inline constexpr float kHoldVcfPeak = 0.20f;
inline constexpr float kHoldVcoRange = 2.0f / 3.0f;

// Self-mod travels. Unspecified knobs stay on the shared row below.
inline constexpr float kFilterLoopCutoff = 0.40f;
inline constexpr float kFilterLoopPeak = 0.70f;
inline constexpr float kMgFilterRate = 0.30f;
inline constexpr float kSteppedCutoffRate = 0.40f;
inline constexpr float kRingDroneRate = 0.25f;
inline constexpr float kDelayedBounceTime = 0.60f;

inline float factoryVca1Initial (int index) noexcept
{
    const bool open = index == kFeedbackPreset
                   || (index >= kFilterLoopPreset && index <= kSelfRingPreset);
    return open ? kFeedbackVca1Initial : 0.0f;
}

// Host knobs restored when a program loads. Voice and Feedback use the shared row from PanelDefaults.h.
// Hold replaces the envelope, filter, and VCO range on that row.
// Filter loop replaces cutoff and peak. MG rate, S&H rate, and integrator time are separate restores.
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
    FactoryProgramKnobs knobs { PanelDefault::kVcfCutoff, PanelDefault::kVcfPeak, PanelDefault::kEg1Attack,
                                PanelDefault::kEg1Decay,  PanelDefault::kEg1Sustain, PanelDefault::kEg1Release,
                                PanelDefault::kVcoRange };
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
    else if (index == kFilterLoopPreset)
    {
        knobs.vcfCutoff = kFilterLoopCutoff;
        knobs.vcfPeak = kFilterLoopPeak;
    }
    return knobs;
}

inline float factoryMgRate (int index) noexcept
{
    if (index == kMgFilterPreset)
        return kMgFilterRate;
    if (index == kRingDronePreset)
        return kRingDroneRate;
    return PanelDefault::kMgRate;
}

inline float factorySampleHoldRate (int index) noexcept
{
    return index == kSteppedCutoffPreset ? kSteppedCutoffRate : PanelDefault::kSampleHoldRate;
}

inline float factoryIntegratorTime (int index) noexcept
{
    return index == kDelayedBouncePreset ? kDelayedBounceTime : PanelDefault::kIntegratorTime;
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

// The cutoff cable is last, so it is the newest edge that closes on the VCF.
inline constexpr FactoryCable kPresetFilterLoop[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryCable kPresetMgFilter[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Mg, MgModule::kTri, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryCable kPresetSteppedCutoff[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    { FactoryModule::Noise, NoiseModule::kWhite, FactoryModule::SampleHold, SampleHold::kIn },
    { FactoryModule::SampleHold, SampleHold::kOut, FactoryModule::Vcf, Vcf::kCutoff },
};

inline constexpr FactoryCable kPresetRingDrone[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Ring, Ring::kA },
    { FactoryModule::Mg, MgModule::kTri, FactoryModule::Ring, Ring::kB },
    { FactoryModule::Ring, Ring::kOut, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
};

inline constexpr FactoryCable kPresetDelayedBounce[] = {
    { FactoryModule::Noise, NoiseModule::kWhite, FactoryModule::Integrator, Integrator::kIn },
    { FactoryModule::Integrator, Integrator::kOut, FactoryModule::Vcf, Vcf::kCutoff },
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
};

// Ring Out to Ring B is the self input. It is listed before the forward path.
// Same-module edges close immediately, and the later edges do not close, so this cable stays the delayed one.
inline constexpr FactoryCable kPresetSelfRing[] = {
    { FactoryModule::Vco, Vco::kSaw, FactoryModule::Ring, Ring::kA },
    { FactoryModule::Ring, Ring::kOut, FactoryModule::Ring, Ring::kB },
    { FactoryModule::Ring, Ring::kOut, FactoryModule::Vcf, Vcf::kSigIn },
    { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
    { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
};

inline constexpr FactoryPreset kFactoryPresets[kFactoryPresetCount] = {
    { "Dry", false, kPresetDry, 2 },
    { "Noise to mixer", true, kPresetNoiseToMixer, 2 },
    { "Voice", true, kPresetVoice, 8 },
    { "Ring", true, kPresetRing, 3 },
    { "S&H", true, kPresetSampleHold, 4 },
    { "Feedback", true, kPresetFeedback, 4 },
    { "Hold", true, kPresetHold, 6 },
    { "Filter loop", true, kPresetFilterLoop, 4 },
    { "MG into filter", true, kPresetMgFilter, 4 },
    { "Stepped cutoff", true, kPresetSteppedCutoff, 5 },
    { "Ring drone", true, kPresetRingDrone, 5 },
    { "Delayed bounce", true, kPresetDelayedBounce, 5 },
    { "Self ring", true, kPresetSelfRing, 5 },
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

inline bool writeFactoryProgramKnobs (PatchGraph& graph, int index)
{
    if (index == kHoldPreset)
    {
        const FactoryProgramKnobs knobs = factoryProgramKnobs (index);
        return graph.writePresetKnob (FactoryModule::Vcf, Vcf::kKnobCutoff, knobs.vcfCutoff)
            && graph.writePresetKnob (FactoryModule::Vcf, Vcf::kKnobPeak, knobs.vcfPeak)
            && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobAttack, knobs.eg1Attack)
            && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobDecay, knobs.eg1Decay)
            && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobSustain, knobs.eg1Sustain)
            && graph.writePresetKnob (FactoryModule::Eg1, Eg1::kKnobRelease, knobs.eg1Release)
            && graph.writePresetKnob (FactoryModule::Vco, Vco::kKnobScale, knobs.vcoRange);
    }

    if (index == kFilterLoopPreset)
    {
        const FactoryProgramKnobs knobs = factoryProgramKnobs (index);
        return graph.writePresetKnob (FactoryModule::Vcf, Vcf::kKnobCutoff, knobs.vcfCutoff)
            && graph.writePresetKnob (FactoryModule::Vcf, Vcf::kKnobPeak, knobs.vcfPeak);
    }

    if (index == kMgFilterPreset || index == kRingDronePreset)
        return graph.writePresetKnob (FactoryModule::Mg, MgModule::kKnobFrequency, factoryMgRate (index));

    if (index == kSteppedCutoffPreset)
        return graph.writePresetKnob (FactoryModule::SampleHold, SampleHold::kKnobRate, factorySampleHoldRate (index));

    if (index == kDelayedBouncePreset)
        return graph.writePresetKnob (FactoryModule::Integrator, Integrator::kKnobTime, factoryIntegratorTime (index));

    return true;
}

// Replaces cables. Mixer levels return to 0.8. The sample-and-hold rate returns to 0.5, then Stepped cutoff writes 0.4.
// Feedback and the six self-mod presets set VCA 1 Initial to 0.7. Voice, Hold, and the other presets clear it.
// The graph blob does not store that knob. Hold writes its envelope, filter, and VCO range.
// Filter loop writes cutoff and peak. MG into filter and Ring drone write the MG rate. Delayed bounce writes integrator time.
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
