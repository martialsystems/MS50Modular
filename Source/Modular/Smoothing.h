// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// RONIN_Redesign §3.5 (N8): per-sample knob smoothing. JUCE-free, no allocation.
// applyHostControls() sets targets once per block; the module ramps per sample with a one-pole.
// The first target after prepare() (or a target set before prepare()) is taken at once, so a fresh
// instance and a recalled patch start on their values instead of ramping up from zero.

#pragma once

#include <cmath>

class KnobSmoother
{
public:
    static constexpr double kGainSeconds = 0.010;     // gain-type knobs, 10 ms
    static constexpr double kCutoffSeconds = 0.005;   // VCF cutoff knob in the log domain, 5 ms

    explicit KnobSmoother (float initial = 0.0f, double seconds = kGainSeconds) noexcept
        : value_ (static_cast<double> (initial)), target_ (initial), seconds_ (seconds) {}

    void prepare (double sampleRate) noexcept
    {
        const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
        coeff_ = -std::expm1 (-1.0 / (seconds_ * rate));
        value_ = static_cast<double> (target_);
        fresh_ = true;
    }

    void setTarget (float target) noexcept
    {
        target_ = target;
        if (fresh_)
            value_ = static_cast<double> (target);
    }

    void snap (float value) noexcept
    {
        target_ = value;
        value_ = static_cast<double> (value);
    }

    float next() noexcept
    {
        fresh_ = false;
        // Double state: a float one-pole stalls about ulp/coeff short of its target (the N1 EG defect).
        value_ += (static_cast<double> (target_) - value_) * coeff_;
        if (std::fabs (static_cast<double> (target_) - value_) < 1.0e-6)
            value_ = static_cast<double> (target_);
        return static_cast<float> (value_);
    }

    float target() const noexcept { return target_; }
    float current() const noexcept { return static_cast<float> (value_); }

private:
    double value_;
    float target_;
    double seconds_;
    double coeff_ = 1.0;
    bool fresh_ = true;
};
