// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include <cstring>

// Working faceplate knobs. Picture columns stay None and get no host parameter.
enum class FaceKnob {
    None = 0,
    VcfCutoff,
    VcfPeak,
    VcfAmount,
    Vca1LowCut,
    Eg1Attack,
    Eg1Decay,
    Eg1Sustain,
    Eg1Release,
    MgRate,
    MgPw,
    VcoRange,
    VcoFine,
    VcoPw,
    VcoFm1,
    VcoFm2,
    Eg2Hold,
    Eg2Delay,
    Eg2Attack,
    Eg2Release,
    IntegratorTime
};

// minimum and maximum are the host range for that parameter. They are not one
// shared range. The faceplate travel is still normalised 0 to 1, and the module
// maps that travel. fallback is the value in the host range.
struct FaceKnobBinding {
    FaceKnob knob = FaceKnob::None;
    int index = 0;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float fallback = 0.0f;
    const char* parameterId = "";
    const char* parameterName = "";
};

inline bool faceKnobText (const char* a, const char* b)
{
    return a != nullptr && b != nullptr && std::strcmp (a, b) == 0;
}

inline FaceKnobBinding faceKnobBinding (const char* section, const char* label)
{
    FaceKnobBinding none;
    if (section == nullptr || label == nullptr)
        return none;

    if (faceKnobText (section, "VCF") && faceKnobText (label, "CUTOFF"))
        return { FaceKnob::VcfCutoff, 0, 0.0f, 1.0f, 0.50f, "vcfCutoff", "VCF Cutoff" };
    if (faceKnobText (section, "VCF") && faceKnobText (label, "PEAK"))
        return { FaceKnob::VcfPeak, 1, 0.0f, 1.0f, 0.30f, "vcfPeak", "VCF Peak" };
    if (faceKnobText (section, "VCF") && faceKnobText (label, "MOD"))
        return { FaceKnob::VcfAmount, 2, 0.0f, 1.0f, 0.68f, "vcfAmount", "VCF Cutoff Amount" };
    if (faceKnobText (section, "VCA 1") && faceKnobText (label, "LOW CUT"))
        return { FaceKnob::Vca1LowCut, 0, 0.0f, 1.0f, 0.68f, "vca1LowCut", "VCA 1 Low Cut" };
    if (faceKnobText (section, "EG 1") && faceKnobText (label, "ATTACK"))
        return { FaceKnob::Eg1Attack, 0, 0.0f, 1.0f, 0.50f, "eg1Attack", "EG 1 Attack" };
    if (faceKnobText (section, "EG 1") && faceKnobText (label, "DECAY"))
        return { FaceKnob::Eg1Decay, 1, 0.0f, 1.0f, 0.30f, "eg1Decay", "EG 1 Decay" };
    if (faceKnobText (section, "EG 1") && faceKnobText (label, "SUSTAIN"))
        return { FaceKnob::Eg1Sustain, 2, 0.0f, 1.0f, 0.68f, "eg1Sustain", "EG 1 Sustain" };
    if (faceKnobText (section, "EG 1") && faceKnobText (label, "RELEASE"))
        return { FaceKnob::Eg1Release, 3, 0.0f, 1.0f, 0.42f, "eg1Release", "EG 1 Release" };
    if (faceKnobText (section, "MG") && faceKnobText (label, "RATE"))
        return { FaceKnob::MgRate, 0, 0.0f, 1.0f, 0.50f, "mgRate", "MG Rate" };
    if (faceKnobText (section, "MG") && faceKnobText (label, "PW"))
        return { FaceKnob::MgPw, 1, 0.0f, 1.0f, 0.30f, "mgPw", "MG PW" };
    if (faceKnobText (section, "VCO") && faceKnobText (label, "RANGE"))
        return { FaceKnob::VcoRange, 0, 0.0f, 1.0f, 0.50f, "vcoRange", "VCO Range" };
    if (faceKnobText (section, "VCO") && faceKnobText (label, "FINE"))
        return { FaceKnob::VcoFine, 3, 0.0f, 1.0f, 0.30f, "vcoFine", "VCO Fine" };
    if (faceKnobText (section, "VCO") && faceKnobText (label, "PW"))
        return { FaceKnob::VcoPw, 4, 0.0f, 1.0f, 0.68f, "vcoPw", "VCO PW" };
    if (faceKnobText (section, "VCO") && faceKnobText (label, "FM 1"))
        return { FaceKnob::VcoFm1, 1, 0.0f, 1.0f, 0.42f, "vcoFm1", "VCO FM 1" };
    if (faceKnobText (section, "VCO") && faceKnobText (label, "FM 2"))
        return { FaceKnob::VcoFm2, 2, 0.0f, 1.0f, 0.78f, "vcoFm2", "VCO FM 2" };
    if (faceKnobText (section, "EG 2") && faceKnobText (label, "HOLD"))
        return { FaceKnob::Eg2Hold, 0, 0.0f, 1.0f, 0.50f, "eg2Hold", "EG 2 Hold" };
    if (faceKnobText (section, "EG 2") && faceKnobText (label, "DELAY"))
        return { FaceKnob::Eg2Delay, 1, 0.0f, 1.0f, 0.30f, "eg2Delay", "EG 2 Delay" };
    if (faceKnobText (section, "EG 2") && faceKnobText (label, "ATTACK"))
        return { FaceKnob::Eg2Attack, 2, 0.0f, 1.0f, 0.68f, "eg2Attack", "EG 2 Attack" };
    if (faceKnobText (section, "EG 2") && faceKnobText (label, "RELEASE"))
        return { FaceKnob::Eg2Release, 3, 0.0f, 1.0f, 0.42f, "eg2Release", "EG 2 Release" };
    if (faceKnobText (section, "INT") && faceKnobText (label, "TIME"))
        return { FaceKnob::IntegratorTime, 0, 0.0f, 1.0f, 0.50f, "integratorTime", "Integrator Time" };

    return none;
}
