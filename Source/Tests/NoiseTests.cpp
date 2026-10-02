// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/ExtIn.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"

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

bool near (float actual, float expected)
{
    const float delta = actual - expected;
    return delta < 1.0e-5f && delta > -1.0e-5f;
}

bool finiteSample (float value)
{
    return std::isfinite (value);
}

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

}

int testNoiseBothJacksMove()
{
    NoiseModule noise;
    check (noise.numPorts() == 2, "two ports");
    const PortDesc whitePort = noise.port (NoiseModule::kWhite);
    const PortDesc pinkPort = noise.port (NoiseModule::kPink);
    check (std::strcmp (whitePort.name, "White") == 0, "white name");
    check (std::strcmp (pinkPort.name, "Pink") == 0, "pink name");
    check (whitePort.type == PortType::Audio && whitePort.dir == PortDir::Out, "white is audio out");
    check (pinkPort.type == PortType::Audio && pinkPort.dir == PortDir::Out, "pink is audio out");

    noise.prepare (48000.0);
    float firstWhite = 0.0f;
    float firstPink = 0.0f;
    bool whiteMoved = false;
    bool pinkMoved = false;
    bool whiteNonZero = false;
    bool pinkNonZero = false;
    float whitePeak = 0.0f;
    float pinkPeak = 0.0f;

    for (int i = 0; i < 256; ++i)
    {
        noise.processSample();
        const float white = noise.portValue[NoiseModule::kWhite];
        const float pink = noise.portValue[NoiseModule::kPink];
        check (finiteSample (white) && finiteSample (pink), "finite sample");
        if (i == 0)
        {
            firstWhite = white;
            firstPink = pink;
        }
        if (white != 0.0f)
            whiteNonZero = true;
        if (pink != 0.0f)
            pinkNonZero = true;
        if (white != firstWhite)
            whiteMoved = true;
        if (pink != firstPink)
            pinkMoved = true;
        const float absWhite = std::fabs (white);
        const float absPink = std::fabs (pink);
        if (absWhite > whitePeak)
            whitePeak = absWhite;
        if (absPink > pinkPeak)
            pinkPeak = absPink;
    }

    check (whiteNonZero, "white leaves 0");
    check (pinkNonZero, "pink leaves 0");
    check (whiteMoved, "white is not constant");
    check (pinkMoved, "pink is not constant");
    check (whitePeak > 0.5f && whitePeak <= 2.51f, "white peak about ±2.5 V");
    check (pinkPeak > 0.05f && pinkPeak <= 4.0f, "pink peak about ±2.5 V");

    PatchGraph graph;
    ExtIn extIn;
    OutputModule output;
    NoiseModule patched;
    const int extIndex = graph.addModule (extIn);
    const int outIndex = graph.addModule (output);
    const int noiseIndex = graph.addModule (patched);
    check (graph.connect (extIndex, 0, outIndex, 0), "dry left");
    check (graph.connect (extIndex, 1, outIndex, 1), "dry right");
    check (graph.cableCount() == 2, "default cable count");
    output.setMix (0.0f);
    output.setLevel (1.0f);
    graph.prepare (48000.0);

    extIn.setHostSample (0.5f, -0.25f);
    graph.process();
    check (near (output.hostLeft(), 0.5f), "mix 0 keeps dry left");
    check (near (output.hostRight(), -0.25f), "mix 0 keeps dry right");

    bool running = false;
    for (int i = 0; i < 32; ++i)
    {
        graph.process();
        if (patched.portValue[NoiseModule::kWhite] != 0.0f
            && patched.portValue[NoiseModule::kPink] != 0.0f)
            running = true;
    }
    check (running, "noise runs with no cable");

    check (graph.connect (noiseIndex, NoiseModule::kWhite, outIndex, 2), "white to wet");
    output.setMix (1.0f);
    extIn.setHostSample (0.0f, 0.0f);
    float firstHost = 0.0f;
    bool hostMoved = false;
    bool hostNonZero = false;
    bool hostMono = true;
    for (int i = 0; i < 256; ++i)
    {
        graph.process();
        const float left = output.hostLeft();
        const float right = output.hostRight();
        if (! finiteSample (left) || ! finiteSample (right) || left != right)
            hostMono = false;
        if (i == 0)
            firstHost = left;
        if (left != 0.0f)
            hostNonZero = true;
        if (left != firstHost)
            hostMoved = true;
    }
    check (hostMono, "wet white is finite and mono");
    check (hostNonZero, "wet white leaves silence");
    check (hostMoved, "wet white moves the host");

    output.setMix (0.0f);
    extIn.setHostSample (0.5f, -0.25f);
    graph.process();
    check (near (output.hostLeft(), 0.5f), "mix 0 restores dry left");
    check (near (output.hostRight(), -0.25f), "mix 0 restores dry right");

    graph.disconnect (noiseIndex, NoiseModule::kWhite, outIndex, 2);
    check (graph.connect (noiseIndex, NoiseModule::kPink, outIndex, 2), "pink to wet");
    output.setMix (1.0f);
    extIn.setHostSample (0.0f, 0.0f);
    bool pinkHostMoved = false;
    float firstPinkHost = 0.0f;
    for (int i = 0; i < 256; ++i)
    {
        graph.process();
        if (i == 0)
            firstPinkHost = output.hostLeft();
        if (output.hostLeft() != firstPinkHost)
            pinkHostMoved = true;
    }
    check (pinkHostMoved, "wet pink moves the host");

    check (graph.connect (noiseIndex, NoiseModule::kWhite, outIndex, 2), "stack white on wet");
    graph.process();
    const float summed = patched.portValue[NoiseModule::kWhite] + patched.portValue[NoiseModule::kPink];
    check (near (output.portValue[2], summed), "wet sums white and pink");

    return finish ("testNoiseBothJacksMove");
}

