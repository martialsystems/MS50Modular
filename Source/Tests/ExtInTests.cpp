// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/DefaultPatch.h"
#include "Modular/Eg1.h"
#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vcf.h"

#include <cmath>
#include <cstdio>

namespace {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

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

void run (PatchGraph& graph, ExtIn& ext, float left, float right, int samples)
{
    ext.setHostSample (left, right);
    for (int i = 0; i < samples; ++i)
        graph.process();
}

float knobForEgSeconds (float seconds)
{
    return static_cast<float> (std::log (static_cast<double> (seconds) / 0.001) / std::log (10000.0));
}

float quietTone (int index)
{
    return static_cast<float> (0.02 * std::sin (2.0 * kPi * 440.0 * static_cast<double> (index) / kRate));
}

}

int testExtInGateFiresAboveThreshold()
{
    PatchGraph graph;
    ExtIn ext;
    const int extIndex = graph.addModule (ext);
    graph.prepare (kRate);
    check (extIndex == 0, "ext in is in the graph");

    run (graph, ext, 0.01f, 0.01f, static_cast<int> (0.1 * kRate));
    check (ext.portValue[3] == 5.0f, "a quiet tone leaves the gate at +5 V");

    run (graph, ext, 0.2f, 0.2f, static_cast<int> (0.03 * kRate));
    check (ext.portValue[3] == 0.0f, "1 V mono drives the gate to 0 V");

    ext.setHostSample (0.0f, 0.0f);
    graph.process();
    check (ext.portValue[3] == 0.0f, "release holds 0 V just after the tone stops");
    run (graph, ext, 0.0f, 0.0f, static_cast<int> (0.25 * kRate));
    check (ext.portValue[3] == 5.0f, "release returns the gate to +5 V");
    check (std::fabs (ext.portValue[2]) < 1.0e-6f, "silence stays on the mono jack");
    return finish ("testExtInGateFiresAboveThreshold");
}

int testExtInButtonForcesGate()
{
    PatchGraph graph;
    ExtIn ext;
    graph.addModule (ext);
    graph.prepare (kRate);

    ext.setButtonHeld (true);
    run (graph, ext, 0.0f, 0.0f, 1);
    check (ext.buttonHeld(), "button stays held");
    check (ext.portValue[3] == 0.0f, "a held button drives the gate to 0 V");

    ext.setButtonHeld (false);
    run (graph, ext, 0.0f, 0.0f, 4);
    check (! ext.buttonHeld(), "button is up");
    check (ext.portValue[3] == 5.0f, "button up drives the gate to +5 V");
    return finish ("testExtInButtonForcesGate");
}

