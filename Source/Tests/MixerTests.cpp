// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/Mixer.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
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

void drive (Mixer& mixer, float in1, float in2, float in3)
{
    mixer.portValue[Mixer::kIn1] = in1;
    mixer.portValue[Mixer::kIn2] = in2;
    mixer.portValue[Mixer::kIn3] = in3;
    mixer.processSample();
}

}

int testMixerSumsThree()
{
    Mixer mixer;
    mixer.prepare (48000.0);
    check (mixer.numKnobs() == 3, "three level knobs");
    check (mixer.numPorts() == 4, "three inputs and one output");
    check (std::fabs (mixer.presetKnob (Mixer::kKnobLevel1) - 0.8f) < 1.0e-6f, "level 1 default");
    check (std::fabs (mixer.presetKnob (Mixer::kKnobLevel2) - 0.8f) < 1.0e-6f, "level 2 default");
    check (std::fabs (mixer.presetKnob (Mixer::kKnobLevel3) - 0.8f) < 1.0e-6f, "level 3 default");

    drive (mixer, 1.0f, 2.0f, 3.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] - (-4.8f)) < 1.0e-5f, "default levels sum and invert");

    mixer.setKnob (Mixer::kKnobLevel1, 1.0f);
    mixer.setKnob (Mixer::kKnobLevel2, 0.5f);
    mixer.setKnob (Mixer::kKnobLevel3, 0.25f);
    for (int settle = 0; settle < 9600; ++settle)   // §3.5: let the 10 ms knob ramp settle
        drive (mixer, 1.0f, 2.0f, 4.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] - (-3.0f)) < 1.0e-5f, "1 + 1 + 1 inverted");

    const FaceKnobBinding level1 = faceKnobBinding ("MIX", "LEVEL 1");
    const FaceKnobBinding level2 = faceKnobBinding ("MIX", "LEVEL 2");
    const FaceKnobBinding level3 = faceKnobBinding ("MIX", "LEVEL 3");
    check (level1.knob == FaceKnob::MixerLevel1 && level1.index == 0 && ronin::exactlyEqual (level1.fallback, 0.8f), "level 1 host");
    check (level2.knob == FaceKnob::MixerLevel2 && level2.index == 1 && ronin::exactlyEqual (level2.fallback, 0.8f), "level 2 host");
    check (level3.knob == FaceKnob::MixerLevel3 && level3.index == 2 && ronin::exactlyEqual (level3.fallback, 0.8f), "level 3 host");
    check (std::strcmp (level1.parameterId, "mixerLevel1") == 0, "level 1 id");
    check (std::strcmp (level1.parameterName, "Mixer Level 1") == 0, "level 1 name");
    check (std::strcmp (level2.parameterName, "Mixer Level 2") == 0, "level 2 name");
    check (std::strcmp (level3.parameterName, "Mixer Level 3") == 0, "level 3 name");

    const int in1 = panelJackIndex ("MIX", "IN 1");
    const int in2 = panelJackIndex ("MIX", "IN 2");
    const int in3 = panelJackIndex ("MIX", "IN 3");
    const int out = panelJackIndex ("MIX", "OUT");
    const int white = panelJackIndex ("NOISE", "WHITE");
    const int wet = panelJackIndex ("OUTPUT", "WET");
    check (in1 >= 0 && in2 >= 0 && in3 >= 0 && out >= 0 && white >= 0 && wet >= 0, "mixer jacks exist");
    check (kPanelJacks[in1].module == 15 && kPanelJacks[in1].port == 0 && kPanelJacks[in1].dir == 0, "in 1");
    check (kPanelJacks[in2].module == 15 && kPanelJacks[in2].port == 1 && kPanelJacks[in2].dir == 0, "in 2");
    check (kPanelJacks[in3].module == 15 && kPanelJacks[in3].port == 2 && kPanelJacks[in3].dir == 0, "in 3");
    check (kPanelJacks[out].module == 15 && kPanelJacks[out].port == 3 && kPanelJacks[out].dir == 1, "out");
    check (ronin::exactlyEqual (kPanelKnobs[panelKnobIndex ("MIX", "LEVEL 1")].valueDefault, 0.8f), "level 1 faceplate");
    check (ronin::exactlyEqual (kPanelKnobs[panelKnobIndex ("MIX", "LEVEL 2")].valueDefault, 0.8f), "level 2 faceplate");
    check (ronin::exactlyEqual (kPanelKnobs[panelKnobIndex ("MIX", "LEVEL 3")].valueDefault, 0.8f), "level 3 faceplate");

    PanelLink refused;
    check (orientPanelJacks (white, in1, 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "mixer is refused until its graph index is passed");
    PanelLink into;
    check (orientPanelJacks (white, in1, 0, 1, 2, into,
                             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14) == PanelLinkResult::Ok,
           "noise white feeds mixer in 1");
    check (into.sourceModule == 2 && into.sourcePort == NoiseModule::kWhite, "white is the source");
    check (into.destModule == 14 && into.destPort == Mixer::kIn1, "in 1 is the destination");
    PanelLink wetLink;
    check (orientPanelJacks (out, wet, 0, 1, 2, wetLink,
                             -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 14) == PanelLinkResult::Ok,
           "mixer out feeds output wet");
    check (wetLink.sourceModule == 14 && wetLink.sourcePort == Mixer::kOut, "mixer out is the source");
    check (wetLink.destModule == 1 && wetLink.destPort == 2, "wet is the destination");

    NoiseModule noise;
    Mixer patched;
    OutputModule output;
    PatchGraph graph;
    const int noiseIndex = graph.addModule (noise);
    const int mixerIndex = graph.addModule (patched);
    const int outputIndex = graph.addModule (output);
    check (graph.connect (noiseIndex, NoiseModule::kWhite, mixerIndex, Mixer::kIn1), "white cable connects");
    check (graph.connect (mixerIndex, Mixer::kOut, outputIndex, 2), "wet cable connects");
    graph.prepare (48000.0);
    graph.process();
    check (std::isfinite (patched.portValue[Mixer::kOut]), "cabled mix is finite");
    return finish ("testMixerSumsThree");
}

