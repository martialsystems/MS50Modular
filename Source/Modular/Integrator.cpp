// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Integrator.h"

#include <cmath>

namespace {

float clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float knobForTau (float seconds)
{
    return static_cast<float> (std::log (static_cast<double> (seconds) / 0.001) / std::log (2000.0));
}

}

Integrator::Integrator()
    : time01_ (knobForTau (0.050f))
{
}

int Integrator::numPorts() const
{
    return 2;
}

PortDesc Integrator::port (int index) const
{
    if (index == kIn)
        return { "In", PortType::CV, PortDir::In };
    return { "Out", PortType::CV, PortDir::Out };
}

int Integrator::numKnobs() const
{
    return 1;
}

void Integrator::setKnob (int knob, float zeroToOne)
{
    if (knob == kKnobTime)
        time01_ = clamp01 (zeroToOne);
}

int Integrator::presetKnobCount() const
{
    return 1;
}

float Integrator::presetKnob (int knob) const
{
    if (knob == kKnobTime)
        return time01_;
    return 0.0f;
}

void Integrator::prepare (double rate)
{
    sampleRate = rate;
    state_ = 0.0f;
}

void Integrator::processSample()
{
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    const double tau = 0.001 * std::pow (2000.0, static_cast<double> (time01_));
    const float coeff = static_cast<float> (1.0 - std::exp (-1.0 / (tau * rate)));
    state_ += (portValue[kIn] - state_) * coeff;
    if (! std::isfinite (state_))
        state_ = 0.0f;
    portValue[kOut] = state_;
}
