// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// HQ 2x mode (RONIN_Redesign §3.2, §5 item 6). The 2fs -> fs decimator from the SHOGUN v2.2 spec §3.4:
// 93-tap linear-phase exact half-band (every second tap zero except the centre; the two end taps are zero),
// pass edge 0.2125 * 2fs, stop edge 0.2875 * 2fs. Designed with the Vaidyanathan-Nguyen trick (a 46-tap
// type-II Parks-McClellan prototype on [0, 0.425]); measured stopband 111.6 dB, passband ripple 4.5e-5 dB.
// Delay (93 - 1) / 2 = 46 samples at 2fs = 23 base samples, an integer, as JCS R11 reports it.
// No JUCE, no allocation. One instance per audio output.
//
// SHARED-CODE NOTE: SHOGUN builds the same decimator; this can move to jidai-common with the JCS header.

namespace Halfband {

inline constexpr int kTaps = 93;
inline constexpr int kCentre = 46;
inline constexpr int kLatencyBaseSamples = 23;
inline constexpr int kPairs = 23;

// h[1], h[3], ..., h[45]; mirrored to h[91], h[89], ..., h[47]. h[46] = 0.5. All other taps are 0.
inline constexpr double kOdd[kPairs] = {
    5.9364683306040488e-06,
    -1.7614233690896581e-05,
    4.2470806935745889e-05,
    -8.8700658838796237e-05,
    0.0001681804345037971,
    -0.00029685449968259694,
    0.00049537888144551097,
    -0.00078973722576147743,
    0.0012118491643317252,
    -0.0018002885204330306,
    0.0026012661241049916,
    -0.0036702901842250533,
    0.0050750910690782556,
    -0.006901073063122997,
    0.0092615035202843372,
    -0.012317055659975533,
    0.016314828794272491,
    -0.021671480782880455,
    0.029168777086991157,
    -0.040485631710322073,
    0.060007585563567861,
    -0.10387284801445018,
    0.31756000191720085
};

class Decimator2x
{
public:
    void reset() noexcept
    {
        for (double& v : buffer_)
            v = 0.0;
        head_ = 0;
    }

    // Push two 2fs samples (older first) and get one fs sample.
    float process (float older, float newer) noexcept
    {
        push (older);
        push (newer);
        // newest sample is at head_ - 1. Tap k reads x[newest - k].
        double acc = 0.5 * at (kCentre);
        for (int i = 0; i < kPairs; ++i)
        {
            const int k = 2 * i + 1;
            acc += kOdd[i] * (at (k) + at (kTaps - 1 - k));
        }
        return static_cast<float> (acc);
    }

    static double tap (int k) noexcept
    {
        if (k == kCentre)
            return 0.5;
        if (k < 0 || k >= kTaps || (k % 2) == 0)
            return 0.0;
        const int i = k < kCentre ? (k - 1) / 2 : (kTaps - 1 - k - 1) / 2;
        return kOdd[i];
    }

private:
    static constexpr int kSize = 128;

    void push (float v) noexcept
    {
        buffer_[head_] = static_cast<double> (v);
        head_ = (head_ + 1) & (kSize - 1);
    }

    double at (int k) const noexcept { return buffer_[(head_ - 1 - k) & (kSize - 1)]; }

    double buffer_[kSize] {};
    int head_ = 0;
};

}
