// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Divider.h"

#include <cmath>

namespace {

// RONIN_Redesign §3.7 / JCS R3: high > 1.0 V, low < 0.5 V (was S-18 0.5/0.3 V). A ±5 V pulse still
// crosses it; a 0/5 V gate now has a 0.5 V dead band against noise.
constexpr float kHighVolts = jcs::kGateHigh;

}

int Divider::numPorts() const
{
    return 3;
}

PortDesc Divider::port (int index) const
{
    if (index == kIn)
        return { "In", PortType::CV, PortDir::In, 0.0f, false, false, PortRole::GateClk };
    if (index == kDiv2)
        return { "Div2", PortType::CV, PortDir::Out, 0.0f, false, false, PortRole::GateClk };
    return { "Div4", PortType::CV, PortDir::Out, 0.0f, false, false, PortRole::GateClk };
}

int Divider::numKnobs() const
{
    return 0;
}

void Divider::setKnob (int, float)
{
}

void Divider::prepare (double rate)
{
    sampleRate = rate;
    schmittHigh_ = false;
    schmitt_.reset();
    div2High_ = false;
    div4High_ = false;
}

void Divider::processSample()
{
    const float input = portValue[kIn];
    const bool rising = std::isfinite (input) && schmitt_.rising (input);
    schmittHigh_ = schmitt_.high;

    if (rising)
    {
        const bool wasDiv2 = div2High_;
        div2High_ = ! div2High_;
        if (div2High_ && ! wasDiv2)
            div4High_ = ! div4High_;
    }

    portValue[kDiv2] = div2High_ ? kHighVolts : 0.0f;
    portValue[kDiv4] = div4High_ ? kHighVolts : 0.0f;
}
