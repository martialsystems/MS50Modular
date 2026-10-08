// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Eg2.h"
#include "EgLaw.h"

#include <cmath>
#include <functional>

namespace {

float holdKnobForSeconds (double seconds)
{
    return static_cast<float> (std::log (seconds / 0.001) / std::log (10000.0));
}

}

Eg2::Eg2()
{
    hold01_ = holdKnobForSeconds (0.001);
    delay01_ = holdKnobForSeconds (0.001);
    attack01_ = static_cast<float> (EgLaw::knobForSeconds (0.02));
    release01_ = static_cast<float> (EgLaw::knobForSeconds (0.20));
    updateCoefficients();
}

float Eg2::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

void Eg2::updateCoefficients()
{
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    aAttack_ = EgLaw::coefficient (EgLaw::attackC(), EgLaw::secondsFor (static_cast<double> (attack01_)), rate);
    aRelease_ = EgLaw::coefficient (EgLaw::decayC(), EgLaw::secondsFor (static_cast<double> (release01_)), rate);
}

int Eg2::numPorts() const
{
    return 4;
}

PortDesc Eg2::port (int index) const
{
    if (index == kTrig)
        return { "Trig", PortType::CV, PortDir::In, jcs::kStrigRest, false, true, PortRole::STrig };
    if (index == kOutPos)
        return { "OutPos", PortType::CV, PortDir::Out };
    if (index == kOutNeg)
        return { "OutNeg", PortType::CV, PortDir::Out };
    return { "DelayTrig", PortType::Gate, PortDir::Out, 0.0f, false, false, PortRole::GateClk };
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
    else if (knob == kKnobAttack || knob == kKnobRelease)
    {
        float& slot = knob == kKnobAttack ? attack01_ : release01_;
        if (std::equal_to<float>{} (slot, value))   // exact: change detection only
            return;
        slot = value;
        updateCoefficients();
    }
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
    x_ = 0.0;
    elapsed_ = 0.0;
    delayLeft_ = 0;
    delayArmed_ = false;
    trig_.reset (false);
    stage_ = Stage::Idle;
    updateCoefficients();
    stageView_.store (0, std::memory_order_relaxed);
    heldView_.store (false, std::memory_order_relaxed);
}

void Eg2::processSample()
{
    const float v = std::isfinite (portValue[kTrig]) ? portValue[kTrig] : jcs::kStrigRest;
    const bool rising = trig_.process (v) == jcs::Edge::Rising;

    if (rising)
    {
        stage_ = Stage::Wait;
        x_ = 0.0;
        elapsed_ = 0.0;
        delayLeft_ = 0;
        delayArmed_ = false;
    }

    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    switch (stage_)
    {
        case Stage::Wait:
        {
            x_ = 0.0;
            elapsed_ += 1.0 / rate;
            const double hold = EgLaw::holdDelaySeconds (static_cast<double> (hold01_));
            const double delay = EgLaw::holdDelaySeconds (static_cast<double> (delay01_));
            if (! delayArmed_ && elapsed_ >= hold)
            {
                delayArmed_ = true;
                delayLeft_ = jcs::triggerPulseSamples (rate);   // JCS R2: >= 1 ms
            }
            if (elapsed_ >= hold + delay)
                stage_ = Stage::Attack;
            break;
        }
        case Stage::Attack:
            x_ += (EgLaw::kAttackTarget - x_) * aAttack_;
            if (x_ >= EgLaw::kPeak)
            {
                x_ = EgLaw::kPeak;
                stage_ = Stage::Release;   // no plateau (S-26 KEPT)
            }
            break;
        case Stage::Release:
            x_ *= 1.0 - aRelease_;
            if (x_ < EgLaw::kSnap)
            {
                x_ = 0.0;
                stage_ = Stage::Idle;
            }
            break;
        case Stage::Idle:
            break;
    }

    if (! std::isfinite (x_))
    {
        x_ = 0.0;
        stage_ = Stage::Idle;
    }

    const float out = static_cast<float> (x_);
    portValue[kOutPos] = out;
    portValue[kOutNeg] = -out;
    if (delayLeft_ > 0)
    {
        portValue[kDelayTrig] = jcs::kGateHigh;   // §5.11 (N11): a JCS R2 5 V pulse (was 1.0 V)
        --delayLeft_;
    }
    else
    {
        portValue[kDelayTrig] = jcs::kGateLow;
    }
    stageView_.store (static_cast<int> (stage_), std::memory_order_relaxed);
    heldView_.store (trig_.held, std::memory_order_relaxed);
}
