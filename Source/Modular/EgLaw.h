// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include <cmath>

// RONIN_Redesign §3.1. Shared by EG 1 and EG 2. No JUCE, no allocation.
// Time knobs are labelled in real time: T = 0.001 * 60000^k seconds (1 ms .. 60 s).
// Attack is an RC toward 6 V that stops at 5 V, so the 0 -> 5 V time is exactly T (c = ln 6).
// Decay and release are distance-to-target decays whose label is a full 5 V swing to within 1 % (c = ln 100).
// State is double and coefficients are -expm1(-c / (T * sr)), so nothing stalls in float (JCS R12).
namespace EgLaw {

inline constexpr double kMinSeconds = 0.001;
inline constexpr double kSpanRatio = 60000.0;     // 1 ms .. 60 s
inline constexpr double kAttackTarget = 6.0;      // volts
inline constexpr double kPeak = 5.0;              // volts
inline constexpr double kSnap = 1.0e-6;           // volts: -134 dB re 5 V
inline constexpr double kSustainSlewSeconds = 0.005;

inline double attackC() noexcept { return std::log (6.0); }
inline double decayC() noexcept { return std::log (100.0); }

inline double clamp01 (double v) noexcept { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

// Knob 0..1 -> seconds (real segment time).
inline double secondsFor (double knob01) noexcept
{
    return kMinSeconds * std::pow (kSpanRatio, clamp01 (knob01));
}

// Seconds -> knob 0..1 (clamped to the 1 ms .. 60 s travel).
inline double knobForSeconds (double seconds) noexcept
{
    if (! (seconds > kMinSeconds))
        return 0.0;
    return clamp01 (std::log (seconds / kMinSeconds) / std::log (kSpanRatio));
}

// a = -expm1(-c / (T * sr)). Computed on a knob or rate change, never per sample.
inline double coefficient (double c, double seconds, double sampleRate) noexcept
{
    const double rate = sampleRate > 1.0 ? sampleRate : 48000.0;
    const double t = seconds > 1.0e-9 ? seconds : 1.0e-9;
    return -std::expm1 (-c / (t * rate));
}

// Old v1 knob law (tau = 0.001 * 10^(4k), used by format-1 states) and the M-R1 migration.
inline double v1Tau (double oldKnob01) noexcept { return 0.001 * std::pow (10000.0, clamp01 (oldKnob01)); }

// M-R1: attack. The old real attack (0 -> 4.99 V) was tau * ln 500.
inline double migrateAttackKnob (double oldKnob01) noexcept
{
    return knobForSeconds (v1Tau (oldKnob01) * std::log (500.0));
}

// M-R1: decay and release. The old time to within 1 % was tau * ln 100.
inline double migrateDecayReleaseKnob (double oldKnob01) noexcept
{
    return knobForSeconds (v1Tau (oldKnob01) * std::log (100.0));
}

// Kept law for EG 2 HOLD and DELAY (timing KEPT, §3.1): 0.001 * 10^(4k) seconds.
inline double holdDelaySeconds (double knob01) noexcept { return v1Tau (knob01); }

}
