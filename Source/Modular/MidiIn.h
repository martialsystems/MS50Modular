// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "MidiJacks.h"
#include "Module.h"

#include <atomic>

// MIDI to CV, the JIDAI rack's mapping (its MIDI > CV jacks): mono, last-note priority, legato.
//   NOTE      V/OCT     (note - 48) / 12 V: MIDI 48 = C3 = 0 V, MIDI 60 = C4 = +1 V. Held after the key is released.
//   HZ/V LIN  HZ/V LIN  2^((note - 48) / 12) V: 1 V = C3, for the VCO's linear input.
//   GATE      GATE      0/5 V while any key is held. A new key over a held one keeps the gate high (legato).
//   VEL       CV        velocity / 127 x 5 V, held until the next key.
// Pitch stops at the +-5 V rail. Note-on with velocity 0 is a note-off. All channels are read (omni).
// The processor calls noteOn / noteOff between samples, at each event's sample position.
class MidiIn : public Module {
public:
    static constexpr int kNote = midijacks::kNote;
    static constexpr int kHzv = midijacks::kHzv;
    static constexpr int kGate = midijacks::kGate;
    static constexpr int kVel = midijacks::kVel;
    static constexpr int kMaxHeld = 16;

    int numPorts() const override { return midijacks::kCount; }
    PortDesc port (int index) const override;
    void setKnob (int, float) override {}
    // Held keys survive prepare (a program change or an HQ switch does not drop a held note).
    void prepare (double rate) override { sampleRate = rate; }
    void processSample() override;

    void noteOn (int note, int velocity);
    void noteOff (int note);
    void allNotesOff();

    // UI read-outs (relaxed).
    int heldCount() const noexcept { return heldShown_.load (std::memory_order_relaxed); }
    int lastNote() const noexcept { return noteShown_.load (std::memory_order_relaxed); }   // -1 before the first key

    // The rack's law, exposed for tests.
    static float noteVolts (int note) noexcept;
    static float hzvVolts (int note) noexcept;
    static float velocityVolts (int velocity) noexcept;

private:
    void pushNote (int note);
    void popNote (int note);
    void publishUi();

    int held_[kMaxHeld] {};
    int heldCount_ = 0;
    float noteV_ = 0.0f;
    float hzvV_ = 0.0f;   // 0 V before the first key, as the rack
    float velV_ = 0.0f;
    std::atomic<int> heldShown_ { 0 };
    std::atomic<int> noteShown_ { -1 };
};