int testExtInButtonOpensVoice()
{
    PatchGraph graph;
    ExtIn ext;
    OutputModule output;
    Vcf vcf;
    Vca1 vca;
    Eg1 eg;
    const int extIndex = graph.addModule (ext);
    const int outIndex = graph.addModule (output);
    const int vcfIndex = graph.addModule (vcf);
    const int vcaIndex = graph.addModule (vca);
    const int egIndex = graph.addModule (eg);
    check (connectFactoryCables (graph, extIndex, outIndex, vcfIndex, vcaIndex, egIndex), "factory cables");

    eg.setKnob (Eg1::kKnobAttack, knobForEgSeconds (0.020f));
    eg.setKnob (Eg1::kKnobRelease, knobForEgSeconds (0.008f));
    eg.setKnob (Eg1::kKnobSustain, 1.0f);
    vcf.setKnob (Vcf::kKnobAmount, 0.0f);
    output.setMix (1.0f);
    output.setLevel (1.0f);
    graph.prepare (kRate);

    double quiet = 0.0;
    for (int i = 0; i < 2400; ++i)
    {
        const float sample = quietTone (i);
        ext.setHostSample (sample, sample);
        graph.process();
        quiet += static_cast<double> (output.hostLeft()) * static_cast<double> (output.hostLeft());
    }
    check (std::sqrt (quiet / 2400.0) < 1.0e-4, "button up, a quiet tone stays silent");
    check (std::fabs (ext.portValue[3] - 5.0f) < 1.0e-3f, "released gate jack is +5 V");
    check (std::fabs (eg.portValue[Eg1::kTrig] - 5.0f) < 1.0e-3f, "released gate reaches EG 1 at +5 V");
    check (std::fabs (eg.portValue[Eg1::kOutA]) < 1.0e-3f, "eg 1 stays idle");

    ext.setButtonHeld (true);
    double early = 0.0;
    double late = 0.0;
    for (int i = 0; i < 4800; ++i)
    {
        const float sample = quietTone (i);
        ext.setHostSample (sample, sample);
        graph.process();
        const double energy = static_cast<double> (output.hostLeft()) * static_cast<double> (output.hostLeft());
        if (i < 64)
            early += energy;
        if (i >= 2400)
            late += energy;
    }
    check (std::fabs (ext.portValue[3]) < 1.0e-3f, "held gate jack is 0 V");
    check (std::fabs (eg.portValue[Eg1::kTrig]) < 1.0e-3f, "held gate reaches EG 1 at 0 V");
    check (eg.portValue[Eg1::kOutA] > 4.0f, "held gate reaches the sustain level");
    check (late / 2400.0 > (early / 64.0) * 4.0, "the tone rises through the attack");
    check (std::sqrt (late / 2400.0) > 0.004, "effect on, button held, the tone is audible");

    ext.setButtonHeld (false);
    double tail = 0.0;
    double dead = 0.0;
    for (int i = 0; i < 9600; ++i)
    {
        const float sample = quietTone (i + 4800);
        ext.setHostSample (sample, sample);
        graph.process();
        const double energy = static_cast<double> (output.hostLeft()) * static_cast<double> (output.hostLeft());
        if (i < 64)
            tail += energy;
        if (i >= 7200)
            dead += energy;
    }
    check (tail / 64.0 > dead / 2400.0 * 4.0, "button up, the tone falls through the release");
    check (std::sqrt (dead / 2400.0) < 1.0e-4, "button up, the tone dies");
    check (std::fabs (eg.portValue[Eg1::kOutA]) < 0.05f, "eg 1 returns to idle");
    check (std::fabs (ext.portValue[3] - 5.0f) < 1.0e-3f, "button up returns the gate jack to +5 V");
    return finish ("testExtInButtonOpensVoice");
}

int testExtInFollowerOpensEgWithoutButton()
{
    PatchGraph graph;
    ExtIn ext;
    Eg1 eg;
    const int extIndex = graph.addModule (ext);
    const int egIndex = graph.addModule (eg);
    check (graph.connect (extIndex, 3, egIndex, Eg1::kTrig), "gate into trig");
    eg.setKnob (Eg1::kKnobAttack, knobForEgSeconds (0.010f));
    graph.prepare (kRate);

    run (graph, ext, 0.4f, 0.4f, static_cast<int> (0.05 * kRate));
    check (! ext.buttonHeld(), "the button is up");
    check (ext.portValue[3] == 0.0f, "the follower drives the gate to 0 V");
    check (std::fabs (eg.portValue[Eg1::kTrig]) < 1.0e-3f, "a followed gate reaches EG 1 at 0 V");
    check (eg.portValue[Eg1::kOutA] > 1.0f, "the follower opens eg 1");

    PatchGraph bare;
    Eg1 idle;
    bare.addModule (idle);
    bare.prepare (kRate);
    for (int i = 0; i < static_cast<int> (0.05 * kRate); ++i)
        bare.process();
    check (std::fabs (idle.portValue[Eg1::kTrig] - 5.0f) < 1.0e-4f, "unpatched trig rests at +5 V");
    check (std::fabs (idle.portValue[Eg1::kOutA]) < 1.0e-3f, "unpatched trig does not hold eg 1 open");
    return finish ("testExtInFollowerOpensEgWithoutButton");
}
