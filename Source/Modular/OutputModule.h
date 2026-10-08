// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"
#include "Smoothing.h"

// Plugin boundary. Not the headphone amplifier.
// Step 3 runs mix 0 and level 1 so a dry cable is a host-unit pass.
// Later schematic defaults are mix 1 and level 0.8 (S-23).
//
// mix_ is the blend coefficient after the effect switch. Off forces 0.
// On writes the Output Mix knob. Schematic trim and Output Level follow.
// Output Level is not preset knob 1.
// Travel 0 is silence, 0.7 (the host default) is unity, and 1 is twice as loud.
// Unity sits on the default so a dry buffer at that Level matches the input.
inline constexpr float outputLevelGain (float knob) noexcept
{
    const float travel = knob < 0.0f ? 0.0f : (knob > 1.0f ? 1.0f : knob);
    constexpr float kUnity = 0.7f;
    if (travel <= kUnity)
        return travel / kUnity;
    return 1.0f + (travel - kUnity) / (1.0f - kUnity);
}

static_assert (outputLevelGain (0.0f) == 0.0f, "output level 0 is silence");
static_assert (outputLevelGain (0.7f) == 1.0f, "output level 0.7 is unity");
static_assert (outputLevelGain (1.0f) == 2.0f, "output level 1 is twice as loud");

class OutputModule : public Module {
public:
    static constexpr float kHostToVolts = jcs::hostToVolts (1.0f);   // JCS R1: 5 V per host unit
    static constexpr float kVoltsToHost = jcs::voltsToHost (1.0f);   // JCS R1 / S-01: 0.2

    int numPorts() const override;
    PortDesc port (int index) const override;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

    void setMix (float zeroToOne);
    void setLevel (float zeroToOne);
    void setOutputLevel (float zeroToOne);
    float outputLevel() const noexcept { return outputLevel_; }

    float hostLeft() const { return hostLeft_; }
    float hostRight() const { return hostRight_; }

private:
    static float clamp01 (float value);

    float mix_ = 0.0f;
    float level_ = 1.0f;
    float outputLevel_ = 0.7f;
    float ampGain_ = outputLevelGain (0.7f);
    KnobSmoother mixSmooth_ { 0.0f };                                   // §3.5: 10 ms per-sample ramp
    KnobSmoother gainSmooth_ { outputLevelGain (0.7f) * kVoltsToHost }; // level * amp gain * volts->host
    float hostLeft_ = 0.0f;
    float hostRight_ = 0.0f;
};
