// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/ExtIn.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
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

bool sameCable (const Cable& a, const Cable& b)
{
    return a.sourceModule == b.sourceModule && a.sourcePort == b.sourcePort
           && a.destModule == b.destModule && a.destPort == b.destPort;
}

}

int testPanelStackRule()
{
    check (kPanelJackCount == 58, "panel jack count");
    check (sizeof (Cable) == sizeof (int) * 4, "Cable stays four ids");

    ExtIn ext;
    OutputModule output;
    NoiseModule noise;
    PatchGraph graph;
    const int extIndex = graph.addModule (ext);
    const int outputIndex = graph.addModule (output);
    const int noiseIndex = graph.addModule (noise);

    const int extL = panelJackIndex ("EXT IN", "L");
    const int extR = panelJackIndex ("EXT IN", "R");
    const int extMono = panelJackIndex ("EXT IN", "MONO");
    const int outL = panelJackIndex ("OUTPUT", "L");
    const int outR = panelJackIndex ("OUTPUT", "R");
    const int outWet = panelJackIndex ("OUTPUT", "WET");
    const int white = panelJackIndex ("NOISE", "WHITE");
    const int vco = panelJackIndex ("VCO", "HZ/V");
    check (extL >= 0 && outL >= 0 && extMono >= 0 && white >= 0 && vco >= 0, "jack names resolve");

    PanelLink dry;
    check (orientPanelJacks (extL, outL, extIndex, outputIndex, noiseIndex, dry) == PanelLinkResult::Ok,
           "ext L to output L");
    check (dry.sourceModule == extIndex && dry.sourcePort == 0, "dry source is Ext In L");
    check (dry.destModule == outputIndex && dry.destPort == 0, "dry dest is Output L");
    check (orientPanelJacks (outL, extL, extIndex, outputIndex, noiseIndex, dry) == PanelLinkResult::Ok,
           "reversed drag still orients output to input");
    check (dry.sourcePort == 0 && dry.destPort == 0, "reversed drag keeps the ports");

    PanelLink bad;
    check (orientPanelJacks (outL, outR, extIndex, outputIndex, noiseIndex, bad) == PanelLinkResult::BadType,
           "input to input");
    check (orientPanelJacks (extL, extR, extIndex, outputIndex, noiseIndex, bad) == PanelLinkResult::BadType,
           "output to output");
    check (orientPanelJacks (extL, extL, extIndex, outputIndex, noiseIndex, bad) == PanelLinkResult::BadType,
           "jack to itself");
    check (orientPanelJacks (vco, outWet, extIndex, outputIndex, noiseIndex, bad) == PanelLinkResult::Unmapped,
           "unmapped jack does not enter the graph");

    check (graph.connect (extIndex, 0, outputIndex, 0), "default left");
    check (graph.connect (extIndex, 1, outputIndex, 1), "default right");

    PanelLink stacked;
    check (orientPanelJacks (extMono, outL, extIndex, outputIndex, noiseIndex, stacked) == PanelLinkResult::Ok,
           "second cable onto an input");
    check (graph.connect (stacked.sourceModule, stacked.sourcePort, stacked.destModule, stacked.destPort),
           "stack connects");

    PanelLink fan;
    check (orientPanelJacks (white, outWet, extIndex, outputIndex, noiseIndex, fan) == PanelLinkResult::Ok, "fan to wet");
    check (orientPanelJacks (white, outR, extIndex, outputIndex, noiseIndex, fan) == PanelLinkResult::Ok,
           "same output fans to a second input");
    check (fan.sourceModule == noiseIndex && fan.sourcePort == 0, "fan source is white");

    Cable before[8] {};
    const int beforeCount = graph.copyPublishedCables (before, 8);
    VisualCable visual[8] {};
    const int visualCount = loadPublishedCables (visual, 8, before, beforeCount, extIndex, outputIndex, noiseIndex);
    check (visualCount == 3, "three published cables become three visual cables");
    check (visual[0].a == extL && visual[0].b == outL, "first visual cable is the dry left pair");
    check (visual[0].color != visual[1].color, "two dry cables do not share a color");

    int plugs[8] {};
    const int onOutputL = plugsAtJack (visual, visualCount, outL, plugs, 8);
    check (onOutputL == 2, "output L holds two plugs");
    const int reversed[2] = { plugs[1], plugs[0] };
    check (reorderJackStack (visual, visualCount, outL, reversed, 2), "stack reorder");

    Cable after[8] {};
    const int afterCount = graph.copyPublishedCables (after, 8);
    check (afterCount == beforeCount, "reorder does not publish");
    bool publishedSame = true;
    for (int i = 0; i < beforeCount; ++i)
    {
        if (! sameCable (before[i], after[i]))
            publishedSame = false;
    }
    check (publishedSame, "stack order is not the graph order");

    bool endpointsSame = true;
    for (int i = 0; i < visualCount; ++i)
    {
        bool found = false;
        for (int j = 0; j < beforeCount; ++j)
        {
            if (visual[i].sourceModule == before[j].sourceModule && visual[i].sourcePort == before[j].sourcePort
                && visual[i].destModule == before[j].destModule && visual[i].destPort == before[j].destPort)
                found = true;
        }
        if (! found)
            endpointsSame = false;
    }
    check (endpointsSame, "reorder keeps each cable's endpoints");

    ext.setHostSample (1.0f, 0.0f);
    graph.prepare (48000.0);
    graph.process();
    const float summed = output.portValue[0];
    check (std::fabs (summed - 7.5f) < 1.0e-4f, "stacked input sums Ext L and Mono");

    return finish ("testPanelStackRule");
}

