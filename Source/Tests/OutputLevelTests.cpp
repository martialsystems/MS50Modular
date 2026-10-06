// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

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
#include <cstring>

namespace {

constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

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

float knobForEgSeconds (float seconds)
{
    return static_cast<float> (std::log (static_cast<double> (seconds) / 0.001) / std::log (10000.0));
}

float quietTone (int index)
{
    return static_cast<float> (0.02 * std::sin (2.0 * kPi * 440.0 * static_cast<double> (index) / kRate));
}

struct DryRack {
    PatchGraph graph;
    ExtIn ext;
    OutputModule output;

    DryRack()
    {
        const int extIndex = graph.addModule (ext);
        const int outIndex = graph.addModule (output);
        graph.connect (extIndex, 0, outIndex, 0);
        graph.connect (extIndex, 1, outIndex, 1);
        output.setMix (outputMixForEffect (false));
        output.setLevel (1.0f);
        output.setOutputLevel (0.7f);
        graph.prepare (kRate);
    }

    void drive (float left, float right)
    {
        ext.setHostSample (left, right);
        graph.process();
    }
};

struct VoiceTail {
    float vcf = 0.0f;
    float hostPeak = 0.0f;
    double rms = 0.0;
};

VoiceTail playVoice (float level, bool held)
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
    check (graph.cableCount() == 8, "eight factory cables");

    eg.setKnob (Eg1::kKnobAttack, knobForEgSeconds (0.020f));
    eg.setKnob (Eg1::kKnobRelease, knobForEgSeconds (0.008f));
    eg.setKnob (Eg1::kKnobSustain, 1.0f);
    vcf.setKnob (Vcf::kKnobCutoff, 1.0f);
    vcf.setKnob (Vcf::kKnobAmount, 0.0f);
    vca.setKnob (Vca1::kKnobLowCut, 0.0f);
    output.setMix (outputMixForEffect (true));
    output.setLevel (1.0f);
    output.setOutputLevel (level);
    graph.prepare (kRate);
    ext.setButtonHeld (held);

    double energy = 0.0;
    int late = 0;
    VoiceTail tail;
    for (int i = 0; i < 4800; ++i)
    {
        const float sample = quietTone (i);
        ext.setHostSample (sample, sample);
        graph.process();
        if (i < 2400)
            continue;
        const float host = output.hostLeft();
        energy += static_cast<double> (host) * static_cast<double> (host);
        ++late;
        tail.vcf = vcf.portValue[Vcf::kSigOut];
        if (std::fabs (host) > tail.hostPeak)
            tail.hostPeak = std::fabs (host);
    }
    tail.rms = std::sqrt (energy / static_cast<double> (late));
    check (graph.cableCount() == 8, "output level does not add a cable");
    return tail;
}

double rmsAfterRelease (float level)
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
    check (connectFactoryCables (graph, extIndex, outIndex, vcfIndex, vcaIndex, egIndex), "release cables");
    eg.setKnob (Eg1::kKnobAttack, knobForEgSeconds (0.020f));
    eg.setKnob (Eg1::kKnobRelease, knobForEgSeconds (0.008f));
    eg.setKnob (Eg1::kKnobSustain, 1.0f);
    vcf.setKnob (Vcf::kKnobCutoff, 1.0f);
    vcf.setKnob (Vcf::kKnobAmount, 0.0f);
    output.setMix (outputMixForEffect (true));
    output.setLevel (1.0f);
    output.setOutputLevel (level);
    graph.prepare (kRate);

    ext.setButtonHeld (true);
    for (int i = 0; i < 4800; ++i)
    {
        const float sample = quietTone (i);
        ext.setHostSample (sample, sample);
        graph.process();
    }

    ext.setButtonHeld (false);
    double energy = 0.0;
    int late = 0;
    for (int i = 0; i < 9600; ++i)
    {
        const float sample = quietTone (i + 4800);
        ext.setHostSample (sample, sample);
        graph.process();
        if (i < 4800)
            continue;
        const float host = output.hostLeft();
        energy += static_cast<double> (host) * static_cast<double> (host);
        ++late;
    }
    check (graph.cableCount() == 8, "release leaves the eight cables");
    return std::sqrt (energy / static_cast<double> (late));
}

}

