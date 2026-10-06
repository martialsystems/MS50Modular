// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "OutputModule.h"

int OutputModule::numPorts() const
{
    return 3;
}

PortDesc OutputModule::port (int index) const
{
    if (index == 0)
        return { "L", PortType::Audio, PortDir::In };
    if (index == 1)
        return { "R", PortType::Audio, PortDir::In };
    return { "Wet", PortType::Audio, PortDir::In };
}

float OutputModule::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

void OutputModule::setKnob (int knob, float zeroToOne)
{
    if (knob == 0)
        setMix (zeroToOne);
    else if (knob == 1)
        setLevel (zeroToOne);
}

int OutputModule::presetKnobCount() const
{
    return 2;
}

float OutputModule::presetKnob (int knob) const
{
    if (knob == 0)
        return mix_;
    if (knob == 1)
        return level_;
    return 0.0f;
}

void OutputModule::setMix (float zeroToOne)
{
    mix_ = clamp01 (zeroToOne);
}

void OutputModule::setLevel (float zeroToOne)
{
    level_ = clamp01 (zeroToOne);
}

void OutputModule::prepare (double rate)
{
    sampleRate = rate;
}

void OutputModule::processSample()
{
    const float wet = portValue[2];
    const float dry = 1.0f - mix_;
    hostLeft_ = (portValue[0] * dry + wet * mix_) * level_ * kVoltsToHost;
    hostRight_ = (portValue[1] * dry + wet * mix_) * level_ * kVoltsToHost;
}
