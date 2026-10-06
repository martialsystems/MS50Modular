// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Eg1.h"

#include <cmath>

namespace {

constexpr float kHeldBelow = 1.5f;
constexpr float kAttackDone = 4.99f;
constexpr float kSettle = 0.01f;
constexpr float kSpan = 5.0f;

void flushState (float& state)
{
    if (! std::isfinite (state) || (state < 1.0e-15f && state > -1.0e-15f))
        state = 0.0f;
}

}

Eg1::Eg1()
{
    attack01_ = knobForSeconds (0.01f);
    decay01_ = knobForSeconds (0.25f);
    sustain01_ = 0.6f;
    release01_ = knobForSeconds (0.30f);
}

float Eg1::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float Eg1::knobForSeconds (float seconds)
{
    return std::log (seconds / 0.001f) / std::log (10000.0f);
}

float Eg1::secondsFor (float knob01) const
{
    return 0.001f * std::pow (10000.0f, clamp01 (knob01));
}

int Eg1::numPorts() const
{
    return 4;
}

PortDesc Eg1::port (int index) const
{
    if (index == kTrig)
        return { "Trig", PortType::CV, PortDir::In, 5.0f };
    if (index == kOutA)
        return { "OutA", PortType::CV, PortDir::Out };
    if (index == kOutB)
        return { "OutB", PortType::CV, PortDir::Out };
    return { "OutC", PortType::CV, PortDir::Out };
}

int Eg1::numKnobs() const
{
    return 4;
}

void Eg1::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    if (knob == kKnobAttack)
        attack01_ = value;
    else if (knob == kKnobDecay)
        decay01_ = value;
    else if (knob == kKnobSustain)
        sustain01_ = value;
    else if (knob == kKnobRelease)
        release01_ = value;
}

void Eg1::prepare (double rate)
{
    sampleRate = rate;
    outA_ = 0.0f;
    wasHeld_ = false;
    stage_ = Stage::Idle;
}

void Eg1::follow (float target, float seconds)
{
    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    const float tau = seconds > 1.0e-6f ? seconds : 1.0e-6f;
    const float coeff = 1.0f - std::exp (-1.0f / (tau * rate));
    outA_ += (target - outA_) * coeff;
    flushState (outA_);
}

void Eg1::processSample()
{
    const bool held = portValue[kTrig] < kHeldBelow;
    const bool rising = held && ! wasHeld_;
    wasHeld_ = held;

    if (rising)
        stage_ = Stage::Attack;
    else if (! held && stage_ != Stage::Idle && stage_ != Stage::Release)
        stage_ = Stage::Release;

    const float sustainVolts = sustain01_ * kSpan;

    if (stage_ == Stage::Attack)
    {
        follow (kSpan, secondsFor (attack01_));
        if (outA_ >= kAttackDone)
            stage_ = Stage::Decay;
    }
    else if (stage_ == Stage::Decay)
    {
        follow (sustainVolts, secondsFor (decay01_));
        if (std::fabs (outA_ - sustainVolts) <= kSettle)
        {
            outA_ = sustainVolts;
            stage_ = Stage::Sustain;
        }
    }
    else if (stage_ == Stage::Sustain)
    {
        outA_ = sustainVolts;
    }
    else if (stage_ == Stage::Release)
    {
        follow (0.0f, secondsFor (release01_));
        if (outA_ <= kSettle)
        {
            outA_ = 0.0f;
            stage_ = Stage::Idle;
        }
    }

    if (! std::isfinite (outA_))
    {
        outA_ = 0.0f;
        stage_ = Stage::Idle;
    }

    portValue[kOutA] = outA_;
    portValue[kOutB] = -outA_;
    portValue[kOutC] = outA_ - sustainVolts;
}
