// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-18. Clock divider, /2 and /4 only. The panel /16 hole is not a port.
class Divider : public Module {
public:
    static constexpr int kIn = 0;
    static constexpr int kDiv2 = 1;
    static constexpr int kDiv4 = 2;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    bool schmittHigh_ = false;
    bool div2High_ = false;
    bool div4High_ = false;
};
