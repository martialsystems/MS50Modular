// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"
#include "Smoothing.h"

// Adding amplifier. Three attenuators, inverting sum, DC path. No offset jack.
class Mixer : public Module {
public:
    static constexpr int kIn1 = 0;
    static constexpr int kIn2 = 1;
    static constexpr int kIn3 = 2;
    static constexpr int kOut = 3;
    static constexpr int kKnobLevel1 = 0;
    static constexpr int kKnobLevel2 = 1;
    static constexpr int kKnobLevel3 = 2;

    Mixer();
    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    float level_[3];
    KnobSmoother levelSmooth_[3] { KnobSmoother { 0.8f }, KnobSmoother { 0.8f }, KnobSmoother { 0.8f } };   // §3.5
};
