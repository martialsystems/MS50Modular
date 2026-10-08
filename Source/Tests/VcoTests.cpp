// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/PatchGraph.h"
#include "Modular/Jcs.h"
#include "Modular/Vco.h"
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr double kRate = 48000.0;
constexpr float kEightFoot = static_cast<float> (jcs::pitch::kC3Hz);   // 8' = C3 (JCS R4, exact)

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

class ConstCv : public Module {
public:
    float level = 0.0f;

    int numPorts() const override { return 1; }

    PortDesc port (int) const override { return { "Out", PortType::CV, PortDir::Out }; }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override { portValue[0] = level; }
};

void drive (Vco& vco, float hzv, float oct, float freqA, float freqB, float pwm, bool hzConnected)
{
    vco.portValue[Vco::kHzPerVolt] = hzv;
    vco.portValue[Vco::kOct] = oct;
    vco.portValue[Vco::kFreqA] = freqA;
    vco.portValue[Vco::kFreqB] = freqB;
    vco.portValue[Vco::kPwm] = pwm;
    vco.inputConnected[Vco::kHzPerVolt] = hzConnected;
    vco.processSample();
}

float measureHz (Vco& vco, int samples, float hzv, float oct, float freqA, float freqB, float pwm, bool hzConnected)
{
    drive (vco, hzv, oct, freqA, freqB, pwm, hzConnected);
    float previous = vco.portValue[Vco::kSaw];
    int crosses = 0;
    for (int i = 1; i < samples; ++i)
    {
        drive (vco, hzv, oct, freqA, freqB, pwm, hzConnected);
        const float saw = vco.portValue[Vco::kSaw];
        if (previous <= 0.0f && saw > 0.0f)
            ++crosses;
        previous = saw;
    }
    const int intervals = samples - 1;
    return static_cast<float> (static_cast<double> (crosses) * kRate / static_cast<double> (intervals));
}

float ratioClose (float actual, float expected)
{
    if (ronin::exactlyEqual (expected, 0.0f))
        return 1.0f;
    return std::fabs (actual - expected) / std::fabs (expected);
}

}

int testScaleDoesNotChangeOctJack()
{
    Vco low;
    Vco high;
    low.prepare (kRate);
    high.prepare (kRate);
    low.setKnob (Vco::kKnobScale, 2.0f / 3.0f);
    high.setKnob (Vco::kKnobScale, 1.0f);
    const float eight = measureHz (low, 48000, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, false);
    const float four = measureHz (high, 48000, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, false);
    const float footageRatio = Vco::footageHzFor (3) / kEightFoot;   // 4' / 8' = 2
    check (ratioClose (four / eight, footageRatio) < 0.02f, "scale ratio follows footage, with the same Oct/V");
    check (ratioClose (eight / (kEightFoot * 2.0f), 1.0f) < 0.02f, "8' with +1 V Oct/V is two times footage");
    return finish ("testScaleDoesNotChangeOctJack");
}

int testOctIsOneVoltPerOctave()
{
    Vco plain;
    Vco up;
    plain.prepare (kRate);
    up.prepare (kRate);
    const float base = measureHz (plain, 48000, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);
    const float doubled = measureHz (up, 48000, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, false);
    check (ratioClose (base, kEightFoot) < 0.02f, "unpatched 8' is the footage");
    check (ratioClose (doubled / base, 2.0f) < 0.02f, "+1 V on Oct/V doubles frequency");
    return finish ("testOctIsOneVoltPerOctave");
}

