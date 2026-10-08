// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Vca1.h"

#include "FactoryPresets.h"

#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;

void flushState (float& state)
{
    if (! std::isfinite (state) || (state < 1.0e-15f && state > -1.0e-15f))
        state = 0.0f;
}

}

float Vca1::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

int Vca1::numPorts() const
{
    return 3;
}

PortDesc Vca1::port (int index) const
{
    if (index == kSigIn)
        return { "SigIn", PortType::Audio, PortDir::In };
    if (index == kEnv)
        return { "Env", PortType::CV, PortDir::In };
    return { "Out", PortType::Audio, PortDir::Out };
}

int Vca1::numKnobs() const
{
    return 2;
}

void Vca1::setKnob (int knob, float zeroToOne)
{
    if (knob == kKnobLowCut)
        lowCut01_ = clamp01 (zeroToOne);
    else if (knob == kKnobIntensity)
    {
        intensity_ = clamp01 (zeroToOne);
        intensitySmooth_.setTarget (intensity_);
    }
    else if (knob == kKnobInitial)
    {
        initial01_ = clamp01 (zeroToOne);
        initialSmooth_.setTarget (initial01_);
    }
}

int Vca1::presetKnobCount() const
{
    return 2;
}

float Vca1::presetKnob (int knob) const
{
    if (knob == kKnobLowCut)
        return lowCut01_;
    if (knob == kKnobIntensity)
        return intensity_;
    return 0.0f;
}

void Vca1::applyFactoryPreset (int index)
{
    setKnob (kKnobInitial, factoryVca1Initial (index));
}

void Vca1::prepare (double rate)
{
    sampleRate = rate;
    low_ = 0.0f;
    intensitySmooth_.prepare (rate);
    initialSmooth_.prepare (rate);
}

float Vca1::lowCutHz() const
{
    // S-11: 10 Hz to 2 kHz, exponential.
    return 10.0f * std::pow (200.0f, lowCut01_);
}

void Vca1::processSample()
{
    const float input = portValue[kSigIn];
    const float hz = lowCutHz();
    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    const float tau = 1.0f / (2.0f * kPi * hz);
    const float coeff = 1.0f - std::exp (-1.0f / (tau * rate));
    low_ += (input - low_) * coeff;
    flushState (low_);

    float envGain = portValue[kEnv] / 5.0f + initialSmooth_.next();
    envGain = clamp01 (envGain);
    float output = (input - low_) * envGain * intensitySmooth_.next();
    if (! std::isfinite (output))
        output = 0.0f;
    portValue[kOut] = output;
}
