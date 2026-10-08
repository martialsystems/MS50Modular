// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// RONIN_Redesign §3.1, §5 items 1-3 and 11, §6 M-R1. Numbers cross-checked against
// jidai-audit/verify/verify_ronin.py (eg_time_labels, proposed_eg_float32, eg_knob_migration) and
// verify_crossunit.py (threshold_chatter_2Hz_sine_plus_50mV_noise).

#include "Modular/EgLaw.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/Jcs.h"

#include <cmath>
#include <cstdint>
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

// Samples from the trigger edge until OutA first reaches 5 V. -1 if it never does within `cap`.
long attackSamples (Eg1& eg, long cap)
{
    for (long n = 1; n <= cap; ++n)
    {
        eg.portValue[Eg1::kTrig] = 0.0f;
        eg.processSample();
        if (eg.portValue[Eg1::kOutA] >= 5.0f)
            return n;
    }
    return -1;
}

// The verify_crossunit.py noisy ramp: 2 Hz sine * 2.5 + 0.8 V plus 50 mV Gaussian noise, 1 s at 48 kHz.
std::vector<float> noisyRamp()
{
    std::vector<float> sig (48000);
    std::uint32_t s = 0x2545F491u;
    auto uniform = [&s]()
    {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (static_cast<double> (s) + 0.5) / 4294967296.0;
    };
    const double pi = 3.14159265358979323846;
    for (int i = 0; i < 48000; ++i)
    {
        const double t = i / 48000.0;
        double v = std::sin (2.0 * pi * 2.0 * t) * 2.5 + 0.8;
        v = v < -5.0 ? -5.0 : (v > 5.0 ? 5.0 : v);
        const double g = std::sqrt (-2.0 * std::log (uniform())) * std::cos (2.0 * pi * uniform());
        sig[static_cast<size_t> (i)] = static_cast<float> (v + 0.05 * g);
    }
    return sig;
}

}

int testEgNoStallInFloat()
{
    // N1: knobs >= 0.735 stalled at 48 kHz and >= 0.66 at 96 kHz. Every knob now reaches decay and sustain.
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        for (float knob : { 0.66f, 0.735f, 0.8f, 1.0f })
        {
            Eg1 eg;
            eg.prepare (rate);
            eg.setKnob (Eg1::kKnobAttack, knob);
            eg.setKnob (Eg1::kKnobDecay, 0.0f);
            eg.setKnob (Eg1::kKnobSustain, 0.6f);
            const double T = EgLaw::secondsFor (knob);
            const long n = attackSamples (eg, static_cast<long> ((T + 1.0) * rate));
            check (n > 0, "long attack reaches 5 V (no float stall)");
            check (std::fabs (n / rate - T) <= 2.0 / rate, "attack time equals its label");
            for (int i = 0; i < static_cast<int> (0.01 * rate); ++i)
            {
                eg.portValue[Eg1::kTrig] = 0.0f;
                eg.processSample();
            }
            check (eg.stage() == Eg1::Stage::Sustain, "decay reaches the sustain stage");
            check (std::fabs (eg.portValue[Eg1::kOutA] - 3.0f) < 1.0e-5f, "sustain sits at 5 S");
        }
    }
    return finish ("testEgNoStallInFloat");
}

int testEgLabelsAreRealTime()
{
    // verify_ronin.py eg_time_labels (proposed columns): attack and release-to--40 dB equal the label at every knob.
    const double rate = 48000.0;
    for (float knob : { 0.0f, 0.25f, 0.5f, 0.75f })
    {
        const double T = EgLaw::secondsFor (knob);
        Eg1 eg;
        eg.prepare (rate);
        eg.setKnob (Eg1::kKnobAttack, knob);
        eg.setKnob (Eg1::kKnobDecay, knob);
        eg.setKnob (Eg1::kKnobRelease, knob);
        eg.setKnob (Eg1::kKnobSustain, 0.0f);
        const long a = attackSamples (eg, static_cast<long> ((T + 1.0) * rate));
        check (std::fabs (a / rate - T) <= 1.5 / rate + 1.0e-9, "attack 0 -> 5 V takes the label time");

        // Decay with sustain 0: a full 5 V swing to within 1 % (0.05 V) takes the label time.
        long d = 0;
        while (eg.portValue[Eg1::kOutA] > 0.05f && d < static_cast<long> ((T + 1.0) * rate))
        {
            eg.portValue[Eg1::kTrig] = 0.0f;
            eg.processSample();
            ++d;
        }
        check (std::fabs (d / rate - T) <= 1.5 / rate + 1.0e-9, "decay 5 V -> 1 % takes the label time");

        // Release from 5 V: retrigger to the top, then lift.
        Eg1 rel;
        rel.prepare (rate);
        rel.setKnob (Eg1::kKnobAttack, 0.0f);
        rel.setKnob (Eg1::kKnobDecay, 1.0f);
        rel.setKnob (Eg1::kKnobRelease, knob);
        attackSamples (rel, 1000);
        long r = 0;
        do
        {
            rel.portValue[Eg1::kTrig] = 5.0f;
            rel.processSample();
            ++r;
        } while (rel.portValue[Eg1::kOutA] > 0.05f && r < static_cast<long> ((T + 1.0) * rate));
        check (std::fabs (r / rate - T) <= 1.5 / rate + 1.0e-9, "release 5 V -> 1 % takes the label time");
    }

    // proposed_eg_float32: T = 1 s, sustain 0.6, decay to 1 % of 5 V above sustain = 0.8008 s.
    Eg1 eg;
    eg.prepare (rate);
    eg.setKnob (Eg1::kKnobAttack, static_cast<float> (EgLaw::knobForSeconds (1.0)));
    eg.setKnob (Eg1::kKnobDecay, static_cast<float> (EgLaw::knobForSeconds (1.0)));
    eg.setKnob (Eg1::kKnobSustain, 0.6f);
    const long a = attackSamples (eg, 2 * 48000);
    check (std::fabs (a / rate - 1.0) < 0.001, "T = 1 s attack is 1.000 s");
    long d = 0;
    while (eg.portValue[Eg1::kOutA] - 3.0f > 0.05f && d < 3 * 48000)
    {
        eg.portValue[Eg1::kTrig] = 0.0f;
        eg.processSample();
        ++d;
    }
    check (std::fabs (d / rate - 0.8008) < 0.001, "T = 1 s decay to 1 % above sustain is 0.8008 s");
    return finish ("testEgLabelsAreRealTime");
}

