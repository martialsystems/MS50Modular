// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Jcs.h"
#include "Module.h"

// Sample and hold. Ext Clock wins when it is patched. Otherwise the rate knob
// runs the internal clock. Clock Out follows whichever clock won.
class SampleHold : public Module {
public:
    static constexpr int kIn = 0;
    static constexpr int kOut = 1;
    static constexpr int kExtClock = 2;
    static constexpr int kClockOut = 3;
    static constexpr int kKnobRate = 0;

    SampleHold();
    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    float rate01_;
    float held_ = 0.0f;
    float lastExt_ = 0.0f;
    jcs::Schmitt extClock_;   // §3.7: JCS R3 Schmitt, high > 1.0 V, low < 0.5 V
    double phase_ = 0.5;
    bool clockHigh_ = false;
};
