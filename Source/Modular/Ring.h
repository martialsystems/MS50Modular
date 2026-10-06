// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-21. Four-quadrant product. Not the VCF diode bridge.
class Ring : public Module {
public:
    static constexpr int kA = 0;
    static constexpr int kB = 1;
    static constexpr int kOut = 2;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;
};
