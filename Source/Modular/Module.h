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

    // Sample-rate-independent knob positions. Filter memory and the noise seed stay out.
    virtual int presetKnobCount() const { return 0; }
    virtual float presetKnob (int) const { return 0.0f; }
    // Factory preset load. The graph blob does not grow for a knob handled here.
    virtual void applyFactoryPreset (int) {}
    // VCO footage is 0..3. Every other module returns -1.
    virtual int presetScaleIndex() const { return -1; }

    float portValue[8] {};
    // Set by the graph each sample for input ports. Output slots stay false.
    bool inputConnected[8] {};
    double sampleRate = 48000.0;
};
