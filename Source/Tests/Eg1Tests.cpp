// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/DefaultPatch.h"
#include "Modular/EffectSwitch.h"
#include "Modular/Eg1.h"
#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vcf.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr double kRate = 48000.0;

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

void hold (Eg1& eg, float trig, int samples)
{
    for (int i = 0; i < samples; ++i)
    {
        eg.portValue[Eg1::kTrig] = trig;
        eg.processSample();
    }
}

class LevelGate : public Module {
public:
    float level = 0.0f;

    int numPorts() const override { return 1; }

    PortDesc port (int) const override { return { "Gate", PortType::Gate, PortDir::Out }; }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override { portValue[0] = level; }
};

class AudioSink : public Module {
public:
    int numPorts() const override { return 1; }

    PortDesc port (int) const override { return { "In", PortType::Audio, PortDir::In }; }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override {}
};

class GateSink : public Module {
public:
    int numPorts() const override { return 1; }

    PortDesc port (int) const override { return { "In", PortType::Gate, PortDir::In }; }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override {}
};

}

int testEg1SustainLevel()
{
    Eg1 eg;
    eg.prepare (kRate);
    hold (eg, 0.0f, static_cast<int> (3.0 * kRate));
    check (std::fabs (eg.portValue[Eg1::kOutA] - 3.0f) < 0.05f, "held envelope sits at sustain * 5");
    check (std::fabs (eg.portValue[Eg1::kOutC]) < 0.05f, "sustain 0.6 centers out c");
    return finish ("testEg1SustainLevel");
}

int testEg1OutBIsNegation()
{
    Eg1 eg;
    eg.prepare (kRate);
    bool negated = true;
    bool centered = true;
    bool finite = true;
    for (int i = 0; i < static_cast<int> (1.0 * kRate); ++i)
    {
        eg.portValue[Eg1::kTrig] = 0.0f;
        eg.processSample();
        const float outA = eg.portValue[Eg1::kOutA];
        const float outB = eg.portValue[Eg1::kOutB];
        const float outC = eg.portValue[Eg1::kOutC];
        if (! std::isfinite (outA) || ! std::isfinite (outB) || ! std::isfinite (outC))
            finite = false;
        if (std::fabs (outB + outA) > 1.0e-5f)
            negated = false;
        if (std::fabs (outC - (outA - 3.0f)) > 1.0e-4f)
            centered = false;
    }
    check (finite, "outputs stay finite");
    check (negated, "out b is the negation of out a");
    check (centered, "out c is out a minus sustain * 5");
    return finish ("testEg1OutBIsNegation");
}

int testEg1ReleasesWhenTriggerLifts()
{
    Eg1 eg;
    eg.prepare (kRate);
    hold (eg, 0.0f, static_cast<int> (3.0 * kRate));
    const float held = eg.portValue[Eg1::kOutA];
    eg.portValue[Eg1::kTrig] = 5.0f;
    eg.processSample();
    const float first = eg.portValue[Eg1::kOutA];
    hold (eg, 5.0f, static_cast<int> (0.4 * kRate));
    const float mid = eg.portValue[Eg1::kOutA];
    check (held > 2.5f, "release starts from the sustain level");
    check (first > 2.5f, "lifting the trigger does not click to zero");
    check (mid < held - 0.5f && mid > 0.2f, "release falls over hundreds of milliseconds");

    eg.portValue[Eg1::kTrig] = 0.0f;
    eg.processSample();
    const float restarted = eg.portValue[Eg1::kOutA];
    hold (eg, 0.0f, static_cast<int> (0.03 * kRate));
    check (restarted > 0.2f, "retrigger keeps the current level");
    check (eg.portValue[Eg1::kOutA] > restarted + 0.5f, "retrigger restarts attack");
    return finish ("testEg1ReleasesWhenTriggerLifts");
}

int testEg1HasDecay()
{
    Eg1 eg;
    eg.prepare (kRate);
    float peak = 0.0f;
    const int attackWindow = static_cast<int> (0.2 * kRate);
    for (int i = 0; i < attackWindow; ++i)
    {
        eg.portValue[Eg1::kTrig] = 0.0f;
        eg.processSample();
        if (eg.portValue[Eg1::kOutA] > peak)
            peak = eg.portValue[Eg1::kOutA];
    }
    hold (eg, 0.0f, static_cast<int> (2.5 * kRate));
    const float later = eg.portValue[Eg1::kOutA];
    check (peak > 4.5f, "attack reaches the top of the span");
    check (std::fabs (later - 3.0f) < 0.05f, "decay settles at sustain");
    check (peak > later + 1.0f, "the peak is above the sustain level");
    return finish ("testEg1HasDecay");
}

