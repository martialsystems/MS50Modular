// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"

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

bool near (float actual, float expected)
{
    const float delta = actual - expected;
    return delta < 1.0e-5f && delta > -1.0e-5f;
}

struct DryRack {
    PatchGraph graph;
    ExtIn extIn;
    OutputModule output;
    int extIndex = -1;
    int outIndex = -1;

    DryRack()
    {
        extIndex = graph.addModule (extIn);
        outIndex = graph.addModule (output);
        graph.connect (extIndex, 0, outIndex, 0);
        graph.connect (extIndex, 1, outIndex, 1);
        output.setMix (0.0f);
        output.setLevel (1.0f);
        graph.prepare (48000.0);
    }

    void drive (float leftUnit, float rightUnit)
    {
        extIn.setHostSample (leftUnit, rightUnit);
        graph.process();
    }
};

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

}

int testDryMixPassesStereo()
{
    DryRack rack;
    check (rack.graph.cableCount() == 2, "L and R cables");

    rack.drive (0.5f, -0.25f);
    check (near (rack.output.hostLeft(), 0.5f), "first left");
    check (near (rack.output.hostRight(), -0.25f), "first right");

    rack.drive (-0.125f, 0.75f);
    check (near (rack.output.hostLeft(), -0.125f), "second left, no delay");
    check (near (rack.output.hostRight(), 0.75f), "second right, no delay");
    return finish ("testDryMixPassesStereo");
}

int testExtInMonoAveragesStereo()
{
    DryRack rack;
    rack.drive (1.0f, -0.5f);

    const float mono = rack.extIn.portValue[2];
    check (mono == 0.5f * (5.0f + -2.5f), "mono volts");
    check (mono == 1.25f, "mono is 1.25 V");
    check (near (rack.output.hostLeft(), 1.0f), "dry left image");
    check (near (rack.output.hostRight(), -0.5f), "dry right image");
    return finish ("testExtInMonoAveragesStereo");
}

int testWetMixIgnoresDry()
{
    DryRack rack;
    rack.output.setMix (1.0f);
    rack.drive (0.8f, -0.4f);
    check (near (rack.output.hostLeft(), 0.0f), "wet unpatched left");
    check (near (rack.output.hostRight(), 0.0f), "wet unpatched right");
    return finish ("testWetMixIgnoresDry");
}

int testLevelZeroIsSilence()
{
    DryRack rack;
    rack.output.setLevel (0.0f);
    rack.drive (0.8f, -0.4f);
    check (near (rack.output.hostLeft(), 0.0f), "level 0 left");
    check (near (rack.output.hostRight(), 0.0f), "level 0 right");
    return finish ("testLevelZeroIsSilence");
}

int testLeftOnlyStaysLeft()
{
    DryRack rack;
    rack.drive (0.4f, 0.0f);
    check (near (rack.output.hostLeft(), 0.4f), "left stays");
    check (near (rack.output.hostRight(), 0.0f), "right stays empty");
    return finish ("testLeftOnlyStaysLeft");
}
