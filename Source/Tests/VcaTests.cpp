// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/DefaultPatch.h"
#include "Modular/EffectSwitch.h"
#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
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

float sineRms (Vca1& vca, float lowCut01, float intensity, float envVolts, float hz, int settle, int measure)
{
    vca.prepare (kRate);
    vca.setKnob (Vca1::kKnobLowCut, lowCut01);
    vca.setKnob (Vca1::kKnobIntensity, intensity);
    double sum = 0.0;
    for (int i = 0; i < settle + measure; ++i)
    {
        const double phase = 2.0 * kPi * static_cast<double> (hz) * static_cast<double> (i) / kRate;
        vca.portValue[Vca1::kSigIn] = static_cast<float> (std::sin (phase));
        vca.portValue[Vca1::kEnv] = envVolts;
        vca.processSample();
        if (i >= settle)
        {
            const float y = vca.portValue[Vca1::kOut];
            sum += static_cast<double> (y) * static_cast<double> (y);
        }
    }
    return static_cast<float> (std::sqrt (sum / static_cast<double> (measure)));
}

}

int testVca1SilentWithoutEnv()
{
    Vca1 vca;
    bool silent = true;
    bool finite = true;
    vca.prepare (kRate);
    vca.setKnob (Vca1::kKnobLowCut, 0.0f);
    vca.setKnob (Vca1::kKnobIntensity, 1.0f);
    for (int i = 0; i < 4000; ++i)
    {
        const double phase = 2.0 * kPi * 1000.0 * static_cast<double> (i) / kRate;
        vca.portValue[Vca1::kSigIn] = static_cast<float> (std::sin (phase));
        vca.portValue[Vca1::kEnv] = 0.0f;
        vca.processSample();
        const float y = vca.portValue[Vca1::kOut];
        if (! std::isfinite (y))
            finite = false;
        if (std::fabs (y) > 1.0e-6f)
            silent = false;
    }
    check (finite, "silent path stays finite");
    check (silent, "unpatched env is silence");
    return finish ("testVca1SilentWithoutEnv");
}

int testVca1IntensityScalesOutput()
{
    Vca1 full;
    Vca1 half;
    const float fullRms = sineRms (full, 0.0f, 1.0f, 5.0f, 1000.0f, 2000, 4800);
    const float halfRms = sineRms (half, 0.0f, 0.5f, 5.0f, 1000.0f, 2000, 4800);
    check (std::isfinite (fullRms) && std::isfinite (halfRms), "intensity tones stay finite");
    check (fullRms > 0.2f, "full intensity passes a tone");
    const float ratio = halfRms / fullRms;
    check (ratio > 0.45f && ratio < 0.55f, "half intensity is half the tone");
    return finish ("testVca1IntensityScalesOutput");
}

int testVca1LowCutDarkens()
{
    Vca1 openLow;
    Vca1 darkLow;
    Vca1 darkHigh;
    const float lowOpen = sineRms (openLow, 0.0f, 1.0f, 5.0f, 40.0f, 24000, 4800);
    const float lowDark = sineRms (darkLow, 1.0f, 1.0f, 5.0f, 40.0f, 24000, 4800);
    const float highDark = sineRms (darkHigh, 1.0f, 1.0f, 5.0f, 2000.0f, 8000, 4800);
    check (std::isfinite (lowOpen) && std::isfinite (lowDark) && std::isfinite (highDark), "low cut stays finite");
    check (lowDark < lowOpen * 0.25f, "high low-cut reduces a 40 Hz sine");
    check (highDark > lowDark * 8.0f, "high low-cut leaves a 2 kHz sine");
    return finish ("testVca1LowCutDarkens");
}

int testVca1NegativeEnvIsClosed()
{
    Vca1 vca;
    bool closed = true;
    vca.prepare (kRate);
    vca.setKnob (Vca1::kKnobIntensity, 1.0f);
    for (int i = 0; i < 2000; ++i)
    {
        vca.portValue[Vca1::kSigIn] = 1.0f;
        vca.portValue[Vca1::kEnv] = -5.0f;
        vca.processSample();
        if (std::fabs (vca.portValue[Vca1::kOut]) > 1.0e-6f)
            closed = false;
    }
    check (closed, "negative env closes the vca");
    return finish ("testVca1NegativeEnvIsClosed");
}

int testVca2PassesDc()
{
    Vca2 vca;
    vca.prepare (kRate);
    vca.portValue[Vca2::kIn] = 3.0f;
    vca.portValue[Vca2::kControl] = 5.0f;
    float last = 0.0f;
    bool finite = true;
    const int samples = static_cast<int> (0.5 * kRate);
    for (int i = 0; i < samples; ++i)
    {
        vca.processSample();
        last = vca.portValue[Vca2::kOut];
        if (! std::isfinite (last))
            finite = false;
    }
    check (finite, "dc path stays finite");
    check (std::fabs (last - 3.0f) < 0.02f, "control +5 V settles a +3 V input near +3 V");
    return finish ("testVca2PassesDc");
}

