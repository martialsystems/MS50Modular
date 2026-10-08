// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/Divider.h"
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

void clock (Divider& divider, float volts)
{
    divider.portValue[Divider::kIn] = volts;
    divider.processSample();
}

}

int testDividerOnlyTwoAndFour()
{
    Divider divider;
    check (divider.numPorts() == 3, "three ports");
    check (divider.numKnobs() == 0, "no frequency knob");
    check (std::strcmp (divider.port (Divider::kIn).name, "In") == 0, "input name");
    check (std::strcmp (divider.port (Divider::kDiv2).name, "Div2") == 0, "div2 name");
    check (std::strcmp (divider.port (Divider::kDiv4).name, "Div4") == 0, "div4 name");
    const FaceKnobBinding ratio = faceKnobBinding ("DIV", "RATIO SWITCH");
    check (ratio.knob == FaceKnob::DividerRatio && ratio.index < 0, "ratio switch is a host setting, not a module knob");
    check (ratio.fallback == 0.0f && std::strcmp (ratio.parameterId, "dividerRatio") == 0, "ratio switch starts on /2");

    const int in = panelJackIndex ("DIV", "IN");
    const int div2 = panelJackIndex ("DIV", "/2");
    const int div4 = panelJackIndex ("DIV", "/4");
    check (in >= 0 && div2 >= 0 && div4 >= 0, "divider jacks exist");
    check (panelJackIndex ("DIV", "/16") < 0, "no /16 jack");
    check (kPanelJacks[in].module == 12 && kPanelJacks[in].port == 0 && kPanelJacks[in].dir == 0, "in");
    check (kPanelJacks[div2].module == 12 && kPanelJacks[div2].port == 1 && kPanelJacks[div2].dir == 1, "/2");
    check (kPanelJacks[div4].module == 12 && kPanelJacks[div4].port == 2 && kPanelJacks[div4].dir == 1, "/4");

    const int wet = panelJackIndex ("OUTPUT", "WET");
    PanelLink refused;
    check (orientPanelJacks (div2, wet, 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "divider is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (div2, wet, 0, 1, 2, linked, -1, -1, -1, -1, -1, -1, -1, -1, 11) == PanelLinkResult::Ok,
           "/2 feeds output wet");
    check (linked.sourceModule == 11 && linked.sourcePort == Divider::kDiv2, "/2 is the source");
    check (linked.destModule == 1 && linked.destPort == 2, "wet is the destination");
    return finish ("testDividerOnlyTwoAndFour");
}

int testDividerSquareCounts()
{
    Divider divider;
    divider.prepare (48000.0);
    int rises2 = 0;
    int rises4 = 0;
    float previous2 = 0.0f;
    float previous4 = 0.0f;
    bool levelsLegal = true;
    for (int edge = 0; edge < 100; ++edge)
    {
        clock (divider, 0.0f);
        clock (divider, 5.0f);   // a JCS 0/5 V gate (§3.7: exactly 1.0 V no longer clocks, > 1.0 V does)
        const float div2 = divider.portValue[Divider::kDiv2];
        const float div4 = divider.portValue[Divider::kDiv4];
        if ((div2 != 0.0f && div2 != 5.0f) || (div4 != 0.0f && div4 != 5.0f))
            levelsLegal = false;
        if (div2 > 2.5f && previous2 < 2.5f)
            ++rises2;
        if (div4 > 2.5f && previous4 < 2.5f)
            ++rises4;
        previous2 = div2;
        previous4 = div4;
    }
    check (levelsLegal, "outputs are 0 V or +5 V");
    check (rises2 == 50, "100 input edges produce 50 /2 edges");
    check (rises4 == 25, "100 input edges produce 25 /4 edges");
    return finish ("testDividerSquareCounts");
}

int testDividerIgnoresTinySignal()
{
    Divider divider;
    divider.prepare (48000.0);
    bool stayedLow = true;
    for (int i = 0; i < 64; ++i)
    {
        clock (divider, (i % 2 == 0) ? 0.1f : -0.1f);
        if (divider.portValue[Divider::kDiv2] != 0.0f || divider.portValue[Divider::kDiv4] != 0.0f)
            stayedLow = false;
    }
    check (stayedLow, "0.1 V does not clock the divider");

    bool crossed = false;
    for (int i = 0; i < 16; ++i)
    {
        const float triangle = (i < 8) ? (-2.5f + 5.0f * static_cast<float> (i) / 8.0f)
                                        : (2.5f - 5.0f * static_cast<float> (i - 8) / 8.0f);
        clock (divider, triangle);
        if (divider.portValue[Divider::kDiv2] == 5.0f)
            crossed = true;
    }
    check (crossed, "a ±2.5 V triangle crosses the Schmitt window");
    return finish ("testDividerIgnoresTinySignal");
}
