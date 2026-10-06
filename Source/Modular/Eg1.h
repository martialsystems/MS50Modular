// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-13, S-14, S-15. ADSR with three outputs. Separate from EG 2.
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

    Eg1();

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    static float clamp01 (float value);
    static float knobForSeconds (float seconds);
    float secondsFor (float knob01) const;
    void follow (float target, float seconds);

    // Schematic defaults: attack 0.01 s, decay 0.25 s, sustain 0.6, release 0.30 s.
    float attack01_ = 0.25f;
    float decay01_ = 0.59948425f;
    float sustain01_ = 0.6f;
    float release01_ = 0.61928095f;
    float outA_ = 0.0f;
    bool wasHeld_ = false;
    Stage stage_ = Stage::Idle;
};
