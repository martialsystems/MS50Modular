// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

enum class PortType { Audio, CV, Gate };
enum class PortDir { In, Out };

struct PortDesc {
    const char* name;
    PortType type;
    PortDir dir;
};
