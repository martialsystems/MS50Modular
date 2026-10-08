// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Vco.h"

#include "Jcs.h"

#include <cmath>

namespace {

// S-03 footage, equal-tempered C at A4 = 440 (JCS R4). 8' is the unpatched panel pitch: C3 = the shared exact
// jcs::pitch::kC3Hz (130.8127826502993 Hz); 32', 16' and 4' are whole octaves of it.
constexpr double kC3 = jcs::pitch::kC3Hz;
constexpr float kFootage[4] = { static_cast<float> (kC3 / 4.0), static_cast<float> (kC3 / 2.0), static_cast<float> (kC3),
                                static_cast<float> (kC3 * 2.0) };

}

float Vco::footageHzFor (int scaleIndex) noexcept
{
    return kFootage[scaleIndex < 0 ? 0 : (scaleIndex > 3 ? 3 : scaleIndex)];
}

float Vco::clamp01 (float value)
{
    return clampf (value, 0.0f, 1.0f);
}

float Vco::clampf (float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

float Vco::polyBlep (float t, float dt)
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

// Two-sample polyBLAMP residual (integrated polyBLEP), unit slope change. RONIN_Redesign §3.2,
// the same form as jidai-audit/verify/verify_ronin.py tri_blamp.
float Vco::polyBlamp (float t, float dt)
{
    if (dt <= 1.0e-8f)
        return 0.0f;
    if (t < dt)
    {
        const float x = t / dt - 1.0f;
        return -(1.0f / 3.0f) * x * x * x;
    }
    if (t > 1.0f - dt)
    {
        const float x = (t - 1.0f) / dt + 1.0f;
        return (1.0f / 3.0f) * x * x * x;
    }
    return 0.0f;
}

int Vco::scaleIndex() const
{
    int index = static_cast<int> (std::round (static_cast<double> (clamp01 (scale01_) * 3.0f)));
    if (index < 0)
        return 0;
    if (index > 3)
        return 3;
    return index;
}

float Vco::footageHz() const
{
    return kFootage[activeScale_];
}

float Vco::fineRatio() const
{
    // The stored knob is 0..1. Zero cents is the center, not the raw 0..1 value.
    const float signedKnob = clamp01 (fine01_) * 2.0f - 1.0f;
    return std::exp2 ((signedKnob * 2.0f) / 12.0f);
}

int Vco::numPorts() const
{
    return 8;
}

PortDesc Vco::port (int index) const
{
    // JCS R4: VCO:HZ/V is the linear HZ/V LIN role (f = footage * max(V, 0.05)), never the rack pitch standard.
    // VCO:V/OCT is the rack standard, relative: f = footage * 2^V, so 8' at 0 V = C3 = jcs::pitch::kC3Hz.
    if (index == kHzPerVolt)
        return { "Hz/V", PortType::CV, PortDir::In, 0.0f, false, false, PortRole::HzvLin };
    if (index == kOct)
        return { "Oct/V", PortType::CV, PortDir::In, 0.0f, false, false, PortRole::VOct };
    if (index == kFreqA)
        return { "FreqA", PortType::CV, PortDir::In };
    if (index == kFreqB)
        return { "FreqB", PortType::CV, PortDir::In };
    if (index == kPwm)
        return { "PWM", PortType::CV, PortDir::In };
    if (index == kSaw)
        return { "Saw", PortType::Audio, PortDir::Out };
    if (index == kTri)
        return { "Tri", PortType::Audio, PortDir::Out };
    return { "Pulse", PortType::Audio, PortDir::Out };
}

int Vco::numKnobs() const
{
    return 5;
}

void Vco::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    if (knob == kKnobScale)
        scale01_ = value;
    else if (knob == kKnobAmountA)
        amountA01_ = value;
    else if (knob == kKnobAmountB)
        amountB01_ = value;
    else if (knob == kKnobFine)
        fine01_ = value;
    else if (knob == kKnobPw)
        pw01_ = value;
}

int Vco::presetKnobCount() const
{
    return 5;
}

