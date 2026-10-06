// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/Mg.h"
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

void step (MgModule& mg, float fm, float pwm)
{
    mg.portValue[MgModule::kFreqMod] = fm;
    mg.portValue[MgModule::kPwm] = pwm;
    mg.processSample();
}

float measureHz (MgModule& mg, float fm, int samples)
{
    step (mg, fm, 0.0f);
    float previous = mg.portValue[MgModule::kSawUp];
    int wraps = 0;
    double rise = 0.0;
    int riseCount = 0;
    for (int i = 1; i < samples; ++i)
    {
        step (mg, fm, 0.0f);
        const float saw = mg.portValue[MgModule::kSawUp];
        const float delta = saw - previous;
        if (delta < -1.0f)
            ++wraps;
        else
        {
            rise += static_cast<double> (delta);
            ++riseCount;
        }
        previous = saw;
    }
    if (wraps > 4)
        return static_cast<float> (static_cast<double> (wraps) * kRate / static_cast<double> (samples));
    if (riseCount == 0)
        return 0.0f;
    return static_cast<float> ((rise / static_cast<double> (riseCount)) * kRate / 5.0);
}

}

int testMgPulseIsUnipolar()
{
    MgModule mg;
    mg.prepare (kRate);
    mg.setKnob (MgModule::kKnobFrequency, 1.0f);
    mg.setKnob (MgModule::kKnobPw, 0.5f);
    float low = 5.0f;
    float high = 0.0f;
    bool hitLow = false;
    bool hitHigh = false;
    for (int i = 0; i < 4800; ++i)
    {
        step (mg, 0.0f, 0.0f);
        const float pulse = mg.portValue[MgModule::kPulse];
        if (pulse < low)
            low = pulse;
        if (pulse > high)
            high = pulse;
        if (pulse == 0.0f)
            hitLow = true;
        if (pulse == 5.0f)
            hitHigh = true;
    }
    check (low >= 0.0f, "pulse never goes negative");
    check (high <= 5.0f, "pulse stays at or below +5 V");
    check (hitLow && hitHigh, "pulse reaches 0 V and +5 V");
    return finish ("testMgPulseIsUnipolar");
}

int testMgTriangleIsBipolar2V5()
{
    MgModule mg;
    mg.prepare (kRate);
    mg.setKnob (MgModule::kKnobFrequency, 1.0f);
    mg.setKnob (MgModule::kKnobPw, 0.5f);
    float low = 0.0f;
    float high = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        step (mg, 0.0f, 0.0f);
        const float tri = mg.portValue[MgModule::kTri];
        if (tri < low)
            low = tri;
        if (tri > high)
            high = tri;
        check (tri <= 2.51f && tri >= -2.51f, "triangle stays inside ±2.5 V");
    }
    check (high > 2.4f && low < -2.4f, "triangle reaches about ±2.5 V");
    return finish ("testMgTriangleIsBipolar2V5");
}

int testMgFreqEndpoints()
{
    MgModule slow;
    slow.prepare (kRate);
    slow.setKnob (MgModule::kKnobFrequency, 0.0f);
    const float lowHz = measureHz (slow, 0.0f, 48000);
    check (std::fabs (lowHz - 0.01f) / 0.01f < 0.05f, "frequency knob 0 is about 0.01 Hz");

    MgModule fast;
    fast.prepare (kRate);
    fast.setKnob (MgModule::kKnobFrequency, 1.0f);
    const float highHz = measureHz (fast, 0.0f, 48000);
    check (std::fabs (highHz - 200.0f) < 2.0f, "frequency knob 1 is about 200 Hz");

    MgModule mid;
    mid.prepare (kRate);
    const float plain = measureHz (mid, 0.0f, 48000);
    MgModule doubled;
    doubled.prepare (kRate);
    const float up = measureHz (doubled, 5.0f, 48000);
    check (std::fabs (up - plain * 2.0f) < 0.15f, "+5 V FM doubles the knob frequency");

    MgModule floored;
    floored.prepare (kRate);
    const float down = measureHz (floored, -5.0f, 48000);
    check (down < 0.02f, "-5 V FM reaches the 0.01 Hz clamp");

    MgModule capped;
    capped.prepare (kRate);
    capped.setKnob (MgModule::kKnobFrequency, 1.0f);
    const float cappedHz = measureHz (capped, 5.0f, 48000);
    check (cappedHz < 205.0f, "+5 V FM does not pass 200 Hz");
    return finish ("testMgFreqEndpoints");
}

