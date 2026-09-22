// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// Plugin boundary. Not the headphone amplifier.
// Step 3 runs mix 0 and level 1 so a dry cable is a host-unit pass.
// Later schematic defaults are mix 1 and level 0.8 (S-23).
class OutputModule : public Module {
public:
    static constexpr float kHostToVolts = 5.0f;
    static constexpr float kVoltsToHost = 0.2f;

    int numPorts() const override;
    PortDesc port (int index) const override;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

    void setMix (float zeroToOne);
    void setLevel (float zeroToOne);

    float hostLeft() const { return hostLeft_; }
    float hostRight() const { return hostRight_; }

private:
    static float clamp01 (float value);

    float mix_ = 0.0f;
    float level_ = 1.0f;
    float hostLeft_ = 0.0f;
    float hostRight_ = 0.0f;
};
