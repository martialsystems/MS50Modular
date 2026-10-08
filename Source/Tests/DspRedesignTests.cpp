// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// RONIN_Redesign §5 items 6-10, 13-14: HQ decimator, MG polyBLEP, smoothing, integrator, Schmitt inputs,
// VCF caching and drive pull, EXT IN gate hysteresis.

#include "Modular/Halfband.h"

#include <cmath>
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

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

double halfbandMagnitude (double f)   // f in cycles per 2fs sample
{
    double re = 0.0;
    double im = 0.0;
    for (int k = 0; k < Halfband::kTaps; ++k)
    {
        const double h = Halfband::Decimator2x::tap (k);
        re += h * std::cos (2.0 * 3.14159265358979323846 * f * k);
        im -= h * std::sin (2.0 * 3.14159265358979323846 * f * k);
    }
    return std::sqrt (re * re + im * im);
}

}

int testHalfbandSpec()
{
    // SHOGUN v2.2 §3.4 stage 1: pass <= 0.2125, stop >= 0.2875 of 2fs, >= 111.5 dB, ripple <= 1e-4 dB.
    double stopMax = 0.0;
    for (int i = 0; i <= 4000; ++i)
    {
        const double f = 0.2875 + (0.5 - 0.2875) * i / 4000.0;
        const double m = halfbandMagnitude (f);
        stopMax = m > stopMax ? m : stopMax;
    }
    double passMin = 10.0;
    double passMax = 0.0;
    for (int i = 0; i <= 4000; ++i)
    {
        const double m = halfbandMagnitude (0.2125 * i / 4000.0);
        passMin = m < passMin ? m : passMin;
        passMax = m > passMax ? m : passMax;
    }
    check (20.0 * std::log10 (stopMax) <= -111.5, "stopband >= 111.5 dB");
    check (20.0 * std::log10 (passMax / passMin) <= 1.0e-4, "passband ripple <= 1e-4 dB");
    // Exact half-band: every even tap except the centre is zero; symmetric (linear phase).
    bool exact = true;
    for (int k = 0; k < Halfband::kTaps; ++k)
    {
        if (k != Halfband::kCentre && (k % 2) == 0 && Halfband::Decimator2x::tap (k) != 0.0)
            exact = false;
        if (Halfband::Decimator2x::tap (k) != Halfband::Decimator2x::tap (Halfband::kTaps - 1 - k))
            exact = false;
    }
    check (exact, "exact half-band, linear phase");

    // Latency: a sample generated in the 2fs domain comes out 23 base samples later.
    Halfband::Decimator2x dec;
    dec.reset();
    int peak = -1;
    float best = 0.0f;
    for (int n = 0; n < 64; ++n)
    {
        const float y = dec.process (0.0f, n == 0 ? 1.0f : 0.0f);
        if (std::fabs (y) > best)
        {
            best = std::fabs (y);
            peak = n;
        }
    }
    check (peak == Halfband::kLatencyBaseSamples && Halfband::kLatencyBaseSamples == 23, "latency is 23 base samples");
    // DC passes at unity.
    Halfband::Decimator2x dc;
    dc.reset();
    float y = 0.0f;
    for (int n = 0; n < 100; ++n)
        y = dc.process (1.0f, 1.0f);
    check (std::fabs (y - 1.0f) < 1.0e-5f, "DC gain 1");
    return finish ("testHalfbandSpec");
}

#include "Modular/Mg.h"
#include "Spectrum.h"

#include <vector>

int testMgPolyBlep()
{
    // verify_ronin.py mg_pulse_alias_dB_200Hz: naive -25.8 dB, polyBLEP -45.9 dB at 199.7 Hz, 48 kHz.
    MgModule mg;
    mg.prepare (48000.0);
    const float knob = static_cast<float> (std::log (199.7 / 0.01) / std::log (20000.0));
    mg.setKnob (MgModule::kKnobFrequency, knob);
    mg.setKnob (MgModule::kKnobPw, 0.5f);
    std::vector<double> pulse (65536);
    std::vector<double> saw (65536);
    float lo = 10.0f;
    float hi = -10.0f;
    for (size_t i = 0; i < pulse.size(); ++i)
    {
        mg.portValue[MgModule::kFreqMod] = 0.0f;
        mg.portValue[MgModule::kPwm] = 0.0f;
        mg.processSample();
        pulse[i] = mg.portValue[MgModule::kPulse] - 2.5;
        saw[i] = mg.portValue[MgModule::kSawUp];
        lo = std::fmin (lo, mg.portValue[MgModule::kPulse]);
        hi = std::fmax (hi, mg.portValue[MgModule::kPulse]);
    }
    const double alias = spectrum::aliasDb (pulse, 199.7, 48000.0);
    std::printf ("  MG pulse alias %.2f dB, saw %.2f dB\n", alias, spectrum::aliasDb (saw, 199.7, 48000.0));
    check (std::fabs (alias + 45.9) < 1.0, "MG pulse alias at 200 Hz is about -45.9 dB");
    check (spectrum::aliasDb (saw, 199.7, 48000.0) < -40.0, "MG saw wrap is band-limited");
    check (lo >= -0.01f && hi <= 5.01f, "pulse stays 0/5 V");

    // At LFO rates the shapes are unchanged: exact 0/5 V pulse and ±2.5 V saw.
    MgModule slow;
    slow.prepare (48000.0);
    slow.setKnob (MgModule::kKnobFrequency, 0.3f);
    bool exact = true;
    for (int i = 0; i < 48000; ++i)
    {
        slow.processSample();
        const float p = slow.portValue[MgModule::kPulse];
        if (! (std::fabs (p) < 1.0e-3f || std::fabs (p - 5.0f) < 1.0e-3f))
            exact = false;
    }
    check (exact, "LFO-rate pulse is 0/5 V");
    return finish ("testMgPolyBlep");
}

