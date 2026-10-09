// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// RONIN state format 2 (JCS R7, RONIN_Redesign §3.9 / §6). JUCE-free so RoninTests can drive it.
// The processor writes XML: <RONIN format="2" unit="RONIN" paramId="value" ...>
//                             <CABLES><CABLE from="VCO:SAW" to="VCF:IN" [legacyInvert="1"] [colour="AARRGGBB"]/></CABLES>
//                           </RONIN>
// Knobs are stored by host parameter id; cables by canonical jack id (JCS R6 bare SECTION:LABEL form), oldest
// first (the order is the cable age JCS R9 walks). Format 1 (no format attribute, index blob "graph") is
// migrated on load: M-R1 EG knobs, M-R2 PARABOLA, M-R3 legacyInvert, M-R5 drive-pull cutoff compensation.

#pragma once

#include "Cable.h"
#include "PatchGraph.h"

#include <string>
#include <vector>

struct RackIndices {
    int ext = -1, output = -1, noise = -1, vcf = -1, vca1 = -1, vca2 = -1, eg1 = -1, mg = -1, vco = -1, eg2 = -1,
        ring = -1, divider = -1, inverter = -1, integrator = -1, mixer = -1, sampleHold = -1;
    int midi = -1;   // MIDI IN (no panel jack; jack ids MIDI:NOTE, MIDI:HZ/V LIN, MIDI:GATE, MIDI:VEL)
};

namespace patchstate {

inline constexpr int kFormat = 2;
inline constexpr const char* kUnit = "RONIN";

// Canonical jack id "SECTION:LABEL" for a graph port, or "" when the port has no panel jack.
std::string jackId (const RackIndices& rack, int module, int port);
// Graph port for a jack id. Accepts the bare form and the "RONIN/" / "RONIN#N/" prefixed forms (JCS R6).
bool jackAddress (const RackIndices& rack, const std::string& id, int& module, int& port);
// JCS R6 per-device alias table, applied on load before binding. Empty: no RONIN jack has been renamed.
const jcs::AliasTable& roninAliases();

// M-R3 (JCS M3): a format-1 cable from a Gate output that is not S-trig into an input that is not an S-trig
// input (and not a Gate input) gets legacyInvert, so it keeps its old S-15 inverted sound. Returns the count.
int markLegacyInvert (PatchGraph& graph, Cable* cables, int count);

// M-R5: which source feeds VCF SigIn directly.
enum class VcfFeed { None, VcoSaw, VcoPulse, Other, Mixed };
VcfFeed directVcfFeed (const Cable* cables, int count, const RackIndices& rack);

// Reference mean |x| for the compensation: saw 2.5 V, pulse 5.0 V (RONIN_Redesign §6, verify_ronin_decisions.py).
double referenceLevel (VcfFeed feed) noexcept;

// The cutoff knob c' that gives, under the new pull, the effective cutoff c gave under the legacy pull at the
// reference input level. Bisection, as verify_ronin_decisions.py comp(). Clamped to [0, 1].
double compensateCutoff (double knob01, double envVolts);

// M-R1 helpers re-exported for the processor (see EgLaw.h).
double migrateEgAttack (double oldKnob01);
double migrateEgDecayRelease (double oldKnob01);
// The old law stalled the attack at knob >= ~0.735 (48 kHz float). Those patches now reach decay (a fix).
bool attackStalledInV1 (double oldKnob01) noexcept;

}
