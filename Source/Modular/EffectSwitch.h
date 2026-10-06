// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// FL Studio's FX slot already has the 0 to 100 percent wet control.
// This plugin's top control is only on or off. Off is dry (mix 0). On is
// fully wet (mix 1), so the host percent is the blend.
inline float outputMixForEffect (bool on) noexcept
{
    return on ? 1.0f : 0.0f;
}
