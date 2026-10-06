// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-11. Separate from VCA 2. No shared mode flag.
class Vca1 : public Module {
public:
    static constexpr int kSigIn = 0;
    static constexpr int kEnv = 1;
    static constexpr int kOut = 2;
    static constexpr int kKnobLowCut = 0;
    static constexpr int kKnobIntensity = 1;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    static float clamp01 (float value);
    float lowCutHz() const;

    // Knob 0 is 10 Hz. Intensity is the schematic 0.85 until a test moves it.
    float lowCut01_ = 0.0f;
    float intensity_ = 0.85f;
    float low_ = 0.0f;
};
