// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// RONIN_Redesign §3.2, §5 items 4-5. Numbers from jidai-audit/verify/verify_ronin.py
// (proposed_tri_polyblamp, vco_tri_shape, vco_saw_alias_dB_below_signal_in_0_20kHz).

#include "Modular/Vco.h"
#include "Spectrum.h"

#include <cmath>
#include <cstdio>
#include <vector>

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

// Runs the VCO at `hz` through the linear HZ/V jack at 8' (130.813 Hz per volt).
std::vector<double> render (Vco& vco, double hz, int samples, int port)
{
    std::vector<double> out (static_cast<size_t> (samples));
    for (int i = 0; i < samples; ++i)
    {
        vco.inputConnected[Vco::kHzPerVolt] = true;
        vco.portValue[Vco::kHzPerVolt] = static_cast<float> (hz / 130.813);
        vco.portValue[Vco::kOct] = 0.0f;
        vco.portValue[Vco::kFreqA] = 0.0f;
        vco.portValue[Vco::kFreqB] = 0.0f;
        vco.portValue[Vco::kPwm] = 0.0f;
        vco.processSample();
        out[static_cast<size_t> (i)] = vco.portValue[port];
    }
    return out;
}

}

int testTriDefaultIsTriangle()
{
    Vco fresh;
    check (fresh.triShape() == Vco::TriShape::Triangle, "a fresh VCO is on TRIANGLE");
    Vco init;
    init.setTriShape (Vco::TriShape::Parabola);
    init.applyFactoryPreset (0);
    check (init.triShape() == Vco::TriShape::Triangle, "INIT sets TRIANGLE");

    Vco vco;
    vco.prepare (48000.0);
    const auto all = render (vco, 400.0, 48000, Vco::kTri);
    const std::vector<double> x (all.begin() + 24000, all.end());
    // verify_ronin.py proposed_tri_polyblamp: H3 -19.1, H5 -28.0, H7 -33.9 dB; even harmonics < -185 dB (double).
    check (std::fabs (spectrum::harmonicDb (x, 400.0, 3, 48000.0) + 19.1) < 0.2, "H3 -19.1 dB");
    check (std::fabs (spectrum::harmonicDb (x, 400.0, 5, 48000.0) + 28.0) < 0.2, "H5 -28.0 dB");
    check (std::fabs (spectrum::harmonicDb (x, 400.0, 7, 48000.0) + 33.9) < 0.2, "H7 -33.9 dB");
    for (int h : { 2, 4, 6, 8 })
        check (spectrum::harmonicDb (x, 400.0, h, 48000.0) < -90.0, "even harmonics are absent (float32 floor)");
    double lo = 0.0;
    double hi = 0.0;
    for (double v : x)
    {
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    check (std::fabs (lo + 4.94) < 0.01 && std::fabs (hi - 4.94) < 0.01, "peaks are ±4.94 V");

    // No DC servo and no start transient: the first 50 ms already average 0 V (the parabola sat at -5.9 V).
    Vco start;
    start.prepare (48000.0);
    const auto first = render (start, 130.813, 2400, Vco::kTri);
    double mean = 0.0;
    for (double v : first)
        mean += v;
    mean /= static_cast<double> (first.size());
    check (std::fabs (mean) < 0.05, "no start transient");

    // Alias at A7 (3520 Hz, 48 kHz): -52.8 dB with PolyBLAMP, -34.9 dB naive.
    Vco high;
    high.prepare (48000.0);
    const auto a7 = render (high, 3520.0, 32768, Vco::kTri);
    const double alias = spectrum::aliasDb (a7, 3520.0, 48000.0);
    check (std::fabs (alias + 52.8) < 0.5, "triangle alias at A7 is -52.8 dB");
    return finish ("testTriDefaultIsTriangle");
}

int testParabolaSelectableModule()
{
    // PARABOLA (legacy) is the old integrated saw, unchanged: every harmonic at 1/n^2 (H2 about -12 dB) and
    // the asymmetric peaks the audit measured (about -3.9 / +6.5 V).
    Vco vco;
    vco.setTriShape (Vco::TriShape::Parabola);
    vco.prepare (48000.0);
    check (vco.triShape() == Vco::TriShape::Parabola, "PARABOLA is selectable");
    const auto all = render (vco, 400.0, 48000, Vco::kTri);
    const std::vector<double> x (all.begin() + 24000, all.end());
    const double h2 = spectrum::harmonicDb (x, 400.0, 2, 48000.0);
    check (h2 > -13.0 && h2 < -11.0, "parabola keeps its H2 near -12 dB");
    double lo = 0.0;
    double hi = 0.0;
    for (double v : x)
    {
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    check (std::fabs (lo + 3.9) < 0.3 && std::fabs (hi - 6.5) < 0.3, "parabola keeps its asymmetric peaks");
    return finish ("testParabolaSelectableModule");
}

int testSawPulsePolyBlepKept()
{
    // §5 item 5 KEPT: 2-sample polyBLEP saw. verify_ronin.py: -36 dB at C6, -29 dB at A7.
    Vco c6;
    c6.prepare (48000.0);
    const double a = spectrum::aliasDb (render (c6, 1046.5, 32768, Vco::kSaw), 1046.5, 48000.0);
    Vco a7;
    a7.prepare (48000.0);
    const double b = spectrum::aliasDb (render (a7, 3520.0, 32768, Vco::kSaw), 3520.0, 48000.0);
    check (std::fabs (a + 36.0) < 1.0, "saw alias at C6 about -36 dB");
    check (std::fabs (b + 29.0) < 1.0, "saw alias at A7 about -29 dB");
    return finish ("testSawPulsePolyBlepKept");
}

int testFootageSwitchesAtWrap()
{
    Vco vco;
    vco.prepare (48000.0);
    // 8' at 0 V: 130.813 Hz. Run partway into a cycle, then move RANGE to 4'.
    for (int i = 0; i < 100; ++i)
    {
        vco.portValue[Vco::kOct] = 0.0f;
        vco.processSample();
    }
    check (vco.activeScaleIndex() == 2, "8' sounding");
    vco.setKnob (Vco::kKnobScale, 1.0f);
    vco.processSample();
    check (vco.activeScaleIndex() == 2, "the footage does not jump mid-cycle");
    float prevSaw = vco.portValue[Vco::kSaw];
    bool switched = false;
    for (int i = 0; i < 48000 && ! switched; ++i)
    {
        vco.processSample();
        const float saw = vco.portValue[Vco::kSaw];
        if (vco.activeScaleIndex() == 3)
        {
            switched = true;
            check (saw < prevSaw, "the switch lands on the saw wrap");
        }
        prevSaw = saw;
    }
    check (switched, "the new footage takes over at the wrap");
    check (std::fabs (Vco::footageHzFor (2) - 130.813f) < 1.0e-4f, "8' is C3 = 130.813 Hz (footage table KEPT)");
    return finish ("testFootageSwitchesAtWrap");
}
