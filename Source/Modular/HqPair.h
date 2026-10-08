// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// HQ 2x sub-sample order (RONIN_Redesign §3.2, JCS R11). Each 2fs sub-sample is rendered and read in its own
// statement, earlier first, and only then handed to the decimator. Never write
// `decimator.process (render(), render())`: C++ leaves argument evaluation order unspecified (g++ evaluates right
// to left), which would feed the later sub-sample first. testHqSubSampleOrder guards this. No JUCE, no allocation.

namespace hq {

struct StereoPair
{
    float earlierLeft = 0.0f;
    float earlierRight = 0.0f;
    float laterLeft = 0.0f;
    float laterRight = 0.0f;
};

// Engine: process() advances one 2fs sample. Output: hostLeft() / hostRight() read the current sample.
template <typename Engine, typename Output>
inline StereoPair renderPair (Engine& engine, const Output& output) noexcept
{
    StereoPair pair;
    engine.process();
    pair.earlierLeft = output.hostLeft();
    pair.earlierRight = output.hostRight();
    engine.process();
    pair.laterLeft = output.hostLeft();
    pair.laterRight = output.hostRight();
    return pair;
}

// Decimator: process (u0, u1) with u0 the earlier 2fs sample (jidai::dsp::Downsampler2x).
template <typename Decimator>
inline float decimate (Decimator& decimator, float earlier, float later) noexcept
{
    const double u0 = static_cast<double> (earlier);
    const double u1 = static_cast<double> (later);
    return static_cast<float> (decimator.process (u0, u1));
}

}
