// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/DefaultPatch.h"
#include "Modular/EffectSwitch.h"
#include "Modular/Eg1.h"
#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vcf.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>

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

bool near (float actual, float expected)
{
    const float delta = actual - expected;
    return delta < 1.0e-5f && delta > -1.0e-5f;
}

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

class VoltOut : public Module {
public:
    explicit VoltOut (float volts)
        : volts_ (volts)
    {
    }

    int numPorts() const override { return 1; }

    PortDesc port (int) const override { return { "Out", PortType::Audio, PortDir::Out }; }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override { portValue[0] = volts_; }

private:
    float volts_;
};

struct BlendRack {
    VoltOut wet;
    PatchGraph graph;
    ExtIn ext;
    OutputModule output;

    explicit BlendRack (float wetVolts)
        : wet (wetVolts)
    {
        const int extIndex = graph.addModule (ext);
        const int outIndex = graph.addModule (output);
        const int wetIndex = graph.addModule (wet);
        check (graph.connect (extIndex, 0, outIndex, 0), "dry left cable");
        check (graph.connect (extIndex, 1, outIndex, 1), "dry right cable");
        check (graph.connect (wetIndex, 0, outIndex, 2), "wet cable");
        output.setLevel (1.0f);
        output.setOutputLevel (0.7f);
        graph.prepare (kRate);
    }

    void run (bool effectOn, float mixKnob, float left, float right)
    {
        output.setMix (outputMixAfterSwitch (effectOn, mixKnob));
        ext.setHostSample (left, right);
        for (int settle = 0; settle < 9600; ++settle)   // §3.5: let the 10 ms knob ramp settle
            graph.process();
    }
};

float hostFromBlend (float dryHost, float wetVolts, float mix, float levelGain)
{
    return (dryHost * (1.0f - mix) + wetVolts * OutputModule::kVoltsToHost * mix) * levelGain;
}

float vcfAtMix (bool effectOn, float mixKnob)
{
    PatchGraph graph;
    ExtIn ext;
    OutputModule output;
    Vcf vcf;
    Vca1 vca;
    Eg1 eg;
    const int extIndex = graph.addModule (ext);
    const int outIndex = graph.addModule (output);
    const int vcfIndex = graph.addModule (vcf);
    const int vcaIndex = graph.addModule (vca);
    const int egIndex = graph.addModule (eg);
    check (connectFactoryCables (graph, extIndex, outIndex, vcfIndex, vcaIndex, egIndex), "factory cables");
    output.setLevel (1.0f);
    output.setOutputLevel (0.7f);
    output.setMix (outputMixAfterSwitch (effectOn, mixKnob));
    graph.prepare (kRate);

    float sig = 0.0f;
    for (int i = 0; i < 128; ++i)
    {
        ext.setHostSample (0.02f, 0.02f);
        graph.process();
        sig = vcf.portValue[Vcf::kSigOut];
    }
    check (graph.cableCount() == 8, "mix leaves the eight cables");
    return sig;
}

}

