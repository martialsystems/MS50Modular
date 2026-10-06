// STAND-IN step 8, replaced in step 20
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Vcf.h"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

void flushState (double& state)
{
    if (! std::isfinite (state) || (state < 1.0e-15 && state > -1.0e-15))
        state = 0.0;
}

}

float Vcf::clamp01 (float value)
{
    if (value < 0.0f)
        return 0.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

int Vcf::numPorts() const
{
    return 3;
}

PortDesc Vcf::port (int index) const
{
    if (index == kSigIn)
        return { "SigIn", PortType::Audio, PortDir::In };
    if (index == kCutoff)
        return { "Cutoff", PortType::CV, PortDir::In };
    return { "SigOut", PortType::Audio, PortDir::Out };
}

int Vcf::numKnobs() const
{
    return 3;
}

void Vcf::setKnob (int knob, float zeroToOne)
{
    const float value = clamp01 (zeroToOne);
    if (knob == kKnobCutoff)
        cutoff01_ = value;
    else if (knob == kKnobPeak)
        peak01_ = value;
    else if (knob == kKnobAmount)
        amount01_ = value;
}

void Vcf::prepare (double rate)
{
    sampleRate = rate;
    z1_ = 0.0;
    z2_ = 0.0;
}

float Vcf::knobHz() const
{
    // S-07: 20 Hz at knob 0, 18 kHz at knob 1. value = min * pow(max / min, knob).
    return 20.0f * std::pow (900.0f, cutoff01_);
}

float Vcf::cutoffHz() const
{
    // S-08: cv = jack volts * amount. ±5 V maps to ±4 octaves around the knob.
    const float cv = portValue[kCutoff] * amount01_;
    float hz = knobHz() * std::pow (2.0f, (cv / 5.0f) * 4.0f);
    if (! std::isfinite (hz))
        return 15.0f;
    if (hz < 15.0f)
        return 15.0f;
    if (hz > 20000.0f)
        return 20000.0f;
    return hz;
}

float Vcf::resonance() const
{
    // S-09: Q = 0.5 + peak * 7.5, and Q stays at or below 8.
    float q = 0.5f + peak01_ * 7.5f;
    if (q < 0.5f)
        return 0.5f;
    if (q > 8.0f)
        return 8.0f;
    return q;
}

void Vcf::processSample()
{
    const float input = portValue[kSigIn];
    const float hz = cutoffHz();
    const float q = resonance();
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;

    double omega = 2.0 * kPi * static_cast<double> (hz) / rate;
    if (omega < 1.0e-4)
        omega = 1.0e-4;
    if (omega > kPi * 0.99)
        omega = kPi * 0.99;

    const double cosine = std::cos (omega);
    const double sine = std::sin (omega);
    const double alpha = sine / (2.0 * static_cast<double> (q));
    const double a0 = 1.0 + alpha;
    const double b0 = ((1.0 - cosine) * 0.5) / a0;
    const double b1 = (1.0 - cosine) / a0;
    const double b2 = b0;
    const double a1 = (-2.0 * cosine) / a0;
    const double a2 = (1.0 - alpha) / a0;

    const double x = static_cast<double> (input);
    const double y = b0 * x + z1_;
    z1_ = b1 * x - a1 * y + z2_;
    z2_ = b2 * x - a2 * y;
    flushState (z1_);
    flushState (z2_);

    float output = static_cast<float> (y);
    if (! std::isfinite (output))
        output = 0.0f;
    portValue[kSigOut] = output;
}
