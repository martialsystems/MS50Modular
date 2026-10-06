// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/SampleHold.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

void stepExt (SampleHold& module, float input, float clock)
{
    module.inputConnected[SampleHold::kExtClock] = true;
    module.portValue[SampleHold::kIn] = input;
    module.portValue[SampleHold::kExtClock] = clock;
    module.processSample();
}

int risingEdges (SampleHold& module, int samples, float rate)
{
    module.prepare (48000.0);
    module.setKnob (SampleHold::kKnobRate, rate);
    module.inputConnected[SampleHold::kExtClock] = false;
    int edges = 0;
    bool wasHigh = false;
    for (int i = 0; i < samples; ++i)
    {
        module.portValue[SampleHold::kIn] = static_cast<float> (i + 1);
        module.processSample();
        const bool high = module.portValue[SampleHold::kClockOut] >= 1.0f;
        if (high && ! wasHigh)
            ++edges;
        wasHigh = high;
    }
    return edges;
}

}

int testSampleHoldHoldsBetweenClocks()
{
    SampleHold module;
    module.prepare (48000.0);
    module.setKnob (SampleHold::kKnobRate, 1.0f);

    stepExt (module, 2.0f, 0.0f);
    check (module.portValue[SampleHold::kOut] == 0.0f, "no edge yet, hold stays 0");
    check (module.portValue[SampleHold::kClockOut] == 0.0f, "clock out is low");

    for (int i = 0; i < 8; ++i)
        stepExt (module, 3.0f + static_cast<float> (i), 0.0f);
    check (module.portValue[SampleHold::kOut] == 0.0f, "input changes do not leak between clocks");

    stepExt (module, 2.0f, 5.0f);
    check (std::fabs (module.portValue[SampleHold::kOut] - 2.0f) < 1.0e-5f, "rising edge holds the input");
    check (std::fabs (module.portValue[SampleHold::kClockOut] - 5.0f) < 1.0e-5f, "clock out is high");

    for (int i = 0; i < 16; ++i)
        stepExt (module, 9.0f, 5.0f);
    check (std::fabs (module.portValue[SampleHold::kOut] - 2.0f) < 1.0e-5f, "hold stays while the clock is high");

    for (int i = 0; i < 16; ++i)
        stepExt (module, 4.0f, 0.0f);
    check (std::fabs (module.portValue[SampleHold::kOut] - 2.0f) < 1.0e-5f, "hold stays while the clock is low");
    check (module.portValue[SampleHold::kClockOut] == 0.0f, "clock out follows the low clock");

    stepExt (module, 4.0f, 5.0f);
    check (std::fabs (module.portValue[SampleHold::kOut] - 4.0f) < 1.0e-5f, "the next rising edge takes the new input");
    return finish ("testSampleHoldHoldsBetweenClocks");
}

int testSampleHoldExtClockWins()
{
    SampleHold module;
    module.prepare (48000.0);
    module.setKnob (SampleHold::kKnobRate, 1.0f);

    bool stolen = false;
    for (int i = 0; i < 4800; ++i)
    {
        stepExt (module, 1.0f + static_cast<float> (i), 0.0f);
        if (module.portValue[SampleHold::kOut] != 0.0f || module.portValue[SampleHold::kClockOut] != 0.0f)
            stolen = true;
    }
    check (! stolen, "a fast internal rate does not sample while ext clock is patched");

    stepExt (module, 1.5f, 5.0f);
    check (std::fabs (module.portValue[SampleHold::kOut] - 1.5f) < 1.0e-5f, "ext rising edge samples");
    check (std::fabs (module.portValue[SampleHold::kClockOut] - 5.0f) < 1.0e-5f, "clock out follows ext clock");

    for (int i = 0; i < 500; ++i)
        stepExt (module, -2.0f, 5.0f);
    check (std::fabs (module.portValue[SampleHold::kOut] - 1.5f) < 1.0e-5f, "ext clock stays the winner while high");
    return finish ("testSampleHoldExtClockWins");
}

