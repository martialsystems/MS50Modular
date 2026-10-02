// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Noise.h"

namespace {

// Three uniforms on [-1, 1) reach ±3. This scale is the S-17 white peak.
constexpr float kWhiteScale = 2.5f / 3.0f;

// Paul Kellett's filter is unity gain at Nyquist, so the low end sits well
// above the white peak. 0.2 brings a long run of this generator back near
// ±2.5 V. The trim is fixed.
constexpr float kPinkTrim = 0.2f;

}

int NoiseModule::numPorts() const
{
    return 2;
}

PortDesc NoiseModule::port (int index) const
{
    if (index == kWhite)
        return { "White", PortType::Audio, PortDir::Out };
    return { "Pink", PortType::Audio, PortDir::Out };
}

int NoiseModule::numKnobs() const
{
    return 0;
}

void NoiseModule::setKnob (int, float)
{
}

void NoiseModule::prepare (double rate)
{
    sampleRate = rate;
    rng_ = kSeed;
    b0_ = 0.0f;
    b1_ = 0.0f;
    b2_ = 0.0f;
    b3_ = 0.0f;
    b4_ = 0.0f;
    b5_ = 0.0f;
    b6_ = 0.0f;
}

float NoiseModule::nextUniform()
{
    std::uint32_t x = rng_;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_ = x;

    // Top 24 bits fill the float mantissa. Range is [-1, 1).
    constexpr float kUnit = 1.0f / 8388608.0f;
    const auto top = static_cast<std::int32_t> (x >> 8);
    return static_cast<float> (top - 8388608) * kUnit;
}

void NoiseModule::processSample()
{
    const float sum = nextUniform() + nextUniform() + nextUniform();
    const float white = sum * kWhiteScale;

    // Paul Kellett, refined one-pole sum, about 3 dB/octave (44.1 kHz set).
    // b6 is the previous sample, so it is applied before it is updated.
    b0_ = 0.99886f * b0_ + white * 0.0555179f;
    b1_ = 0.99332f * b1_ + white * 0.0750759f;
    b2_ = 0.96900f * b2_ + white * 0.1538520f;
    b3_ = 0.86650f * b3_ + white * 0.3104856f;
    b4_ = 0.55000f * b4_ + white * 0.5329522f;
    b5_ = -0.7616f * b5_ - white * 0.0168980f;
    const float pink = (b0_ + b1_ + b2_ + b3_ + b4_ + b5_ + b6_ + white * 0.5362f) * kPinkTrim;
    b6_ = white * 0.115926f;

    portValue[kWhite] = white;
    portValue[kPink] = pink;
}