int testVca2ControlDoesNotClick()
{
    Vca2 vca;
    vca.prepare (kRate);
    vca.portValue[Vca2::kIn] = 3.0f;
    vca.portValue[Vca2::kControl] = 5.0f;
    vca.processSample();
    const float first = vca.portValue[Vca2::kOut];
    for (int i = 0; i < 480; ++i)
        vca.processSample();
    const float early = vca.portValue[Vca2::kOut];
    for (int i = 0; i < 20000; ++i)
        vca.processSample();
    const float later = vca.portValue[Vca2::kOut];
    check (std::fabs (first) < 0.05f, "the first sample is not the settled gain");
    check (early > first + 0.2f && later > early + 0.5f, "control steps through a ramp");
    return finish ("testVca2ControlDoesNotClick");
}

int testVca2NoKnobs()
{
    // Name kept for the runner. VCA 2 now has Initial and Mod. At their defaults the law is the old CV-only one.
    Vca2 vca;
    check (vca.numKnobs() == 2, "vca 2 has Initial and Mod");
    check (vca.numPorts() == 3, "vca 2 has three ports");
    check (vca.port (Vca2::kIn).type == PortType::CV && vca.port (Vca2::kIn).dir == PortDir::In, "in is cv");
    check (vca.port (Vca2::kControl).type == PortType::CV, "control is cv");
    check (vca.port (Vca2::kOut).dir == PortDir::Out, "out is an output");
    vca.prepare (kRate);
    vca.portValue[Vca2::kIn] = 3.0f;
    vca.portValue[Vca2::kControl] = 0.0f;
    for (int i = 0; i < 20000; ++i)
        vca.processSample();
    check (std::fabs (vca.portValue[Vca2::kOut]) < 1.0e-6f, "Initial 0 with no CV is closed");

    vca.setKnob (Vca2::kKnobInitial, 1.0f);
    for (int i = 0; i < 20000; ++i)
        vca.processSample();
    check (std::fabs (vca.portValue[Vca2::kOut] - 3.0f) < 0.02f, "Initial 1 passes audio with no CV");

    vca.setKnob (Vca2::kKnobMod, 0.0f);
    vca.portValue[Vca2::kControl] = 5.0f;
    for (int i = 0; i < 20000; ++i)
        vca.processSample();
    check (std::fabs (vca.portValue[Vca2::kOut]) < 1.0e-3f, "Mod 0 closes VCA 2");
    return finish ("testVca2NoKnobs");
}

int testVcaPanelJacks()
{
    const int vcaIn = panelJackIndex ("VCA 1", "IN");
    const int vcaEnv = panelJackIndex ("VCA 1", "ENV");
    const int vcaOut = panelJackIndex ("VCA 1", "OUT");
    const int modIn = panelJackIndex ("VCA 2", "IN");
    const int modCv = panelJackIndex ("VCA 2", "CV");
    const int modOut = panelJackIndex ("VCA 2", "OUT");
    check (vcaIn >= 0 && vcaEnv >= 0 && vcaOut >= 0 && modIn >= 0 && modOut >= 0, "jacks exist");
    check (kPanelJacks[vcaIn].module == 5 && kPanelJacks[vcaIn].port == 0 && kPanelJacks[vcaIn].dir == 0, "vca 1 in");
    check (kPanelJacks[vcaEnv].module == 5 && kPanelJacks[vcaEnv].port == 1, "vca 1 env");
    check (kPanelJacks[vcaOut].module == 5 && kPanelJacks[vcaOut].port == 2 && kPanelJacks[vcaOut].dir == 1, "vca 1 out");
    check (kPanelJacks[modIn].module == 6 && kPanelJacks[modCv].port == 1 && kPanelJacks[modOut].dir == 1, "vca 2 jacks");

    PanelLink refused;
    check (orientPanelJacks (vcaIn, vcaOut, 0, 1, 2, refused, 3, -1, -1) == PanelLinkResult::Unmapped,
           "vca 1 is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (vcaIn, vcaEnv, 0, 1, 2, linked, 3, 4, 5) == PanelLinkResult::BadType,
           "two vca 1 inputs do not patch");
    const int vcfOut = panelJackIndex ("VCF", "OUT");
    const int wet = panelJackIndex ("OUTPUT", "WET");
    PanelLink into;
    check (orientPanelJacks (vcfOut, vcaIn, 0, 1, 2, into, 3, 4, 5) == PanelLinkResult::Ok, "vcf feeds vca 1");
    check (into.sourceModule == 3 && into.sourcePort == Vcf::kSigOut && into.destModule == 4
               && into.destPort == Vca1::kSigIn,
           "sig out to vca in");
    PanelLink wetLink;
    check (orientPanelJacks (vcaOut, wet, 0, 1, 2, wetLink, 3, 4, 5) == PanelLinkResult::Ok, "vca 1 feeds wet");
    check (wetLink.sourceModule == 4 && wetLink.sourcePort == Vca1::kOut && wetLink.destModule == 1 && wetLink.destPort == 2,
           "vca out to wet");

    const FaceKnobBinding lowCut = faceKnobBinding ("VCA 1", "LOW CUT");
    check (lowCut.knob == FaceKnob::Vca1LowCut && std::strcmp (lowCut.parameterName, "VCA 1 Low Cut") == 0, "low cut name");
    check (lowCut.minimum == 0.0f && lowCut.maximum == 1.0f && lowCut.fallback == 0.68f, "low cut range");
    const FaceKnobBinding initial = faceKnobBinding ("VCA 1", "INITIAL");
    const FaceKnobBinding mod = faceKnobBinding ("VCA 1", "MOD");
    const FaceKnobBinding initial2 = faceKnobBinding ("VCA 2", "INITIAL");
    const FaceKnobBinding mod2 = faceKnobBinding ("VCA 2", "MOD");
    check (initial.knob == FaceKnob::Vca1Initial && initial.index == Vca1::kKnobInitial && initial.fallback == 0.0f
               && std::strcmp (initial.parameterId, "vca1Initial") == 0,
           "VCA 1 Initial is a host knob on the module Initial, default 0");
    check (mod.knob == FaceKnob::Vca1Mod && mod.index == Vca1::kKnobIntensity && mod.fallback == 0.85f,
           "VCA 1 Mod is the module Intensity, default 0.85");
    check (initial2.knob == FaceKnob::Vca2Initial && initial2.index == Vca2::kKnobInitial && initial2.fallback == 0.0f,
           "VCA 2 Initial is a host knob, default 0");
    check (mod2.knob == FaceKnob::Vca2Mod && mod2.index == Vca2::kKnobMod && mod2.fallback == 1.0f,
           "VCA 2 Mod is a host knob, default 1");

    // Initial 0 leaves Env in charge. Initial above 0 passes audio with no gate.
    Vca1 vca;
    vca.prepare (kRate);
    vca.setKnob (Vca1::kKnobIntensity, mod.fallback);
    vca.setKnob (Vca1::kKnobInitial, 0.0f);
    vca.portValue[Vca1::kSigIn] = 3.0f;
    vca.portValue[Vca1::kEnv] = 0.0f;
    vca.processSample();
    check (std::fabs (vca.portValue[Vca1::kOut]) < 1.0e-6f, "Initial 0 with no Env is silent");
    double sum = 0.0;
    vca.setKnob (Vca1::kKnobInitial, 0.7f);
    for (int i = 0; i < 4800; ++i)
    {
        vca.portValue[Vca1::kSigIn] = (i / 24) % 2 == 0 ? 3.0f : -3.0f;
        vca.processSample();
        sum += std::fabs (vca.portValue[Vca1::kOut]);
    }
    check (sum / 4800.0 > 0.5, "Initial 0.7 passes audio with no gate");
    return finish ("testVcaPanelJacks");
}

