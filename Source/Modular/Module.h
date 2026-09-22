// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Port.h"

class Module {
public:
    virtual ~Module();
    virtual int numPorts() const = 0;
    virtual PortDesc port (int index) const = 0;
    virtual void setKnob (int knob, float zeroToOne) = 0;
    virtual void prepare (double sampleRate) = 0;
    virtual void processSample() = 0;

    float portValue[8] {};
    double sampleRate = 48000.0;
};
