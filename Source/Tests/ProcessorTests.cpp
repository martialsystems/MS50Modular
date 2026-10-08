// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// Processor-level tests (JUCE): RONIN_Redesign §6 tests that need the real plugin state and latency reporting.

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace {

int gChecks = 0;
int gFailed = 0;
int gPassed = 0;

void check (bool ok, const char* message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message);
        ++gChecks;
    }
}

void finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    (failed == 0 ? gPassed : gFailed) += 1;
}

void runBlocks (RoninAudioProcessor& p, int blocks, int size = 256, float input = 0.0f)
{
    juce::AudioBuffer<float> buffer (2, size);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < size; ++i)
                buffer.setSample (ch, i, input);
        p.processBlock (buffer, midi);
    }
}

juce::MemoryBlock saveState (RoninAudioProcessor& p)
{
    juce::MemoryBlock block;
    p.getStateInformation (block);
    return block;
}

}

void testHqDefaultOff()
{
    RoninAudioProcessor p;
    check (p.hqParameter() != nullptr && ! p.hqParameter()->get(), "HQ parameter defaults OFF");
    p.prepareToPlay (48000.0, 256);
    check (p.getLatencySamples() == 0, "a new instance reports 0 latency");
    check (! p.hqActive(), "a new instance runs at 1x");
    check (std::fabs (p.engineSampleRate() - 48000.0) < 1.0e-9, "engine at the host rate");

    p.hqParameter()->setValueNotifyingHost (1.0f);
    p.prepareToPlay (48000.0, 256);
    check (p.getLatencySamples() == 23, "HQ on reports 23 samples");
    check (std::fabs (p.engineSampleRate() - 96000.0) < 1.0e-9, "HQ runs the engine at 2x");
    runBlocks (p, 4);

    // A fresh instance is never on, even after another instance turned it on.
    RoninAudioProcessor q;
    q.prepareToPlay (44100.0, 128);
    check (q.getLatencySamples() == 0 && ! q.hqActive(), "the default is never on");

    // INIT (a new patch) does not turn HQ on.
    p.setCurrentProgram (kInitPreset);
    check (p.hqParameter()->get(), "INIT leaves the user's HQ choice alone");
    finish ("testHqDefaultOff");
}

void testHqLatencyIsTwentyThree()
{
    // EXT IN L -> OUTPUT L dry path: an impulse comes out 23 samples later with HQ on, 0 with HQ off.
    for (bool hq : { false, true })
    {
        RoninAudioProcessor p;
        p.hqParameter()->setValueNotifyingHost (hq ? 1.0f : 0.0f);
        p.effectParameter()->setValueNotifyingHost (0.0f);   // effect off: dry
        p.prepareToPlay (48000.0, 64);
        runBlocks (p, 8, 64);
        juce::AudioBuffer<float> buffer (2, 64);
        juce::MidiBuffer midi;
        std::vector<float> out;
        for (int b = 0; b < 4; ++b)
        {
            buffer.clear();
            if (b == 0)
                buffer.setSample (0, 0, 0.5f);
            p.processBlock (buffer, midi);
            for (int i = 0; i < 64; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        int peak = 0;
        for (int i = 1; i < static_cast<int> (out.size()); ++i)
            if (std::fabs (out[static_cast<size_t> (i)]) > std::fabs (out[static_cast<size_t> (peak)]))
                peak = i;
        check (peak == (hq ? 23 : 0), hq ? "HQ dry impulse peaks at 23 samples" : "1x dry impulse at 0 samples");
        check (peak == p.getLatencySamples(), "reported latency matches the measured path");
        // DC gain of the dry path is the same in both modes (the halfband has unity DC gain).
        double sum = 0.0;
        for (float v : out)
            sum += static_cast<double> (v);
        check (std::fabs (std::fabs (sum) - 0.5) < 0.01, "the dry impulse keeps its area in both modes");
    }
    finish ("testHqLatencyIsTwentyThree");
}

void testTriDefaultFreshAndInit()
{
    RoninAudioProcessor p;
    check (p.triShape() == Vco::TriShape::Triangle, "a fresh instance is on TRIANGLE");
    p.triShapeParameter()->setValueNotifyingHost (1.0f);
    check (p.triShape() == Vco::TriShape::Parabola, "PARABOLA (legacy) is selectable");
    p.setCurrentProgram (kInitPreset);
    check (p.triShape() == Vco::TriShape::Triangle, "INIT (File -> New) is TRIANGLE");
    finish ("testTriDefaultFreshAndInit");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    testHqDefaultOff();
    testHqLatencyIsTwentyThree();
    testTriDefaultFreshAndInit();
    std::printf ("ProcessorTests: %d passed, %d failed\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
