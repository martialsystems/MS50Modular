// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/EffectSwitch.h"
#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
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

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

bool finiteSample (float value)
{
    return std::isfinite (value);
}

float measureRms (Vcf& vcf, float hz, int settle, int measure)
{
    double sum = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        const double phase = 2.0 * kPi * static_cast<double> (hz) * static_cast<double> (i) / kRate;
        vcf.portValue[Vcf::kSigIn] = static_cast<float> (std::sin (phase));
        vcf.portValue[Vcf::kCutoff] = 0.0f;
        vcf.processSample();
        if (i >= settle)
        {
            const float y = vcf.portValue[Vcf::kSigOut];
            sum += static_cast<double> (y) * static_cast<double> (y);
        }
    }
    return static_cast<float> (std::sqrt (sum / static_cast<double> (measure)));
}

}

int testVcfPassesDcOrLow()
{
    check (std::strcmp (Vcf::kStandIn, "STAND-IN step 8, replaced in step 20") == 0, "stand-in marker");

    Vcf dc;
    dc.prepare (kRate);
    dc.setKnob (Vcf::kKnobCutoff, 1.0f);
    dc.setKnob (Vcf::kKnobPeak, 0.0f);
    dc.setKnob (Vcf::kKnobAmount, 0.0f);
    float last = 0.0f;
    for (int i = 0; i < 2000; ++i)
    {
        dc.portValue[Vcf::kSigIn] = 1.0f;
        dc.portValue[Vcf::kCutoff] = 0.0f;
        dc.processSample();
        last = dc.portValue[Vcf::kSigOut];
        check (finiteSample (last), "dc sample finite");
    }
    check (std::fabs (last - 1.0f) < 0.02f, "high cutoff passes DC");

    Vcf low;
    low.prepare (kRate);
    low.setKnob (Vcf::kKnobCutoff, 0.0f);
    low.setKnob (Vcf::kKnobPeak, 0.0f);
    low.setKnob (Vcf::kKnobAmount, 0.0f);
    const float lowTone = measureRms (low, 40.0f, 12000, 4800);
    Vcf high;
    high.prepare (kRate);
    high.setKnob (Vcf::kKnobCutoff, 0.0f);
    high.setKnob (Vcf::kKnobPeak, 0.0f);
    high.setKnob (Vcf::kKnobAmount, 0.0f);
    const float highTone = measureRms (high, 12000.0f, 12000, 4800);
    check (lowTone > highTone * 10.0f, "low cutoff keeps a low tone and rejects a high one");
    return finish ("testVcfPassesDcOrLow");
}

int testVcfPeakIncreasesResonance()
{
    auto tailEnergy = [] (float peak) {
        Vcf vcf;
        vcf.prepare (kRate);
        vcf.setKnob (Vcf::kKnobCutoff, 0.55f);
        vcf.setKnob (Vcf::kKnobPeak, peak);
        vcf.setKnob (Vcf::kKnobAmount, 0.0f);
        double energy = 0.0;
        bool finite = true;
        const int excite = 1440;
        const int tail = 4800;
        for (int i = 0; i < excite + tail; ++i)
        {
            float input = 0.0f;
            if (i < excite)
                input = static_cast<float> (std::sin (2.0 * kPi * 840.0 * static_cast<double> (i) / kRate));
            vcf.portValue[Vcf::kSigIn] = input;
            vcf.portValue[Vcf::kCutoff] = 0.0f;
            vcf.processSample();
            const float y = vcf.portValue[Vcf::kSigOut];
            if (! finiteSample (y))
                finite = false;
            if (i >= excite)
                energy += static_cast<double> (y) * static_cast<double> (y);
        }
        check (finite, "resonance burst stays finite");
        return energy;
    };

    const double open = tailEnergy (0.0f);
    const double peaked = tailEnergy (1.0f);
    check (peaked > open * 1.5, "peak 1 rings longer than peak 0");

    Vcf driven;
    driven.prepare (kRate);
    driven.setKnob (Vcf::kKnobCutoff, 0.7f);
    driven.setKnob (Vcf::kKnobPeak, 1.0f);
    driven.setKnob (Vcf::kKnobAmount, 0.0f);
    bool finite = true;
    for (int i = 0; i < 96000; ++i)
    {
        driven.portValue[Vcf::kSigIn] = static_cast<float> (5.0 * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate));
        driven.portValue[Vcf::kCutoff] = 0.0f;
        driven.processSample();
        if (! finiteSample (driven.portValue[Vcf::kSigOut]))
            finite = false;
    }
    check (finite, "two seconds at peak 1 stays finite");
    return finish ("testVcfPeakIncreasesResonance");
}

