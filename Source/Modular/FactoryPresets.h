// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Eg1.h"
#include "Eg2.h"
#include "ExtIn.h"
#include "Integrator.h"
#include "Mg.h"
#include "Mixer.h"
#include "Noise.h"
#include "PanelDefaults.h"
#include "PatchGraph.h"
#include "PatchState.h"
#include "Ring.h"
#include "SampleHold.h"
#include "Vca1.h"
#include "Vca2.h"
#include "Vcf.h"
#include "Vco.h"

#include <cstddef>
#include <cstring>

// Module numbers are the processor's addModule order.
namespace FactoryModule {
inline constexpr int Ext = 0;
inline constexpr int Output = 1;
inline constexpr int Noise = 2;
inline constexpr int Vcf = 3;
inline constexpr int Vca1 = 4;
inline constexpr int Vca2 = 5;
inline constexpr int Eg1 = 6;
inline constexpr int Mg = 7;
inline constexpr int Vco = 8;
inline constexpr int Eg2 = 9;
inline constexpr int Ring = 10;
inline constexpr int Divider = 11;
inline constexpr int Inverter = 12;
inline constexpr int Integrator = 13;
inline constexpr int Mixer = 14;
inline constexpr int SampleHold = 15;
}

// The factory bank (docs/presets.md). 01 INIT is the fresh-instance patch: EXT IN Mono through the VCF and VCA 1
// to Output Wet, the dry L and R cables, and EG 1 on the Ext In gate opening VCA 1 and moving the cutoff. Effect
// on, every knob on the PanelDefaults.h table, VCA 1 Initial 0. The other programs are written the way state
// format 2 saves a patch: knobs by host parameter id (only the ones that differ from the PanelDefaults.h table),
// cables by canonical jack id, oldest first. Every program starts the VCO on the true TRIANGLE
// (Vco::applyFactoryPreset; RONIN_Redesign M-R2, decided by the user). PARABOLA (legacy) stays selectable on the
// VOICE tab and is what user-saved format-1 states load on.
inline constexpr int kFactoryPresetCount = 22;
inline constexpr int kDefaultFactoryPreset = 0;
inline constexpr int kInitPreset = 0;

struct FactoryKnob {
    const char* id;     // host parameter id (state format 2 attribute)
    float value;        // knob travel 0..1
};

struct FactoryJackCable {
    const char* from;   // canonical jack id, JCS R6 bare SECTION:LABEL
    const char* to;
};

struct FactoryPreset {
    const char* name;          // 12 characters at most: the PRESET menu shows ">NN NAME" on 16 LCD cells
    const char* description;   // what it does and what it expects at EXT IN
    bool effectOn;
    const FactoryJackCable* cables;
    int cableCount;
    const FactoryKnob* knobs;
    int knobCount;
};

