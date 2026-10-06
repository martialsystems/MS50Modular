// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-12. Separate from VCA 1. No knobs, and no low-cut.
class Vca2 : public Module {
public:
    static constexpr int kIn = 0;
    static constexpr int kControl = 1;
    static constexpr int kOut = 2;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    float gain_ = 0.0f;
};
