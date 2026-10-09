// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// MIDI IN (the rack's MIDI > CV law: note, gate, velocity, last-note legato), its jack ids, format-1 blobs from the
// 16-module rack, and MG host sync (phase lock across relocate and loop, stopped and no-tempo fallbacks).

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
#include "Modular/Divider.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/ExtIn.h"
#include "Modular/Integrator.h"
#include "Modular/Inverter.h"
#include "Modular/Mg.h"
#include "Modular/MidiIn.h"
#include "Modular/Mixer.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/PatchState.h"
#include "Modular/Ring.h"
#include "Modular/SampleHold.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "Modular/Vco.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int gChecks = 0;

void check (bool ok, const std::string& message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message.c_str());
        ++gChecks;
    }
}

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

template <typename A, typename B>
bool near (A a, B b, double tol = 1.0e-6)
{
    return std::fabs (static_cast<double> (a) - static_cast<double> (b)) <= tol;
}

void tick (MidiIn& m) { m.processSample(); }

struct Rack16 {
    ExtIn ext; OutputModule out; NoiseModule noise; Vcf vcf; Vca1 vca1; Vca2 vca2; Eg1 eg1; MgModule mg; Vco vco;
    Eg2 eg2; Ring ring; Divider divider; Inverter inverter; Integrator integrator; Mixer mixer; SampleHold sampleHold;
    PatchGraph graph;
    RackIndices r;
    explicit Rack16 (MidiIn* midi = nullptr)
    {
        r.ext = graph.addModule (ext); r.output = graph.addModule (out); r.noise = graph.addModule (noise);
        r.vcf = graph.addModule (vcf); r.vca1 = graph.addModule (vca1); r.vca2 = graph.addModule (vca2);
        r.eg1 = graph.addModule (eg1); r.mg = graph.addModule (mg); r.vco = graph.addModule (vco);
        r.eg2 = graph.addModule (eg2); r.ring = graph.addModule (ring); r.divider = graph.addModule (divider);
        r.inverter = graph.addModule (inverter); r.integrator = graph.addModule (integrator);
        r.mixer = graph.addModule (mixer); r.sampleHold = graph.addModule (sampleHold);
        if (midi != nullptr)
            r.midi = graph.addModule (*midi);
    }
};

int findPort (const Rack16& rack, const char* section, const char* label, int& module)
{
    int port = -1;
    module = -1;
    if (! patchstate::jackAddress (rack.r, std::string (section) + ":" + label, module, port))
        return -1;
    return port;
}

void runMg (MgModule& mg, MgModule::HostClock& clock, int samples, double rate)
{
    mg.setHostClock (clock);
    for (int i = 0; i < samples; ++i)
    {
        mg.portValue[MgModule::kFreqMod] = 0.0f;
        mg.portValue[MgModule::kPwm] = 0.0f;
        mg.processSample();
    }
    if (clock.playing)
        clock.ppq += clock.bpm / 60.0 * static_cast<double> (samples) / rate;
}

double frac (double x) { return x - std::floor (x); }

}