int testVcaFactoryWetIsSilent()
{
    PatchGraph graph;
    ExtIn ext;
    OutputModule output;
    Vcf vcf;
    Vca1 vca1;
    Vca2 vca2;
    const int extIndex = graph.addModule (ext);
    const int outIndex = graph.addModule (output);
    const int vcfIndex = graph.addModule (vcf);
    const int vcaIndex = graph.addModule (vca1);
    graph.addModule (vca2);
    check (connectFactoryCables (graph, extIndex, outIndex, vcfIndex, vcaIndex), "factory cables connect");

    Cable cables[8];
    const int count = graph.copyPublishedCables (cables, 8);
    check (count == 5, "step 9 patch has five cables");
    check (cables[0].sourcePort == 2 && cables[0].destPort == Vcf::kSigIn, "mono into the filter");
    check (cables[1].sourcePort == Vcf::kSigOut && cables[1].destPort == Vca1::kSigIn, "filter into vca 1");
    check (cables[2].sourcePort == Vca1::kOut && cables[2].destPort == 2, "vca 1 into wet");
    check (cables[3].sourcePort == 0 && cables[3].destPort == 0, "dry left");
    check (cables[4].sourcePort == 1 && cables[4].destPort == 1, "dry right");

    output.setLevel (1.0f);
    output.setMix (outputMixForEffect (true));
    graph.prepare (kRate);
    double sum = 0.0;
    bool finite = true;
    for (int i = 0; i < 4800; ++i)
    {
        const float sample = static_cast<float> (0.8 * std::sin (2.0 * kPi * 440.0 * static_cast<double> (i) / kRate));
        ext.setHostSample (sample, sample);
        graph.process();
        const float left = output.hostLeft();
        if (! std::isfinite (left) || ! std::isfinite (output.hostRight()))
            finite = false;
        sum += static_cast<double> (left) * static_cast<double> (left);
    }
    check (finite, "unpatched env stays finite");
    check (std::sqrt (sum / 4800.0) < 1.0e-4, "mix 1 is silent while env is unpatched");

    output.setMix (outputMixForEffect (false));
    ext.setHostSample (0.25f, -0.5f);
    graph.process();
    check (std::fabs (output.hostLeft() - 0.25f) < 1.0e-5f, "effect off keeps dry left");
    check (std::fabs (output.hostRight() + 0.5f) < 1.0e-5f, "effect off keeps dry right");
    return finish ("testVcaFactoryWetIsSilent");
}
