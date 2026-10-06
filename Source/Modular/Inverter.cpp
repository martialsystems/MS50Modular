// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Inverter.h"

#include <cmath>

int Inverter::numPorts() const
{
    return 2;
}

PortDesc Inverter::port (int index) const
{
    if (index == kIn)
        return { "In", PortType::CV, PortDir::In };
    return { "Out", PortType::CV, PortDir::Out };
}

int Inverter::numKnobs() const
{
    return 0;
}

void Inverter::setKnob (int, float)
{
}

void Inverter::prepare (double rate)
{
    sampleRate = rate;
}

void Inverter::processSample()
{
    const float negated = -portValue[kIn];
    portValue[kOut] = std::isfinite (negated) ? negated : 0.0f;
}
