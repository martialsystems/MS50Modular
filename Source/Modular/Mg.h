// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-16 modulation generator. Not the VCO: levels are ±2.5 V and the pulse is unipolar.
class MgModule : public Module {
public:
    static constexpr int kFreqMod = 0;
    static constexpr int kPwm = 1;
    static constexpr int kTri = 2;
    static constexpr int kSawUp = 3;
    static constexpr int kSawDown = 4;
    static constexpr int kPulse = 5;
    static constexpr int kKnobFrequency = 0;
    static constexpr int kKnobPw = 1;

    MgModule();

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    static float clamp01 (float value);
    static float clampf (float value, float low, float high);
    float knobHz() const;
    static float morph (float phase, float sym);

    // Class default is 5 Hz. The faceplate host default is a different travel.
    float freq01_ = 0.0f;
    float pw01_ = 0.5f;
    double phase_ = 0.0;
};