// ---- §5.8 smoothing (N8) ----------------------------------------------------------------------

#include "Modular/Mixer.h"
#include "Modular/OutputModule.h"
#include "Modular/Vca1.h"
#include "Modular/Vcf.h"

int testKnobSmoothing()
{
    constexpr double rate = 48000.0;
    const int tau = static_cast<int> (0.010 * rate);   // 480 samples

    // VCA 1 intensity 0 -> 1 with a steady +1 V input and the gate fully open: a 10 ms one-pole, no step.
    // The 10 Hz low cut also moves, so compare against a twin that sat at intensity 1 all along.
    Vca1 vca;
    Vca1 twin;
    for (Vca1* v : { &vca, &twin })
    {
        v->setKnob (Vca1::kKnobLowCut, 0.0f);
        v->setKnob (Vca1::kKnobIntensity, v == &twin ? 1.0f : 0.0f);
        v->setKnob (Vca1::kKnobInitial, 1.0f);
        v->prepare (rate);
        v->portValue[Vca1::kSigIn] = 0.0f;
        for (int i = 0; i < 4800; ++i)
            v->processSample();
        v->portValue[Vca1::kSigIn] = 1.0f;
    }
    check (std::fabs (vca.portValue[Vca1::kOut]) < 1.0e-6f, "intensity 0 is silent");
    vca.setKnob (Vca1::kKnobIntensity, 1.0f);
    vca.processSample();
    twin.processSample();
    const float first = vca.portValue[Vca1::kOut] / twin.portValue[Vca1::kOut];
    for (int i = 1; i < tau; ++i)
    {
        vca.processSample();
        twin.processSample();
    }
    const float atTau = vca.portValue[Vca1::kOut] / twin.portValue[Vca1::kOut];
    check (first > 0.0f && first < 0.01f, "VCA 1 intensity does not step");
    check (std::fabs (atTau - (1.0f - std::exp (-1.0f))) < 0.003f, "VCA 1 intensity is a 10 ms one-pole");

    // Mixer level: 0.8 -> 0 reaches 1/e of the way after 10 ms and is exactly 0 after 200 ms.
    Mixer mixer;
    mixer.prepare (rate);
    mixer.portValue[Mixer::kIn1] = 1.0f;
    mixer.portValue[Mixer::kIn2] = 0.0f;
    mixer.portValue[Mixer::kIn3] = 0.0f;
    mixer.processSample();
    check (std::fabs (mixer.portValue[Mixer::kOut] + 0.8f) < 1.0e-6f, "mixer starts on its knob, no ramp from 0");
    mixer.setKnob (Mixer::kKnobLevel1, 0.0f);
    for (int i = 0; i < tau; ++i)
        mixer.processSample();
    const float mixAtTau = -mixer.portValue[Mixer::kOut];
    check (std::fabs (mixAtTau - 0.8f * std::exp (-1.0f)) < 0.002f, "mixer level is a 10 ms one-pole");
    for (int i = 0; i < 9600; ++i)
        mixer.processSample();
    check (mixer.portValue[Mixer::kOut] == 0.0f, "mixer level lands exactly on 0");
    check (mixer.presetKnob (Mixer::kKnobLevel1) == 0.0f, "the stored knob is the target");

    // Output level: a level jump does not step the host output.
    OutputModule out;
    out.setMix (0.0f);
    out.setLevel (1.0f);
    out.setOutputLevel (0.7f);
    out.prepare (rate);
    out.portValue[0] = 1.0f;
    out.portValue[1] = 1.0f;
    out.processSample();
    const float before = out.hostLeft();
    out.setOutputLevel (1.0f);
    out.processSample();
    const float after = out.hostLeft();
    check (std::fabs (before - 0.2f) < 1.0e-6f, "output starts on its level");
    check (after - before < 0.01f && after > before, "output level ramps, no step");

    // VCF cutoff knob 0 -> 1 ramps in the log domain over 5 ms: after one tau the knob is at 1 - 1/e.
    Vcf vcf;
    vcf.setKnob (Vcf::kKnobCutoff, 0.0f);
    vcf.setKnob (Vcf::kKnobAmount, 0.0f);
    vcf.prepare (rate);
    vcf.portValue[Vcf::kSigIn] = 0.0f;
    vcf.processSample();
    check (std::fabs (vcf.effectiveHz() - 20.0f) < 0.5f, "cutoff starts on the knob (20 Hz)");
    vcf.setKnob (Vcf::kKnobCutoff, 1.0f);
    for (int i = 0; i < static_cast<int> (0.005 * rate); ++i)
        vcf.processSample();
    const double expected = 20.0 * std::pow (900.0, 1.0 - std::exp (-1.0));
    std::printf ("  vcf at tau %.1f Hz (log-domain expects %.1f)\n", vcf.effectiveHz(), expected);
    check (std::fabs (std::log2 (vcf.effectiveHz() / expected)) < 0.02, "cutoff ramp is log-domain, 5 ms");
    for (int i = 0; i < 9600; ++i)
        vcf.processSample();
    check (std::fabs (vcf.effectiveHz() - 18000.0f) < 1.0f, "cutoff lands on 18 kHz");
    return finish ("testKnobSmoothing");
}
