// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-20. One-pole lag. Not a glide stage inside the VCO.
class Integrator : public Module {
public:
    static constexpr int kIn = 0;
    static constexpr int kOut = 1;
    static constexpr int kKnobTime = 0;

    Integrator();
    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    // Schematic default is 50 ms. The faceplate host value is applied by the processor.
    float time01_;
    float state_ = 0.0f;
};
