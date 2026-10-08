// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// RONIN_Redesign §3.9 / §5.12: graph JCS R9 (every feedback cable one sample, no double run),
// R3s (S-trig conversion only into EG Trig, legacyInvert per migrated cable), R10 (typed rest, no latching).

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/Divider.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/ExtIn.h"
#include "Modular/Integrator.h"
#include "Modular/Inverter.h"
#include "Modular/Mg.h"
#include "Modular/Mixer.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Ring.h"
#include "Modular/SampleHold.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "Modular/Vco.h"

#include <cmath>
#include <cstdio>

namespace {

int gChecks = 0;

void check (bool ok, const char* message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message);
        ++gChecks;
    }
}

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

// Out = In + bias; counts how many times it runs.
class CountingModule : public Module {
public:
    explicit CountingModule (float bias = 0.0f) : bias_ (bias) {}
    int numPorts() const override { return 2; }
    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", PortType::Audio, PortDir::In };
        return { "Out", PortType::Audio, PortDir::Out };
    }
    void setKnob (int, float) override {}
    void prepare (double rate) override { sampleRate = rate; runs = 0; }
    void processSample() override
    {
        ++runs;
        portValue[1] = portValue[0] + bias_;
    }
    int runs = 0;

private:
    float bias_;
};

// A Gate output (not S-trig) at a settable level: 0 V or 5 V (JCS R2).
class GateSource : public Module {
public:
    int numPorts() const override { return 1; }
    PortDesc port (int) const override { return { "Gate", PortType::Gate, PortDir::Out, 0.0f, false, false, PortRole::GateClk }; }
    void setKnob (int, float) override {}
    void prepare (double rate) override { sampleRate = rate; }
    void processSample() override { portValue[0] = level; }
    float level = 0.0f;
};

}

int testGraphFeedbackOneSampleNoDoubleRun()
{
    // Two loops through B: A -> B -> A and B -> D -> B. The old rule ran B twice per sample.
    PatchGraph graph;
    CountingModule src (1.0f);
    CountingModule a;
    CountingModule b;
    CountingModule d;
    const int is = graph.addModule (src);
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int id = graph.addModule (d);
    check (graph.connect (is, 1, ia, 0), "src into A");
    check (graph.connect (ia, 1, ib, 0), "A into B");
    check (graph.connect (ib, 1, ia, 0), "B into A (loop 1)");
    check (graph.connect (ib, 1, id, 0), "B into D");
    check (graph.connect (id, 1, ib, 0), "D into B (loop 2)");
    check (graph.delayedCableCount() == 2, "each loop-closing cable is delayed");
    graph.prepare (48000.0);

    for (int n = 0; n < 100; ++n)
        graph.process();
    check (src.runs == 100 && a.runs == 100 && b.runs == 100 && d.runs == 100, "every module runs exactly once per sample");

    // Self-patch: an impulse through a self loop recirculates once per sample.
    PatchGraph self;
    CountingModule loop;
    const int il = self.addModule (loop);
    check (self.connect (il, 1, il, 0), "self patch");
    check (self.cableIsDelayed (0), "self patch is delayed");
    self.prepare (48000.0);
    loop.portValue[1] = 0.0f;
    self.process();
    check (ronin::exactlyEqual (loop.portValue[1], 0.0f) && loop.runs == 1, "self loop: one run, starts at 0");

    // Loop timing: src (1.0) -> A -> B -> A. A at sample n = 1 + B[n-1]; B[n] = A[n].
    PatchGraph timing;
    CountingModule t0 (1.0f);
    CountingModule ta;
    CountingModule tb;
    const int i0 = timing.addModule (t0);
    const int ita = timing.addModule (ta);
    const int itb = timing.addModule (tb);
    timing.connect (i0, 1, ita, 0);
    timing.connect (ita, 1, itb, 0);
    timing.connect (itb, 1, ita, 0);
    timing.prepare (48000.0);
    timing.process();
    check (ronin::exactlyEqual (ta.portValue[1], 1.0f) && ronin::exactlyEqual (tb.portValue[1], 1.0f), "sample 0: B hears A at once");
    timing.process();
    check (ronin::exactlyEqual (ta.portValue[1], 2.0f), "sample 1: A hears B one sample late");
    timing.process();
    check (ronin::exactlyEqual (ta.portValue[1], 3.0f), "sample 2: the loop accumulates one step per sample");
    return finish ("testGraphFeedbackOneSampleNoDoubleRun");
}

