// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Eg1.h"
#include "EgLaw.h"

#include <cmath>
#include <functional>

Eg1::Eg1()
{
    attack01_ = static_cast<float> (EgLaw::knobForSeconds (0.01));
    decay01_ = static_cast<float> (EgLaw::knobForSeconds (0.25));
    sustain01_ = 0.6f;
    release01_ = static_cast<float> (EgLaw::knobForSeconds (0.30));
    updateCoefficients();
}

float Eg1::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

double Eg1::secondsForKnob (float knob01)
{
    return EgLaw::secondsFor (static_cast<double> (knob01));
}

int Eg1::numPorts() const
{
    return 4;
}

PortDesc Eg1::port (int index) const
{
    // JCS R3s: the only S-trig input type. Unpatched rest +5 V (released, S-15 KEPT).
    if (index == kTrig)
        return { "Trig", PortType::CV, PortDir::In, jcs::kStrigRest, false, true, PortRole::STrig };
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

void Eg1::updateCoefficients()
{
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    aAttack_ = EgLaw::coefficient (EgLaw::attackC(), EgLaw::secondsFor (static_cast<double> (attack01_)), rate);
    aDecay_ = EgLaw::coefficient (EgLaw::decayC(), EgLaw::secondsFor (static_cast<double> (decay01_)), rate);
    aRelease_ = EgLaw::coefficient (EgLaw::decayC(), EgLaw::secondsFor (static_cast<double> (release01_)), rate);
    aSustain_ = EgLaw::coefficient (1.0, EgLaw::kSustainSlewSeconds, rate);
}

void Eg1::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    float* slot = nullptr;
    if (knob == kKnobAttack)
        slot = &attack01_;
    else if (knob == kKnobDecay)
        slot = &decay01_;
    else if (knob == kKnobSustain)
        slot = &sustain01_;
    else if (knob == kKnobRelease)
        slot = &release01_;
    if (slot == nullptr || std::equal_to<float>{} (*slot, value))   // exact: change detection only
        return;
    *slot = value;
    // Coefficients change only when a knob moves (the processor pushes knobs once per block).
    if (knob != kKnobSustain)
        updateCoefficients();
}

int Eg1::presetKnobCount() const
{
    return 4;
}

float Eg1::presetKnob (int knob) const
{
    if (knob == kKnobAttack)
        return attack01_;
    if (knob == kKnobDecay)
        return decay01_;
    if (knob == kKnobSustain)
        return sustain01_;
    if (knob == kKnobRelease)
        return release01_;
    return 0.0f;
}

void Eg1::prepare (double rate)
{
    sampleRate = rate;
    x_ = 0.0;
    trig_.reset (false);
    stage_ = Stage::Idle;
    updateCoefficients();
    stageView_.store (0, std::memory_order_relaxed);
    heldView_.store (false, std::memory_order_relaxed);
}

void Eg1::processSample()
{
    const float v = std::isfinite (portValue[kTrig]) ? portValue[kTrig] : jcs::kStrigRest;
    const jcs::Edge edge = trig_.process (v);
    const bool held = trig_.held;

    // S-15: a new trigger restarts the attack from the current level.
    if (edge == jcs::Edge::Rising)
        stage_ = Stage::Attack;
    else if (! held && stage_ != Stage::Idle && stage_ != Stage::Release)
        stage_ = Stage::Release;

    const double sustainVolts = static_cast<double> (sustain01_) * EgLaw::kPeak;

    switch (stage_)
    {
        case Stage::Attack:
            x_ += (EgLaw::kAttackTarget - x_) * aAttack_;
            if (x_ >= EgLaw::kPeak)
            {
                x_ = EgLaw::kPeak;
                stage_ = Stage::Decay;
            }
            break;
        case Stage::Decay:
        {
            // Distance to target, in double.
            const double e = (x_ - sustainVolts) * (1.0 - aDecay_);
            x_ = sustainVolts + e;
            if (std::fabs (e) < EgLaw::kSnap)
            {
                x_ = sustainVolts;
                stage_ = Stage::Sustain;
            }
            break;
        }
        case Stage::Sustain:
            // A moving S knob does not step: 5 ms one-pole toward 5 S.
            x_ += (sustainVolts - x_) * aSustain_;
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

    const float outA = static_cast<float> (x_);
    portValue[kOutA] = outA;
    portValue[kOutB] = -outA;
    portValue[kOutC] = static_cast<float> (x_ - sustainVolts);
    stageView_.store (static_cast<int> (stage_), std::memory_order_relaxed);
    heldView_.store (held, std::memory_order_relaxed);
}
