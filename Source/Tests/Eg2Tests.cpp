// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/Eg2.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

float knobForSeconds (float seconds)
{
    return static_cast<float> (std::log (static_cast<double> (seconds) / 0.001) / std::log (10000.0));
}

void step (Eg2& eg, float trig)
{
    eg.portValue[Eg2::kTrig] = trig;
    eg.processSample();
}

}

int testEg2HasNoSustainKnob()
{
    Eg2 eg;
    check (eg.numKnobs() == 4, "four knobs");
    check (faceKnobBinding ("EG 2", "SUSTAIN").knob == FaceKnob::None, "no sustain parameter");
    check (faceKnobBinding ("EG 2", "DECAY").knob == FaceKnob::None, "no decay parameter");
    const FaceKnobBinding hold = faceKnobBinding ("EG 2", "HOLD");
    const FaceKnobBinding delay = faceKnobBinding ("EG 2", "DELAY");
    const FaceKnobBinding attack = faceKnobBinding ("EG 2", "ATTACK");
    const FaceKnobBinding release = faceKnobBinding ("EG 2", "RELEASE");
    check (hold.knob == FaceKnob::Eg2Hold && std::strcmp (hold.parameterName, "EG 2 Hold") == 0, "hold name");
    check (hold.fallback == 0.30f && delay.fallback == 0.0f, "hold and delay faceplate");
    check (attack.knob == FaceKnob::Eg2Attack && attack.fallback == 0.2079f, "attack faceplate");
    check (release.knob == FaceKnob::Eg2Release && release.fallback == 0.39f, "release faceplate");
    return finish ("testEg2HasNoSustainKnob");
}

int testEg2ReturnsToZeroWithoutAPlateau()
{
    Eg2 eg;
    eg.prepare (kRate);
    eg.setKnob (Eg2::kKnobHold, knobForSeconds (0.001f));
    eg.setKnob (Eg2::kKnobDelay, knobForSeconds (0.001f));
    eg.setKnob (Eg2::kKnobAttack, knobForSeconds (0.010f));
    eg.setKnob (Eg2::kKnobRelease, knobForSeconds (0.010f));

    int above = 0;
    float peak = 0.0f;
    bool backToZeroWhileHeld = false;
    const int samples = static_cast<int> (kRate);
    for (int i = 0; i < samples; ++i)
    {
        step (eg, 0.0f);
        const float out = eg.portValue[Eg2::kOutPos];
        if (out > peak)
            peak = out;
        if (out > 4.5f)
            ++above;
        if (i > static_cast<int> (0.4 * kRate) && std::fabs (out) < 0.05f)
            backToZeroWhileHeld = true;
    }
    check (peak > 4.5f, "the envelope reaches the top");
    check (above < static_cast<int> (0.08 * kRate), "there is no sustain plateau");
    check (backToZeroWhileHeld, "it returns to 0 while the trigger is still held");
    return finish ("testEg2ReturnsToZeroWithoutAPlateau");
}

int testEg2DelayTrigAfterHold()
{
    Eg2 eg;
    eg.prepare (kRate);
    eg.setKnob (Eg2::kKnobHold, knobForSeconds (0.020f));
    eg.setKnob (Eg2::kKnobDelay, knobForSeconds (0.030f));
    eg.setKnob (Eg2::kKnobAttack, knobForSeconds (0.010f));
    eg.setKnob (Eg2::kKnobRelease, knobForSeconds (0.010f));

    int firstHigh = -1;
    int highCount = 0;
    bool roseBeforeWaitEnded = false;
    const int samples = static_cast<int> (0.1 * kRate);
    for (int i = 0; i < samples; ++i)
    {
        step (eg, 0.0f);
        if (eg.portValue[Eg2::kDelayTrig] > 0.5f)
        {
            if (firstHigh < 0)
                firstHigh = i;
            ++highCount;
        }
        if (i < static_cast<int> (0.045 * kRate) && eg.portValue[Eg2::kOutPos] > 0.05f)
            roseBeforeWaitEnded = true;
    }
    const int holdSamples = static_cast<int> (0.020 * kRate);
    const int pulseSamples = static_cast<int> (std::lround (0.001 * kRate));
    check (firstHigh >= holdSamples - 2 && firstHigh <= holdSamples + 2, "delay trig starts at the end of hold");
    check (std::abs (highCount - pulseSamples) <= 1, "delay trig stays high for 1 ms");
    check (! roseBeforeWaitEnded, "the envelope stays at 0 through hold and delay");
    return finish ("testEg2DelayTrigAfterHold");
}