int testGraphStrigScopeAndLegacyInvert()
{
    PatchGraph graph;
    GateSource gate;
    Eg1 eg;
    Vca1 vca;
    const int ig = graph.addModule (gate);
    const int ie = graph.addModule (eg);
    const int iv = graph.addModule (vca);
    check (graph.connect (ig, 0, ie, Eg1::kTrig), "gate into EG 1 TRIG");
    check (graph.connect (ig, 0, iv, Vca1::kEnv), "gate into VCA 1 ENV");
    graph.prepare (48000.0);

    gate.level = 5.0f;
    graph.process();
    check (ronin::exactlyEqual (eg.portValue[Eg1::kTrig], 0.0f), "R3s: a high gate into EG TRIG is held (0 V)");
    check (ronin::exactlyEqual (vca.portValue[Vca1::kEnv], 5.0f), "R3s: a high gate into VCA ENV is raw +5 V (was 0 V)");
    gate.level = 0.0f;
    graph.process();
    check (ronin::exactlyEqual (eg.portValue[Eg1::kTrig], 5.0f), "R3s: a low gate into EG TRIG is released (+5 V)");
    check (ronin::exactlyEqual (vca.portValue[Vca1::kEnv], 0.0f), "R3s: a low gate into VCA ENV is raw 0 V");

    // A migrated format-1 cable keeps the old S-15 inversion (JCS M3).
    Cable cables[2];
    graph.copyPublishedCables (cables, 2);
    cables[1].legacyInvert = true;
    check (graph.setCables (cables, 2), "setCables keeps legacyInvert");
    Cable back[2];
    graph.copyPublishedCables (back, 2);
    check (back[1].legacyInvert && ! back[0].legacyInvert, "the flag survives publish");
    gate.level = 5.0f;
    graph.process();
    check (ronin::exactlyEqual (vca.portValue[Vca1::kEnv], 0.0f), "legacyInvert: high gate into VCA ENV is 0 V, the old sound");
    gate.level = 0.0f;
    graph.process();
    check (ronin::exactlyEqual (vca.portValue[Vca1::kEnv], 5.0f), "legacyInvert: low gate into VCA ENV is +5 V, the old sound");

    // New cables never get the flag.
    graph.disconnect (ig, 0, iv, Vca1::kEnv);
    check (graph.connect (ig, 0, iv, Vca1::kEnv), "reconnect");
    Cable fresh[2];
    graph.copyPublishedCables (fresh, 2);
    check (! fresh[1].legacyInvert, "a new cable is never legacyInvert");

    // EG 2 DelayTrig (Gate) into EG 1 Trig is converted; EXT IN-style S-trig volts pass as written.
    PortDesc strigOut { "Gate", PortType::Gate, PortDir::Out, 5.0f, true };
    PortDesc trigIn = eg.port (Eg1::kTrig);
    check (ronin::exactlyEqual (PatchGraph::cableVolts (0.0f, strigOut, trigIn, false), 0.0f), "S-trig source passes as written (held)");
    check (ronin::exactlyEqual (PatchGraph::cableVolts (5.0f, strigOut, trigIn, true), 5.0f), "S-trig source is never inverted");
    return finish ("testGraphStrigScopeAndLegacyInvert");
}

int testGraphTypedRestNoLatch()
{
    PatchGraph graph;
    GateSource gate;
    Eg1 eg;
    Vca1 vca;
    const int ig = graph.addModule (gate);
    const int ie = graph.addModule (eg);
    const int iv = graph.addModule (vca);
    graph.connect (ig, 0, ie, Eg1::kTrig);
    graph.connect (ig, 0, iv, Vca1::kEnv);
    graph.prepare (48000.0);
    gate.level = 5.0f;
    for (int i = 0; i < 10; ++i)
        graph.process();
    check (ronin::exactlyEqual (eg.portValue[Eg1::kTrig], 0.0f) && ronin::exactlyEqual (vca.portValue[Vca1::kEnv], 5.0f), "patched values arrive");

    graph.disconnect (ig, 0, ie, Eg1::kTrig);
    graph.disconnect (ig, 0, iv, Vca1::kEnv);
    graph.process();
    check (ronin::exactlyEqual (eg.portValue[Eg1::kTrig], jcs::kStrigRest), "R10: unpatched S-trig rests at +5 V");
    check (ronin::exactlyEqual (vca.portValue[Vca1::kEnv], 0.0f), "R10: unpatched CV rests at 0 V, nothing latches");
    check (! eg.inputConnected[Eg1::kTrig], "inputConnected clears");
    return finish ("testGraphTypedRestNoLatch");
}

