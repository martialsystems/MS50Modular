// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// Jidai Cable Standard v1.1 helpers used by RONIN (JCS R2, R3, R3s, R14). Header-only, no JUCE, no allocation.
//
// SHARED-HEADER SWAP POINT. Jidai Commander is writing the collection-wide header in
// jidai-collection/jidai-common/include/jidai/jcs/ (Detect.h, Roles.h, Volts.h, JackId.h). This file mirrors
// its names and semantics (jcs::Schmitt, jcs::StrigDetector, jcs::triggerPulseSamples, jcs::gateVolts,
// jcs::strigVoltsFor, jcs::Role in the same order, jcs::roleInfo, jcs::cableBadge) so the swap is:
//   1. replace the body below with  #include <jidai/jcs/Detect.h>  and  #include <jidai/jcs/Roles.h>
//   2. replace  namespace ronin_jcs  with  namespace jcs = jidai::jcs;
//   3. add the jidai-common include directory in CMakeLists.txt.
// Every RONIN call site goes through the `jcs::` alias, so no other file changes.

#include <cstdint>
#include <cstring>

namespace ronin_jcs {

inline constexpr float kGateLow = 0.0f;       // JCS R2
inline constexpr float kGateHigh = 5.0f;      // JCS R2
inline constexpr float kTrigHigh = 1.0f;      // JCS R3: goes high above this
inline constexpr float kTrigLow = 0.5f;       // JCS R3: goes low below this
inline constexpr float kStrigHeld = 1.0f;     // JCS R3s: held below this
inline constexpr float kStrigRelease = 1.5f;  // JCS R3s: released above this
inline constexpr float kStrigRest = 5.0f;     // JCS R3s / R10: unpatched S-trig input rests released

constexpr float gateVolts (bool high) noexcept { return high ? kGateHigh : kGateLow; }

// JCS R3s: what a non-S-trig Gate source contributes on a cable into an S-trig input.
constexpr float strigVoltsFor (bool sourceHigh) noexcept { return sourceHigh ? 0.0f : 5.0f; }

// JCS R2: trigger pulses last at least 1 ms so they survive a 2-sample detector.
inline int triggerPulseSamples (double sampleRate) noexcept
{
    const double n = 0.001 * sampleRate;
    const long r = static_cast<long> (n + 0.5);
    return r < 1 ? 1 : static_cast<int> (r);
}

enum class Edge { None, Rising, Falling };

// JCS R3: the single V-trig detector. High when V > 1.0, low when V < 0.5. State persists across blocks.
struct Schmitt
{
    bool high = false;

    Edge process (float v) noexcept
    {
        if (! high && v > kTrigHigh)
        {
            high = true;
            return Edge::Rising;
        }
        if (high && v < kTrigLow)
        {
            high = false;
            return Edge::Falling;
        }
        return Edge::None;
    }
    bool rising (float v) noexcept { return process (v) == Edge::Rising; }
    void reset (bool isHigh = false) noexcept { high = isHigh; }
};

// JCS R3s: S-trig input (active low). Held when V < 1.0, released when V > 1.5.
// Edge::Rising means "became held" (a trigger starts).
struct StrigDetector
{
    bool held = false;

    Edge process (float v) noexcept
    {
        if (! held && v < kStrigHeld)
        {
            held = true;
            return Edge::Rising;
        }
        if (held && v > kStrigRelease)
        {
            held = false;
            return Edge::Falling;
        }
        return Edge::None;
    }
    void reset (bool isHeld = false) noexcept { held = isHeld; }
};

// JCS R14 signal roles, ordered by luminance (dark to light). Same order as jidai::jcs::Role.
enum class Role : std::uint8_t { STrig = 0, Audio, VOct, GateClk, HzvLin, CV };
inline constexpr int kRoleCount = 6;

struct RoleInfo
{
    const char* name;
    std::uint32_t rgb;
    const char* glyph;      // UTF-8
    double luminance;
};

inline const RoleInfo& roleInfo (Role r) noexcept
{
    static const RoleInfo table[kRoleCount] = {
        { "S-TRIG",   0xb129b1, "\xe2\x8a\x94", 0.141 },
        { "AUDIO",    0xe53b2f, "\xe2\x88\xbf", 0.200 },
        { "V/OCT",    0x6590f3, "\xe2\x99\xaa", 0.292 },
        { "GATE/CLK", 0x2ec554, "\xe2\x8a\x93", 0.412 },
        { "HZ/V LIN", 0x5cd5ed, "\xc6\x92",     0.560 },
        { "CV",       0xf7e77d, "\xe2\x89\x88", 0.784 },
    };
    const int i = static_cast<int> (r);
    return table[i >= 0 && i < kRoleCount ? i : static_cast<int> (Role::CV)];
}

constexpr std::uint32_t argb (std::uint32_t rgb) noexcept { return 0xff000000u | rgb; }
inline std::uint32_t roleArgb (Role r) noexcept { return argb (roleInfo (r).rgb); }

enum class Badge { None, PitchLaw, AudioIntoClock, GateToStrig };

// JCS R4.3 / R14 cable warnings. A warning, never a refusal.
inline Badge cableBadge (Role source, Role dest) noexcept
{
    if ((source == Role::VOct && dest == Role::HzvLin) || (source == Role::HzvLin && dest == Role::VOct))
        return Badge::PitchLaw;
    if (source == Role::Audio && dest == Role::GateClk)
        return Badge::AudioIntoClock;
    if (source == Role::GateClk && dest == Role::STrig)
        return Badge::GateToStrig;
    return Badge::None;
}

inline const char* badgeText (Badge b) noexcept
{
    switch (b)
    {
        case Badge::PitchLaw: return "\xe2\x89\xa0 V/OCT and HZ/V LIN (linear) do not match";
        case Badge::AudioIntoClock: return "\xe2\x89\xa0 audio into a clock input";
        case Badge::GateToStrig: return "\xe2\x8a\x93\xe2\x86\x92\xe2\x8a\x94 gate converted to S-trig";
        case Badge::None: break;
    }
    return "";
}

} // namespace ronin_jcs

namespace jcs = ronin_jcs;
