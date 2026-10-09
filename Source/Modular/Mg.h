// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Module.h"

#include <atomic>

// S-16 modulation generator. Not the VCO: levels are ±2.5 V and the pulse is unipolar.
class MgModule : public Module {
public:
    static constexpr int kFreqMod = 0;
    static constexpr int kPwm = 1;
    static constexpr int kTri = 2;
    static constexpr int kSawUp = 3;
    static constexpr int kSawDown = 4;
    static constexpr int kPulse = 5;
    static constexpr int kKnobFrequency = 0;
    static constexpr int kKnobPw = 1;

    MgModule();

    // Host tempo sync (MG SYNC on a tab; FREE by default and for every older patch). The note divisions, long to short.
    static constexpr int kSyncDivisionCount = 16;
    static constexpr int kDefaultSyncDivision = 7;   // 1/4
    static const char* syncDivisionName (int index) noexcept;
    // Beats (quarter notes) per MG cycle.
    static double syncDivisionBeats (int index) noexcept;

    // The host transport at the first sample of a block. ppq advances by bpm / (60 x sampleRate) per sample.
    struct HostClock {
        bool valid = false;     // the host reports tempo and position
        bool playing = false;
        double bpm = 120.0;
        double ppq = 0.0;
    };
    void setSync (bool on, int division) noexcept;
    void setHostClock (const HostClock& clock) noexcept;
    bool syncOn() const noexcept { return sync_; }
    // True while the phase follows the song position (SYNC on, host playing with a tempo). UI read-out.
    bool phaseLocked() const noexcept { return locked_.load (std::memory_order_relaxed); }
    // The MG rate in Hz at the last sample. UI read-out.
    float rateHz() const noexcept { return rateShown_.load (std::memory_order_relaxed); }
    // Phase 0..1 at the last sample (tests).
    double phase() const noexcept { return phase_; }

    int numPorts() const override;
    PortDesc port (int index) const override;
    int numKnobs() const;
    void setKnob (int knob, float zeroToOne) override;
    int presetKnobCount() const override;
    float presetKnob (int knob) const override;
    void prepare (double sampleRate) override;
    void processSample() override;

private:
    static float clamp01 (float value);
    static float clampf (float value, float low, float high);
    float knobHz() const;
    static float morph (float phase, float sym);
    void renderOutputs (double dtD);

    // Class default is 5 Hz. The faceplate host default is a different travel.
    float freq01_ = 0.0f;
    float pw01_ = 0.5f;
    double phase_ = 0.0;
    bool sync_ = false;
    int division_ = kDefaultSyncDivision;
    HostClock clock_;
    std::atomic<bool> locked_ { false };
    std::atomic<float> rateShown_ { 0.0f };
};
