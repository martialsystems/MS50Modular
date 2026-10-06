// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-19. Unity inversion. Offset trim stays at 0. No panel knob.
class Inverter : public Module {
public:
    static constexpr int kIn = 0;
    static constexpr int kOut = 1;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;
};
