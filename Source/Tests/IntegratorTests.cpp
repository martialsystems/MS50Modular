// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/Integrator.h"
#include "Modular/Vco.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

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

void drive (Integrator& lag, float input)
{
    lag.portValue[Integrator::kIn] = input;
    lag.processSample();
}

}

int testIntegratorSettlesToInput()
{
    Integrator lag;
    lag.prepare (kRate);
    const int samples = static_cast<int> (kRate);
    for (int i = 0; i < samples; ++i)
        drive (lag, 4.0f);
    check (std::fabs (lag.portValue[Integrator::kOut] - 4.0f) < 0.01f, "+4 V settles at +4 V");

    Integrator negative;
    negative.prepare (kRate);
    for (int i = 0; i < samples; ++i)
        drive (negative, -3.0f);
    check (std::fabs (negative.portValue[Integrator::kOut] + 3.0f) < 0.01f, "-3 V settles at -3 V");
    return finish ("testIntegratorSettlesToInput");
}

int testIntegratorSameSign()
{
    Integrator lag;
    lag.prepare (kRate);
    bool positive = true;
    for (int i = 0; i < 4000; ++i)
    {
        drive (lag, 4.0f);
        const float out = lag.portValue[Integrator::kOut];
        if (out < -1.0e-5f || out > 4.01f)
            positive = false;
    }
    check (positive, "a positive step stays positive and does not run past the input");

    Integrator falling;
    falling.prepare (kRate);
    bool negative = true;
    for (int i = 0; i < 4000; ++i)
    {
        drive (falling, -4.0f);
        const float out = falling.portValue[Integrator::kOut];
        if (out > 1.0e-5f || out < -4.01f)
            negative = false;
    }
    check (negative, "a negative step stays negative");
    return finish ("testIntegratorSameSign");
}

int testIntegratorSlowIsSlower()
{
    Integrator fast;
    Integrator slow;
    fast.prepare (kRate);
    slow.prepare (kRate);
    fast.setKnob (Integrator::kKnobTime, 0.0f);
    slow.setKnob (Integrator::kKnobTime, 1.0f);
    const int samples = static_cast<int> (0.020 * kRate);
    for (int i = 0; i < samples; ++i)
    {
        drive (fast, 4.0f);
        drive (slow, 4.0f);
    }
    const float fastOut = fast.portValue[Integrator::kOut];
    const float slowOut = slow.portValue[Integrator::kOut];
    check (fastOut > 3.5f, "1 ms lag is near the input after 20 ms");
    check (slowOut < 0.2f, "2 s lag has barely moved after 20 ms");
    check (std::fabs (4.0f - slowOut) > std::fabs (4.0f - fastOut), "the slow time is farther from the input");
    return finish ("testIntegratorSlowIsSlower");
}

int testIntegratorIsItsOwnModule()
{
    Vco vco;
    check (vco.numKnobs() == 5, "vco knob count is unchanged");
    check (faceKnobBinding ("VCO", "GLIDE").knob == FaceKnob::None, "vco has no glide parameter");
    const FaceKnobBinding time = faceKnobBinding ("INT", "TIME");
    check (time.knob == FaceKnob::IntegratorTime, "time is a host parameter");
    check (std::strcmp (time.parameterName, "Integrator Time") == 0, "time name");
    check (ronin::exactlyEqual (time.fallback, 0.50f) && time.index == 0, "time faceplate");

    Integrator lag;
    check (lag.numKnobs() == 1, "the integrator has its own time knob");
    check (lag.numPorts() == 2, "in and out");

    const int in = panelJackIndex ("INT", "IN");
    const int out = panelJackIndex ("INT", "OUT");
    const int vcoHz = panelJackIndex ("VCO", "HZ/V");
    const int wet = panelJackIndex ("OUTPUT", "WET");
    check (in >= 0 && out >= 0 && vcoHz >= 0 && wet >= 0, "integrator jacks exist");
    check (kPanelJacks[in].module == 14 && kPanelJacks[in].port == 0 && kPanelJacks[in].dir == 0, "in");
    check (kPanelJacks[out].module == 14 && kPanelJacks[out].port == 1 && kPanelJacks[out].dir == 1, "out");
    check (kPanelJacks[in].module != kPanelJacks[vcoHz].module, "integrator is not a vco jack");

    PanelLink refused;
    check (orientPanelJacks (out, wet, 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "integrator is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (out, wet, 0, 1, 2, linked,
                             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 13) == PanelLinkResult::Ok,
           "integrator out feeds output wet");
    check (linked.sourceModule == 13 && linked.sourcePort == Integrator::kOut, "integrator out is the source");

    const char* path = RONIN_PROCESSOR_SOURCE;
    std::ifstream input (path);
    check (static_cast<bool> (input), "processor source is readable");
    const std::string text ((std::istreambuf_iterator<char> (input)), std::istreambuf_iterator<char>());
    const auto vcoAt = text.find ("addModule (vco)");
    const auto lagAt = text.find ("addModule (integrator)");
    check (vcoAt != std::string::npos && lagAt != std::string::npos && lagAt > vcoAt,
           "the rack adds the integrator after the vco");
    return finish ("testIntegratorIsItsOwnModule");
}