int testMgPwAffectsPulseAndTriangle()
{
    auto dutyAndShape = [] (float pw, float& duty, float& fallingError, float& risingError) {
        MgModule mg;
        mg.prepare (kRate);
        mg.setKnob (MgModule::kKnobFrequency, 1.0f);
        mg.setKnob (MgModule::kKnobPw, pw);
        int high = 0;
        double fall = 0.0;
        double rise = 0.0;
        const int samples = 4800;
        for (int i = 0; i < samples; ++i)
        {
            step (mg, 0.0f, 0.0f);
            if (mg.portValue[MgModule::kPulse] > 2.5f)
                ++high;
            const float tri = mg.portValue[MgModule::kTri];
            fall += std::fabs (static_cast<double> (tri - mg.portValue[MgModule::kSawDown]));
            rise += std::fabs (static_cast<double> (tri - mg.portValue[MgModule::kSawUp]));
        }
        duty = static_cast<float> (high) / static_cast<float> (samples);
        fallingError = static_cast<float> (fall / samples);
        risingError = static_cast<float> (rise / samples);
    };

    float duty = 0.0f;
    float falling = 0.0f;
    float rising = 0.0f;
    dutyAndShape (0.0f, duty, falling, rising);
    check (std::fabs (duty - 0.05f) < 0.02f, "PW 0 pulse duty is about 0.05");
    check (falling < 0.05f, "PW 0 triangle is the falling saw");
    check (rising > 0.5f, "PW 0 triangle is not the rising saw");

    dutyAndShape (0.5f, duty, falling, rising);
    check (std::fabs (duty - 0.50f) < 0.02f, "PW 0.5 pulse duty is about 0.5");
    check (falling > 0.4f && rising > 0.4f, "PW 0.5 triangle is neither saw");

    dutyAndShape (1.0f, duty, falling, rising);
    check (std::fabs (duty - 0.95f) < 0.02f, "PW 1 pulse duty is about 0.95");
    check (rising < 0.05f, "PW 1 triangle is the rising saw");
    check (falling > 0.5f, "PW 1 triangle is not the falling saw");
    return finish ("testMgPwAffectsPulseAndTriangle");
}

int testMgSawJacksOpposite()
{
    const float widths[3] = { 0.0f, 0.5f, 1.0f };
    for (float pw : widths)
    {
        MgModule mg;
        mg.prepare (kRate);
        mg.setKnob (MgModule::kKnobFrequency, 1.0f);
        mg.setKnob (MgModule::kKnobPw, pw);
        float low = 0.0f;
        float high = 0.0f;
        for (int i = 0; i < 2400; ++i)
        {
            step (mg, 0.0f, 2.5f);
            const float up = mg.portValue[MgModule::kSawUp];
            const float down = mg.portValue[MgModule::kSawDown];
            check (std::fabs (up + down) < 1.0e-4f, "saw jacks sum to 0");
            if (up < low)
                low = up;
            if (up > high)
                high = up;
        }
        check (high > 2.4f && low < -2.4f, "saw jacks reach ±2.5 V");
    }
    return finish ("testMgSawJacksOpposite");
}

int testMgPanelJacks()
{
    const int fm = panelJackIndex ("MG", "FM");
    const int pwm = panelJackIndex ("MG", "PWM");
    const int tri = panelJackIndex ("MG", "TRI");
    const int saw = panelJackIndex ("MG", "SAW");
    const int inv = panelJackIndex ("MG", "INV SAW");
    const int pulse = panelJackIndex ("MG", "PULSE");
    check (fm >= 0 && pwm >= 0 && tri >= 0 && saw >= 0 && inv >= 0 && pulse >= 0, "mg jacks exist");
    check (kPanelJacks[fm].module == 8 && kPanelJacks[fm].port == 0 && kPanelJacks[fm].dir == 0, "fm in");
    check (kPanelJacks[pwm].module == 8 && kPanelJacks[pwm].port == 1 && kPanelJacks[pwm].dir == 0, "pwm in");
    check (kPanelJacks[tri].module == 8 && kPanelJacks[tri].port == 2 && kPanelJacks[tri].dir == 1, "tri out");
    check (kPanelJacks[saw].module == 8 && kPanelJacks[saw].port == 3 && kPanelJacks[saw].dir == 1, "saw out");
    check (kPanelJacks[inv].module == 8 && kPanelJacks[inv].port == 4 && kPanelJacks[inv].dir == 1, "inv saw out");
    check (kPanelJacks[pulse].module == 8 && kPanelJacks[pulse].port == 5 && kPanelJacks[pulse].dir == 1, "pulse out");

    PanelLink refused;
    check (orientPanelJacks (tri, panelJackIndex ("VCF", "CUTOFF"), 0, 1, 2, refused, 3) == PanelLinkResult::Unmapped,
           "mg jack is refused until the module index is passed");
    PanelLink linked;
    check (orientPanelJacks (tri, panelJackIndex ("VCF", "CUTOFF"), 0, 1, 2, linked, 3, -1, -1, -1, 4) == PanelLinkResult::Ok,
           "tri feeds cutoff");
    check (linked.sourceModule == 4 && linked.sourcePort == MgModule::kTri, "tri is the source");
    check (linked.destModule == 3 && linked.destPort == 1, "cutoff is the destination");
    PanelLink bothIn;
    check (orientPanelJacks (fm, pwm, 0, 1, 2, bothIn, 3, -1, -1, -1, 4) == PanelLinkResult::BadType,
           "two mg inputs do not patch");

    const FaceKnobBinding rate = faceKnobBinding ("MG", "RATE");
    const FaceKnobBinding width = faceKnobBinding ("MG", "PW");
    check (rate.knob == FaceKnob::MgRate && std::strcmp (rate.parameterName, "MG Rate") == 0, "rate name");
    check (rate.minimum == 0.0f && rate.maximum == 1.0f && rate.fallback == 0.50f, "rate range");
    check (width.knob == FaceKnob::MgPw && std::strcmp (width.parameterName, "MG PW") == 0, "pw name");
    check (width.fallback == 0.30f, "pw faceplate default");
    check (faceKnobBinding ("VCO", "RANGE").knob == FaceKnob::VcoRange, "vco range is a parameter");
    return finish ("testMgPanelJacks");
}