int testEg2NegIsNegation()
{
    Eg2 eg;
    eg.prepare (kRate);
    eg.setKnob (Eg2::kKnobAttack, knobForSeconds (0.005f));
    eg.setKnob (Eg2::kKnobRelease, knobForSeconds (0.020f));
    bool negated = true;
    float peak = 0.0f;
    for (int i = 0; i < 4000; ++i)
    {
        step (eg, 0.0f);
        const float out = eg.portValue[Eg2::kOutPos];
        if (out > peak)
            peak = out;
        if (std::fabs (eg.portValue[Eg2::kOutNeg] + out) > 1.0e-5f)
            negated = false;
    }
    check (negated, "out minus is the negation of out plus");
    check (peak > 1.0f, "the cycle produced a level");
    return finish ("testEg2NegIsNegation");
}

int testEg2Restart()
{
    Eg2 eg;
    eg.prepare (kRate);
    eg.setKnob (Eg2::kKnobHold, knobForSeconds (0.020f));
    eg.setKnob (Eg2::kKnobDelay, knobForSeconds (0.001f));
    eg.setKnob (Eg2::kKnobAttack, knobForSeconds (0.005f));
    eg.setKnob (Eg2::kKnobRelease, knobForSeconds (0.050f));

    for (int i = 0; i < static_cast<int> (0.040 * kRate); ++i)
        step (eg, 0.0f);
    check (eg.portValue[Eg2::kOutPos] > 1.0f, "attack has started");

    step (eg, 5.0f);
    step (eg, 5.0f);
    step (eg, 0.0f);
    check (std::fabs (eg.portValue[Eg2::kOutPos]) < 1.0e-4f, "a new edge clears the envelope");

    bool stayedDown = true;
    for (int i = 0; i < static_cast<int> (0.015 * kRate); ++i)
    {
        step (eg, 0.0f);
        if (eg.portValue[Eg2::kOutPos] > 0.05f)
            stayedDown = false;
    }
    check (stayedDown, "the restart waits out the new hold");
    return finish ("testEg2Restart");
}

int testEg2PanelJacks()
{
    const int trig = panelJackIndex ("EG 2", "TRIG");
    const int pos = panelJackIndex ("EG 2", "OUT +");
    const int neg = panelJackIndex ("EG 2", "OUT −");
    const int delay = panelJackIndex ("EG 2", "DELAY");
    check (trig >= 0 && pos >= 0 && neg >= 0 && delay >= 0, "eg 2 jacks exist");
    check (kPanelJacks[trig].module == 10 && kPanelJacks[trig].port == 0 && kPanelJacks[trig].dir == 0, "trig");
    check (kPanelJacks[pos].module == 10 && kPanelJacks[pos].port == 1 && kPanelJacks[pos].dir == 1, "out plus");
    check (kPanelJacks[neg].module == 10 && kPanelJacks[neg].port == 2 && kPanelJacks[neg].dir == 1, "out minus");
    check (kPanelJacks[delay].module == 10 && kPanelJacks[delay].port == 3 && kPanelJacks[delay].dir == 1, "delay trig");

    Eg2 eg;
    check (eg.port (Eg2::kDelayTrig).type == PortType::Gate, "delay trig is a gate");
    check (eg.port (Eg2::kTrig).rest == 5.0f, "trig rests at +5 V");

    PanelLink refused;
    check (orientPanelJacks (delay, trig, 0, 1, 2, refused) == PanelLinkResult::Unmapped,
           "eg 2 is refused until its graph index is passed");
    PanelLink linked;
    check (orientPanelJacks (delay, panelJackIndex ("EG 1", "TRIG"), 0, 1, 2, linked,
                             3, 4, 5, 6, -1, -1, 9) == PanelLinkResult::Ok,
           "delay trig feeds eg 1");
    check (linked.sourceModule == 9 && linked.sourcePort == Eg2::kDelayTrig, "delay trig is the source");
    return finish ("testEg2PanelJacks");
}
