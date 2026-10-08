// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Mixer.h"

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

}

Mixer::Mixer()
{
    level_[0] = 0.8f;
    level_[1] = 0.8f;
    level_[2] = 0.8f;
}

int Mixer::numPorts() const
{
    return 4;
}

PortDesc Mixer::port (int index) const
{
    if (index == kIn1)
        return { "In 1", PortType::Audio, PortDir::In };
    if (index == kIn2)
        return { "In 2", PortType::Audio, PortDir::In };
    if (index == kIn3)
        return { "In 3", PortType::Audio, PortDir::In };
    return { "Out", PortType::Audio, PortDir::Out };
}

int Mixer::numKnobs() const
{
    return 3;
}

void Mixer::setKnob (int knob, float zeroToOne)
{
    if (knob >= kKnobLevel1 && knob <= kKnobLevel3)
    {
        level_[knob] = clamp01 (zeroToOne);
        levelSmooth_[knob].setTarget (level_[knob]);
    }
}

int Mixer::presetKnobCount() const
{
    return 3;
}

float Mixer::presetKnob (int knob) const
{
    if (knob >= kKnobLevel1 && knob <= kKnobLevel3)
        return level_[knob];
    return 0.0f;
}

void Mixer::prepare (double rate)
{
    sampleRate = rate;
    for (auto& smooth : levelSmooth_)
        smooth.prepare (rate);
}

void Mixer::processSample()
{
    float sum = 0.0f;
    const float input[3] = { portValue[kIn1], portValue[kIn2], portValue[kIn3] };
    for (int i = 0; i < 3; ++i)
    {
        const float level = levelSmooth_[i].next();
        if (level == 0.0f)
            continue;
        const float sample = std::isfinite (input[i]) ? input[i] : 0.0f;
        sum += sample * level;
    }
    const float inverted = -sum;
    portValue[kOut] = std::isfinite (inverted) ? inverted : 0.0f;
}
