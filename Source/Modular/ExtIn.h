// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// Plugin boundary. Not an MS-50 module. S-01: host ±1 maps to ±5 V.
class ExtIn : public Module {
public:
    static constexpr float kHostToVolts = 5.0f;
    static constexpr float kVoltsToHost = 0.2f;

    int numPorts() const override;
    PortDesc port (int index) const override;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

    void setHostSample (float leftUnit, float rightUnit);

private:
    float inLVolts_ = 0.0f;
    float inRVolts_ = 0.0f;
};
