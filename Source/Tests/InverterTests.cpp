// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/Inverter.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

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

void drive (Inverter& inverter, float input)
{
    inverter.portValue[Inverter::kIn] = input;
    inverter.processSample();
}

}

int testInverterNegatesDc()
{
    Inverter inverter;
    inverter.prepare (48000.0);
    bool held = true;
    for (int i = 0; i < 100; ++i)
    {
        drive (inverter, 4.0f);
        if (std::fabs (inverter.portValue[Inverter::kOut] + 4.0f) > 1.0e-5f)
            held = false;
    }
    check (held, "+4 V stays -4 V");
    drive (inverter, -2.5f);
    check (std::fabs (inverter.portValue[Inverter::kOut] - 2.5f) < 1.0e-5f, "-2.5 V becomes +2.5 V");
    drive (inverter, 0.0f);
    check (ronin::exactlyEqual (inverter.portValue[Inverter::kOut], 0.0f), "0 V stays 0 V");
    return finish ("testInverterNegatesDc");
}

int testInverterNegatesAudio()
{
    Inverter inverter;
    inverter.prepare (48000.0);
    bool negated = true;
    for (int i = 0; i < 32; ++i)
    {
        const float input = static_cast<float> (std::sin (0.2 * static_cast<double> (i)) * 3.0 + 1.0);
        drive (inverter, input);
        if (std::fabs (inverter.portValue[Inverter::kOut] + input) > 1.0e-4f)
            negated = false;
    }
    check (negated, "each audio sample is negated, offset included");
    return finish ("testInverterNegatesAudio");
}

int testInverterHasNoKnobs()
{
    Inverter inverter;
    check (inverter.numKnobs() == 0, "no knobs");
    inverter.prepare (48000.0);
    inverter.setKnob (0, 1.0f);
    drive (inverter, 1.0f);
    check (std::fabs (inverter.portValue[Inverter::kOut] + 1.0f) < 1.0e-5f, "setKnob adds no offset");
    check (faceKnobBinding ("INV", "OFFSET").knob == FaceKnob::None, "no offset parameter");

    const int in = panelJackIndex ("INV", "IN");
    const int out = panelJackIndex ("INV", "OUT");
    const int wet = panelJackIndex ("OUTPUT", "WET");
    check (in >= 0 && out >= 0 && wet >= 0, "inverter jacks exist");
    check (kPanelJacks[in].module == 13 && kPanelJacks[in].port == 0 && kPanelJacks[in].dir == 0, "in");
    check (kPanelJacks[out].module == 13 && kPanelJacks[out].port == 1 && kPanelJacks[out].dir == 1, "out");

    PanelLink refused;
    check (orientPanelJacks (out, wet, 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "inverter is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (out, wet, 0, 1, 2, linked,
                             -1, -1, -1, -1, -1, -1, -1, -1, -1, 12) == PanelLinkResult::Ok,
           "inverter out feeds output wet");
    check (linked.sourceModule == 12 && linked.sourcePort == Inverter::kOut, "inverter out is the source");
    check (linked.destModule == 1 && linked.destPort == 2, "wet is the destination");
    return finish ("testInverterHasNoKnobs");
}