// Program knobs and cables. Clock rates are written for 125 BPM: MG RATE 0.6791 is 8.33 Hz (16ths), 0.6091 is
// 4.17 Hz (8ths), 0.5391 is 2.08 Hz (quarters), 0.4691 is 1.04 Hz (half notes); S&H RATE 0.6403 is 8.33 Hz and
// 0.5399 is 4.17 Hz. EG times are the real-time labels (secondsFor: 0.0630 = 2 ms, 0.2093 = 10 ms, 0.3556 = 50 ms,
// 0.4351 = 120 ms, 0.4816 = 200 ms, 0.5184 = 300 ms). Cutoffs are voiced with the 0.004 drive pull in place.
namespace factorybank {

#define RONIN_DRY_CABLES { "EXT IN:L", "OUTPUT:L" }, { "EXT IN:R", "OUTPUT:R" }

inline constexpr FactoryJackCable kInitCables[] = {
    { "EXT IN:MONO", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "EXT IN:GATE", "EG 1:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" }, { "EG 1:OUT A", "VCF:CUTOFF" },
};

// ---- Effects on the input (EXT IN) ----

inline constexpr FactoryJackCable kAutoFilterCables[] = {
    { "EXT IN:MONO", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:TRI", "VCF:CUTOFF" },
};
inline constexpr FactoryKnob kAutoFilterKnobs[] = {
    { "vcfCutoff", 0.56f }, { "vcfPeak", 0.62f }, { "vcfAmount", 0.75f }, { "vca1Initial", 1.0f },
    { "mgRate", 0.3991f }, { "mgPw", 0.5f },
};

inline constexpr FactoryJackCable kEnvFilterCables[] = {
    { "EXT IN:MONO", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "EXT IN:GATE", "EG 1:TRIG" }, { "EG 1:OUT A", "VCF:CUTOFF" },
};
inline constexpr FactoryKnob kEnvFilterKnobs[] = {
    { "vcfCutoff", 0.40f }, { "vcfPeak", 0.72f }, { "vcfAmount", 0.80f }, { "vca1Initial", 1.0f },
    { "eg1Attack", 0.2093f }, { "eg1Decay", 0.5184f }, { "eg1Sustain", 0.25f }, { "eg1Release", 0.4816f },
};

inline constexpr FactoryJackCable kTranceGateCables[] = {
    { "EXT IN:MONO", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "INT:IN" }, { "INT:OUT", "VCA 1:ENV" },
};
inline constexpr FactoryKnob kTranceGateKnobs[] = {
    { "vca1Mod", 1.0f }, { "mgRate", 0.6791f }, { "mgPw", 0.6111f }, { "integratorTime", 0.2117f },
};

inline constexpr FactoryJackCable kPumpCables[] = {
    { "EXT IN:MONO", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 2:TRIG" }, { "EG 2:OUT \xe2\x88\x92", "VCA 1:ENV" },   // the panel label is "OUT " + U+2212 MINUS SIGN
};
inline constexpr FactoryKnob kPumpKnobs[] = {
    { "vca1Initial", 1.0f }, { "vca1Mod", 1.0f }, { "mgRate", 0.5391f }, { "mgPw", 0.5f },
    { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f }, { "eg2Release", 0.5184f },
};

inline constexpr FactoryJackCable kSampleHoldFilterCables[] = {
    { "EXT IN:MONO", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "NOISE:WHITE", "S&H:IN" }, { "S&H:OUT", "VCF:CUTOFF" },
};
inline constexpr FactoryKnob kSampleHoldFilterKnobs[] = {
    { "vcfCutoff", 0.58f }, { "vcfPeak", 0.70f }, { "vcfAmount", 0.55f }, { "vca1Initial", 1.0f },
    { "sampleHoldRate", 0.6403f },
};

inline constexpr FactoryJackCable kRingModCables[] = {
    { "EXT IN:MONO", "RING:A" }, { "VCO:TRI", "RING:B" }, { "RING:OUT", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" },
    { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES, { "MG:TRI", "VCO:FM 1" },
};
inline constexpr FactoryKnob kRingModKnobs[] = {
    { "vcoRange", 0.6667f }, { "vcoFm1", 0.12f }, { "vcfCutoff", 0.80f }, { "vcfPeak", 0.10f },
    { "vca1Initial", 1.0f }, { "vca1Mod", 1.0f }, { "mgRate", 0.3000f },
};

// ---- Acid basses: self-clocked 16ths at 125 BPM ----
// MG PULSE clocks EG 1 (the VCA gate), EG 2 (the filter snap) and the divider. DIV /2 and /4 through the mixer
// and inverter write a four-step pitch pattern, which INT slews (the slide) into VCO V/OCT. In the rack, a
// sequencer's pitch goes into INT IN (it sums with the pattern; turn MIX LEVEL 1 and 2 down for its notes alone),
// its gate replaces the MG PULSE cables, and an accent CV can go into VCF CUTOFF.

#define RONIN_ACID_CORE                                                                                            \
    { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,                                  \
    { "MG:PULSE", "EG 1:TRIG" }, { "MG:PULSE", "EG 2:TRIG" }, { "MG:PULSE", "DIV:IN" },                          \
    { "EG 1:OUT A", "VCA 1:ENV" }, { "EG 2:OUT +", "VCF:CUTOFF" },                                               \
    { "DIV:/2", "MIX:IN 1" }, { "DIV:/4", "MIX:IN 2" }, { "MIX:OUT", "INV:IN" }, { "INV:OUT", "INT:IN" },        \
    { "INT:OUT", "VCO:V/OCT" }

inline constexpr FactoryJackCable kAcidLineCables[] = { { "VCO:SAW", "VCF:IN" }, RONIN_ACID_CORE };
inline constexpr FactoryKnob kAcidLineKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcfCutoff", 0.34f }, { "vcfPeak", 0.85f }, { "vcfAmount", 0.72f },
    { "vca1Mod", 0.64f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.5184f }, { "eg1Sustain", 0.80f },
    { "eg1Release", 0.2723f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f }, { "eg2Release", 0.4351f },
    { "mgRate", 0.6791f }, { "mgPw", 0.6667f }, { "mixerLevel1", 0.2f }, { "mixerLevel2", 0.1167f },
    { "mixerLevel3", 0.0f }, { "integratorTime", 0.3029f },
};

inline constexpr FactoryJackCable kAcidSquelchCables[] = { { "VCO:SAW", "VCF:IN" }, RONIN_ACID_CORE };
inline constexpr FactoryKnob kAcidSquelchKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcfCutoff", 0.28f }, { "vcfPeak", 1.0f }, { "vcfAmount", 0.88f },
    { "vca1Mod", 0.62f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.5649f }, { "eg1Sustain", 0.85f },
    { "eg1Release", 0.3556f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f }, { "eg2Release", 0.5184f },
    { "mgRate", 0.6791f }, { "mgPw", 0.5000f }, { "mixerLevel1", 0.2f }, { "mixerLevel2", 0.05f },
    { "mixerLevel3", 0.0f }, { "integratorTime", 0.3563f },
};

inline constexpr FactoryJackCable kAcidAccentCables[] = {
    { "VCO:SAW", "VCF:IN" }, RONIN_ACID_CORE, { "NOISE:WHITE", "S&H:IN" }, { "MG:PULSE", "S&H:CLOCK" },
    { "S&H:OUT", "VCF:CUTOFF" },
};
inline constexpr FactoryKnob kAcidAccentKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcfCutoff", 0.32f }, { "vcfPeak", 0.90f }, { "vcfAmount", 0.70f },
    { "vca1Mod", 0.55f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.5184f }, { "eg1Sustain", 0.80f },
    { "eg1Release", 0.2723f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f }, { "eg2Release", 0.4554f },
    { "mgRate", 0.6791f }, { "mgPw", 0.6667f }, { "mixerLevel1", 0.2f }, { "mixerLevel2", 0.1167f },
    { "mixerLevel3", 0.0f }, { "integratorTime", 0.3029f },
};

inline constexpr FactoryJackCable kAcidPulseCables[] = {
    { "VCO:PULSE", "VCF:IN" }, RONIN_ACID_CORE, { "EG 2:OUT +", "VCO:PWM" },
};
inline constexpr FactoryKnob kAcidPulseKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcoPw", 0.20f }, { "vcfCutoff", 0.36f }, { "vcfPeak", 0.82f },
    { "vcfAmount", 0.70f }, { "vca1Mod", 0.49f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.5184f },
    { "eg1Sustain", 0.80f }, { "eg1Release", 0.2723f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f },
    { "eg2Release", 0.4351f }, { "mgRate", 0.6791f }, { "mgPw", 0.6667f }, { "mixerLevel1", 0.2f },
    { "mixerLevel2", 0.1167f }, { "mixerLevel3", 0.0f }, { "integratorTime", 0.3029f },
};

inline constexpr FactoryJackCable kAcidDriveCables[] = {
    { "VCO:SAW", "VCF:IN" }, { "VCO:PULSE", "VCF:IN" }, RONIN_ACID_CORE,
};
inline constexpr FactoryKnob kAcidDriveKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcoPw", 0.35f }, { "vcfCutoff", 0.40f }, { "vcfPeak", 0.92f },
    { "vcfAmount", 0.75f }, { "vca1Mod", 0.60f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.5184f },
    { "eg1Sustain", 0.80f }, { "eg1Release", 0.2723f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f },
    { "eg2Release", 0.4351f }, { "mgRate", 0.6791f }, { "mgPw", 0.6667f }, { "mixerLevel1", 0.2f },
    { "mixerLevel2", 0.1167f }, { "mixerLevel3", 0.0f }, { "integratorTime", 0.3029f },
};

inline constexpr FactoryJackCable kAcidSlideCables[] = { { "VCO:SAW", "VCF:IN" }, RONIN_ACID_CORE };
inline constexpr FactoryKnob kAcidSlideKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcfCutoff", 0.33f }, { "vcfPeak", 0.88f }, { "vcfAmount", 0.65f },
    { "vca1Mod", 0.70f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.6279f }, { "eg1Sustain", 0.90f },
    { "eg1Release", 0.3556f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0630f }, { "eg2Release", 0.4816f },
    { "mgRate", 0.6791f }, { "mgPw", 0.2778f }, { "mixerLevel1", 0.2f }, { "mixerLevel2", 0.0833f },
    { "mixerLevel3", 0.0f }, { "integratorTime", 0.3941f },
};

