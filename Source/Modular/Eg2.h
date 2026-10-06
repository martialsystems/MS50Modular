// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-26. Hold, delay, attack, release. No decay and no sustain. Not EG 1.
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

private:
    enum class Stage { Idle, Wait, Attack, Release };

    static float clamp01 (float value);
    float secondsFor (float knob01) const;
    void follow (float target, float seconds);

    float hold01_ = 0.0f;
    float delay01_ = 0.0f;
    float attack01_ = 0.0f;
    float release01_ = 0.0f;
    float out_ = 0.0f;
    double elapsed_ = 0.0;
    int delayLeft_ = 0;
    bool delayArmed_ = false;
    bool wasHeld_ = false;
    Stage stage_ = Stage::Idle;
};