int testOutputLevelScalesDry()
{
    const FaceKnobBinding level = faceKnobBinding ("OUTPUT", "LEVEL");
    check (level.knob == FaceKnob::OutputLevel, "output level is a host knob");
    check (std::strcmp (level.parameterId, "outputLevel") == 0, "parameter id");
    check (std::strcmp (level.parameterName, "Output Level") == 0, "parameter name");
    check (level.minimum == 0.0f && level.maximum == 1.0f, "travel stays 0 to 1");
    check (std::fabs (level.fallback - 0.7f) < 1.0e-6f, "default is 0.7");
    check (near (outputLevelGain (level.fallback), 1.0f), "default level is unity gain");
    check (near (outputLevelGain (1.0f), 2.0f), "full level is twice as loud");
    check (near (outputLevelGain (0.0f), 0.0f), "zero level is silence");
    check (level.index != 0 && level.index != 1, "not an output preset knob");
    check (faceKnobBinding ("OUTPUT", "MIX").knob == FaceKnob::OutputMix, "output mix is a separate host knob");

    const int face = panelKnobIndex ("OUTPUT", "LEVEL");
    check (face >= 0, "output level is on the plate");
    check (std::fabs (kPanelKnobs[face].valueDefault - 0.7f) < 1.0e-6f, "faceplate default is 0.7");

    OutputModule trimmed;
    trimmed.setLevel (1.0f);
    trimmed.setOutputLevel (0.0f);
    check (trimmed.presetKnobCount() == 2, "preset still stores mix and the schematic trim");
    check (near (trimmed.presetKnob (1), 1.0f), "output level does not replace the trim");
    trimmed.setLevel (0.0f);
    trimmed.setOutputLevel (1.0f);
    check (near (trimmed.presetKnob (1), 0.0f), "the trim still mutes on its own");

    DryRack dry;
    const float left[4] = { 0.25f, -0.5f, 0.125f, -0.0625f };
    const float right[4] = { -0.125f, 0.5f, 0.0f, 0.25f };
    for (int i = 0; i < 4; ++i)
    {
        dry.drive (left[i], right[i]);
        check (near (dry.output.hostLeft(), left[i]), "default level matches the dry left");
        check (near (dry.output.hostRight(), right[i]), "default level matches the dry right");
    }

    for (int n = 0; n < 32; ++n)
    {
        const float l = static_cast<float> (0.25 * std::sin (2.0 * kPi * 220.0 * static_cast<double> (n) / kRate));
        const float r = static_cast<float> (0.25 * std::sin (2.0 * kPi * 220.0 * static_cast<double> (n) / kRate + kPi / 2.0));
        dry.drive (l, r);
        check (near (dry.output.hostLeft(), l), "default level sine left");
        check (near (dry.output.hostRight(), r), "default level sine right");
    }

    dry.output.setOutputLevel (1.0f);
    dry.drive (0.25f, -0.4f);
    check (near (dry.output.hostLeft(), 0.5f), "full level doubles the dry left");
    check (near (dry.output.hostRight(), -0.8f), "full level doubles the dry right");
    check (std::fabs (dry.output.hostLeft()) > 0.25f, "effect off, raising level makes the track louder");

    dry.output.setOutputLevel (0.0f);
    dry.drive (0.25f, -0.4f);
    check (near (dry.output.hostLeft(), 0.0f), "level 0 mutes the dry left");
    check (near (dry.output.hostRight(), 0.0f), "level 0 mutes the dry right");

    dry.output.setLevel (0.0f);
    dry.output.setOutputLevel (1.0f);
    dry.drive (0.25f, -0.4f);
    check (near (dry.output.hostLeft(), 0.0f), "schematic trim 0 stays silent at full level");
    check (near (dry.output.hostRight(), 0.0f), "schematic trim 0 stays silent on the right");

    const VoiceTail released = playVoice (1.0f, false);
    check (released.rms < 1.0e-4, "effect on, gate released, full level stays silent");
    check (rmsAfterRelease (1.0f) < 1.0e-4, "effect on, gate let go, full level returns to silence");

    const VoiceTail held = playVoice (0.7f, true);
    const VoiceTail louder = playVoice (1.0f, true);
    check (held.rms > 0.004, "effect on, gate held, the voice is audible");
    check (louder.rms > held.rms, "effect on, gate held, raising level makes the voice louder");
    check (held.hostPeak > 1.0e-4f, "held voice has a peak");
    check (std::fabs (louder.hostPeak / held.hostPeak - 2.0f) < 1.0e-3f, "full level doubles the held voice");
    check (std::fabs (louder.vcf - held.vcf) < 1.0e-4f, "the amp does not scale the vcf");

    return finish ("testOutputLevelScalesDry");
}