int testEgKnobLawAndMigration()
{
    check (std::fabs (EgLaw::secondsFor (0.0) - 0.001) < 1.0e-12, "knob 0 is 1 ms");
    check (std::fabs (EgLaw::secondsFor (1.0) - 60.0) < 1.0e-9, "knob 1 is 60 s");
    // verify_ronin.py eg_knob_migration.
    check (std::fabs (EgLaw::migrateAttackKnob (0.5) - 0.5846) < 5.0e-5, "M-R1 attack 0.5 -> 0.5846");
    check (std::fabs (EgLaw::secondsFor (EgLaw::migrateAttackKnob (0.5)) - 0.6215) < 5.0e-5, "M-R1 0.5 keeps 0.6215 s");
    check (std::fabs (EgLaw::migrateAttackKnob (0.25) - 0.3753) < 5.0e-5, "M-R1 attack 0.25 -> 0.3753");
    check (std::fabs (EgLaw::migrateAttackKnob (0.735) - 0.7814) < 5.0e-5, "M-R1 attack 0.735 -> 0.7814");
    check (EgLaw::migrateAttackKnob (1.0) == 1.0, "M-R1 attack 1.0 (62 s) clamps to 60 s");
    check (std::fabs (EgLaw::migrateDecayReleaseKnob (0.5) - 0.5574) < 5.0e-5, "M-R1 D/R 0.5 -> 0.5574");
    check (std::fabs (EgLaw::migrateDecayReleaseKnob (1.0) - 0.9760) < 5.0e-5, "M-R1 D/R 1.0 -> 0.9760");
    check (std::fabs (EgLaw::migrateDecayReleaseKnob (0.0) - 0.1388) < 5.0e-5, "M-R1 D/R 0.0 -> 0.1388");
    return finish ("testEgKnobLawAndMigration");
}

int testEgSnapAndSustainSlew()
{
    Eg1 eg;
    eg.prepare (48000.0);
    eg.setKnob (Eg1::kKnobAttack, 0.0f);
    eg.setKnob (Eg1::kKnobDecay, 0.0f);
    eg.setKnob (Eg1::kKnobRelease, 0.25f);
    eg.setKnob (Eg1::kKnobSustain, 0.6f);
    for (int i = 0; i < 4800; ++i)
    {
        eg.portValue[Eg1::kTrig] = 0.0f;
        eg.processSample();
    }
    // A moving S knob does not step: a 5 ms one-pole.
    eg.setKnob (Eg1::kKnobSustain, 0.2f);
    eg.portValue[Eg1::kTrig] = 0.0f;
    eg.processSample();
    const float afterOne = eg.portValue[Eg1::kOutA];
    check (afterOne < 3.0f && afterOne > 2.9f, "sustain knob change slews, it does not step");
    for (int i = 0; i < 4800; ++i)
        eg.processSample();
    check (std::fabs (eg.portValue[Eg1::kOutA] - 1.0f) < 1.0e-4f, "sustain follows the knob");

    // Release snaps to 0 only below 1e-6 V (-134 dB), not at 0.01 V (-54 dB).
    float lastNonZero = 1.0f;
    bool idle = false;
    for (int i = 0; i < 48000 && ! idle; ++i)
    {
        eg.portValue[Eg1::kTrig] = 5.0f;
        eg.processSample();
        const float v = eg.portValue[Eg1::kOutA];
        if (v > 0.0f)
            lastNonZero = v;
        idle = eg.stage() == Eg1::Stage::Idle;
    }
    check (idle, "release ends in idle");
    check (lastNonZero < 2.0e-6f, "the snap happens below 1e-6 V");
    return finish ("testEgSnapAndSustainSlew");
}

