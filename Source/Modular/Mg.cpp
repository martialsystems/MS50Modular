// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Mg.h"

#include <cmath>

namespace {

constexpr float kMinHz = 0.01f;
constexpr float kMaxHz = 200.0f;

}

MgModule::MgModule()
{
    freq01_ = std::log (500.0f) / std::log (20000.0f);
}

float MgModule::clamp01 (float value)
{
    return clampf (value, 0.0f, 1.0f);
}

float MgModule::clampf (float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

// Two-sample polyBLEP residual for a unit (-1 -> +1) step (the VCO's form). RONIN_Redesign §3.3.
static float mgPolyBlep (float t, float dt)
{
    if (dt <= 1.0e-8f)
        return 0.0f;
    if (t < dt)
    {
        const float x = t / dt;
        return x + x - x * x - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        const float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

float MgModule::knobHz() const
{
    return kMinHz * std::pow (20000.0f, clamp01 (freq01_));
}

float MgModule::morph (float phase, float sym)
{
    // Rise from -2.5 V to +2.5 V over [0, sym), then fall. sym 0 is a falling saw,
    // sym 0.5 is a triangle, sym 1 is a rising saw.
    if (sym <= 1.0e-6f)
        return 2.5f - 5.0f * phase;
    if (sym >= 1.0f - 1.0e-6f)
        return -2.5f + 5.0f * phase;
    if (phase < sym)
        return -2.5f + 5.0f * (phase / sym);
    return 2.5f - 5.0f * ((phase - sym) / (1.0f - sym));
}

int MgModule::numPorts() const
{
    return 6;
}

PortDesc MgModule::port (int index) const
{
    if (index == kFreqMod)
        return { "FreqMod", PortType::CV, PortDir::In };
    if (index == kPwm)
        return { "PWM", PortType::CV, PortDir::In };
    // JCS R14: MG outputs are CV by role (yellow), though typed Audio for the graph.
    if (index == kTri)
        return { "Tri", PortType::Audio, PortDir::Out, 0.0f, false, false, PortRole::Cv };
    if (index == kSawUp)
        return { "SawUp", PortType::Audio, PortDir::Out, 0.0f, false, false, PortRole::Cv };
    if (index == kSawDown)
        return { "SawDown", PortType::Audio, PortDir::Out, 0.0f, false, false, PortRole::Cv };
    return { "Pulse", PortType::Audio, PortDir::Out, 0.0f, false, false, PortRole::Cv };
}

int MgModule::numKnobs() const
{
    return 2;
}

void MgModule::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    if (knob == kKnobFrequency)
        freq01_ = value;
    else if (knob == kKnobPw)
        pw01_ = value;
}

int MgModule::presetKnobCount() const
{
    return 2;
}

float MgModule::presetKnob (int knob) const
{
    if (knob == kKnobFrequency)
        return freq01_;
    if (knob == kKnobPw)
        return pw01_;
    return 0.0f;
}

void MgModule::prepare (double rate)
{
    sampleRate = rate;
    phase_ = 0.0;
}

void MgModule::processSample()
{
    const float base = knobHz();
    const float fm = portValue[kFreqMod];
    // S-16 scale factor: (FreqMod volts / 5) * knobHz.
    // +5 V doubles the knob frequency. -5 V subtracts one knobHz, then the 0.01 Hz clamp.
    const float hz = clampf (base + (fm / 5.0f) * base, kMinHz, kMaxHz);
    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    const double dtD = static_cast<double> (hz) / static_cast<double> (rate);
    phase_ += dtD;
    if (phase_ >= 1.0)
        phase_ -= std::floor (phase_);

    const float phase = static_cast<float> (phase_);
    const float dt = static_cast<float> (dtD);
    const float sym = clampf (pw01_ + portValue[kPwm] / 5.0f, 0.0f, 1.0f);
    const float duty = 0.05f + sym * 0.90f;
    // §3.3: polyBLEP on the saw wrap (a -5 V step) and on both pulse edges (0/5 V). Levels and shapes KEPT.
    // At LFO rates dt is tiny and the correction vanishes.
    const float sawUp = -2.5f + 5.0f * phase - 2.5f * mgPolyBlep (phase, dt);
    const float sawDown = -sawUp;
    float pulseUnit = phase < duty ? 1.0f : -1.0f;
    pulseUnit += mgPolyBlep (phase, dt);
    float fall = phase - duty;
    if (fall < 0.0f)
        fall += 1.0f;
    pulseUnit -= mgPolyBlep (fall, dt);

    portValue[kTri] = morph (phase, sym);
    portValue[kSawUp] = sawUp;
    portValue[kSawDown] = sawDown;
    portValue[kPulse] = 2.5f + 2.5f * pulseUnit;
}
