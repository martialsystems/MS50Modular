// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Eg2.h"

#include <cmath>

namespace {

constexpr float kHeldBelow = 1.5f;
constexpr float kAttackDone = 4.99f;
constexpr float kSettle = 0.01f;
constexpr float kSpan = 5.0f;

float knobForSeconds (float seconds)
{
    return std::log (seconds / 0.001f) / std::log (10000.0f);
}

}

Eg2::Eg2()
{
    hold01_ = knobForSeconds (0.001f);
    delay01_ = knobForSeconds (0.001f);
    attack01_ = knobForSeconds (0.02f);
    release01_ = knobForSeconds (0.20f);
}

float Eg2::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float Eg2::secondsFor (float knob01) const
{
    return 0.001f * std::pow (10000.0f, clamp01 (knob01));
}

void Eg2::follow (float target, float seconds)
{
    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    const float tau = seconds > 1.0e-6f ? seconds : 1.0e-6f;
    const float coeff = 1.0f - std::exp (-1.0f / (tau * rate));
    out_ += (target - out_) * coeff;
    if (! std::isfinite (out_) || (out_ < 1.0e-15f && out_ > -1.0e-15f))
        out_ = 0.0f;
}

int Eg2::numPorts() const
{
    return 4;
}

PortDesc Eg2::port (int index) const
{
    if (index == kTrig)
        return { "Trig", PortType::CV, PortDir::In, 5.0f };
    if (index == kOutPos)
        return { "OutPos", PortType::CV, PortDir::Out };
    if (index == kOutNeg)
        return { "OutNeg", PortType::CV, PortDir::Out };
    return { "DelayTrig", PortType::Gate, PortDir::Out };
}

int Eg2::numKnobs() const
{
    return 4;
}

void Eg2::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    if (knob == kKnobHold)
        hold01_ = value;
    else if (knob == kKnobDelay)
        delay01_ = value;
    else if (knob == kKnobAttack)
        attack01_ = value;
    else if (knob == kKnobRelease)
        release01_ = value;
}

int Eg2::presetKnobCount() const
{
    return 4;
}

float Eg2::presetKnob (int knob) const
{
    if (knob == kKnobHold)
        return hold01_;
    if (knob == kKnobDelay)
        return delay01_;
    if (knob == kKnobAttack)
        return attack01_;
    if (knob == kKnobRelease)
        return release01_;
    return 0.0f;
}

void Eg2::prepare (double rate)
{
    sampleRate = rate;
    out_ = 0.0f;
    elapsed_ = 0.0;
    delayLeft_ = 0;
    delayArmed_ = false;
    wasHeld_ = false;
    stage_ = Stage::Idle;
}

void Eg2::processSample()
{
    const bool held = portValue[kTrig] < kHeldBelow;
    const bool rising = held && ! wasHeld_;
    wasHeld_ = held;

    if (rising)
    {
        stage_ = Stage::Wait;
        out_ = 0.0f;
        elapsed_ = 0.0;
        delayLeft_ = 0;
        delayArmed_ = false;
    }

    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    if (stage_ == Stage::Wait)
    {
        out_ = 0.0f;
        elapsed_ += 1.0 / static_cast<double> (rate);
        const double hold = static_cast<double> (secondsFor (hold01_));
        const double delay = static_cast<double> (secondsFor (delay01_));
        if (! delayArmed_ && elapsed_ >= hold)
        {
            delayArmed_ = true;
            const int width = static_cast<int> (std::lround (0.001 * static_cast<double> (rate)));
            delayLeft_ = width > 1 ? width : 1;
        }
        if (elapsed_ >= hold + delay)
            stage_ = Stage::Attack;
    }
    else if (stage_ == Stage::Attack)
    {
        follow (kSpan, secondsFor (attack01_));
        if (out_ >= kAttackDone)
            stage_ = Stage::Release;
    }
    else if (stage_ == Stage::Release)
    {
        follow (0.0f, secondsFor (release01_));
        if (out_ <= kSettle)
        {
            out_ = 0.0f;
            stage_ = Stage::Idle;
        }
    }

    portValue[kOutPos] = out_;
    portValue[kOutNeg] = -out_;
    if (delayLeft_ > 0)
    {
        portValue[kDelayTrig] = 1.0f;
        --delayLeft_;
    }
    else
    {
        portValue[kDelayTrig] = 0.0f;
    }
}