int testSampleHoldRateChangesInternalClock()
{
    SampleHold probe;
    probe.prepare (48000.0);
    check (std::fabs (probe.presetKnob (SampleHold::kKnobRate) - 0.50f) < 1.0e-6f, "rate default");
    const FaceKnobBinding rate = faceKnobBinding ("S&H", "RATE");
    check (rate.knob == FaceKnob::SampleHoldRate && rate.index == 0 && rate.fallback == 0.50f, "rate host");
    check (std::strcmp (rate.parameterId, "sampleHoldRate") == 0, "rate id");
    check (std::strcmp (rate.parameterName, "S&H Rate") == 0, "rate name");

    const int fast = risingEdges (probe, 4800, 1.0f);
    SampleHold slowModule;
    const int slow = risingEdges (slowModule, 4800, 0.0f);
    check (fast > slow, "a higher rate produces more internal clocks");
    check (fast >= 8 && fast <= 12, "knob 1 is about 100 Hz");
    check (slow == 0, "knob 0 has not edged inside a tenth of a second");

    SampleHold held;
    held.prepare (48000.0);
    held.setKnob (SampleHold::kKnobRate, 1.0f);
    held.inputConnected[SampleHold::kExtClock] = false;
    float captured = 0.0f;
    bool sawEdge = false;
    for (int i = 0; i < 4800 && ! sawEdge; ++i)
    {
        held.portValue[SampleHold::kIn] = 2.25f;
        held.processSample();
        if (held.portValue[SampleHold::kClockOut] >= 1.0f)
        {
            captured = held.portValue[SampleHold::kOut];
            sawEdge = true;
        }
    }
    check (sawEdge, "internal clock rises");
    check (std::fabs (captured - 2.25f) < 1.0e-4f, "internal edge holds the input");
    const float frozen = held.portValue[SampleHold::kOut];
    bool stayed = true;
    for (int i = 0; i < 200; ++i)
    {
        held.portValue[SampleHold::kIn] = -6.0f;
        const float before = held.portValue[SampleHold::kOut];
        held.processSample();
        if (held.portValue[SampleHold::kClockOut] >= 1.0f && before == frozen)
        {
            if (std::fabs (held.portValue[SampleHold::kOut] - frozen) > 1.0e-4f)
                stayed = false;
        }
    }
    check (stayed, "the hold does not track the input between internal edges");

    const int in = panelJackIndex ("S&H", "IN");
    const int out = panelJackIndex ("S&H", "OUT");
    const int clock = panelJackIndex ("S&H", "CLOCK");
    check (in >= 0 && out >= 0 && clock >= 0, "sample and hold jacks exist");
    check (kPanelJacks[in].module == 16 && kPanelJacks[in].port == 0 && kPanelJacks[in].dir == 0, "in");
    check (kPanelJacks[out].module == 16 && kPanelJacks[out].port == 1 && kPanelJacks[out].dir == 1, "out");
    check (kPanelJacks[clock].module == 16 && kPanelJacks[clock].port == SampleHold::kExtClock && kPanelJacks[clock].dir == 0,
           "clock jack is ext clock");
    int shJacks = 0;
    for (int i = 0; i < kPanelJackCount; ++i)
    {
        if (std::strcmp (kPanelJacks[i].section, "S&H") == 0)
            ++shJacks;
    }
    check (shJacks == 3, "clock out has no panel hole");
    check (panelJackIndex ("DIV", "/16") < 0, "the /16 hole is gone");
    check (kPanelJackCount == 57, "jack count");

    PanelLink refused;
    check (orientPanelJacks (out, panelJackIndex ("VCF", "CUTOFF"), 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "sample and hold is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (out, panelJackIndex ("VCF", "CUTOFF"), 0, 1, 2, linked,
                             3, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 15) == PanelLinkResult::Ok,
           "sample and hold out feeds cutoff");
    check (linked.sourceModule == 15 && linked.sourcePort == SampleHold::kOut, "out is the source");
    check (linked.destModule == 3 && linked.destPort == 1, "cutoff is the destination");
    return finish ("testSampleHoldRateChangesInternalClock");
}
