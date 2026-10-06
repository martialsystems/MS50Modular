// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// Step 8 low-pass. Step 20 replaces the cutoff law and the feedback clip.
class Vcf : public Module {
public:
    static constexpr const char* kStandIn = "STAND-IN step 8, replaced in step 20";
    static constexpr int kSigIn = 0;
    static constexpr int kCutoff = 1;
    static constexpr int kSigOut = 2;
    static constexpr int kKnobCutoff = 0;
    static constexpr int kKnobPeak = 1;
    static constexpr int kKnobAmount = 2;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    static float clamp01 (float value);
    float knobHz() const;
    float cutoffHz() const;
    float resonance() const;

    // Schematic defaults (S-07, S-09, S-08). The processor then pushes the faceplate.
    float cutoff01_ = 0.55f;
    float peak01_ = 0.15f;
    float amount01_ = 0.4f;
    double z1_ = 0.0;
    double z2_ = 0.0;
};