// JCS R15: a port is over range once |V| > 5.5 V has lasted more than 10 ms; 5.5 V itself is not over range.
int testGraphOverRangeR15()
{
    PatchGraph graph;
    CountingModule src (6.0f);   // Out = 0 V rest + 6 V
    CountingModule safe (5.5f);
    const int is = graph.addModule (src);
    const int iz = graph.addModule (safe);
    graph.prepare (48000.0);
    for (int i = 0; i < 479; ++i)
        graph.process();
    check (! graph.portOverRange (is, 1), "under 10 ms of 6 V is not yet over range");
    for (int i = 0; i < 2; ++i)
        graph.process();
    check (graph.portOverRange (is, 1), "6 V for 10 ms lights the flag");
    check (! graph.portOverRange (iz, 1), "exactly 5.5 V is in range");
    check (! graph.portOverRange (is, 0), "the 0 V input stays dark");
    check (! graph.portOverRange (99, 0) && ! graph.portOverRange (is, 99), "bad ports read false");
    graph.prepare (96000.0);
    for (int i = 0; i < 900; ++i)
        graph.process();
    check (! graph.portOverRange (is, 1), "the 10 ms window follows the sample rate");
    for (int i = 0; i < 100; ++i)
        graph.process();
    check (graph.portOverRange (is, 1), "960 samples at 96 kHz is 10 ms");
    return finish ("testGraphOverRangeR15");
}

