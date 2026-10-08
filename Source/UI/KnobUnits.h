// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// RONIN_Redesign §4.1: knob hover/drag read-outs in real units (MAIN overlay, ENV tab). JUCE-free and testable.
// The laws are the modules' own: VCO footage/fine, VCF S-07/S-09, EG real time (EgLaw), MG, S&H, INT, EXT IN.

#pragma once

#include "Modular/EgLaw.h"

#include <jidai/jcs/Pitch.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
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
            return fmt ("Q %.2f", 0.5 + 7.5 * v);
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
    // Shared JCS R4 law and names (jidai-common Pitch.h): MIDI 48 = C3 = jcs kC3Hz (130.8127826502993 Hz, exact).
    using jidai::jcs::pitch::Law;
    const double midi = jidai::jcs::pitch::note (Law::VOct, jidai::jcs::pitch::hzToVolts (Law::VOct, hzValue));
    const long nearest = std::lround (midi);
    double cents = std::round ((midi - static_cast<double> (nearest)) * 100.0);
    cents += 0.0;   // -0 + 0 = +0: no "-0 c"
    char name[16];
    jidai::jcs::pitch::noteName (static_cast<int> (nearest), name, static_cast<int> (sizeof name));
    char text[40];
    std::snprintf (text, sizeof text, "%s %+.0f c", name, cents);
    return text;
}

// MAIN right-click -> type a value (RONIN_Redesign §4.1). Accepts what realUnits prints: "1.07 kHz", "621 ms",
// "2.5 s", "+34 c", "Q 4", "3 V", "58 %", "8'". A bare number takes the display's own unit (Hz, s or ms per
// alwaysMs, cents, V, %). False when the text does not parse or the knob is the DIV switch.
inline bool parseKnob (const char* section, const char* label, const std::string& text, bool alwaysMs, double& knob)
{
    std::string t;
    for (char ch : text)
        if (ch != ' ' && ch != '\t')
            t += static_cast<char> (std::tolower (static_cast<unsigned char> (ch)));
    if (t.empty() || is (section, "DIV"))
        return false;
    if (t[0] == 'q')
        t.erase (0, 1);
    const char* begin = t.c_str();
    char* end = nullptr;
    const double number = std::strtod (begin, &end);
    if (end == begin || ! std::isfinite (number))
        return false;
    const std::string unit (end);
    auto ends = [&unit] (const char* u) { return unit == u; };
    auto logKnob = [] (double value, double lo, double ratio) {
        return value > lo ? std::log (value / lo) / std::log (ratio) : 0.0;
    };
    auto secondsFrom = [&] (double& s) {
        if (ends ("ms")) { s = number / 1000.0; return true; }
        if (ends ("s")) { s = number; return true; }
        if (unit.empty()) { s = alwaysMs ? number / 1000.0 : number; return true; }
        return false;
    };
    auto hzFrom = [&] (double& h) {
        if (ends ("khz") || ends ("k")) { h = number * 1000.0; return true; }
        if (ends ("hz") || unit.empty()) { h = number; return true; }
        return false;
    };
    double k = 0.0;
    double v = 0.0;
    bool ok = false;
    if (is (section, "VCO") && is (label, "RANGE"))
    {
        const int feet = static_cast<int> (std::lround (number));
        const int index = feet == 32 ? 0 : (feet == 16 ? 1 : (feet == 8 ? 2 : (feet == 4 ? 3 : -1)));
        ok = index >= 0 && (unit.empty() || unit[0] == '\'');
        k = index / 3.0;
    }
    else if (is (section, "VCO") && is (label, "FINE"))
    {
        ok = unit.empty() || ends ("c");
        k = (number / 200.0 + 1.0) * 0.5;
    }
    else if (is (section, "VCF") && is (label, "CUTOFF"))
    {
        ok = hzFrom (v);
        k = logKnob (v, 20.0, 900.0);
    }
    else if (is (section, "VCF") && is (label, "PEAK"))
    {
        ok = unit.empty();
        k = (number - 0.5) / 7.5;
    }
    else if (is (section, "VCF") && is (label, "MOD"))
    {
        ok = unit.empty() || unit.rfind ("oct", 0) == 0;
        k = number / 4.0;
    }
    else if (is (section, "VCA 1") && is (label, "LOW CUT"))
    {
        ok = hzFrom (v);
        k = logKnob (v, 10.0, 200.0);
    }
    else if (is (section, "MG") && is (label, "RATE"))
    {
        ok = hzFrom (v);
        k = logKnob (v, 0.01, 20000.0);
    }
    else if (is (section, "S&H") && is (label, "RATE"))
    {
        ok = hzFrom (v);
        k = logKnob (v, 0.1, 1000.0);
    }
    else if ((is (section, "EG 1") || is (section, "EG 2")) && is (label, "SUSTAIN"))
    {
        ok = unit.empty() || ends ("v");
        k = number / EgLaw::kPeak;
    }
    else if ((is (section, "EG 1") || is (section, "EG 2")) && (is (label, "HOLD") || is (label, "DELAY")))
    {
        ok = secondsFrom (v);
        k = logKnob (v, 0.001, 10000.0);
    }
    else if (is (section, "EG 1") || is (section, "EG 2"))
    {
        ok = secondsFrom (v);
        k = EgLaw::knobForSeconds (v);
    }
    else if (is (section, "INT") && is (label, "TIME"))
    {
        // INT TIME always displays in ms.
        ok = ends ("s") ? (v = number, true) : ((ends ("ms") || unit.empty()) ? (v = number / 1000.0, true) : false);
        k = logKnob (v, 0.001, 2000.0);
    }
    else if (is (section, "EXT IN") && is (label, "THRESHOLD"))
    {
        ok = unit.empty() || ends ("v");
        k = logKnob (number, 0.05, 40.0);
    }
    else if (is (section, "EXT IN") && is (label, "RELEASE"))
    {
        ok = secondsFrom (v);
        k = logKnob (v, 0.010, 50.0);
    }
    else
    {
        ok = unit.empty() || ends ("%");
        k = number / 100.0;
    }
    if (! ok || ! std::isfinite (k))
        return false;
    knob = k < 0.0 ? 0.0 : (k > 1.0 ? 1.0 : k);
    return true;
}

}
