// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// The MIDI section's jack ids, shared by the module and the patch format. No dependencies, so the JIDAI rack can
// compile PatchState.cpp without the module. The names follow the rack's own MIDI > CV jacks.

#pragma once

namespace midijacks {

inline constexpr const char* kSection = "MIDI";
inline constexpr int kCount = 4;
inline constexpr int kNote = 0;   // V/OCT: (note - 48) / 12 V, 0 V = C3 = MIDI 48
inline constexpr int kHzv = 1;    // HZ/V LIN: 2^((note - 48) / 12) V, 1 V = C3
inline constexpr int kGate = 2;   // 0/5 V while any key is held
inline constexpr int kVel = 3;    // velocity, 0..5 V

inline const char* label (int port) noexcept
{
    static const char* labels[kCount] = { "NOTE", "HZ/V LIN", "GATE", "VEL" };
    return port >= 0 && port < kCount ? labels[port] : "";
}

}
