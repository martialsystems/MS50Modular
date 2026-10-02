// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

#include <cstdint>

// KOD-A40042 stand-in (S-17). White and Pink audio outputs. Panel controls: none.
// The internal trim is fixed. Both outputs run on every sample.
class NoiseModule : public Module {
public:
    static constexpr int kWhite = 0;
    static constexpr int kPink = 1;
    static constexpr std::uint32_t kSeed = 0xA341316Cu;

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    float nextUniform();

    std::uint32_t rng_ = kSeed;
    float b0_ = 0.0f;
    float b1_ = 0.0f;
    float b2_ = 0.0f;
    float b3_ = 0.0f;
    float b4_ = 0.0f;
    float b5_ = 0.0f;
    float b6_ = 0.0f;
};
