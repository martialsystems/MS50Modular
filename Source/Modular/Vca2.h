// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-12. Separate from VCA 1. No low-cut. Initial and Mod are panel knobs, not preset blob knobs.
class Vca2 : public Module {
public:
    static constexpr int kIn = 0;
    static constexpr int kControl = 1;
    static constexpr int kOut = 2;
    static constexpr int kKnobInitial = 0;
    static constexpr int kKnobMod = 1;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    // Initial 0 and Mod 1 give gain = clamp01(control / 5), the law before these knobs.
    float initial01_ = 0.0f;
    float mod_ = 1.0f;
    float gain_ = 0.0f;
};
