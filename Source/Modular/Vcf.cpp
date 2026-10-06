// step 20, S-10
// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// PAPER-SUBSTITUTE: Is = 2.52e-9, n = 1.752. Those are the DAFx 1N4148 fit, not a CA3019 measurement.
// D5 to D12: tanh on the feedback state. S-10b: 5 Hz one-pole highpass on the output.

#include "Vcf.h"

#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kIs = 2.52e-9;
constexpr double kN = 1.752;
constexpr double kVt = 0.02585;
constexpr double kBridgeC = 22.0e-9;
constexpr double kInputPull = 0.012;
constexpr double kDiodeVolts = 5.0;

void flushState (double& state)
{
    if (! std::isfinite (state) || (state < 1.0e-15 && state > -1.0e-15))
        state = 0.0;
}

double biasForHz (double hz)
{
    const double nVt = kN * kVt;
    const double ib = hz * (2.0 * kPi * nVt * kBridgeC);
    if (ib <= 0.0)
        return 0.0;
    return nVt * std::log (ib / kIs + 1.0);
}

double hzFromBias (double vBias)
{
    const double nVt = kN * kVt;
    if (vBias < 1.0e-4)
        vBias = 1.0e-4;
    const double ib = kIs * (std::exp (vBias / nVt) - 1.0);
    if (ib <= 0.0)
        return 15.0;
    // Symmetric linearized bridge: both arms are rd = n*VT/Ib.
    const double rd = nVt / ib;
    const double r1 = rd;
    const double r2 = rd;
    return 1.0 / (2.0 * kPi * std::sqrt (r1 * r2) * kBridgeC);
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

int Vcf::presetKnobCount() const
{
    return 3;
}

float Vcf::presetKnob (int knob) const
{
    if (knob == kKnobCutoff)
        return cutoff01_;
    if (knob == kKnobPeak)
        return peak01_;
    if (knob == kKnobAmount)
        return amount01_;
    return 0.0f;
}

void Vcf::prepare (double rate)
{
    sampleRate = rate;
    z1_ = 0.0;
    z2_ = 0.0;
    env_ = 0.0;
    hpX_ = 0.0;
    hpY_ = 0.0;
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
    float input = portValue[kSigIn];
    if (! std::isfinite (input))
        input = 0.0f;

    const float q = resonance();
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    const double envA = 1.0 - std::exp (-1.0 / (0.03 * rate));
    env_ += (std::fabs (static_cast<double> (input)) - env_) * envA;
    flushState (env_);

    // S-07 and S-08 set the unpulled bias. S-10 turns that bias into bridge current.
    const double target = static_cast<double> (cutoffHz());
    double vBias = biasForHz (target) - kInputPull * env_;
    double hz = hzFromBias (vBias);
    if (! std::isfinite (hz) || hz < 15.0)
        hz = 15.0;
    if (hz > 20000.0)
        hz = 20000.0;
    if (hz > rate * 0.45)
        hz = rate * 0.45;

    const double g = std::tan (kPi * hz / rate);
    const double k = 1.0 / static_cast<double> (q);
    const double a1 = 1.0 / (1.0 + g * (g + k));
    const double a2 = g * a1;
    const double a3 = g * a2;

    // D5 to D12. In the linear region tanh returns the feedback state unchanged.
    const double feedback = kDiodeVolts * std::tanh (z2_ / kDiodeVolts);
    const double v3 = static_cast<double> (input) - feedback;
    const double v1 = a1 * z1_ + a2 * v3;
    const double v2 = feedback + a2 * z1_ + a3 * v3;
    z1_ = 2.0 * v1 - z1_;
    z2_ = 2.0 * v2 - feedback;
    flushState (z1_);
    flushState (z2_);

    // S-10b. Output coupling, one pole at 5 Hz.
    const double hpA = std::exp (-2.0 * kPi * 5.0 / rate);
    const double hp = hpA * (hpY_ + v2 - hpX_);
    hpX_ = v2;
    hpY_ = hp;
    flushState (hpX_);
    flushState (hpY_);

    float output = static_cast<float> (hp);
    if (! std::isfinite (output))
        output = 0.0f;
    portValue[kSigOut] = output;
}
