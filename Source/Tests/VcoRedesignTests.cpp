// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// RONIN_Redesign §3.2, §5 items 4-5. Numbers from jidai-audit/verify/verify_ronin.py
// (proposed_tri_polyblamp, vco_tri_shape, vco_saw_alias_dB_below_signal_in_0_20kHz).

#include "Tests/TestSuite.h"
#include "Modular/Jcs.h"
#include "Modular/Vco.h"
#include "Spectrum.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
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

// Runs the VCO at `hz` through the linear HZ/V jack at 8' (kC3Hz = 130.8127826502993 Hz per volt).
std::vector<double> render (Vco& vco, double hz, int samples, int port)
{
    std::vector<double> out (static_cast<size_t> (samples));
    for (int i = 0; i < samples; ++i)
    {
        vco.inputConnected[Vco::kHzPerVolt] = true;
        vco.portValue[Vco::kHzPerVolt] = static_cast<float> (hz / jcs::pitch::kC3Hz);
        vco.portValue[Vco::kOct] = 0.0f;
        vco.portValue[Vco::kFreqA] = 0.0f;
        vco.portValue[Vco::kFreqB] = 0.0f;
        vco.portValue[Vco::kPwm] = 0.0f;
        vco.processSample();
        out[static_cast<size_t> (i)] = static_cast<double> (vco.portValue[port]);
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
    const auto first = render (start, jcs::pitch::kC3Hz, 2400, Vco::kTri);
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
    // 8' at 0 V: C3 = kC3Hz. Run partway into a cycle, then move RANGE to 4'.
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
    check (std::equal_to<float>() (Vco::footageHzFor (2), static_cast<float> (jcs::pitch::kC3Hz)),
           "8' is C3 = the shared exact kC3Hz (130.8127826502993 Hz)");
    check (std::equal_to<float>() (Vco::footageHzFor (0) * 4.0f, Vco::footageHzFor (2))
               && std::equal_to<float>() (Vco::footageHzFor (1) * 2.0f, Vco::footageHzFor (2))
               && std::equal_to<float>() (Vco::footageHzFor (3), Vco::footageHzFor (2) * 2.0f),
           "32', 16', 4' are exact octaves of 8'");
    return finish ("testFootageSwitchesAtWrap");
}

// §5.13c / §6: VCO:HZ/V is role HZ/V LIN (JCS R4, R14) and its law f = footage * max(V, 0.05) is unchanged.
int testHzvJackRoleLin()
{
    Vco vco;
    const PortDesc hzv = vco.port (Vco::kHzPerVolt);
    const PortDesc oct = vco.port (Vco::kOct);
    check (portRole (hzv) == jcs::Role::HzvLin, "VCO:HZ/V port role is HZ/V LIN");
    check (std::strcmp (jcs::roleInfo (portRole (hzv)).name, "HZ/V LIN") == 0, "the role is named HZ/V LIN");
    check (jcs::roleInfo (jcs::Role::HzvLin).rgb == 0x5cd5edu, "HZ/V LIN colour #5cd5ed");
    check (portRole (oct) == jcs::Role::VOct, "VCO:V/OCT port role is V/OCT");
    check (jcs::cableBadge (jcs::Role::VOct, portRole (hzv)) == jcs::Badge::PitchLaw, "V/OCT into HZ/V LIN warns (not refused)");

    auto hzAt = [] (float volts) {
        Vco v;
        v.setKnob (Vco::kKnobFine, 0.5f);
        v.prepare (48000.0);
        v.inputConnected[Vco::kHzPerVolt] = true;
        v.portValue[Vco::kHzPerVolt] = volts;
        for (int i = 0; i < 64; ++i)
            v.processSample();
        return static_cast<double> (v.lastHz());
    };
    Vco ref;
    ref.prepare (48000.0);
    ref.processSample();
    const double footage = static_cast<double> (Vco::footageHzFor (ref.activeScaleIndex()));
    const double one = hzAt (1.0f);
    std::printf ("  footage %.4f Hz, 1 V -> %.4f Hz\n", footage, one);
    check (std::fabs (one / footage - 1.0) < 1.0e-4, "1 V plays the footage reference (f = footage * V)");
    check (std::fabs (hzAt (2.0f) / one - 2.0) < 1.0e-4, "2 V doubles it: linear, not 1 V/oct");
    check (std::fabs (hzAt (0.5f) / one - 0.5) < 1.0e-4, "0.5 V halves it");
    check (std::fabs (hzAt (0.0f) / one - 0.05) < 1.0e-4, "0 V floors at 0.05 V");
    check (std::fabs (hzAt (-3.0f) / one - 0.05) < 1.0e-4, "negative volts floor at 0.05 V");
    return finish ("testHzvJackRoleLin");
}
