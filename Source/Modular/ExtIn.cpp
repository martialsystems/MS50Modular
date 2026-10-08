// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "ExtIn.h"

#include <cmath>

namespace {

constexpr float kAttackSeconds = 0.005f;

float knobForRatio (float ratio, float span)
{
    if (ratio < 1.0f)
        ratio = 1.0f;
    return std::log (ratio) / std::log (span);
}

}

ExtIn::ExtIn()
{
    threshold01_ = knobForRatio (0.2f / 0.05f, 40.0f);
    release01_ = knobForRatio (0.080f / 0.010f, 50.0f);
}

float ExtIn::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

float ExtIn::thresholdVolts() const
{
    return 0.05f * std::pow (40.0f, clamp01 (threshold01_));
}

float ExtIn::releaseSeconds() const
{
    return 0.010f * std::pow (50.0f, clamp01 (release01_));
}

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
    return { "Gate", PortType::Gate, PortDir::Out, 5.0f, true };
}

void ExtIn::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    if (knob == kKnobThreshold)
        threshold01_ = value;
    else if (knob == kKnobRelease)
        release01_ = value;
}

int ExtIn::presetKnobCount() const
{
    return 2;
}

float ExtIn::presetKnob (int knob) const
{
    if (knob == kKnobThreshold)
        return threshold01_;
    if (knob == kKnobRelease)
        return release01_;
    return 0.0f;
}

void ExtIn::prepare (double rate)
{
    sampleRate = rate;
    env_ = 0.0f;
    followerOpen_ = false;
}

void ExtIn::setHostSample (float leftUnit, float rightUnit)
{
    inLVolts_ = leftUnit * kHostToVolts;
    inRVolts_ = rightUnit * kHostToVolts;
}

void ExtIn::setButtonHeld (bool held)
{
    buttonHeld_.store (held, std::memory_order_release);
}

bool ExtIn::buttonHeld() const
{
    return buttonHeld_.load (std::memory_order_acquire);
}

void ExtIn::processSample()
{
    portValue[0] = inLVolts_;
    portValue[1] = inRVolts_;
    const float mono = 0.5f * (inLVolts_ + inRVolts_);
    portValue[2] = mono;

    const float level = std::fabs (mono);
    const float rate = static_cast<float> (sampleRate > 1.0 ? sampleRate : 48000.0);
    const float tau = level > env_ ? kAttackSeconds : releaseSeconds();
    const float coeff = 1.0f - std::exp (-1.0f / (tau * rate));
    env_ += (level - env_) * coeff;
    if (! std::isfinite (env_) || (env_ < 1.0e-15f && env_ > -1.0e-15f))
        env_ = 0.0f;

    // §3.8 (N15): the follower compare has hysteresis. It opens when env > th and closes when env < 0.7 th,
    // so a low bass rippling across the threshold no longer chatters the gate. The follower is KEPT.
    const float th = thresholdVolts();
    if (! followerOpen_ && env_ > th)
        followerOpen_ = true;
    else if (followerOpen_ && env_ < kCloseRatio * th)
        followerOpen_ = false;

    // S-trig. Held is 0 V. Released rests at +5 V. EG 1 treats below 1.0 V as held (JCS R3s).
    const bool open = buttonHeld() || followerOpen_;
    portValue[3] = open ? 0.0f : 5.0f;
}