int testMidiNoteMapping()
{
    // The rack's law: MIDI 48 = C3 = 0 V, 1 V per octave, the +-5 V rail.
    check (near (MidiIn::noteVolts (48), 0.0), "MIDI 48 (C3) = 0 V");
    check (near (MidiIn::noteVolts (60), 1.0), "MIDI 60 (C4) = +1 V");
    check (near (MidiIn::noteVolts (36), -1.0), "MIDI 36 (C2) = -1 V");
    check (near (MidiIn::noteVolts (49), 1.0 / 12.0), "a semitone is 1/12 V");
    check (near (MidiIn::noteVolts (0), -4.0), "MIDI 0 = -4 V");
    check (near (MidiIn::noteVolts (108), 5.0), "MIDI 108 = +5 V");
    check (near (MidiIn::noteVolts (127), 5.0), "MIDI 127 stops at the +5 V rail");
    check (near (MidiIn::hzvVolts (48), 1.0), "HZ/V LIN: C3 = 1 V");
    check (near (MidiIn::hzvVolts (60), 2.0), "HZ/V LIN: C4 = 2 V");
    check (near (MidiIn::hzvVolts (36), 0.5), "HZ/V LIN: C2 = 0.5 V");

    MidiIn m;
    m.prepare (48000.0);
    tick (m);
    check (near (m.portValue[MidiIn::kNote], 0.0) && near (m.portValue[MidiIn::kGate], 0.0)
               && near (m.portValue[MidiIn::kVel], 0.0) && m.lastNote() == -1,
           "before any key: 0 V, gate low, velocity 0");
    m.noteOn (72, 100);
    tick (m);
    check (near (m.portValue[MidiIn::kNote], 2.0) && near (m.portValue[MidiIn::kHzv], 4.0), "MIDI 72 -> +2 V, 4 V lin");
    check (m.lastNote() == 72 && m.heldCount() == 1, "UI read-out: last note and held count");

    const PortDesc note = m.port (MidiIn::kNote);
    const PortDesc hzv = m.port (MidiIn::kHzv);
    const PortDesc gate = m.port (MidiIn::kGate);
    const PortDesc vel = m.port (MidiIn::kVel);
    check (note.type == PortType::CV && note.role == PortRole::VOct && note.dir == PortDir::Out, "NOTE is a V/OCT output");
    check (hzv.type == PortType::CV && hzv.role == PortRole::HzvLin, "HZ/V LIN is a Hz/V output");
    check (gate.type == PortType::Gate && ! gate.strigVolts, "GATE is a gate (S-trig into an EG TRIG)");
    check (vel.type == PortType::CV, "VEL is a CV");
    return finish ("testMidiNoteMapping");
}

int testMidiGateVelocity()
{
    MidiIn m;
    m.prepare (48000.0);
    m.noteOn (60, 127);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 5.0), "gate 5 V while held");
    check (near (m.portValue[MidiIn::kVel], 5.0), "velocity 127 = 5 V");
    m.noteOff (60);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 0.0), "gate 0 V after release");
    check (near (m.portValue[MidiIn::kVel], 5.0) && near (m.portValue[MidiIn::kNote], 1.0),
           "velocity and pitch hold after release");
    m.noteOn (50, 64);
    tick (m);
    check (near (m.portValue[MidiIn::kVel], 64.0 / 127.0 * 5.0, 1.0e-5), "velocity 64 = 64/127 x 5 V");
    m.noteOn (50, 0);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 0.0), "note-on with velocity 0 is a note-off");
    check (near (m.portValue[MidiIn::kVel], 64.0 / 127.0 * 5.0, 1.0e-5), "velocity 0 note-on leaves VEL alone");
    m.noteOn (40, 10);
    m.noteOn (41, 10);
    m.allNotesOff();
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 0.0) && m.heldCount() == 0, "all notes off drops the gate");
    m.noteOff (99);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 0.0), "releasing a key that is not held is harmless");
    return finish ("testMidiGateVelocity");
}

int testMidiLegatoLastNote()
{
    MidiIn m;
    m.prepare (48000.0);
    m.noteOn (48, 100);
    tick (m);
    m.noteOn (60, 80);
    tick (m);
    check (near (m.portValue[MidiIn::kNote], 1.0), "the newest key wins (last-note priority)");
    check (near (m.portValue[MidiIn::kGate], 5.0), "legato: the gate stays high, no retrigger gap");
    check (near (m.portValue[MidiIn::kVel], 80.0 / 127.0 * 5.0, 1.0e-5), "velocity follows the newest key");
    m.noteOn (55, 90);
    tick (m);
    m.noteOff (60);
    tick (m);
    check (near (m.portValue[MidiIn::kNote], 7.0 / 12.0), "releasing an older key keeps the newest pitch");
    m.noteOff (55);
    tick (m);
    check (near (m.portValue[MidiIn::kNote], 0.0) && near (m.portValue[MidiIn::kGate], 5.0),
           "releasing the newest returns to the newest still held, gate still high");
    m.noteOff (48);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 0.0) && near (m.portValue[MidiIn::kNote], 0.0),
           "last key up: gate low, pitch held");
    m.noteOn (62, 100);
    m.noteOn (62, 100);
    m.noteOff (62);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 0.0), "a repeated key is one stack entry");
    for (int k = 0; k < 17; ++k)
        m.noteOn (30 + k, 100);
    check (m.heldCount() == MidiIn::kMaxHeld, "the held stack is 16 deep");
    tick (m);
    check (near (m.portValue[MidiIn::kNote], MidiIn::noteVolts (45)), "a 17th key does not take the pitch");
    m.allNotesOff();
    m.noteOn (52, 100);
    m.prepare (96000.0);
    tick (m);
    check (near (m.portValue[MidiIn::kGate], 5.0), "prepare keeps a held key");
    return finish ("testMidiLegatoLastNote");
}

