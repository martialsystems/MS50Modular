// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

#include <atomic>

// S-02 through S-06. Separate from MG: these outputs are ±5 V.
// RONIN_Redesign §3.2: pitch law KEPT (VCO:V/OCT relative 1 V/oct, VCO:HZ/V role HZ/V LIN, linear),
// TRI SHAPE TRIANGLE (PolyBLAMP) or PARABOLA (legacy integrated saw), footage switches at a saw wrap.
class Vco : public Module {
public:
    // Stored value of the TRI SHAPE setting (VOICE tab). New patches / INIT / fresh instances use Triangle;
    // user-saved format-1 states load on Parabola (M-R2). The stored v2 value always wins.
    enum class TriShape { Triangle = 0, Parabola = 1 };

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
    // INIT (and any later factory program) starts on the true TRIANGLE (M-R2, decided by the user).
    void applyFactoryPreset (int index) override;
    void prepare (double sampleRate) override;
    void processSample() override;

    void setTriShape (TriShape shape) noexcept { triShape_ = shape; }
    TriShape triShape() const noexcept { return triShape_; }
    // Footage index actually sounding (it follows the RANGE knob at the next saw wrap).
    int activeScaleIndex() const noexcept { return activeScale_; }
    // Last computed frequency in Hz, for the VOICE tab tuner. Relaxed, read by the UI.
    float lastHz() const noexcept { return lastHz_.load (std::memory_order_relaxed); }
    static float footageHzFor (int scaleIndex) noexcept;

    static float polyBlamp (float t, float dt);

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
    TriShape triShape_ = TriShape::Triangle;
    int activeScale_ = 2;
    bool started_ = false;
    std::atomic<float> lastHz_ { 0.0f };
};
