// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

// S-02 through S-06. Separate from MG: these outputs are ±5 V.
class Vco : public Module {
public:
    static constexpr int kHzPerVolt = 0;
    static constexpr int kOct = 1;
    static constexpr int kFreqA = 2;
    static constexpr int kFreqB = 3;
    static constexpr int kPwm = 4;
    static constexpr int kSaw = 5;
    static constexpr int kTri = 6;
    static constexpr int kPulse = 7;
    static constexpr int kKnobScale = 0;
    static constexpr int kKnobAmountA = 1;
    static constexpr int kKnobAmountB = 2;
    static constexpr int kKnobFine = 3;
    static constexpr int kKnobPw = 4;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    int presetScaleIndex() const override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    static float clamp01 (float value);
    static float clampf (float value, float low, float high);
    static float polyBlep (float t, float dt);
    int scaleIndex() const;
    float footageHz() const;
    float fineRatio() const;

    // Scale 0.5 is 8'. Fine 0.5 is zero cents. Amounts start at 0. PW starts at 0.5.
    float scale01_ = 0.5f;
    float amountA01_ = 0.0f;
    float amountB01_ = 0.0f;
    float fine01_ = 0.5f;
    float pw01_ = 0.5f;
    double phase_ = 0.0;
    double triState_ = 0.0;
};
