// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include <cstdint>

struct Cable {
    int sourceModule = -1;
    int sourcePort = -1;
    int destModule = -1;
    int destPort = -1;
    // JCS M3 / RONIN M-R3: a cable migrated from a format-1 state that ran Gate -> a non-S-trig input keeps the
    // old S-15 inversion. New cables never get it.
    bool legacyInvert = false;
    // JCS R14: 0 = colour by role; otherwise a per-cable 0xAARRGGBB override. Never changes the sum (S-27).
    std::uint32_t colour = 0;
};