int testEgTrigHysteresis()
{
    // JCS R3s: held below 1.0 V, released above 1.5 V. verify_crossunit.py: 2 held edges on the noisy ramp.
    const std::vector<float> sig = noisyRamp();
    jcs::StrigDetector det;
    det.reset (sig[0] < 1.0f);
    int edges = 0;
    int legacyEdges = 0;
    bool legacyPrev = sig[0] < 1.5f;
    for (size_t i = 1; i < sig.size(); ++i)
    {
        if (det.process (sig[i]) == jcs::Edge::Rising)
            ++edges;
        const bool legacy = sig[i] < 1.5f;
        if (legacy && ! legacyPrev)
            ++legacyEdges;
        legacyPrev = legacy;
    }
    check (edges == 2, "S-trig detector counts 2 held edges on the noisy ramp");
    check (legacyEdges > 20, "the old single 1.5 V compare chatters on the same ramp");

    // The same ramp through EG 1 and EG 2: their held flag changes exactly twice to held.
    Eg1 eg1;
    Eg2 eg2;
    eg1.prepare (48000.0);
    eg2.prepare (48000.0);
    eg1.portValue[Eg1::kTrig] = sig[0];
    eg1.processSample();
    eg2.portValue[Eg2::kTrig] = sig[0];
    eg2.processSample();
    bool was1 = eg1.triggerHeld();
    bool was2 = eg2.triggerHeld();
    int egEdges1 = 0;
    int egEdges2 = 0;
    for (size_t i = 1; i < sig.size(); ++i)
    {
        eg1.portValue[Eg1::kTrig] = sig[i];
        eg1.processSample();
        eg2.portValue[Eg2::kTrig] = sig[i];
        eg2.processSample();
        if (eg1.triggerHeld() && ! was1)
            ++egEdges1;
        if (eg2.triggerHeld() && ! was2)
            ++egEdges2;
        was1 = eg1.triggerHeld();
        was2 = eg2.triggerHeld();
    }
    check (egEdges1 == 2, "EG 1 TRIG sees 2 triggers");
    check (egEdges2 == 2, "EG 2 TRIG sees 2 triggers");

    // Between the thresholds the state holds.
    Eg1 eg;
    eg.prepare (48000.0);
    eg.portValue[Eg1::kTrig] = 1.2f;
    eg.processSample();
    check (! eg.triggerHeld(), "1.2 V from rest stays released");
    eg.portValue[Eg1::kTrig] = 0.9f;
    eg.processSample();
    check (eg.triggerHeld(), "0.9 V is held");
    eg.portValue[Eg1::kTrig] = 1.4f;
    eg.processSample();
    check (eg.triggerHeld(), "1.4 V after held stays held");
    eg.portValue[Eg1::kTrig] = 1.6f;
    eg.processSample();
    check (! eg.triggerHeld(), "1.6 V releases");
    return finish ("testEgTrigHysteresis");
}

int testEg2LabelsAndDelayTrig()
{
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Eg2 eg;
        eg.prepare (rate);
        eg.setKnob (Eg2::kKnobHold, 0.25f);   // kept law: 0.001 * 10^(4k) = 10 ms
        eg.setKnob (Eg2::kKnobDelay, 0.0f);   // 1 ms
        eg.setKnob (Eg2::kKnobAttack, 0.5f);  // 0.2449 s real
        eg.setKnob (Eg2::kKnobRelease, 0.5f);
        long n = 0;
        long pulseStart = -1;
        long pulseLen = 0;
        float pulseLevel = 0.0f;
        long attackStart = -1;
        long peakAt = -1;
        while (n < static_cast<long> (rate) && peakAt < 0)
        {
            eg.portValue[Eg2::kTrig] = 0.0f;
            eg.processSample();
            const float dt = eg.portValue[Eg2::kDelayTrig];
            if (dt > 0.0f)
            {
                if (pulseStart < 0)
                    pulseStart = n;
                ++pulseLen;
                pulseLevel = dt;
            }
            if (attackStart < 0 && eg.portValue[Eg2::kOutPos] > 0.0f)
                attackStart = n;
            if (eg.portValue[Eg2::kOutPos] >= 5.0f)
                peakAt = n;
            ++n;
        }
        const long width = std::lround (0.001 * rate) > 1 ? std::lround (0.001 * rate) : 1;
        check (pulseLen == width, "DelayTrig lasts max(1, round(0.001 sr)) samples");
        check (std::fabs (pulseStart / rate - 0.010) <= 1.5 / rate, "DelayTrig fires at the end of hold");
        check (pulseLevel == 1.0f || pulseLevel == 5.0f, "DelayTrig level");
        const double T = EgLaw::secondsFor (0.5);
        check (attackStart > 0 && std::fabs ((peakAt - attackStart + 1) / rate - T) <= 2.0 / rate,
               "EG 2 attack takes its label time");
    }
    return finish ("testEg2LabelsAndDelayTrig");
}