int testOutputMixBlendsWet()
{
    const FaceKnobBinding mix = faceKnobBinding ("OUTPUT", "MIX");
    const FaceKnobBinding level = faceKnobBinding ("OUTPUT", "LEVEL");
    check (mix.knob == FaceKnob::OutputMix, "output mix is a host knob");
    check (std::strcmp (mix.parameterId, "outputMix") == 0, "parameter id");
    check (std::strcmp (mix.parameterName, "Output Mix") == 0, "parameter name");
    check (ronin::exactlyEqual (mix.minimum, 0.0f) && ronin::exactlyEqual (mix.maximum, 1.0f), "travel stays 0 to 1");
    check (ronin::exactlyEqual (mix.fallback, 1.0f), "default mix is the patch");
    check (mix.index != 0 && mix.index != 1, "not an output preset knob");
    check (mix.knob != level.knob, "mix is not output level");
    check (std::strcmp (level.parameterId, "outputLevel") == 0, "level id stays");
    check (std::fabs (level.fallback - 0.7f) < 1.0e-6f, "level default stays 0.7");

    const int face = panelKnobIndex ("OUTPUT", "MIX");
    check (face >= 0, "output mix is on the plate");
    check (std::fabs (kPanelKnobs[face].valueDefault - 1.0f) < 1.0e-6f, "faceplate mix default is 1");

    OutputModule trimmed;
    trimmed.setLevel (1.0f);
    trimmed.setOutputLevel (0.7f);
    trimmed.setMix (0.25f);
    check (trimmed.presetKnobCount() == 2, "preset still stores mix and the schematic trim");
    check (near (trimmed.presetKnob (1), 1.0f), "mix does not replace the trim");
    check (near (trimmed.outputLevel(), 0.7f), "mix does not move output level");
    check (near (outputLevelGain (0.7f), 1.0f), "unity gain stays at 0.7");

    constexpr float kDryL = 0.50f;
    constexpr float kDryR = -0.25f;
    constexpr float kWet = 1.0f;
    BlendRack rack (kWet);
    check (rack.graph.cableCount() == 3, "blend rack keeps its three cables");

    const float knobs[] = { 0.0f, 0.25f, 0.5f, 1.0f };
    for (float knob : knobs)
    {
        rack.run (false, knob, kDryL, kDryR);
        check (near (rack.output.hostLeft(), kDryL), "effect off ignores mix on the left");
        check (near (rack.output.hostRight(), kDryR), "effect off ignores mix on the right");
    }

    for (float knob : knobs)
    {
        rack.run (true, knob, kDryL, kDryR);
        check (near (rack.output.hostLeft(), hostFromBlend (kDryL, kWet, knob, 1.0f)), "effect on blends the left");
        check (near (rack.output.hostRight(), hostFromBlend (kDryR, kWet, knob, 1.0f)), "effect on blends the right");
    }

    rack.output.setOutputLevel (1.0f);
    rack.run (true, 1.0f, kDryL, kDryR);
    check (near (rack.output.hostLeft(), hostFromBlend (kDryL, kWet, 1.0f, outputLevelGain (1.0f))),
           "level follows the blend");
    check (near (rack.output.outputLevel(), 1.0f), "full level stays on its own knob");

    rack.output.setOutputLevel (0.0f);
    rack.run (true, 1.0f, kDryL, kDryR);
    check (near (rack.output.hostLeft(), 0.0f) && near (rack.output.hostRight(), 0.0f), "level 0 mutes the blend");

    rack.output.setOutputLevel (1.0f);
    rack.output.setLevel (0.0f);
    rack.run (true, 1.0f, kDryL, kDryR);
    check (near (rack.output.hostLeft(), 0.0f), "schematic trim still mutes");
    check (rack.graph.cableCount() == 3, "the blend does not add a cable");

    const float wetOn = vcfAtMix (true, 1.0f);
    const float dryOn = vcfAtMix (true, 0.0f);
    const float wetOff = vcfAtMix (false, 1.0f);
    check (near (wetOn, dryOn), "mix does not move the vcf");
    check (near (wetOn, wetOff), "the switch does not move the vcf");

    return finish ("testOutputMixBlendsWet");
}

int testEffectOnIsWet()
{
    check (ronin::exactlyEqual (outputMixAfterSwitch (false, 0.0f), 0.0f), "off at mix 0 is dry");
    check (ronin::exactlyEqual (outputMixAfterSwitch (false, 1.0f), 0.0f), "off at mix 1 is dry");
    check (ronin::exactlyEqual (outputMixAfterSwitch (true, 0.0f), 0.0f), "on at mix 0 is dry");
    check (ronin::exactlyEqual (outputMixAfterSwitch (true, 1.0f), 1.0f), "on at mix 1 is the patch");
    check (ronin::exactlyEqual (outputMixForEffect (false), 0.0f) && ronin::exactlyEqual (outputMixForEffect (true), 1.0f),
           "default knob is dry off and wet on");

    constexpr float kDryL = 0.40f;
    constexpr float kDryR = 0.15f;
    constexpr float kWet = 3.0f;
    BlendRack rack (kWet);
    rack.output.setOutputLevel (0.7f);

    rack.run (true, 1.0f, kDryL, kDryR);
    check (near (rack.output.hostLeft(), hostFromBlend (kDryL, kWet, 1.0f, 1.0f)), "effect on is the wet patch");
    check (near (rack.output.hostRight(), hostFromBlend (kDryR, kWet, 1.0f, 1.0f)), "effect on wets the right");
    check (! near (rack.output.hostLeft(), kDryL), "effect on is not the dry cable");

    rack.run (false, 1.0f, kDryL, kDryR);
    check (near (rack.output.hostLeft(), kDryL), "effect off passes the dry left cable");
    check (near (rack.output.hostRight(), kDryR), "effect off passes the dry right cable");

    rack.run (true, 0.0f, kDryL, kDryR);
    check (near (rack.output.hostLeft(), kDryL) && near (rack.output.hostRight(), kDryR),
           "effect on at mix 0 is still dry");

    check (near (rack.output.outputLevel(), 0.7f), "the switch does not move output level");
    check (rack.graph.cableCount() == 3, "the switch does not change the cables");

    return finish ("testEffectOnIsWet");
}
