// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// Test-only spectrum helpers, the same measurements as jidai-audit/verify/verify_ronin.py
// (Blackman window, harmonics at the nearest bin, alias energy in 0-20 kHz outside +-6 Hz of each harmonic).

#pragma once

#include <cmath>
#include <complex>
#include <vector>

namespace spectrum {

inline constexpr double kPi = 3.14159265358979323846;

inline std::vector<double> blackman (size_t n)
{
    std::vector<double> w (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double a = 2.0 * kPi * static_cast<double> (i) / static_cast<double> (n - 1);
        w[i] = 0.42 - 0.5 * std::cos (a) + 0.08 * std::cos (2.0 * a);
    }
    return w;
}

// In-place radix-2 FFT. n must be a power of two.
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPi / static_cast<double> (len);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k];
                const auto v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// |DFT| at frequency f (Hz) of the windowed signal (single bin, direct sum).
inline double binMagnitude (const std::vector<double>& x, double f, double sr)
{
    const auto w = blackman (x.size());
    const double n = static_cast<double> (x.size());
    const double k = std::round (f / (sr / n));
    std::complex<double> acc (0.0, 0.0);
    for (size_t i = 0; i < x.size(); ++i)
    {
        const double ang = -2.0 * kPi * k * static_cast<double> (i) / n;
        acc += x[i] * w[i] * std::complex<double> (std::cos (ang), std::sin (ang));
    }
    return std::abs (acc);
}

// Harmonic h level in dB relative to the fundamental.
inline double harmonicDb (const std::vector<double>& x, double f0, int h, double sr)
{
    const double a1 = binMagnitude (x, f0, sr);
    const double ah = binMagnitude (x, f0 * h, sr);
    return 20.0 * std::log10 (ah / a1 + 1.0e-12);
}

// Alias energy relative to harmonic energy in 0-20 kHz, dB. x.size() must be a power of two.
inline double aliasDb (const std::vector<double>& x, double f0, double sr)
{
    const auto w = blackman (x.size());
    std::vector<std::complex<double>> a (x.size());
    for (size_t i = 0; i < x.size(); ++i)
        a[i] = x[i] * w[i];
    fft (a);
    const size_t bins = x.size() / 2 + 1;
    double harm = 0.0;
    double rest = 0.0;
    const int top = static_cast<int> (sr / 2.0 / f0);
    for (size_t i = 0; i < bins; ++i)
    {
        const double f = static_cast<double> (i) * sr / static_cast<double> (x.size());
        if (f >= 20000.0)
            continue;
        const double p = std::norm (a[i]);
        bool isHarm = false;
        for (int h = 1; h <= top && ! isHarm; ++h)
            isHarm = std::fabs (f - h * f0) < 6.0;
        (isHarm ? harm : rest) += p;
    }
    return 10.0 * std::log10 (rest / harm);
}

}
