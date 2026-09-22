// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "ExtIn.h"

int ExtIn::numPorts() const
{
    return 4;
}

PortDesc ExtIn::port (int index) const
{
    if (index == 0)
        return { "L", PortType::Audio, PortDir::Out };
    if (index == 1)
        return { "R", PortType::Audio, PortDir::Out };
    if (index == 2)
        return { "Mono", PortType::Audio, PortDir::Out };
    return { "Gate", PortType::Gate, PortDir::Out };
}

void ExtIn::setKnob (int, float)
{
}

void ExtIn::prepare (double rate)
{
    sampleRate = rate;
}

void ExtIn::setHostSample (float leftUnit, float rightUnit)
{
    inLVolts_ = leftUnit * kHostToVolts;
    inRVolts_ = rightUnit * kHostToVolts;
}

void ExtIn::processSample()
{
    portValue[0] = inLVolts_;
    portValue[1] = inRVolts_;
    portValue[2] = 0.5f * (inLVolts_ + inRVolts_);
    portValue[3] = 0.0f;
}
