// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// The one default table. FaceKnobs fallbacks, layout.json "default", PanelGeometry.inc,
// double-click reset and factory program load all read these values.
// A fresh instance is the Voice program with these knobs. Hold overrides some of them in FactoryPresets.h.
namespace PanelDefault {

inline constexpr float kVcoRange = 0.50f;
inline constexpr float kVcoFine = 0.50f;
inline constexpr float kVcoPw = 0.50f;
inline constexpr float kVcoFm1 = 0.0f;
inline constexpr float kVcoFm2 = 0.0f;

inline constexpr float kVcfCutoff = 0.45f;
inline constexpr float kVcfPeak = 0.20f;
inline constexpr float kVcfMod = 0.40f;

// Initial 0 leaves Env in charge. Mod 0.85 is the schematic intensity.
inline constexpr float kVca1Initial = 0.0f;
inline constexpr float kVca1Mod = 0.85f;
inline constexpr float kVca1LowCut = 0.0f;

// Initial 0 and Mod 1 are the CV-only law VCA 2 had before it had knobs.
inline constexpr float kVca2Initial = 0.0f;
inline constexpr float kVca2Mod = 1.0f;

inline constexpr float kMgRate = 0.50f;
inline constexpr float kMgPw = 0.50f;

inline constexpr float kEg1Attack = 0.05f;
inline constexpr float kEg1Decay = 0.30f;
inline constexpr float kEg1Sustain = 0.60f;
inline constexpr float kEg1Release = 0.30f;

inline constexpr float kEg2Hold = 0.30f;
inline constexpr float kEg2Delay = 0.0f;
inline constexpr float kEg2Attack = 0.05f;
inline constexpr float kEg2Release = 0.30f;

inline constexpr float kSampleHoldRate = 0.50f;
inline constexpr float kIntegratorTime = 0.50f;
inline constexpr float kMixerLevel = 0.80f;
inline constexpr float kOutputMix = 1.0f;
inline constexpr float kOutputLevel = 0.70f;

// Ext In gate. 0.05 V * 40^v and 10 ms * 50^v: 0.2 V and 80 ms.
inline constexpr float kExtInThreshold = 0.3758f;
inline constexpr float kExtInRelease = 0.5316f;

// Divider switch. 0 is /2, 1 is /4.
inline constexpr float kDividerRatio = 0.0f;

}
