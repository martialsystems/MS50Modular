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
    updateCoefficient();
}

void Integrator::updateCoefficient()
{
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    const double tau = 0.001 * std::pow (2000.0, static_cast<double> (time01_));
    coeff_ = -std::expm1 (-1.0 / (tau * rate));
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
    {
        const float value = clamp01 (zeroToOne);
        if (value != time01_)
        {
            time01_ = value;
            updateCoefficient();
        }
    }
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
    state_ = 0.0;
    updateCoefficient();
}

void Integrator::processSample()
{
    state_ += (static_cast<double> (portValue[kIn]) - state_) * coeff_;
    if (! std::isfinite (state_) || std::fabs (state_) < 1.0e-15)
        state_ = 0.0;
    portValue[kOut] = static_cast<float> (state_);
}
