// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

#include <atomic>

// Plugin boundary. Not a hardware module. S-01: host ±1 maps to ±5 V.
// S-22: absolute mono into a one-pole follower. The momentary button ORs the gate.
class ExtIn : public Module {
public:
    static constexpr float kHostToVolts = 5.0f;
    static constexpr float kVoltsToHost = 0.2f;
    static constexpr int kKnobThreshold = 0;
    static constexpr int kKnobRelease = 1;

    ExtIn();

    int numPorts() const override;
    PortDesc port (int index) const override;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

    void setHostSample (float leftUnit, float rightUnit);
    void setButtonHeld (bool held);
    bool buttonHeld() const;

private:
    static float clamp01 (float value);
    float thresholdVolts() const;
    float releaseSeconds() const;

    float inLVolts_ = 0.0f;
    float inRVolts_ = 0.0f;
    float env_ = 0.0f;
    // Schematic defaults: 0.2 V and 80 ms. Set in the constructor from those voltages.
    float threshold01_ = 0.0f;
    float release01_ = 0.0f;
    std::atomic<bool> buttonHeld_ { false };
};
