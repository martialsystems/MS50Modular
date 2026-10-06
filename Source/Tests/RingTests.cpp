// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/PatchGraph.h"
#include "Modular/Ring.h"
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

void drive (Ring& ring, float a, float b)
{
    ring.portValue[Ring::kA] = a;
    ring.portValue[Ring::kB] = b;
    ring.processSample();
}

class FixedCv : public Module {
public:
    explicit FixedCv (float value)
        : value_ (value)
    {
    }

    int numPorts() const override { return 1; }

    PortDesc port (int) const override
    {
        return { "Out", PortType::CV, PortDir::Out };
    }

    void setKnob (int, float) override {}
    void prepare (double rate) override { sampleRate = rate; }
    void processSample() override { portValue[0] = value_; }

private:
    float value_;
};

}

int testRingFourQuadrant()
{
    Ring ring;
    ring.prepare (48000.0);
    drive (ring, 5.0f, 5.0f);
    check (std::fabs (ring.portValue[Ring::kOut] - 5.0f) < 1.0e-5f, "+5 and +5 produce +5");
    drive (ring, 5.0f, -5.0f);
    check (std::fabs (ring.portValue[Ring::kOut] + 5.0f) < 1.0e-5f, "+5 and -5 produce -5");
    drive (ring, -5.0f, -5.0f);
    check (std::fabs (ring.portValue[Ring::kOut] - 5.0f) < 1.0e-5f, "-5 and -5 produce +5");
    drive (ring, -5.0f, 5.0f);
    check (std::fabs (ring.portValue[Ring::kOut] + 5.0f) < 1.0e-5f, "-5 and +5 produce -5");
    return finish ("testRingFourQuadrant");
}

int testRingZeroKills()
{
    Ring ring;
    ring.prepare (48000.0);
    drive (ring, 5.0f, 0.0f);
    check (ring.portValue[Ring::kOut] == 0.0f, "B at 0 kills the product");
    drive (ring, 0.0f, -3.0f);
    check (ring.portValue[Ring::kOut] == 0.0f, "A at 0 kills the product");
    return finish ("testRingZeroKills");
}

int testRingPassesDcProduct()
{
    Ring ring;
    ring.prepare (48000.0);
    drive (ring, 2.5f, 4.0f);
    const float first = ring.portValue[Ring::kOut];
    check (std::fabs (first - 2.0f) < 1.0e-5f, "2.5 V times 4 V is 2 V");
    for (int i = 0; i < 200; ++i)
        drive (ring, 2.5f, 4.0f);
    check (std::fabs (ring.portValue[Ring::kOut] - first) < 1.0e-5f, "a constant product does not sag");

    FixedCv left (2.5f);
    FixedCv right (4.0f);
    Ring patched;
    PatchGraph graph;
    const int leftIndex = graph.addModule (left);
    const int rightIndex = graph.addModule (right);
    const int ringIndex = graph.addModule (patched);
    check (graph.connect (leftIndex, 0, ringIndex, Ring::kA), "A takes the left constant");
    check (graph.connect (rightIndex, 0, ringIndex, Ring::kB), "B takes the right constant");
    graph.prepare (48000.0);
    graph.process();
    check (std::fabs (patched.portValue[Ring::kOut] - 2.0f) < 1.0e-4f, "cabled DC is the same product");
    return finish ("testRingPassesDcProduct");
}

int testRingHasNoKnobs()
{
    Ring ring;
    check (ring.numKnobs() == 0, "no knobs");
    ring.prepare (48000.0);
    drive (ring, 5.0f, 5.0f);
    ring.setKnob (0, 0.0f);
    drive (ring, 5.0f, 5.0f);
    check (std::fabs (ring.portValue[Ring::kOut] - 5.0f) < 1.0e-5f, "setKnob does not scale the product");
    check (faceKnobBinding ("RING", "NULL").knob == FaceKnob::None, "no null parameter");
    for (int i = 0; i < kPanelKnobCount; ++i)
        check (std::strcmp (kPanelKnobs[i].section, "RING") != 0, "ring has no faceplate knob");
    return finish ("testRingHasNoKnobs");
}

int testRingPanelJacks()
{
    const int a = panelJackIndex ("RING", "A");
    const int b = panelJackIndex ("RING", "B");
    const int out = panelJackIndex ("RING", "OUT");
    const int wet = panelJackIndex ("OUTPUT", "WET");
    const int sh = panelJackIndex ("S&H", "IN");
    check (a >= 0 && b >= 0 && out >= 0 && wet >= 0 && sh >= 0, "ring jacks exist");
    check (kPanelJacks[a].module == 11 && kPanelJacks[a].port == 0 && kPanelJacks[a].dir == 0, "A");
    check (kPanelJacks[b].module == 11 && kPanelJacks[b].port == 1 && kPanelJacks[b].dir == 0, "B");
    check (kPanelJacks[out].module == 11 && kPanelJacks[out].port == 2 && kPanelJacks[out].dir == 1, "out");
    check (kPanelJacks[sh].module == 0, "sample and hold stays unmapped");

    PanelLink refused;
    check (orientPanelJacks (out, wet, 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "ring is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (out, wet, 0, 1, 2, linked, -1, -1, -1, -1, -1, -1, -1, 10) == PanelLinkResult::Ok,
           "ring out feeds output wet");
    check (linked.sourceModule == 10 && linked.sourcePort == Ring::kOut, "ring out is the source");
    check (linked.destModule == 1 && linked.destPort == 2, "wet is the destination");
    return finish ("testRingPanelJacks");
}