int testVcfPositiveCvRaisesCutoff()
{
    auto tone = [] (float cv) {
        Vcf vcf;
        vcf.prepare (kRate);
        vcf.setKnob (Vcf::kKnobCutoff, 0.0f);
        vcf.setKnob (Vcf::kKnobPeak, 0.0f);
        vcf.setKnob (Vcf::kKnobAmount, 1.0f);
        double sum = 0.0;
        const int settle = 8000;
        const int measure = 4800;
        for (int i = 0; i < settle + measure; ++i)
        {
            const double phase = 2.0 * kPi * 200.0 * static_cast<double> (i) / kRate;
            vcf.portValue[Vcf::kSigIn] = static_cast<float> (std::sin (phase));
            vcf.portValue[Vcf::kCutoff] = cv;
            vcf.processSample();
            if (i >= settle)
            {
                const float y = vcf.portValue[Vcf::kSigOut];
                sum += static_cast<double> (y) * static_cast<double> (y);
            }
        }
        return std::sqrt (sum / static_cast<double> (measure));
    };

    const double closed = tone (0.0f);
    const double raised = tone (5.0f);
    check (raised > closed * 4.0, "positive cutoff CV raises the cutoff");
    return finish ("testVcfPositiveCvRaisesCutoff");
}

int testVcfWetPathQuieterAtLowCutoff()
{
    auto hostRms = [] (float cutoff) {
        PatchGraph graph;
        ExtIn ext;
        OutputModule output;
        Vcf vcf;
        const int extIndex = graph.addModule (ext);
        const int outIndex = graph.addModule (output);
        const int vcfIndex = graph.addModule (vcf);
        check (graph.connect (extIndex, 0, outIndex, 0), "dry left");
        check (graph.connect (extIndex, 1, outIndex, 1), "dry right");
        check (graph.connect (extIndex, 2, vcfIndex, Vcf::kSigIn), "mono into vcf");
        check (graph.connect (vcfIndex, Vcf::kSigOut, outIndex, 2), "vcf into wet");
        output.setMix (1.0f);
        output.setLevel (1.0f);
        vcf.setKnob (Vcf::kKnobCutoff, cutoff);
        vcf.setKnob (Vcf::kKnobPeak, cutoff > 0.5f ? 1.0f : 0.0f);
        vcf.setKnob (Vcf::kKnobAmount, 0.0f);
        graph.prepare (kRate);

        double sum = 0.0;
        bool finite = true;
        const int total = cutoff > 0.5f ? 96000 : 16000;
        const int settle = 4000;
        for (int i = 0; i < total; ++i)
        {
            const float sample = static_cast<float> (0.8 * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate));
            ext.setHostSample (sample, sample);
            graph.process();
            const float left = output.hostLeft();
            if (! finiteSample (left) || ! finiteSample (output.hostRight()))
                finite = false;
            if (i >= settle && i < settle + 4800)
                sum += static_cast<double> (left) * static_cast<double> (left);
        }
        check (finite, "wet path stays finite");
        return std::sqrt (sum / 4800.0);
    };

    const double quiet = hostRms (0.0f);
    const double open = hostRms (1.0f);
    check (quiet < open, "mix 1 is quieter at cutoff 0 than cutoff 1");

    PatchGraph dry;
    ExtIn ext;
    OutputModule output;
    const int extIndex = dry.addModule (ext);
    const int outIndex = dry.addModule (output);
    dry.connect (extIndex, 0, outIndex, 0);
    dry.connect (extIndex, 1, outIndex, 1);
    output.setMix (outputMixForEffect (false));
    output.setLevel (1.0f);
    dry.prepare (kRate);
    ext.setHostSample (0.25f, -0.5f);
    dry.process();
    check (std::fabs (output.hostLeft() - 0.25f) < 1.0e-5f, "effect off keeps dry left");
    check (std::fabs (output.hostRight() + 0.5f) < 1.0e-5f, "effect off keeps dry right");
    return finish ("testVcfWetPathQuieterAtLowCutoff");
}