// ---- Basses and leads ----

inline constexpr FactoryJackCable kSubBassCables[] = {
    { "VCO:TRI", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 1:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" },
};
inline constexpr FactoryKnob kSubBassKnobs[] = {
    { "vcoRange", 0.3333f }, { "vca1Mod", 0.60f },
    { "eg1Attack", 0.1463f }, { "eg1Decay", 0.5649f }, { "eg1Sustain", 0.70f }, { "eg1Release", 0.3983f },
    { "mgRate", 0.5391f }, { "mgPw", 0.5f },
};

inline constexpr FactoryJackCable kOffbeatBassCables[] = {
    { "VCO:SAW", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 1:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" }, { "EG 1:OUT A", "VCF:CUTOFF" },
};
inline constexpr FactoryKnob kOffbeatBassKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcfCutoff", 0.42f }, { "vcfPeak", 0.30f }, { "vcfAmount", 0.45f },
    { "vca1Mod", 0.72f }, { "eg1Attack", 0.0630f }, { "eg1Decay", 0.4554f }, { "eg1Sustain", 0.35f },
    { "eg1Release", 0.3556f }, { "mgRate", 0.5391f }, { "mgPw", 0.5f },
};

inline constexpr FactoryJackCable kPwmLeadCables[] = {
    { "VCO:PULSE", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 1:TRIG" }, { "MG:PULSE", "EG 2:TRIG" }, { "MG:PULSE", "DIV:IN" },
    { "EG 1:OUT A", "VCA 1:ENV" }, { "EG 1:OUT A", "VCF:CUTOFF" }, { "EG 2:OUT +", "VCO:PWM" },
    { "DIV:/2", "MIX:IN 1" }, { "DIV:/4", "MIX:IN 2" }, { "MIX:OUT", "INV:IN" }, { "INV:OUT", "INT:IN" },
    { "INT:OUT", "VCO:V/OCT" },
};
inline constexpr FactoryKnob kPwmLeadKnobs[] = {
    { "vcoRange", 0.6667f }, { "vcoPw", 0.30f }, { "vcfCutoff", 0.56f }, { "vcfPeak", 0.25f },
    { "vcfAmount", 0.35f }, { "vca1Mod", 0.55f }, { "eg1Attack", 0.1463f }, { "eg1Decay", 0.5649f },
    { "eg1Sustain", 0.70f }, { "eg1Release", 0.4816f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.4816f },
    { "eg2Release", 0.5649f }, { "mgRate", 0.6091f }, { "mgPw", 0.3889f }, { "mixerLevel1", 0.1167f },
    { "mixerLevel2", 0.2f }, { "mixerLevel3", 0.0f }, { "integratorTime", 0.4475f },
};

inline constexpr FactoryJackCable kSirenCables[] = {
    { "VCO:SAW", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:TRI", "VCO:FM 1" },
};
inline constexpr FactoryKnob kSirenKnobs[] = {
    { "vcoRange", 0.6667f }, { "vcoFm1", 0.40f }, { "vcfCutoff", 0.62f }, { "vcfPeak", 0.35f },
    { "vca1Initial", 1.0f }, { "vca1Mod", 0.55f }, { "mgRate", 0.3500f }, { "mgPw", 0.5f },
};

// ---- Percussion: self-clocked at 125 BPM ----

inline constexpr FactoryJackCable kKickDrumCables[] = {
    { "VCO:TRI", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 1:TRIG" }, { "MG:PULSE", "EG 2:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" },
    { "EG 2:OUT +", "VCO:FM 1" },
};
inline constexpr FactoryKnob kKickDrumKnobs[] = {
    { "vcoRange", 0.3333f }, { "vcoFine", 0.0f }, { "vcoFm1", 0.50f }, { "vca1Mod", 0.55f },
    { "eg1Attack", 0.0f }, { "eg1Decay", 0.5184f }, { "eg1Sustain", 0.0f }, { "eg1Release", 0.4816f },
    { "eg2Hold", 0.0f }, { "eg2Attack", 0.0f }, { "eg2Release", 0.3556f }, { "mgRate", 0.5391f },
    { "mgPw", 0.80f },
};

inline constexpr FactoryJackCable kOffbeatHatsCables[] = {
    { "NOISE:WHITE", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 1:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" },
};
inline constexpr FactoryKnob kOffbeatHatsKnobs[] = {
    { "vcfCutoff", 0.95f }, { "vcfPeak", 0.30f }, { "vca1Mod", 1.0f }, { "vca1LowCut", 1.0f },
    { "eg1Attack", 0.0f }, { "eg1Decay", 0.4351f }, { "eg1Sustain", 0.0f }, { "eg1Release", 0.4351f },
    { "mgRate", 0.5391f }, { "mgPw", 0.5f },
};

inline constexpr FactoryJackCable kNoiseSnareCables[] = {
    { "NOISE:WHITE", "MIX:IN 1" }, { "VCO:TRI", "MIX:IN 2" }, { "MIX:OUT", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" },
    { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES, { "MG:PULSE", "EG 1:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" },
};
inline constexpr FactoryKnob kNoiseSnareKnobs[] = {
    { "vcoRange", 0.6667f }, { "vcoFine", 0.75f }, { "vcfCutoff", 0.78f }, { "vcfPeak", 0.35f },
    { "vca1Mod", 0.85f }, { "vca1LowCut", 0.45f }, { "eg1Attack", 0.0f }, { "eg1Decay", 0.4554f },
    { "eg1Sustain", 0.0f }, { "eg1Release", 0.4554f }, { "mgRate", 0.4691f }, { "mgPw", 0.5f },
    { "mixerLevel1", 0.8f }, { "mixerLevel2", 0.45f }, { "mixerLevel3", 0.0f },
};

inline constexpr FactoryJackCable kLaserZapCables[] = {
    { "VCO:SAW", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:PULSE", "EG 1:TRIG" }, { "MG:PULSE", "EG 2:TRIG" }, { "EG 1:OUT A", "VCA 1:ENV" },
    { "EG 2:OUT +", "VCO:FM 1" },
};
inline constexpr FactoryKnob kLaserZapKnobs[] = {
    { "vcoRange", 0.6667f }, { "vcoFm1", 0.80f }, { "vcfCutoff", 0.70f }, { "vcfPeak", 0.55f },
    { "vca1Mod", 0.58f }, { "eg1Attack", 0.0f }, { "eg1Decay", 0.4816f }, { "eg1Sustain", 0.0f },
    { "eg1Release", 0.4351f }, { "eg2Hold", 0.0f }, { "eg2Attack", 0.0f }, { "eg2Release", 0.4351f },
    { "mgRate", 0.6091f }, { "mgPw", 0.70f },
};

inline constexpr FactoryJackCable kNoiseRiserCables[] = {
    { "NOISE:WHITE", "VCF:IN" }, { "VCF:OUT", "VCA 1:IN" }, { "VCA 1:OUT", "OUTPUT:WET" }, RONIN_DRY_CABLES,
    { "MG:SAW", "VCF:CUTOFF" }, { "MG:SAW", "VCA 1:ENV" },
};
inline constexpr FactoryKnob kNoiseRiserKnobs[] = {
    { "vcfCutoff", 0.62f }, { "vcfPeak", 0.65f }, { "vcfAmount", 1.0f }, { "vca1Initial", 0.5f },
    { "vca1Mod", 1.0f }, { "mgRate", 0.2591f }, { "mgPw", 0.5f },
};

#undef RONIN_ACID_CORE
#undef RONIN_DRY_CABLES

template <typename T, std::size_t N>
constexpr int count (const T (&)[N]) noexcept { return static_cast<int> (N); }

}

inline constexpr FactoryPreset kFactoryPresets[kFactoryPresetCount] = {
    { "INIT", "The fresh-instance patch. Input: any audio; its level opens EG 1, which opens VCA 1 and the filter.",
      true, factorybank::kInitCables, factorybank::count (factorybank::kInitCables), nullptr, 0 },
#define RONIN_PRESET(name, text, cables, knobs) \
    { name, text, true, factorybank::cables, factorybank::count (factorybank::cables), factorybank::knobs, \
      factorybank::count (factorybank::knobs) }
    RONIN_PRESET ("AUTO FILTER", "Resonant low-pass swept by the MG, one sweep per bar. Input: any audio (pads, loops, chords).",
                  kAutoFilterCables, kAutoFilterKnobs),
    RONIN_PRESET ("ENV FILTER", "Envelope filter: each hit at EXT IN snaps the resonant cutoff open. Input: drums, bass or plucked parts.",
                  kEnvFilterCables, kEnvFilterKnobs),
    RONIN_PRESET ("TRANCE GATE", "16th-note gate chopping the input, edges softened by INT. Input: sustained audio (pads, chords, vocals).",
                  kTranceGateCables, kTranceGateKnobs),
    RONIN_PRESET ("PUMP", "Quarter-note ducking from EG 2 into VCA 1, the side-chain pump. Input: sustained audio or a full mix.",
                  kPumpCables, kPumpKnobs),
    RONIN_PRESET ("S&H FILTER", "Random stepped filter: S&H samples noise in 16ths and moves the cutoff. Input: any audio, best with pads or loops.",
                  kSampleHoldFilterCables, kSampleHoldFilterKnobs),
    RONIN_PRESET ("RING MOD", "The input times the VCO triangle, with slow MG drift on the carrier. Input: any audio; voices and drums go metallic.",
                  kRingModCables, kRingModKnobs),
    RONIN_PRESET ("ACID LINE", "Self-playing acid bass: 16ths at 125 BPM, octave and fifth pattern, short slide. No input needed.",
                  kAcidLineCables, kAcidLineKnobs),
    RONIN_PRESET ("ACID SQUELCH", "Acid bass at full peak with a longer filter snap, a minor-third pattern and a slower slide. No input needed.",
                  kAcidSquelchCables, kAcidSquelchKnobs),
    RONIN_PRESET ("ACID ACCENT", "Acid bass with random accents: S&H noise on the cutoff per step. No input needed.",
                  kAcidAccentCables, kAcidAccentKnobs),
    RONIN_PRESET ("ACID PULSE", "Acid bass on the narrow pulse, EG 2 also sweeping the pulse width. No input needed.",
                  kAcidPulseCables, kAcidPulseKnobs),
    RONIN_PRESET ("ACID DRIVE", "Saw and pulse summed into the VCF for a hotter, driven acid line. No input needed.",
                  kAcidDriveCables, kAcidDriveKnobs),
    RONIN_PRESET ("ACID SLIDE", "Long legato gates, a fourth and octave pattern and a slow slide: the rubbery acid line. No input needed.",
                  kAcidSlideCables, kAcidSlideKnobs),
    RONIN_PRESET ("SUB BASS", "Round triangle sub on quarter notes, straight into VCA 1 with no filter. No input needed.", kSubBassCables, kSubBassKnobs),
    RONIN_PRESET ("OFFBEAT BASS", "Plucky saw bass on the second half of each beat, the off-beat bass. No input needed.",
                  kOffbeatBassCables, kOffbeatBassKnobs),
    RONIN_PRESET ("PWM LEAD", "Pulse-width lead: EG 2 sweeps the width on every note, an 8th-note fifth and octave pattern with glide. No input needed.",
                  kPwmLeadCables, kPwmLeadKnobs),
    RONIN_PRESET ("SIREN", "Rising and falling siren: MG triangle on the VCO pitch. Plays continuously, no input needed.",
                  kSirenCables, kSirenKnobs),
    RONIN_PRESET ("KICK DRUM", "Four-on-the-floor kick: triangle with an EG 2 pitch drop. No input needed.",
                  kKickDrumCables, kKickDrumKnobs),
    RONIN_PRESET ("OFFBEAT HATS", "High-passed noise hats on the off-beats. No input needed.", kOffbeatHatsCables,
                  kOffbeatHatsKnobs),
    RONIN_PRESET ("NOISE SNARE", "Noise and triangle snare on beats 2 and 4. No input needed.", kNoiseSnareCables,
                  kNoiseSnareKnobs),
    RONIN_PRESET ("LASER ZAP", "8th-note laser zaps: EG 2 drops the VCO pitch fast. No input needed.", kLaserZapCables,
                  kLaserZapKnobs),
    RONIN_PRESET ("NOISE RISER", "Four-bar noise riser: the MG saw opens the filter and the VCA, then resets. No input needed.",
                  kNoiseRiserCables, kNoiseRiserKnobs),
#undef RONIN_PRESET
};

// A program's value for a host knob: its own entry, or the PanelDefaults.h table value (the fallback).
inline float factoryKnob (int index, const char* id, float fallback) noexcept
{
    if (index < 0 || index >= kFactoryPresetCount || id == nullptr)
        return fallback;
    const FactoryPreset& preset = kFactoryPresets[index];
    for (int i = 0; i < preset.knobCount; ++i)
        if (std::strcmp (preset.knobs[i].id, id) == 0)
            return preset.knobs[i].value;
    return fallback;
}

// The processor's module numbers (its addModule order) as the jack-id table needs them.
inline RackIndices factoryRack() noexcept
{
    RackIndices r;
    r.ext = FactoryModule::Ext;
    r.output = FactoryModule::Output;
    r.noise = FactoryModule::Noise;
    r.vcf = FactoryModule::Vcf;
    r.vca1 = FactoryModule::Vca1;
    r.vca2 = FactoryModule::Vca2;
    r.eg1 = FactoryModule::Eg1;
    r.mg = FactoryModule::Mg;
    r.vco = FactoryModule::Vco;
    r.eg2 = FactoryModule::Eg2;
    r.ring = FactoryModule::Ring;
    r.divider = FactoryModule::Divider;
    r.inverter = FactoryModule::Inverter;
    r.integrator = FactoryModule::Integrator;
    r.mixer = FactoryModule::Mixer;
    r.sampleHold = FactoryModule::SampleHold;
    return r;
}

// A program's cables as graph cables (jack ids resolved on factoryRack()). False if a jack id does not resolve.
inline bool factoryPresetCables (int index, Cable* cables, int capacity, int& count)
{
    count = 0;
    if (index < 0 || index >= kFactoryPresetCount || cables == nullptr)
        return false;
    const FactoryPreset& preset = kFactoryPresets[index];
    if (preset.cableCount < 0 || preset.cableCount > capacity)
        return false;
    const RackIndices rack = factoryRack();
    for (int i = 0; i < preset.cableCount; ++i)
    {
        Cable cable;
        if (! patchstate::jackAddress (rack, preset.cables[i].from, cable.sourceModule, cable.sourcePort)
            || ! patchstate::jackAddress (rack, preset.cables[i].to, cable.destModule, cable.destPort))
            return false;
        cables[count++] = cable;
    }
    return true;
}

// Factory loads set VCA 1 Initial (Vca1::applyFactoryPreset). INIT leaves it at the default table value.
inline float factoryVca1Initial (int index) noexcept
{
    return factoryKnob (index, "vca1Initial", PanelDefault::kVca1Initial);
}

// Host knobs restored when a program loads: the program's value or the PanelDefaults.h row.
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

inline FactoryProgramKnobs factoryProgramKnobs (int index) noexcept
{
    return { factoryKnob (index, "vcfCutoff", PanelDefault::kVcfCutoff),
             factoryKnob (index, "vcfPeak", PanelDefault::kVcfPeak),
             factoryKnob (index, "eg1Attack", PanelDefault::kEg1Attack),
             factoryKnob (index, "eg1Decay", PanelDefault::kEg1Decay),
             factoryKnob (index, "eg1Sustain", PanelDefault::kEg1Sustain),
             factoryKnob (index, "eg1Release", PanelDefault::kEg1Release),
             factoryKnob (index, "vcoRange", PanelDefault::kVcoRange) };
}

inline float factoryMgRate (int index) noexcept { return factoryKnob (index, "mgRate", PanelDefault::kMgRate); }
inline float factorySampleHoldRate (int index) noexcept
{
    return factoryKnob (index, "sampleHoldRate", PanelDefault::kSampleHoldRate);
}
inline float factoryIntegratorTime (int index) noexcept
{
    return factoryKnob (index, "integratorTime", PanelDefault::kIntegratorTime);
}

struct FactoryCable {
    int sourceModule;
    int sourcePort;
    int destModule;
    int destPort;
};

// INIT's cables as graph indices: the same eight cables, in the same order, as connectFactoryCables in
// DefaultPatch.h. FactoryPresetTests checks that INIT's jack ids resolve to exactly these.
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

inline const char* factoryPresetName (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return "";
    return kFactoryPresets[index].name;
}

inline const char* factoryPresetDescription (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return "";
    return kFactoryPresets[index].description;
}

inline bool factoryPresetEffect (int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return false;
    return kFactoryPresets[index].effectOn;
}

// Module knob for a host parameter id, for the JUCE-free loader. The OUTPUT knobs and the divider switch are
// processor settings, not module knobs, so they have no entry (the processor applies every host knob anyway).
struct FactoryKnobTarget {
    const char* id;
    int module;
    int knob;
};

inline constexpr FactoryKnobTarget kFactoryKnobTargets[] = {
    { "vcfCutoff", FactoryModule::Vcf, Vcf::kKnobCutoff },
    { "vcfPeak", FactoryModule::Vcf, Vcf::kKnobPeak },
    { "vcfAmount", FactoryModule::Vcf, Vcf::kKnobAmount },
    { "vca1Initial", FactoryModule::Vca1, Vca1::kKnobInitial },
    { "vca1Mod", FactoryModule::Vca1, Vca1::kKnobIntensity },
    { "vca1LowCut", FactoryModule::Vca1, Vca1::kKnobLowCut },
    { "vca2Initial", FactoryModule::Vca2, Vca2::kKnobInitial },
    { "vca2Mod", FactoryModule::Vca2, Vca2::kKnobMod },
    { "eg1Attack", FactoryModule::Eg1, Eg1::kKnobAttack },
    { "eg1Decay", FactoryModule::Eg1, Eg1::kKnobDecay },
    { "eg1Sustain", FactoryModule::Eg1, Eg1::kKnobSustain },
    { "eg1Release", FactoryModule::Eg1, Eg1::kKnobRelease },
    { "mgRate", FactoryModule::Mg, MgModule::kKnobFrequency },
    { "mgPw", FactoryModule::Mg, MgModule::kKnobPw },
    { "vcoRange", FactoryModule::Vco, Vco::kKnobScale },
    { "vcoFine", FactoryModule::Vco, Vco::kKnobFine },
    { "vcoPw", FactoryModule::Vco, Vco::kKnobPw },
    { "vcoFm1", FactoryModule::Vco, Vco::kKnobAmountA },
    { "vcoFm2", FactoryModule::Vco, Vco::kKnobAmountB },
    { "eg2Hold", FactoryModule::Eg2, Eg2::kKnobHold },
    { "eg2Delay", FactoryModule::Eg2, Eg2::kKnobDelay },
    { "eg2Attack", FactoryModule::Eg2, Eg2::kKnobAttack },
    { "eg2Release", FactoryModule::Eg2, Eg2::kKnobRelease },
    { "integratorTime", FactoryModule::Integrator, Integrator::kKnobTime },
    { "mixerLevel1", FactoryModule::Mixer, Mixer::kKnobLevel1 },
    { "mixerLevel2", FactoryModule::Mixer, Mixer::kKnobLevel2 },
    { "mixerLevel3", FactoryModule::Mixer, Mixer::kKnobLevel3 },
    { "sampleHoldRate", FactoryModule::SampleHold, SampleHold::kKnobRate },
    { "extInThreshold", FactoryModule::Ext, ExtIn::kKnobThreshold },
    { "extInRelease", FactoryModule::Ext, ExtIn::kKnobRelease },
};

// Graph knobs a program writes beyond the mixer and S&H restores in loadFactoryPreset: its own knob entries.
// INIT has none. (The processor also resets every other host knob to the table; this JUCE-free loader does not.)
inline bool writeFactoryProgramKnobs (PatchGraph& graph, int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return false;
    const FactoryPreset& preset = kFactoryPresets[index];
    bool ok = true;
    for (int i = 0; i < preset.knobCount; ++i)
    {
        for (const auto& target : kFactoryKnobTargets)
        {
            if (std::strcmp (target.id, preset.knobs[i].id) != 0)
                continue;
            if (Module* module = graph.moduleAt (target.module))
                module->setKnob (target.knob, preset.knobs[i].value);
            else
                ok = false;
        }
    }
    return ok;
}

// Replaces cables (jack ids resolved on factoryRack()). Mixer levels return to 0.8 and the sample-and-hold rate
// to 0.5, then the program's own knobs are written. Each module's applyFactoryPreset runs; VCA 1 sets Initial
// from factoryVca1Initial. The graph blob does not store that knob.
inline bool loadFactoryPreset (PatchGraph& graph, int index)
{
    if (index < 0 || index >= kFactoryPresetCount)
        return false;

    Cable cables[PatchGraph::kMaxCables] {};
    int count = 0;
    if (! factoryPresetCables (index, cables, PatchGraph::kMaxCables, count))
        return false;
    if (! graph.setCables (cables, count))
        return false;

    // The shared row first (mixer levels 0.8, S&H rate 0.5), then the program's own knobs on top.
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