float Vco::presetKnob (int knob) const
{
    if (knob == kKnobScale)
        return scale01_;
    if (knob == kKnobAmountA)
        return amountA01_;
    if (knob == kKnobAmountB)
        return amountB01_;
    if (knob == kKnobFine)
        return fine01_;
    if (knob == kKnobPw)
        return pw01_;
    return 0.0f;
}

void Vco::applyFactoryPreset (int)
{
    triShape_ = TriShape::Triangle;
}

int Vco::presetScaleIndex() const
{
    return scaleIndex();
}

void Vco::prepare (double rate)
{
    sampleRate = rate;
    phase_ = 0.0;
    triState_ = 0.0;
    activeScale_ = scaleIndex();
    started_ = false;
}

void Vco::processSample()
{
    // §3.5: a RANGE change lands at the next saw wrap (immediately before the first sample after prepare).
    if (! started_)
        activeScale_ = scaleIndex();
    started_ = true;
    const float footage = footageHz();
    const float oct = portValue[kOct] + portValue[kFreqA] * amountA01_ + portValue[kFreqB] * amountB01_;
    const float octExpo = std::exp2 (oct);
    float linear = footage;
    if (inputConnected[kHzPerVolt])
    {
        float volts = portValue[kHzPerVolt];
        constexpr float floorVolts = static_cast<float> (jcs::pitch::kRoninLinFloor);   // JCS R4.2 / S-03: 0.05 V
        if (volts < floorVolts)
            volts = floorVolts;
        linear = footage * volts;
    }

    float hz = linear * fineRatio() * octExpo;
    if (! std::isfinite (hz) || hz < 0.0f)
        hz = footage;

    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    float dt = hz / rate;
    // PolyBLEP residual has to land inside one sample. This is not a footage law.
    if (dt > 0.45f)
        dt = 0.45f;
    if (dt < 0.0f)
        dt = 0.0f;

    lastHz_.store (hz, std::memory_order_relaxed);

    phase_ += static_cast<double> (dt);
    if (phase_ >= 1.0)
    {
        phase_ -= std::floor (phase_);
        activeScale_ = scaleIndex();
    }
    const float phase = static_cast<float> (phase_);

    float sawUnit = 2.0f * phase - 1.0f;
    sawUnit -= polyBlep (phase, dt);

    float duty = 0.05f + clamp01 (pw01_) * 0.90f + portValue[kPwm] / 5.0f * 0.5f;
    duty = clampf (duty, 0.05f, 0.95f);
    float pulseUnit = phase < duty ? 1.0f : -1.0f;
    pulseUnit += polyBlep (phase, dt);
    float fall = phase - duty;
    if (fall < 0.0f)
        fall += 1.0f;
    pulseUnit -= polyBlep (fall, dt);

    // PARABOLA (legacy): the integral of the polyBLEP saw. The naive integral is a parabola of height 0.25, so 40
    // brings it to about ±5 V; the slow servo removes residual DC. Kept bit-for-bit for user v1 patches (M-R2).
    triState_ += static_cast<double> (sawUnit) * static_cast<double> (dt);
    triState_ -= triState_ * 1.0e-4;
    if (! std::isfinite (triState_))
        triState_ = 0.0;

    float tri = 0.0f;
    if (triShape_ == TriShape::Parabola)
    {
        tri = static_cast<float> (triState_ * 40.0);
    }
    else
    {
        // TRIANGLE: naive triangle with PolyBLAMP at both corners (phase 0 and 1/2), ±5 V.
        // Odd harmonics only, no DC servo, no start transient (RONIN_Redesign §3.2).
        float t = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
        t += 4.0f * dt * polyBlamp (phase, dt);
        float half = phase + 0.5f;
        if (half >= 1.0f)
            half -= 1.0f;
        t -= 4.0f * dt * polyBlamp (half, dt);
        tri = t * 5.0f;
    }
    if (! std::isfinite (tri))
        tri = 0.0f;

    portValue[kSaw] = sawUnit * 5.0f;
    portValue[kTri] = tri;
    portValue[kPulse] = pulseUnit * 5.0f;
}