int testNoiseSeedRepeats()
{
    NoiseModule first;
    NoiseModule second;
    first.prepare (48000.0);
    second.prepare (48000.0);

    for (int i = 0; i < 64; ++i)
    {
        first.processSample();
        second.processSample();
        check (first.portValue[NoiseModule::kWhite] == second.portValue[NoiseModule::kWhite], "white matches");
        check (first.portValue[NoiseModule::kPink] == second.portValue[NoiseModule::kPink], "pink matches");
    }

    NoiseModule restarted;
    first.prepare (48000.0);
    restarted.prepare (48000.0);
    for (int i = 0; i < 64; ++i)
    {
        first.processSample();
        restarted.processSample();
        check (first.portValue[NoiseModule::kWhite] == restarted.portValue[NoiseModule::kWhite], "prepare reseeds white");
        check (first.portValue[NoiseModule::kPink] == restarted.portValue[NoiseModule::kPink], "prepare clears pink");
    }

    return finish ("testNoiseSeedRepeats");
}

int testNoisePinkIsDarkerThanWhite()
{
    constexpr int kCount = 16384;
    NoiseModule noise;
    noise.prepare (48000.0);

    float previousWhite = 0.0f;
    float previousPink = 0.0f;
    double whiteSlope = 0.0;
    double pinkSlope = 0.0;
    float whitePeak = 0.0f;
    float pinkPeak = 0.0f;

    for (int i = 0; i < kCount; ++i)
    {
        noise.processSample();
        const float white = noise.portValue[NoiseModule::kWhite];
        const float pink = noise.portValue[NoiseModule::kPink];
        check (finiteSample (white) && finiteSample (pink), "finite spectrum sample");
        const float absWhite = std::fabs (white);
        const float absPink = std::fabs (pink);
        if (absWhite > whitePeak)
            whitePeak = absWhite;
        if (absPink > pinkPeak)
            pinkPeak = absPink;
        if (i > 0)
        {
            whiteSlope += static_cast<double> (std::fabs (white - previousWhite));
            pinkSlope += static_cast<double> (std::fabs (pink - previousPink));
        }
        previousWhite = white;
        previousPink = pink;
    }

    check (whitePeak <= 2.51f, "white stays near ±2.5 V");
    check (pinkPeak <= 4.0f, "pink stays near ±2.5 V");
    check (pinkSlope > 0.0 && whiteSlope > 0.0, "both slopes move");
    check (pinkSlope < whiteSlope, "pink high-band slope is lower");
    return finish ("testNoisePinkIsDarkerThanWhite");
}

int testNoiseHasNoKnobs()
{
    NoiseModule noise;
    check (noise.numKnobs() == 0, "knob count is 0");

    NoiseModule untouched;
    noise.prepare (48000.0);
    untouched.prepare (48000.0);
    noise.setKnob (0, 1.0f);
    noise.setKnob (1, 0.0f);
    noise.setKnob (-1, 0.5f);

    for (int i = 0; i < 64; ++i)
    {
        noise.processSample();
        untouched.processSample();
        check (noise.portValue[NoiseModule::kWhite] == untouched.portValue[NoiseModule::kWhite], "setKnob leaves white");
        check (noise.portValue[NoiseModule::kPink] == untouched.portValue[NoiseModule::kPink], "setKnob leaves pink");
    }

    return finish ("testNoiseHasNoKnobs");
}
