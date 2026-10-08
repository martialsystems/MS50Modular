// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

#include <atomic>

// S-26. Hold, delay, attack, release. No decay and no sustain. Not EG 1.
// RONIN_Redesign §3.1: the EG 1 segment laws (EgLaw.h), JCS R3s trigger, DelayTrig 5.0 V for >= 1 ms (JCS R2).
// HOLD and DELAY keep their 1 ms .. 10 s timing law.
class Eg2 : public Module {
public:
    static constexpr int kTrig = 0;
    static constexpr int kOutPos = 1;
    static constexpr int kOutNeg = 2;
    static constexpr int kDelayTrig = 3;
    static constexpr int kKnobHold = 0;
    static constexpr int kKnobDelay = 1;
    static constexpr int kKnobAttack = 2;
    static constexpr int kKnobRelease = 3;

    Eg2();

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

    enum class Stage { Idle = 0, Wait, Attack, Release };
    Stage stage() const noexcept { return static_cast<Stage> (stageView_.load (std::memory_order_relaxed)); }
    bool triggerHeld() const noexcept { return heldView_.load (std::memory_order_relaxed); }

private:
    static float clamp01 (float value);
    void updateCoefficients();

    float hold01_ = 0.0f;
    float delay01_ = 0.0f;
    float attack01_ = 0.0f;
    float release01_ = 0.0f;
    double x_ = 0.0;
    double aAttack_ = 1.0;
    double aRelease_ = 1.0;
    double elapsed_ = 0.0;
    int delayLeft_ = 0;
    bool delayArmed_ = false;
    jcs::StrigDetector trig_;
    Stage stage_ = Stage::Idle;
    std::atomic<int> stageView_ { 0 };
    std::atomic<bool> heldView_ { false };
};
