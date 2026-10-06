// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// The panel rocker gates the Output Mix knob.
// Printed OFF is the left half. Printed ON is the right half.
// Off ignores the knob and passes the dry L/R cables.
// On uses the knob: dry * (1 - mix) + wet * mix. Output Level follows that blend.
// FL Studio's slot percent is a further blend outside the plugin.
// outputMixForEffect is this law with the knob at its default of 1,
// so on is the patch and off is dry.
inline float outputMixAfterSwitch (bool effectOn, float mixKnob) noexcept
{
    if (! effectOn)
        return 0.0f;
    if (mixKnob < 0.0f)
        return 0.0f;
    if (mixKnob > 1.0f)
        return 1.0f;
    return mixKnob;
}

inline float outputMixForEffect (bool on) noexcept
{
    return outputMixAfterSwitch (on, 1.0f);
}
