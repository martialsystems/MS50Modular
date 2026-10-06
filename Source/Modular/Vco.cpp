// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Vco.h"

#include <cmath>

namespace {

// S-03 footage. 8' is the unpatched panel pitch.
constexpr float kFootage[4] = { 32.703f, 65.406f, 130.813f, 261.626f };

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
    return kFootage[scaleIndex()];
}

float Vco::fineRatio() const
{
    // The stored knob is 0..1. Zero cents is the center, not the raw 0..1 value.
    const float signedKnob = clamp01 (fine01_) * 2.0f - 1.0f;
    return std::pow (2.0f, (signedKnob * 2.0f) / 12.0f);
}

int Vco::numPorts() const
{
    return 8;
}

PortDesc Vco::port (int index) const
{
    if (index == kHzPerVolt)
        return { "Hz/V", PortType::CV, PortDir::In };
    if (index == kOct)
        return { "Oct/V", PortType::CV, PortDir::In };
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

int Vco::presetScaleIndex() const
{
    return scaleIndex();
}

void Vco::prepare (double rate)
{
    sampleRate = rate;
    phase_ = 0.0;
    triState_ = 0.0;
}

void Vco::processSample()
{
    const float footage = footageHz();
    const float oct = portValue[kOct] + portValue[kFreqA] * amountA01_ + portValue[kFreqB] * amountB01_;
    const float octExpo = std::pow (2.0f, oct);
    float linear = footage;
    if (inputConnected[kHzPerVolt])
    {
        float volts = portValue[kHzPerVolt];
        if (volts < 0.05f)
            volts = 0.05f;
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

    phase_ += static_cast<double> (dt);
    if (phase_ >= 1.0)
        phase_ -= std::floor (phase_);
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

    // Integral of the polyblep saw. The naive integral is a parabola of height 0.25,
    // so 40 brings that shape to ±5 V. The slow servo only removes residual DC.
    triState_ += static_cast<double> (sawUnit) * static_cast<double> (dt);
    triState_ -= triState_ * 1.0e-4;
    if (! std::isfinite (triState_))
        triState_ = 0.0;
    float tri = static_cast<float> (triState_ * 40.0);
    if (! std::isfinite (tri))
        tri = 0.0f;

    portValue[kSaw] = sawUnit * 5.0f;
    portValue[kTri] = tri;
    portValue[kPulse] = pulseUnit * 5.0f;
}
