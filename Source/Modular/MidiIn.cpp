// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "MidiIn.h"

#include <jidai/jcs/Detect.h>
#include <jidai/jcs/Pitch.h>

PortDesc MidiIn::port (int index) const
{
    if (index == kNote)
        return { "NOTE", PortType::CV, PortDir::Out, 0.0f, false, false, PortRole::VOct };
    if (index == kHzv)
        return { "HZ/V LIN", PortType::CV, PortDir::Out, 0.0f, false, false, PortRole::HzvLin };
    if (index == kGate)
        return { "GATE", PortType::Gate, PortDir::Out };
    return { "VEL", PortType::CV, PortDir::Out };
}

float MidiIn::noteVolts (int note) noexcept
{
    namespace pitch = jidai::jcs::pitch;
    bool over = false;
    return static_cast<float> (pitch::clampPitch (pitch::voltsForNote (pitch::Law::VOct, static_cast<double> (note)), over));
}

float MidiIn::hzvVolts (int note) noexcept
{
    namespace pitch = jidai::jcs::pitch;
    bool over = false;
    return static_cast<float> (pitch::clampPitch (pitch::voltsForNote (pitch::Law::HzvLin, static_cast<double> (note)), over));
}

float MidiIn::velocityVolts (int velocity) noexcept
{
    return static_cast<float> (velocity) / 127.0f * 5.0f;
}

void MidiIn::noteOn (int note, int velocity)
{
    if (note < 0 || note > 127)
        return;
    if (velocity <= 0)
    {
        noteOff (note);
        return;
    }
    pushNote (note);
    velV_ = velocityVolts (velocity > 127 ? 127 : velocity);
    noteV_ = noteVolts (held_[heldCount_ - 1]);
    hzvV_ = hzvVolts (held_[heldCount_ - 1]);
    publishUi();
}

void MidiIn::noteOff (int note)
{
    popNote (note);
    // Last-note priority: releasing the newest key returns to the newest key still held; the pitch of the last
    // key stays on NOTE after everything is released.
    if (heldCount_ > 0)
    {
        noteV_ = noteVolts (held_[heldCount_ - 1]);
        hzvV_ = hzvVolts (held_[heldCount_ - 1]);
    }
    publishUi();
}

void MidiIn::allNotesOff()
{
    heldCount_ = 0;
    publishUi();
}

void MidiIn::pushNote (int note)
{
    popNote (note);
    if (heldCount_ < kMaxHeld)   // as the rack: a 17th simultaneous key is not stacked
        held_[heldCount_++] = note;
}

void MidiIn::popNote (int note)
{
    for (int i = 0; i < heldCount_; ++i)
    {
        if (held_[i] == note)
        {
            for (int k = i; k + 1 < heldCount_; ++k)
                held_[k] = held_[k + 1];
            --heldCount_;
            return;
        }
    }
}

void MidiIn::publishUi()
{
    heldShown_.store (heldCount_, std::memory_order_relaxed);
    if (heldCount_ > 0)
        noteShown_.store (held_[heldCount_ - 1], std::memory_order_relaxed);
}

void MidiIn::processSample()
{
    portValue[kNote] = noteV_;
    portValue[kHzv] = hzvV_;
    portValue[kGate] = heldCount_ > 0 ? jidai::jcs::kGateHigh : jidai::jcs::kGateLow;
    portValue[kVel] = velV_;
}