int testHzPerVoltIsLinear()
{
    Vco open;
    open.prepare (kRate);
    const float unpatched = measureHz (open, 48000, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);
    check (unpatched > kEightFoot * 0.5f, "unpatched Hz/V does not fall to the 0.05 floor");

    Vco one;
    one.prepare (kRate);
    const float atOne = measureHz (one, 48000, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, true);
    Vco two;
    two.prepare (kRate);
    const float atTwo = measureHz (two, 96000, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, true);
    check (ratioClose (atOne, kEightFoot) < 0.02f, "1 V on a patched Hz/V is footage");
    check (ratioClose (atTwo / atOne, 2.0f) < 0.02f, "2 V is twice 1 V");

    Vco floored;
    floored.prepare (kRate);
    const float atZero = measureHz (floored, 96000, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, true);
    check (ratioClose (atZero, kEightFoot * 0.05f) < 0.2f, "patched 0 V uses the 0.05 floor");

    PatchGraph graph;
    Vco vco;
    ConstCv cv;
    const int cvIndex = graph.addModule (cv);
    const int vcoIndex = graph.addModule (vco);
    graph.prepare (kRate);
    int graphCrosses = 0;
    float previous = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        graph.process();
        const float saw = vco.portValue[Vco::kSaw];
        if (i > 0 && previous <= 0.0f && saw > 0.0f)
            ++graphCrosses;
        previous = saw;
    }
    const float graphHz = static_cast<float> (static_cast<double> (graphCrosses) * kRate / 47999.0);
    check (graphHz > kEightFoot * 0.5f, "graph leaves an unpatched Hz/V off the 0.05 floor");
    check (graph.connect (cvIndex, 0, vcoIndex, Vco::kHzPerVolt), "Hz/V cable");
    cv.level = 0.0f;
    previous = vco.portValue[Vco::kSaw];
    graphCrosses = 0;
    for (int i = 0; i < 96000; ++i)
    {
        graph.process();
        const float saw = vco.portValue[Vco::kSaw];
        if (i > 0 && previous <= 0.0f && saw > 0.0f)
            ++graphCrosses;
        previous = saw;
    }
    const float patchedHz = static_cast<float> (static_cast<double> (graphCrosses) * kRate / 95999.0);
    check (patchedHz < kEightFoot * 0.2f, "graph marks a patched 0 V Hz/V so the floor applies");
    return finish ("testHzPerVoltIsLinear");
}

int testThreeOutputsAlwaysRun()
{
    Vco vco;
    vco.prepare (kRate);
    float sawLow = 0.0f;
    float sawHigh = 0.0f;
    float triLow = 0.0f;
    float triHigh = 0.0f;
    float pulseLow = 0.0f;
    float pulseHigh = 0.0f;
    bool finite = true;
    for (int i = 0; i < 8000; ++i)
    {
        drive (vco, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);
        const float saw = vco.portValue[Vco::kSaw];
        const float tri = vco.portValue[Vco::kTri];
        const float pulse = vco.portValue[Vco::kPulse];
        if (! std::isfinite (saw) || ! std::isfinite (tri) || ! std::isfinite (pulse))
            finite = false;
        if (i < 2000)
            continue;
        if (saw < sawLow)
            sawLow = saw;
        if (saw > sawHigh)
            sawHigh = saw;
        if (tri < triLow)
            triLow = tri;
        if (tri > triHigh)
            triHigh = tri;
        if (pulse < pulseLow)
            pulseLow = pulse;
        if (pulse > pulseHigh)
            pulseHigh = pulse;
    }
    check (finite, "saw, triangle, and pulse stay finite");
    check (sawHigh - sawLow > 8.0f, "saw reaches about ±5 V");
    check (triHigh - triLow > 2.0f, "triangle keeps moving");
    check (pulseHigh - pulseLow > 8.0f, "pulse reaches about ±5 V");
    check (triHigh < 12.0f && triLow > -12.0f, "triangle stays near ±5 V");
    return finish ("testThreeOutputsAlwaysRun");
}