int testVcfPanelJacks()
{
    const int vcfIn = panelJackIndex ("VCF", "IN");
    const int vcfOut = panelJackIndex ("VCF", "OUT");
    const int extMono = panelJackIndex ("EXT IN", "MONO");
    const int outWet = panelJackIndex ("OUTPUT", "WET");
    const int vco = panelJackIndex ("VCO", "HZ/V");
    check (vcfIn >= 0 && vcfOut >= 0 && vco >= 0, "jacks exist");
    check (kPanelJacks[vcfIn].module == 4 && kPanelJacks[vcfIn].port == 0 && kPanelJacks[vcfIn].dir == 0, "vcf in");
    check (kPanelJacks[vcfOut].module == 4 && kPanelJacks[vcfOut].dir == 1, "vcf out");
    check (kPanelJacks[vco].module == 0, "vco stays unmapped");

    PanelLink refused;
    check (orientPanelJacks (extMono, vcfIn, 0, 1, 2, refused, -1) == PanelLinkResult::Unmapped,
           "vcf jack is refused until the module index is passed");
    PanelLink into;
    check (orientPanelJacks (extMono, vcfIn, 0, 1, 2, into, 3) == PanelLinkResult::Ok, "mono feeds vcf");
    check (into.sourceModule == 0 && into.sourcePort == 2 && into.destModule == 3 && into.destPort == Vcf::kSigIn,
           "mono to sig in");
    PanelLink out;
    check (orientPanelJacks (vcfOut, outWet, 0, 1, 2, out, 3) == PanelLinkResult::Ok, "vcf feeds wet");
    check (out.sourceModule == 3 && out.sourcePort == Vcf::kSigOut && out.destModule == 1 && out.destPort == 2,
           "sig out to wet");

    const FaceKnobBinding cutoff = faceKnobBinding ("VCF", "CUTOFF");
    const FaceKnobBinding peak = faceKnobBinding ("VCF", "PEAK");
    const FaceKnobBinding amount = faceKnobBinding ("VCF", "MOD");
    check (cutoff.knob == FaceKnob::VcfCutoff && std::strcmp (cutoff.parameterName, "VCF Cutoff") == 0, "cutoff name");
    check (peak.knob == FaceKnob::VcfPeak && std::strcmp (peak.parameterName, "VCF Peak") == 0, "peak name");
    check (amount.knob == FaceKnob::VcfAmount && std::strcmp (amount.parameterName, "VCF Cutoff Amount") == 0,
           "amount name");
    check (cutoff.minimum == 0.0f && cutoff.maximum == 1.0f && cutoff.fallback == 0.50f, "cutoff range");
    check (peak.fallback == 0.30f && amount.fallback == 0.68f, "peak and amount defaults");
    check (faceKnobBinding ("OUTPUT", "MIX").knob == FaceKnob::None, "column mix is a picture");
    check (faceKnobBinding ("DIV", "RATIO SWITCH").knob == FaceKnob::None, "divider stays a picture");
    check (outputMixForEffect (false) == 0.0f && outputMixForEffect (true) == 1.0f, "effect switch is on or off");
    return finish ("testVcfPanelJacks");
}