int testPanelKnobs()
{
    check (kPanelKnobCount == 31, "panel knob count");

    const int vco = panelJackIndex ("VCO", "HZ/V");
    const int extL = panelJackIndex ("EXT IN", "L");
    const int outL = panelJackIndex ("OUTPUT", "L");
    check (vco >= 0 && extL >= 0 && outL >= 0, "VCO, Ext In, and Output jacks exist");
    check (kPanelJacks[vco].x < kPanelJacks[extL].x, "VCO stays left of Ext In");
    check (kPanelJacks[extL].x < kPanelJacks[outL].x, "Ext In sits left of Output");
    check (kPowerW == 40.0f && kPowerH == 20.0f, "power rocker is the on off control");
    check (kHoldHitW == 34.0f && kHoldHitH == 34.0f, "hold key is the square cap");
    check (kPresetChars == 16, "preset screen is 16 characters");

    float leftOfOutput = -1.0f;
    const char* beside = "";
    for (int i = 0; i < kPanelJackCount; ++i)
    {
        if (kPanelJacks[i].x < kPanelJacks[outL].x && kPanelJacks[i].x > leftOfOutput)
        {
            leftOfOutput = kPanelJacks[i].x;
            beside = kPanelJacks[i].section;
        }
    }
    check (std::strcmp (beside, "EXT IN") == 0, "the column beside Output is Ext In");

    const int cutoff = panelKnobIndex ("VCF", "CUTOFF");
    const int ratio = panelKnobIndex ("DIV", "RATIO SWITCH");
    check (cutoff >= 0 && ratio >= 0, "cutoff and divider switch exist");
    const float start = kPanelKnobs[cutoff].valueDefault;
    check (std::fabs (panelKnobDrag (start, 40.0f, false, false) - (start + 0.2f)) < 1.0e-4f,
           "drag up 40px adds 0.2");
    check (std::fabs (panelKnobDrag (start, 200.0f, true, false) - (start + 0.2f)) < 1.0e-4f,
           "shift drag is five times finer");
    check (panelKnobDrag (start, 5000.0f, false, false) == 1.0f, "drag clamps at 1");
    check (std::fabs (panelKnobWheel (start + 0.4f, 100.0f, false, false) - (start + 0.3f)) < 1.0e-4f,
           "wheel down removes 0.1");
    check (std::fabs (panelKnobFromWheel (0.5f, 1.0f, false, false, false) - 0.6f) < 1.0e-4f,
           "wheel up adds 0.1");
    check (std::fabs (panelKnobFromWheel (0.5f, 1.0f, true, false, false) - 0.4f) < 1.0e-4f,
           "reversed wheel up is a downward push");

    check (kPanelKnobs[ratio].kind == 1, "divider control is the switch");
    check (kPanelKnobs[ratio].valueDefault == 0.5f, "switch default is 4");
    check (panelKnobSwitchClick (0.5f) == 1.0f, "click steps 4 to 16");
    check (panelKnobSwitchClick (1.0f) == 0.0f, "click steps 16 to 2");
    check (panelKnobSwitchClick (0.0f) == 0.5f, "click steps 2 to 4");
    check (panelKnobDrag (1.0f, -40.0f, false, true) == 0.5f, "switch drag snaps");
    check (panelKnobFromWheel (0.5f, 1.0f, false, false, true) == 1.0f, "wheel up steps the switch");
    check (panelKnobAngleDegrees (false, 0.0f) == -135.0f, "rotary start angle");
    check (panelKnobAngleDegrees (false, 1.0f) == 135.0f, "rotary end angle");
    check (panelKnobAngleDegrees (true, 0.5f) == 0.0f, "switch centre angle");

    return finish ("testPanelKnobs");
}
