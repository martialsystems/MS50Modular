// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Jcs.h"

enum class PortType { Audio, CV, Gate };
enum class PortDir { In, Out };

// JCS R14 signal role. Default resolves from the type: Audio -> AUDIO, CV -> CV, Gate -> GATE/CLK.
// The role drives colour, glyph and warnings only. The graph still enforces kAllowed by PortType.
enum class PortRole { Default, Audio, Cv, VOct, HzvLin, GateClk, STrig };

struct PortDesc {
    const char* name;
    PortType type;
    PortDir dir;
    // Unpatched inputs are filled with this before the module runs (JCS R10). Gate inputs too: nothing latches.
    float rest = 0.0f;
    // Gate output already carries S-trig volts: 0 V held, +5 V released.
    bool strigVolts = false;
    // S-trig input (JCS R3s). Only EG 1 TRIG and EG 2 TRIG. Only cables landing here are polarity-converted.
    bool strigInput = false;
    PortRole role = PortRole::Default;
};

inline jcs::Role portRole (const PortDesc& desc) noexcept
{
    switch (desc.role)
    {
        case PortRole::Audio: return jcs::Role::Audio;
        case PortRole::Cv: return jcs::Role::CV;
        case PortRole::VOct: return jcs::Role::VOct;
        case PortRole::HzvLin: return jcs::Role::HzvLin;
        case PortRole::GateClk: return jcs::Role::GateClk;
        case PortRole::STrig: return jcs::Role::STrig;
        case PortRole::Default: break;
    }
    if (desc.strigInput)
        return jcs::Role::STrig;
    if (desc.type == PortType::Audio)
        return jcs::Role::Audio;
    if (desc.type == PortType::Gate)
        return jcs::Role::GateClk;
    return jcs::Role::CV;
}