int testMidiJackIdsAndFormat1()
{
    MidiIn midi;
    Rack16 rack (&midi);
    check (rack.r.midi == 16, "MIDI IN joins after the 16 panel modules");
    const char* labels[] = { "NOTE", "HZ/V LIN", "GATE", "VEL" };
    for (int p = 0; p < 4; ++p)
    {
        const std::string id = patchstate::jackId (rack.r, rack.r.midi, p);
        check (id == std::string ("MIDI:") + labels[p], "jack id " + id);
        int module = -1;
        const int port = findPort (rack, "MIDI", labels[p], module);
        check (module == rack.r.midi && port == p, id + " resolves to its port");
    }
    int module = -1;
    check (findPort (rack, "MIDI", "PITCH", module) < 0, "an unknown MIDI label does not resolve");

    int eg1Module = -1;
    const int trig = findPort (rack, "EG 1", "TRIG", eg1Module);
    int vcoModule = -1;
    const int voct = findPort (rack, "VCO", "V/OCT", vcoModule);
    check (trig >= 0 && voct >= 0, "EG 1:TRIG and VCO:V/OCT exist");
    check (rack.graph.connect (rack.r.midi, MidiIn::kGate, eg1Module, trig), "MIDI:GATE -> EG 1:TRIG is legal");
    check (rack.graph.connect (rack.r.midi, MidiIn::kNote, vcoModule, voct), "MIDI:NOTE -> VCO:V/OCT is legal");

    Rack16 plain;
    check (findPort (plain, "MIDI", "GATE", module) < 0, "a rack without MIDI IN does not resolve MIDI ids");

    plain.graph.connect (plain.r.vco, 0, plain.r.vcf, 0);
    unsigned char blob[4096];
    const int bytes = plain.graph.getState (blob, static_cast<int> (sizeof blob));
    MidiIn midi2;
    Rack16 grown (&midi2);
    check (bytes > 0 && grown.graph.setState (blob, bytes), "a 16-module format-1 blob loads with MIDI IN added");
    Cable cables[8];
    check (grown.graph.copyPublishedCables (cables, 8) == plain.graph.copyPublishedCables (cables, 8),
           "its cables came back");
    unsigned char big[4096];
    const int bigBytes = grown.graph.getState (big, static_cast<int> (sizeof big));
    check (! plain.graph.setState (big, bigBytes), "a blob with more modules than the rack is rejected");
    return finish ("testMidiJackIdsAndFormat1");
}

