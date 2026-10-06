// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Divider.h"

#include <cmath>

namespace {

// S-18. High at +0.5 V, low at +0.3 V. A ±5 V pulse crosses this window.
constexpr float kSchmittHigh = 0.5f;
constexpr float kSchmittLow = 0.3f;
constexpr float kHighVolts = 5.0f;

}

int Divider::numPorts() const
{
    return 3;
}

PortDesc Divider::port (int index) const
{
    if (index == kIn)
        return { "In", PortType::CV, PortDir::In };
    if (index == kDiv2)
        return { "Div2", PortType::CV, PortDir::Out };
    return { "Div4", PortType::CV, PortDir::Out };
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
    div2High_ = false;
    div4High_ = false;
}

void Divider::processSample()
{
    const float input = portValue[kIn];
    const bool wasHigh = schmittHigh_;
    if (std::isfinite (input))
    {
        if (! schmittHigh_ && input >= kSchmittHigh)
            schmittHigh_ = true;
        else if (schmittHigh_ && input <= kSchmittLow)
            schmittHigh_ = false;
    }

    if (schmittHigh_ && ! wasHigh)
    {
        const bool wasDiv2 = div2High_;
        div2High_ = ! div2High_;
        if (div2High_ && ! wasDiv2)
            div4High_ = ! div4High_;
    }

    portValue[kDiv2] = div2High_ ? kHighVolts : 0.0f;
    portValue[kDiv4] = div4High_ ? kHighVolts : 0.0f;
}
