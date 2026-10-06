// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

enum class PortType { Audio, CV, Gate };
enum class PortDir { In, Out };

struct PortDesc {
    const char* name;
    PortType type;
    PortDir dir;
    // Unpatched audio and CV inputs are filled with this before the module runs.
    // Unpatched Gate inputs are left untouched. 0 keeps the older ports closed.
    float rest = 0.0f;
};
