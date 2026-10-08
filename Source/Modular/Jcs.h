// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// Jidai Cable Standard v1.1 for RONIN: the shared jidai-common header (vendored at 829ebd2 in
// third_party/jidai-common, see VENDOR.md). RONIN code calls it through the `jcs::` alias:
//   Detect.h  R2 gate levels, R3 Schmitt 1.0/0.5 V, R3s StrigDetector 1.0/1.5 V and strigVoltsFor, trigger width
//   Volts.h   R1 host x5 / x0.2, R15 OverRangeLed (5.5 V for more than 10 ms)
//   Pitch.h   R4 V/OCT and HZ/V LIN (jcs::pitch), RONIN HZ/V floor 0.05 V, note names
//   Roles.h   R14 roles, colours, glyphs, cable badges
//   JackId.h  R6 jack-id parser and alias tables
// This file used to hold a local mirror of those helpers; it is now only the adapter.

#include <jidai/CableStandard.h>

namespace jcs = jidai::jcs;