int testMixerIsInverted()
{
    Mixer mixer;
    mixer.prepare (48000.0);
    mixer.setKnob (Mixer::kKnobLevel1, 1.0f);
    mixer.setKnob (Mixer::kKnobLevel2, 0.0f);
    mixer.setKnob (Mixer::kKnobLevel3, 0.0f);
    drive (mixer, 1.25f, 9.0f, -9.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] + 1.25f) < 1.0e-5f, "positive input leaves inverted");
    drive (mixer, -0.5f, 4.0f, 4.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] - 0.5f) < 1.0e-5f, "negative input leaves positive");
    drive (mixer, 0.0f, 3.0f, -3.0f);
    check (ronin::exactlyEqual (mixer.portValue[Mixer::kOut], 0.0f), "zero input stays zero");
    return finish ("testMixerIsInverted");
}

int testMixerLevelZeroMutesThatInput()
{
    Mixer mixer;
    mixer.prepare (48000.0);
    mixer.setKnob (Mixer::kKnobLevel1, 1.0f);
    mixer.setKnob (Mixer::kKnobLevel2, 0.0f);
    mixer.setKnob (Mixer::kKnobLevel3, 1.0f);
    drive (mixer, 2.0f, 5.0f, 3.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] + 5.0f) < 1.0e-5f, "muted input drops out of the sum");
    drive (mixer, 2.0f, -8.0f, 3.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] + 5.0f) < 1.0e-5f, "changing the muted input does nothing");
    drive (mixer, 2.0f, 5.0f, 0.0f);
    check (std::fabs (mixer.portValue[Mixer::kOut] + 2.0f) < 1.0e-5f, "the other inputs still pass");
    return finish ("testMixerLevelZeroMutesThatInput");
}
