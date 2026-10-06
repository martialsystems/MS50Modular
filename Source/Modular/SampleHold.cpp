// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "SampleHold.h"

#include <cmath>

namespace {

float clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

// Schematic lists a rate pot and no Hertz. This stand-in is exponential,
// 0.1 Hz at knob 0 through 100 Hz at knob 1.
double internalHz (float rate01)
{
    return 0.1 * std::pow (1000.0, static_cast<double> (clamp01 (rate01)));
}

}

SampleHold::SampleHold()
    : rate01_ (0.50f)
{
}

int SampleHold::numPorts() const
{
    return 4;
}

PortDesc SampleHold::port (int index) const
{
    if (index == kIn)
        return { "In", PortType::Audio, PortDir::In };
    if (index == kExtClock)
        return { "Ext Clock", PortType::CV, PortDir::In };
    if (index == kClockOut)
        return { "Clock Out", PortType::CV, PortDir::Out };
    return { "Out", PortType::CV, PortDir::Out };
}

int SampleHold::numKnobs() const
{
    return 1;
}

void SampleHold::setKnob (int knob, float zeroToOne)
{
    if (knob == kKnobRate)
        rate01_ = clamp01 (zeroToOne);
}

int SampleHold::presetKnobCount() const
{
    return 1;
}

float SampleHold::presetKnob (int knob) const
{
    if (knob == kKnobRate)
        return rate01_;
    return 0.0f;
}

void SampleHold::prepare (double rate)
{
    sampleRate = rate;
    held_ = 0.0f;
    lastExt_ = 0.0f;
    phase_ = 0.5;
    clockHigh_ = false;
}

void SampleHold::processSample()
{
    bool high = false;
    bool rising = false;

    if (inputConnected[kExtClock])
    {
        const float ext = std::isfinite (portValue[kExtClock]) ? portValue[kExtClock] : 0.0f;
        high = ext >= 1.0f;
        rising = high && lastExt_ < 1.0f;
        lastExt_ = ext;
    }
    else
    {
        const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
        phase_ += internalHz (rate01_) / rate;
        if (phase_ >= 1.0)
            phase_ -= std::floor (phase_);
        high = phase_ < 0.5;
        rising = high && ! clockHigh_;
        lastExt_ = 0.0f;
    }

    clockHigh_ = high;
    if (rising)
    {
        const float sample = std::isfinite (portValue[kIn]) ? portValue[kIn] : 0.0f;
        held_ = sample;
    }

    portValue[kOut] = held_;
    portValue[kClockOut] = high ? 5.0f : 0.0f;
}