int testMgSyncPhaseLock()
{
    constexpr double rate = 48000.0;
    check (MgModule::kSyncDivisionCount == 16
               && std::string (MgModule::syncDivisionName (MgModule::kDefaultSyncDivision)) == "1/4",
           "16 divisions, 1/4 by default");
    check (near (MgModule::syncDivisionBeats (2), 4.0) && near (MgModule::syncDivisionBeats (13), 0.25)
               && near (MgModule::syncDivisionBeats (11), 1.0 / 3.0),
           "1 BAR = 4 beats, 1/16 = 1/4 beat, 1/8T = 1/3 beat");

    MgModule mg;
    mg.prepare (rate);
    mg.setKnob (MgModule::kKnobFrequency, 0.3f);
    mg.setSync (true, 7);
    MgModule::HostClock clock;
    clock.valid = true;
    clock.playing = true;
    clock.bpm = 120.0;
    clock.ppq = 0.0;
    for (int b = 0; b < 20; ++b)
        runMg (mg, clock, 512, rate);
    const double expected = frac (clock.ppq - 120.0 / 60.0 / rate);   // the last rendered sample
    check (near (mg.phase(), expected, 1.0e-9), "phase = song position in cycles");
    check (mg.phaseLocked(), "locked while playing");
    check (near (mg.rateHz(), 2.0, 1.0e-4), "120 BPM at 1/4 = 2 Hz");

    clock.ppq = 10.25;
    runMg (mg, clock, 1, rate);
    check (near (mg.phase(), 0.25, 1.0e-9), "relocate: the phase jumps with the song position");

    clock.ppq = 3.5;
    runMg (mg, clock, 12000, rate);
    clock.ppq = 2.0;
    MgModule fresh;
    fresh.prepare (rate);
    fresh.setKnob (MgModule::kKnobFrequency, 0.3f);
    fresh.setSync (true, 7);
    MgModule::HostClock freshClock = clock;
    bool same = true;
    for (int b = 0; b < 8; ++b)
    {
        runMg (mg, clock, 300, rate);
        runMg (fresh, freshClock, 300, rate);
        for (int p = MgModule::kTri; p <= MgModule::kPulse; ++p)
            same = same && ronin::exactlyEqual (mg.portValue[p], fresh.portValue[p]);
    }
    check (same, "loop: after the loop point the MG matches one started there");

    mg.setSync (true, 2);
    clock.ppq = 6.0;
    runMg (mg, clock, 1, rate);
    check (near (mg.phase(), 0.5, 1.0e-9), "1 BAR at beat 6 is half a cycle");

    mg.setSync (true, 7);
    clock.ppq = 1.0;
    mg.setHostClock (clock);
    mg.portValue[MgModule::kFreqMod] = 5.0f;
    mg.processSample();
    mg.processSample();
    check (near (mg.phase(), frac (1.0 + 120.0 / 60.0 / rate), 1.0e-9), "FREQ MOD is ignored while locked");
    return finish ("testMgSyncPhaseLock");
}

int testMgSyncFallbacks()
{
    constexpr double rate = 48000.0;
    MgModule mg;
    mg.prepare (rate);
    mg.setKnob (MgModule::kKnobFrequency, 0.3f);
    mg.setSync (true, 10);
    MgModule::HostClock clock;
    clock.valid = true;
    clock.playing = true;
    clock.bpm = 90.0;
    clock.ppq = 0.3;
    runMg (mg, clock, 100, rate);
    const double before = mg.phase();

    clock.playing = false;
    runMg (mg, clock, 1000, rate);
    const double dt = 90.0 / (60.0 * rate * 0.5);
    check (near (mg.phase(), frac (before + 1000.0 * dt), 1.0e-9), "stopped: the division rate, no jump");
    check (! mg.phaseLocked(), "not locked while stopped");
    check (near (mg.rateHz(), 3.0, 1.0e-4), "90 BPM at 1/8 = 3 Hz");

    MgModule freeMg;
    freeMg.prepare (rate);
    freeMg.setKnob (MgModule::kKnobFrequency, 0.3f);
    MgModule::HostClock none;
    MgModule noTempo;
    noTempo.prepare (rate);
    noTempo.setKnob (MgModule::kKnobFrequency, 0.3f);
    noTempo.setSync (true, 10);
    runMg (freeMg, none, 5000, rate);
    runMg (noTempo, none, 5000, rate);
    check (near (freeMg.phase(), noTempo.phase(), 1.0e-6), "no host tempo: runs at the RATE knob");
    check (near (freeMg.rateHz(), noTempo.rateHz(), 1.0e-4), "no host tempo: same rate as FREE");

    MgModule a;
    MgModule b;
    a.prepare (rate);
    b.prepare (rate);
    a.setKnob (MgModule::kKnobFrequency, 0.6f);
    b.setKnob (MgModule::kKnobFrequency, 0.6f);
    b.setSync (false, 3);
    MgModule::HostClock playing;
    playing.valid = true;
    playing.playing = true;
    playing.bpm = 133.0;
    playing.ppq = 7.7;
    bool same = true;
    for (int i = 0; i < 4; ++i)
    {
        runMg (a, none, 1000, rate);
        runMg (b, playing, 1000, rate);
        for (int p = MgModule::kTri; p <= MgModule::kPulse; ++p)
            same = same && ronin::exactlyEqual (a.portValue[p], b.portValue[p]);
    }
    check (same, "FREE: bit-identical to an MG that never saw a host clock");
    return finish ("testMgSyncFallbacks");
}
