// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"
#include "Smoothing.h"

#include <atomic>
#include <cmath>

// Step 20 diode-bridge stand-in. Knob ids stay on Cutoff, Peak, and Amount.
class Vcf : public Module {
public:
    static constexpr const char* kStandIn = "step 20, S-10";
    static constexpr int kSigIn = 0;
    static constexpr int kCutoff = 1;
    static constexpr int kSigOut = 2;
    static constexpr int kKnobCutoff = 0;
    static constexpr int kKnobPeak = 1;
    static constexpr int kKnobAmount = 2;
    // Drive pull: input level (30 ms mean |x| follower) subtracts pull*env volts from the bridge bias.
    static constexpr double kInputPull = 0.004;          // §5.13b, was 0.012 (format-1 sound)
    static constexpr double kLegacyInputPull = 0.012;

    // The bias law without the 0.45 fs clamp, for the M-R5 migration and the VOICE tab readout.
    static double effectiveHzFor (double knobHz, double envVolts, double pull) noexcept;
    static double knobHzFor (double knob01) noexcept { return 20.0 * std::pow (900.0, knob01); }

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

    // Effective cutoff of the last sample (after CV, smoothing and drive pull). Read by the UI and tests.
    float effectiveHz() const noexcept { return effectiveHz_.load (std::memory_order_relaxed); }

private:
    static float clamp01 (float value);
    float knobHz() const;
    float cutoffHz() const;
    float resonance() const;

    // Schematic defaults (S-07, S-09, S-08). The processor then pushes the faceplate.
    float cutoff01_ = 0.55f;
    // §3.5: knobHz = 20*900^k is exponential in k, so a linear ramp of k is a log-domain ramp of Hz.
    KnobSmoother cutoffSmooth_ { 0.55f, KnobSmoother::kCutoffSeconds };
    float peak01_ = 0.15f;
    float amount01_ = 0.4f;
    double z1_ = 0.0;
    double z2_ = 0.0;
    double env_ = 0.0;
    double hpX_ = 0.0;
    double hpY_ = 0.0;
    double envA_ = -std::expm1 (-1.0 / (0.03 * 48000.0));            // set again in prepare()
    double hpA_ = std::exp (-2.0 * 3.14159265358979323846 * 5.0 / 48000.0);
    std::atomic<float> effectiveHz_ { 0.0f };
};
