// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Vca2.h"

#include <cmath>

namespace {

void flushState (float& state)
{
    if (! std::isfinite (state) || (state < 1.0e-15f && state > -1.0e-15f))
        state = 0.0f;
}

float clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

}

int Vca2::numPorts() const
{
    return 3;
}

PortDesc Vca2::port (int index) const
{
    if (index == kIn)
        return { "In", PortType::CV, PortDir::In };
    if (index == kControl)
        return { "Control", PortType::CV, PortDir::In };
    return { "Out", PortType::CV, PortDir::Out };
}

int Vca2::numKnobs() const
{
    return 2;
}

void Vca2::setKnob (int knob, float zeroToOne)
{
    if (knob == kKnobInitial)
        initial01_ = clamp01 (zeroToOne);
    else if (knob == kKnobMod)
        mod_ = clamp01 (zeroToOne);
}

void Vca2::prepare (double rate)
{
    sampleRate = rate;
    gain_ = 0.0f;
}

void Vca2::processSample()
{
    const float target = clamp01 (portValue[kControl] / 5.0f + initial01_) * mod_;
    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    const float coeff = 1.0f - std::exp (-1.0f / (0.020f * rate));
    gain_ += (target - gain_) * coeff;
    flushState (gain_);

    float output = portValue[kIn] * gain_;
    if (! std::isfinite (output))
        output = 0.0f;
    portValue[kOut] = output;
}
