// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

#include <atomic>

// S-13, S-14, S-15. ADSR with three outputs. Separate from EG 2.
// RONIN_Redesign §3.1: double state, real-time labels (1 ms .. 60 s), 6 V attack target, -134 dB snap,
// S-trig input with JCS R3s hysteresis (held below 1.0 V, released above 1.5 V).
class Eg1 : public Module {
public:
    static constexpr int kTrig = 0;
    static constexpr int kOutA = 1;
    static constexpr int kOutB = 2;
    static constexpr int kOutC = 3;
    static constexpr int kKnobAttack = 0;
    static constexpr int kKnobDecay = 1;
    static constexpr int kKnobSustain = 2;
    static constexpr int kKnobRelease = 3;

    enum class Stage { Idle = 0, Attack, Decay, Sustain, Release };

    Eg1();

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

    // UI readouts (ENV tab). Relaxed atomics: no lock on the audio thread.
    Stage stage() const noexcept { return static_cast<Stage> (stageView_.load (std::memory_order_relaxed)); }
    bool triggerHeld() const noexcept { return heldView_.load (std::memory_order_relaxed); }
    static double secondsForKnob (float knob01);

private:
    static float clamp01 (float value);
    void updateCoefficients();

    // Schematic defaults: attack 0.01 s, decay 0.25 s, sustain 0.6, release 0.30 s (real times).
    float attack01_ = 0.0f;
    float decay01_ = 0.0f;
    float sustain01_ = 0.6f;
    float release01_ = 0.0f;
    double x_ = 0.0;
    double aAttack_ = 1.0;
    double aDecay_ = 1.0;
    double aRelease_ = 1.0;
    double aSustain_ = 1.0;
    jcs::StrigDetector trig_;
    Stage stage_ = Stage::Idle;
    std::atomic<int> stageView_ { 0 };
    std::atomic<bool> heldView_ { false };
};
