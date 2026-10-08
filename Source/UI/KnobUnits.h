// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// RONIN_Redesign §4.1: knob hover/drag read-outs in real units (MAIN overlay, ENV tab). JUCE-free and testable.
// The laws are the modules' own: VCO footage/fine, VCF S-07/S-09, EG real time (EgLaw), MG, S&H, INT, EXT IN.

#pragma once

#include "Modular/EgLaw.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace knobunits {

inline std::string fmt (const char* pattern, double v)
{
    char text[64];
    std::snprintf (text, sizeof text, pattern, v);
    return text;
}

inline std::string hz (double value)
{
    if (value >= 1000.0)
        return fmt (value >= 10000.0 ? "%.1f kHz" : "%.2f kHz", value / 1000.0);
    if (value >= 100.0)
        return fmt ("%.0f Hz", value);
    if (value >= 10.0)
        return fmt ("%.1f Hz", value);
    if (value >= 1.0)
        return fmt ("%.2f Hz", value);
    return fmt ("%.3f Hz", value);
}

// "s" display: ms below one second, seconds above. "ms" display: always milliseconds.
inline std::string seconds (double s, bool alwaysMs)
{
    const double ms = s * 1000.0;
    if (alwaysMs || s < 1.0)
        return fmt (ms < 10.0 ? "%.2f ms" : (ms < 100.0 ? "%.1f ms" : "%.0f ms"), ms);
    return fmt (s < 10.0 ? "%.2f s" : "%.1f s", s);
}

inline std::string percent (double v) { return fmt ("%.0f %%", v * 100.0); }

inline const char* footageName (int index)
{
    static const char* names[4] = { "32' (C1)", "16' (C2)", "8' (C3)", "4' (C4)" };
    return names[index < 0 ? 0 : (index > 3 ? 3 : index)];
}

inline bool is (const char* a, const char* b) { return std::strcmp (a, b) == 0; }

inline std::string realUnits (const char* section, const char* label, float knob, bool alwaysMs = false)
{
    const double v = knob < 0.0f ? 0.0 : (knob > 1.0f ? 1.0 : static_cast<double> (knob));
    if (is (section, "VCO"))
    {
        if (is (label, "RANGE"))
            return footageName (static_cast<int> (std::lround (v * 3.0)));
        if (is (label, "FINE"))
            return fmt ("%+.0f c", (v * 2.0 - 1.0) * 200.0);
        return percent (v);
    }
    if (is (section, "VCF"))
    {
        if (is (label, "CUTOFF"))
            return hz (20.0 * std::pow (900.0, v));
        if (is (label, "PEAK"))
            return fmt ("Q %.1f", 0.5 + 7.5 * v);
        if (is (label, "MOD"))
            return fmt ("%.2f oct / 5 V", 4.0 * v);
    }
    if (is (section, "VCA 1") && is (label, "LOW CUT"))
        return hz (10.0 * std::pow (200.0, v));
    if (is (section, "MG") && is (label, "RATE"))
        return hz (0.01 * std::pow (20000.0, v));
    if (is (section, "EG 1") || is (section, "EG 2"))
    {
        if (is (label, "SUSTAIN"))
            return fmt ("%.2f V", v * EgLaw::kPeak);
        if (is (label, "HOLD") || is (label, "DELAY"))
            return seconds (EgLaw::holdDelaySeconds (v), alwaysMs);
        return seconds (EgLaw::secondsFor (v), alwaysMs);
    }
    if (is (section, "S&H") && is (label, "RATE"))
        return hz (0.1 * std::pow (1000.0, v));
    if (is (section, "INT") && is (label, "TIME"))
        return seconds (0.001 * std::pow (2000.0, v), true);
    if (is (section, "EXT IN"))
    {
        if (is (label, "THRESHOLD"))
            return fmt ("%.3f V", 0.05 * std::pow (40.0, v));
        if (is (label, "RELEASE"))
            return seconds (0.010 * std::pow (50.0, v), alwaysMs);
    }
    if (is (section, "DIV"))
        return v < 0.5 ? "/2" : "/4";
    return percent (v);
}

// Tuner: MIDI note naming with C3 = MIDI 48 = 130.81 Hz (JCS R4).
inline std::string noteName (double hzValue)
{
    if (! (hzValue > 0.0))
        return "--";
    const double midi = 69.0 + 12.0 * std::log2 (hzValue / 440.0);
    const long nearest = std::lround (midi);
    double cents = std::round ((midi - static_cast<double> (nearest)) * 100.0);
    cents += 0.0;   // -0 + 0 = +0: no "-0 c"
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int pc = static_cast<int> (((nearest % 12) + 12) % 12);
    const long octave = static_cast<long> (std::floor (static_cast<double> (nearest) / 12.0)) - 1;   // MIDI 48 = C3
    char text[32];
    std::snprintf (text, sizeof text, "%s%ld %+.0f c", names[pc], octave, cents);
    return text;
}

}