int testEg1ThreeJacks()
{
    Eg1 eg;
    check (eg.numKnobs() == 4, "four timing knobs");
    check (eg.numPorts() == 4, "trig plus three outputs");
    const PortDesc trig = eg.port (Eg1::kTrig);
    check (trig.type == PortType::CV && trig.dir == PortDir::In && trig.rest == 5.0f, "trig rests at +5 V");
    check (std::strcmp (trig.name, "Trig") == 0, "trig name");
    check (eg.port (Eg1::kOutA).dir == PortDir::Out && std::strcmp (eg.port (Eg1::kOutA).name, "OutA") == 0, "out a");
    check (eg.port (Eg1::kOutB).dir == PortDir::Out && std::strcmp (eg.port (Eg1::kOutB).name, "OutB") == 0, "out b");
    check (eg.port (Eg1::kOutC).dir == PortDir::Out && std::strcmp (eg.port (Eg1::kOutC).name, "OutC") == 0, "out c");

    const int trigJack = panelJackIndex ("EG 1", "TRIG");
    const int outA = panelJackIndex ("EG 1", "OUT A");
    const int outB = panelJackIndex ("EG 1", "OUT B");
    const int outC = panelJackIndex ("EG 1", "OUT C");
    check (trigJack >= 0 && outA >= 0 && outB >= 0 && outC >= 0, "panel jacks exist");
    check (kPanelJacks[trigJack].module == 7 && kPanelJacks[trigJack].port == 0 && kPanelJacks[trigJack].dir == 0,
           "trig jack");
    check (kPanelJacks[outA].module == 7 && kPanelJacks[outA].port == 1 && kPanelJacks[outA].dir == 1, "out a jack");
    check (kPanelJacks[outB].port == 2 && kPanelJacks[outC].port == 3 && kPanelJacks[outC].dir == 1, "out b and out c");

    PanelLink refused;
    check (orientPanelJacks (trigJack, outA, 0, 1, 2, refused, 3, 4, 5, -1) == PanelLinkResult::Unmapped,
           "eg 1 is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (outA, trigJack, 0, 1, 2, linked, 3, 4, 5, 6) == PanelLinkResult::Ok, "out a feeds trig");
    check (linked.sourceModule == 6 && linked.sourcePort == Eg1::kOutA && linked.destPort == Eg1::kTrig,
           "oriented out a to trig");

    const FaceKnobBinding attack = faceKnobBinding ("EG 1", "ATTACK");
    const FaceKnobBinding decay = faceKnobBinding ("EG 1", "DECAY");
    const FaceKnobBinding sustain = faceKnobBinding ("EG 1", "SUSTAIN");
    const FaceKnobBinding release = faceKnobBinding ("EG 1", "RELEASE");
    check (std::strcmp (attack.parameterName, "EG 1 Attack") == 0 && attack.fallback == 0.05f, "attack name");
    check (std::strcmp (decay.parameterName, "EG 1 Decay") == 0 && decay.fallback == 0.30f, "decay name");
    check (std::strcmp (sustain.parameterName, "EG 1 Sustain") == 0 && sustain.fallback == 0.60f, "sustain name");
    check (std::strcmp (release.parameterName, "EG 1 Release") == 0 && release.fallback == 0.30f, "release name");
    check (attack.minimum == 0.0f && attack.maximum == 1.0f, "attack range");
    check (faceKnobBinding ("EG 2", "ATTACK").knob == FaceKnob::Eg2Attack, "eg 2 attack is a parameter");
    return finish ("testEg1ThreeJacks");
}

int testEg1UnpatchedTrigIsIdle()
{
    PatchGraph graph;
    Eg1 eg;
    graph.addModule (eg);
    graph.prepare (kRate);
    bool idle = true;
    for (int i = 0; i < static_cast<int> (0.5 * kRate); ++i)
    {
        graph.process();
        if (std::fabs (eg.portValue[Eg1::kOutA]) > 1.0e-4f)
            idle = false;
    }
    check (std::fabs (eg.portValue[Eg1::kTrig] - 5.0f) < 1.0e-4f, "unpatched trig is +5 V");
    check (idle, "unpatched trig does not hold the envelope open");
    return finish ("testEg1UnpatchedTrigIsIdle");
}

int testEg1PromotedGate()
{
    PatchGraph graph;
    LevelGate gate;
    Eg1 eg;
    AudioSink audio;
    GateSink gateIn;
    const int gateIndex = graph.addModule (gate);
    const int egIndex = graph.addModule (eg);
    const int audioIndex = graph.addModule (audio);
    const int gateInIndex = graph.addModule (gateIn);
    check (graph.connect (gateIndex, 0, egIndex, Eg1::kTrig), "gate reaches trig");
    check (graph.connect (gateIndex, 0, audioIndex, 0), "gate reaches audio");
    check (graph.connect (gateIndex, 0, gateInIndex, 0), "gate reaches gate");
    graph.prepare (kRate);

    gate.level = 0.0f;
    bool idle = true;
    for (int i = 0; i < 2000; ++i)
    {
        graph.process();
        if (std::fabs (eg.portValue[Eg1::kOutA]) > 1.0e-4f)
            idle = false;
    }
    check (std::fabs (eg.portValue[Eg1::kTrig] - 5.0f) < 1.0e-4f, "a released gate promotes to +5 V");
    check (std::fabs (audio.portValue[0] - 5.0f) < 1.0e-4f, "a released gate promotes into audio");
    check (std::fabs (gateIn.portValue[0]) < 1.0e-4f, "gate to gate stays raw");
    check (idle, "a released gate leaves the envelope idle");

    gate.level = 1.0f;
    for (int i = 0; i < static_cast<int> (0.2 * kRate); ++i)
        graph.process();
    check (std::fabs (eg.portValue[Eg1::kTrig]) < 1.0e-4f, "a held gate promotes to 0 V");
    check (std::fabs (audio.portValue[0]) < 1.0e-4f, "a held gate promotes into audio as 0 V");
    check (std::fabs (gateIn.portValue[0] - 1.0f) < 1.0e-4f, "a held gate stays 1 on a gate input");
    check (eg.portValue[Eg1::kOutA] > 2.0f, "a held gate opens the envelope");
    return finish ("testEg1PromotedGate");
}

int testEg1FactoryPatch()
{
    PatchGraph graph;
    ExtIn ext;
    OutputModule output;
    Vcf vcf;
    Vca1 vca1;
    Eg1 eg;
    const int extIndex = graph.addModule (ext);
    const int outIndex = graph.addModule (output);
    const int vcfIndex = graph.addModule (vcf);
    const int vcaIndex = graph.addModule (vca1);
    const int egIndex = graph.addModule (eg);
    check (connectFactoryCables (graph, extIndex, outIndex, vcfIndex, vcaIndex, egIndex), "eight cables connect");

    Cable cables[8];
    const int count = graph.copyPublishedCables (cables, 8);
    check (count == 8, "default patch has eight cables");
    check (cables[0].sourcePort == 2 && cables[0].destPort == Vcf::kSigIn, "mono into the filter");
    check (cables[1].destPort == Vca1::kSigIn, "filter into vca 1");
    check (cables[2].destPort == 2, "vca 1 into wet");
    check (cables[3].sourcePort == 0 && cables[3].destPort == 0, "dry left");
    check (cables[4].sourcePort == 1 && cables[4].destPort == 1, "dry right");
    check (cables[5].sourcePort == 3 && cables[5].destModule == egIndex && cables[5].destPort == Eg1::kTrig,
           "gate into trig");
    check (cables[6].sourcePort == Eg1::kOutA && cables[6].destPort == Vca1::kEnv, "out a into env");
    check (cables[7].sourcePort == Eg1::kOutA && cables[7].destPort == Vcf::kCutoff, "out a into cutoff");

    output.setLevel (1.0f);
    output.setMix (outputMixForEffect (true));
    graph.prepare (kRate);
    // 0.02 host is 0.1 V of mono, under the 0.2 V follower threshold.
    ext.setHostSample (0.02f, 0.02f);
    double sum = 0.0;
    for (int i = 0; i < 4800; ++i)
    {
        graph.process();
        sum += static_cast<double> (output.hostLeft()) * static_cast<double> (output.hostLeft());
    }
    check (std::fabs (eg.portValue[Eg1::kTrig] - 5.0f) < 1.0e-4f, "a quiet input leaves the gate released");
    check (std::fabs (eg.portValue[Eg1::kOutA]) < 1.0e-3f, "a quiet input does not open eg 1");
    check (std::sqrt (sum / 4800.0) < 1.0e-4, "effect on stays silent until the gate is held");

    output.setMix (outputMixForEffect (false));
    ext.setHostSample (0.25f, -0.5f);
    graph.process();
    check (std::fabs (output.hostLeft() - 0.25f) < 1.0e-5f, "effect off keeps dry left");
    check (std::fabs (output.hostRight() + 0.5f) < 1.0e-5f, "effect off keeps dry right");
    return finish ("testEg1FactoryPatch");
}