// The patch bay's "gate converted to S-trig" badge (PatchGraph::cableBadge) appears exactly where the graph converts
// a cable (cableVolts): for every output -> input pair of RONIN's modules, and for every source/destination type,
// S-trig flag and role combination. DIV /2 and /4 (GATE/CLK role, CV type) into EG TRIG pass as written: no badge.
int testBadgeMatchesGraphConversion()
{
    ExtIn ext;
    OutputModule out;
    NoiseModule noise;
    Vcf vcf;
    Vca1 vca1;
    Vca2 vca2;
    Eg1 eg1;
    MgModule mg;
    Vco vco;
    Eg2 eg2;
    Ring ring;
    Divider div;
    Inverter inv;
    Integrator integ;
    Mixer mix;
    SampleHold sh;
    const Module* mods[] = { &ext, &out, &noise, &vcf, &vca1, &vca2, &eg1, &mg, &vco, &eg2, &ring, &div, &inv, &integ, &mix, &sh };

    auto graphConverts = [] (const PortDesc& from, const PortDesc& to)
    {
        const bool high = ! ronin::exactlyEqual (PatchGraph::cableVolts (5.0f, from, to, false), 5.0f);
        const bool low = ! ronin::exactlyEqual (PatchGraph::cableVolts (0.0f, from, to, false), 0.0f);
        return high && low;
    };

    int pairs = 0, converting = 0, disagree = 0;
    for (const Module* s : mods)
        for (int sp = 0; sp < s->numPorts(); ++sp)
        {
            const PortDesc from = s->port (sp);
            if (from.dir != PortDir::Out)
                continue;
            for (const Module* d : mods)
                for (int dp = 0; dp < d->numPorts(); ++dp)
                {
                    const PortDesc to = d->port (dp);
                    if (to.dir != PortDir::In)
                        continue;
                    ++pairs;
                    const bool converts = graphConverts (from, to);
                    const bool badge = PatchGraph::cableBadge (from, to) == jcs::Badge::GateToStrig;
                    if (converts != badge || converts != PatchGraph::convertsToStrig (from, to))
                    {
                        std::printf ("  %s -> %s: graph %d badge %d\n", from.name, to.name, converts ? 1 : 0, badge ? 1 : 0);
                        ++disagree;
                    }
                    converting += converts ? 1 : 0;
                }
        }
    check (pairs > 300 && disagree == 0, "every module output -> input: badge exactly where the graph converts");
    check (converting == 2, "only EG 2 DELAY into EG 1 TRIG and EG 2 TRIG converts");

    // Every type x S-trig flag x role combination, source and destination.
    const PortType types[] = { PortType::Audio, PortType::CV, PortType::Gate };
    const PortRole roles[] = { PortRole::Default, PortRole::Audio, PortRole::Cv, PortRole::VOct, PortRole::HzvLin,
                               PortRole::GateClk, PortRole::STrig };
    int combos = 0, combosDisagree = 0;
    for (PortType st : types)
        for (int sv = 0; sv < 2; ++sv)
            for (PortRole sr : roles)
                for (PortType dt : types)
                    for (int di = 0; di < 2; ++di)
                        for (PortRole dr : roles)
                        {
                            const PortDesc from { "src", st, PortDir::Out, 0.0f, sv == 1, false, sr };
                            const PortDesc to { "dst", dt, PortDir::In, 0.0f, false, di == 1, dr };
                            ++combos;
                            const bool badge = PatchGraph::cableBadge (from, to) == jcs::Badge::GateToStrig;
                            if (badge != graphConverts (from, to))
                                ++combosDisagree;
                        }
    check (combos == 1764 && combosDisagree == 0, "every role pair: badge exactly where the graph converts");

    // The named cases.
    const PortDesc egTrig = eg1.port (Eg1::kTrig);
    check (PatchGraph::cableBadge (div.port (Divider::kDiv2), egTrig) == jcs::Badge::None
               && ronin::exactlyEqual (PatchGraph::cableVolts (5.0f, div.port (Divider::kDiv2), egTrig, false), 5.0f),
           "DIV /2 -> EG 1 TRIG: no badge, volts as written");
    check (PatchGraph::cableBadge (div.port (Divider::kDiv4), eg2.port (Eg2::kTrig)) == jcs::Badge::None,
           "DIV /4 -> EG 2 TRIG: no badge");
    check (PatchGraph::cableBadge (sh.port (SampleHold::kClockOut), egTrig) == jcs::Badge::None,
           "S&H clock -> EG TRIG: no badge");
    check (PatchGraph::cableBadge (eg2.port (Eg2::kDelayTrig), egTrig) == jcs::Badge::GateToStrig
               && ronin::exactlyEqual (PatchGraph::cableVolts (5.0f, eg2.port (Eg2::kDelayTrig), egTrig, false), 0.0f),
           "EG 2 DELAY -> EG 1 TRIG: converted, with the badge");
    const PortDesc extGate = ext.port (3);
    check (portRole (extGate) == jcs::Role::STrig && extGate.strigVolts && extGate.type == PortType::Gate,
           "EXT IN GATE is an S-TRIG output (S-trig volts, Gate type)");
    check (PatchGraph::cableBadge (extGate, egTrig) == jcs::Badge::None
               && ronin::exactlyEqual (PatchGraph::cableVolts (0.0f, extGate, egTrig, false), 0.0f),
           "EXT IN GATE -> EG 1 TRIG: no badge, volts as written");
    check (portRole (div.port (Divider::kIn)) == jcs::Role::GateClk && portRole (div.port (Divider::kDiv2)) == jcs::Role::GateClk
               && portRole (div.port (Divider::kDiv4)) == jcs::Role::GateClk
               && portRole (sh.port (SampleHold::kExtClock)) == jcs::Role::GateClk,
           "DIV IN, /2, /4 and S&H CLOCK are GATE/CLK");
    check (portRole (mg.port (MgModule::kTri)) == jcs::Role::CV && portRole (mg.port (MgModule::kSawUp)) == jcs::Role::CV
               && portRole (mg.port (MgModule::kSawDown)) == jcs::Role::CV && portRole (mg.port (MgModule::kPulse)) == jcs::Role::CV,
           "MG TRI, SAW, INV SAW and PULSE are CV");
    return finish ("testBadgeMatchesGraphConversion");
}