int testPwmMovesDutyNotPitch()
{
    Vco narrow;
    Vco wide;
    narrow.prepare (kRate);
    wide.prepare (kRate);
    narrow.setKnob (Vco::kKnobPw, 0.2f);
    wide.setKnob (Vco::kKnobPw, 0.2f);
    const float pitchA = measureHz (narrow, 48000, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false);
    const float pitchB = measureHz (wide, 48000, 0.0f, 0.0f, 0.0f, 0.0f, 5.0f, false);

    auto duty = [] (float pwm) {
        Vco vco;
        vco.prepare (kRate);
        vco.setKnob (Vco::kKnobPw, 0.2f);
        int high = 0;
        const int samples = 4800;
        for (int i = 0; i < samples; ++i)
        {
            drive (vco, 0.0f, 0.0f, 0.0f, 0.0f, pwm, false);
            if (vco.portValue[Vco::kPulse] > 0.0f)
                ++high;
        }
        return static_cast<float> (high) / static_cast<float> (samples);
    };

    const float dutyA = duty (0.0f);
    const float dutyB = duty (5.0f);
    check (ratioClose (pitchA, pitchB) < 0.02f, "PWM does not move pitch");
    check (std::fabs (dutyA - 0.23f) < 0.04f, "PW 0.2 duty is about 0.23");
    check (dutyB > dutyA + 0.3f, "PWM opens the pulse");
    return finish ("testPwmMovesDutyNotPitch");
}

int testVcoPanelJacks()
{
    const char* labels[8] = { "HZ/V", "V/OCT", "FM 1", "FM 2", "PWM", "TRI", "SAW", "PULSE" };
    const int ports[8] = { 0, 1, 2, 3, 4, 6, 5, 7 };
    const int dirs[8] = { 0, 0, 0, 0, 0, 1, 1, 1 };
    for (int i = 0; i < 8; ++i)
    {
        const int jack = panelJackIndex ("VCO", labels[i]);
        check (jack >= 0, "vco jack exists");
        if (jack < 0)
            continue;
        check (kPanelJacks[jack].module == 9, "vco module id");
        check (kPanelJacks[jack].port == ports[i], "vco port");
        check (kPanelJacks[jack].dir == dirs[i], "vco direction");
    }

    PanelLink refused;
    check (orientPanelJacks (panelJackIndex ("VCO", "SAW"), panelJackIndex ("OUTPUT", "WET"),
                             0, 1, 2, refused, 3) == PanelLinkResult::Unmapped,
           "vco jack is refused until the module index is passed");
    PanelLink linked;
    check (orientPanelJacks (panelJackIndex ("VCO", "SAW"), panelJackIndex ("OUTPUT", "WET"),
                             0, 1, 2, linked, 3, -1, -1, -1, -1, 8) == PanelLinkResult::Ok,
           "saw feeds wet");
    check (linked.sourceModule == 8 && linked.sourcePort == Vco::kSaw, "saw is the source");
    check (linked.destModule == 1 && linked.destPort == 2, "wet is the destination");

    const FaceKnobBinding range = faceKnobBinding ("VCO", "RANGE");
    const FaceKnobBinding fine = faceKnobBinding ("VCO", "FINE");
    const FaceKnobBinding pw = faceKnobBinding ("VCO", "PW");
    const FaceKnobBinding fm1 = faceKnobBinding ("VCO", "FM 1");
    const FaceKnobBinding fm2 = faceKnobBinding ("VCO", "FM 2");
    check (range.knob == FaceKnob::VcoRange && std::strcmp (range.parameterName, "VCO Range") == 0, "range name");
    check (ronin::exactlyEqual (range.fallback, 0.50f), "range faceplate");
    check (fine.knob == FaceKnob::VcoFine && ronin::exactlyEqual (fine.fallback, 0.50f), "fine faceplate");
    check (pw.knob == FaceKnob::VcoPw && ronin::exactlyEqual (pw.fallback, 0.50f), "pw faceplate");
    check (fm1.knob == FaceKnob::VcoFm1 && ronin::exactlyEqual (fm1.fallback, 0.0f), "fm 1 faceplate");
    check (fm2.knob == FaceKnob::VcoFm2 && ronin::exactlyEqual (fm2.fallback, 0.0f), "fm 2 faceplate");
    return finish ("testVcoPanelJacks");
}
