// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Ring.h"

#include <cmath>

int Ring::numPorts() const
{
    return 3;
}

PortDesc Ring::port (int index) const
{
    if (index == kA)
        return { "A", PortType::CV, PortDir::In };
    if (index == kB)
        return { "B", PortType::CV, PortDir::In };
    return { "Out", PortType::CV, PortDir::Out };
}

int Ring::numKnobs() const
{
    return 0;
}

void Ring::setKnob (int, float)
{
}

void Ring::prepare (double rate)
{
    sampleRate = rate;
}

void Ring::processSample()
{
    // Out = (A * B) / 5. Bleed is 0. DC passes.
    const float product = (portValue[kA] * portValue[kB]) / 5.0f;
    portValue[kOut] = std::isfinite (product) ? product : 0.0f;
}
