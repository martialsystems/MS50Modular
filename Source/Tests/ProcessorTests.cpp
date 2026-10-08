// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// Processor-level tests (JUCE): RONIN_Redesign §6 tests that need the real plugin state and latency reporting.

#include "PluginProcessor.h"
#include "Modular/PatchState.h"
#include "UI/PatchBayLogic.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <functional>
#include <vector>

namespace {

int gChecks = 0;
int gFailed = 0;
int gPassed = 0;

void check (bool ok, const char* message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message);
        ++gChecks;
    }
}

void finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    (failed == 0 ? gPassed : gFailed) += 1;
}

void runBlocks (RoninAudioProcessor& p, int blocks, int size = 256, float input = 0.0f)
{
    juce::AudioBuffer<float> buffer (2, size);
    juce::MidiBuffer midi;
    for (int b = 0; b < blocks; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < size; ++i)
                buffer.setSample (ch, i, input);
        p.processBlock (buffer, midi);
    }
}

juce::MemoryBlock saveState (RoninAudioProcessor& p)
{
    juce::MemoryBlock block;
    p.getStateInformation (block);
    return block;
}

}

static void testHqDefaultOff()
{
    RoninAudioProcessor p;
    check (p.hqParameter() != nullptr && ! p.hqParameter()->get(), "HQ parameter defaults OFF");
    p.prepareToPlay (48000.0, 256);
    check (p.getLatencySamples() == 0, "a new instance reports 0 latency");
    check (! p.hqActive(), "a new instance runs at 1x");
    check (std::fabs (p.engineSampleRate() - 48000.0) < 1.0e-9, "engine at the host rate");

    p.hqParameter()->setValueNotifyingHost (1.0f);
    p.prepareToPlay (48000.0, 256);
    check (p.getLatencySamples() == 23, "HQ on reports 23 samples");
    check (std::fabs (p.engineSampleRate() - 96000.0) < 1.0e-9, "HQ runs the engine at 2x");
    runBlocks (p, 4);

    // A fresh instance is never on, even after another instance turned it on.
    RoninAudioProcessor q;
    q.prepareToPlay (44100.0, 128);
    check (q.getLatencySamples() == 0 && ! q.hqActive(), "the default is never on");

    // INIT (a new patch) does not turn HQ on.
    p.setCurrentProgram (kInitPreset);
    check (p.hqParameter()->get(), "INIT leaves the user's HQ choice alone");
    finish ("testHqDefaultOff");
}

static void testHqLatencyIsTwentyThree()
{
    // EXT IN L -> OUTPUT L dry path: an impulse comes out 23 samples later with HQ on, 0 with HQ off.
    for (bool hq : { false, true })
    {
        RoninAudioProcessor p;
        p.hqParameter()->setValueNotifyingHost (hq ? 1.0f : 0.0f);
        p.effectParameter()->setValueNotifyingHost (0.0f);   // effect off: dry
        p.prepareToPlay (48000.0, 64);
        runBlocks (p, 8, 64);
        juce::AudioBuffer<float> buffer (2, 64);
        juce::MidiBuffer midi;
        std::vector<float> out;
        for (int b = 0; b < 4; ++b)
        {
            buffer.clear();
            if (b == 0)
                buffer.setSample (0, 0, 0.5f);
            p.processBlock (buffer, midi);
            for (int i = 0; i < 64; ++i)
                out.push_back (buffer.getSample (0, i));
        }
        int peak = 0;
        for (int i = 1; i < static_cast<int> (out.size()); ++i)
            if (std::fabs (out[static_cast<size_t> (i)]) > std::fabs (out[static_cast<size_t> (peak)]))
                peak = i;
        check (peak == (hq ? 23 : 0), hq ? "HQ dry impulse peaks at 23 samples" : "1x dry impulse at 0 samples");
        check (peak == p.getLatencySamples(), "reported latency matches the measured path");
        // DC gain of the dry path is the same in both modes (the halfband has unity DC gain).
        double sum = 0.0;
        for (float v : out)
            sum += static_cast<double> (v);
        check (std::fabs (std::fabs (sum) - 0.5) < 0.01, "the dry impulse keeps its area in both modes");
    }
    finish ("testHqLatencyIsTwentyThree");
}

static void testTriDefaultFreshAndInit()
{
    RoninAudioProcessor p;
    check (p.triShape() == Vco::TriShape::Triangle, "a fresh instance is on TRIANGLE");
    p.triShapeParameter()->setValueNotifyingHost (1.0f);
    check (p.triShape() == Vco::TriShape::Parabola, "PARABOLA (legacy) is selectable");
    p.setCurrentProgram (kInitPreset);
    check (p.triShape() == Vco::TriShape::Triangle, "INIT (File -> New) is TRIANGLE");
    finish ("testTriDefaultFreshAndInit");
}

// ---- §6 migration and state format 2 ---------------------------------------------------------

namespace {

// A format-1 session: parameter attributes plus the v1 index blob, built from a rack in the processor's order.
juce::MemoryBlock makeV1State (const RoninAudioProcessor& /*layout*/, const std::vector<Cable>& cables,
                               const std::vector<std::pair<const char*, double>>& params)
{
    ExtIn ext; OutputModule output; NoiseModule noise; Vcf vcf; Vca1 vca1; Vca2 vca2; Eg1 eg1; MgModule mg; Vco vco;
    Eg2 eg2; Ring ring; Divider divider; Inverter inverter; Integrator integrator; Mixer mixer; SampleHold sh;
    PatchGraph g;
    Module* order[] = { &ext, &output, &noise, &vcf, &vca1, &vca2, &eg1, &mg, &vco, &eg2, &ring, &divider, &inverter,
                        &integrator, &mixer, &sh };
    for (Module* m : order)
        g.addModule (*m);
    jassert (layout.vcoGraphIndex() == 8 && layout.vcfGraphIndex() == 3 && layout.sampleHoldGraphIndex() == 15);
    for (const Cable& c : cables)
        g.connect (c.sourceModule, c.sourcePort, c.destModule, c.destPort);
    unsigned char blob[4096];
    const int bytes = g.getState (blob, static_cast<int> (sizeof blob));
    juce::XmlElement xml ("RONIN");   // format 1 has no format attribute
    for (const auto& kv : params)
        xml.setAttribute (kv.first, kv.second);
    xml.setAttribute ("graph", juce::String::toHexString (blob, bytes));
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (xml, block);
    return block;
}

bool reportHas (const RoninAudioProcessor& p, const char* text)
{
    for (const auto& line : p.loadReport())
        if (line.contains (text))
            return true;
    return false;
}

float param01 (RoninAudioProcessor& p, const char* section, const char* label)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p.parameterForPanelKnob (section, label));
    return parameter != nullptr ? parameter->getValue() : -1.0f;
}

double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

}

static void testTriDefaultByOrigin()
{
    RoninAudioProcessor fresh;
    check (fresh.triShape() == Vco::TriShape::Triangle, "fresh instance -> TRIANGLE");
    fresh.triShapeParameter()->setValueNotifyingHost (1.0f);
    fresh.setCurrentProgram (kInitPreset);
    check (fresh.triShape() == Vco::TriShape::Triangle, "new / INIT -> TRIANGLE");

    RoninAudioProcessor user;
    const auto v1 = makeV1State (user, {}, { { "vcfCutoff", 0.45 } });
    user.setStateInformation (v1.getData(), static_cast<int> (v1.getSize()));
    check (user.triShape() == Vco::TriShape::Parabola, "user format-1 state -> PARABOLA (legacy)");
    check (reportHas (user, "TRI SHAPE set to PARABOLA"), "the load report records M-R2");
    auto* attack = dynamic_cast<juce::RangedAudioParameter*> (user.parameterForPanelKnob ("EG 1", "ATTACK"));
    check (attack != nullptr && std::fabs (attack->getValue() - PanelDefault::kEg1Attack) < 1.0e-6f,
           "an EG knob the session did not store is not migrated twice");

    for (int stored : { 0, 1 })
    {
        RoninAudioProcessor saver;
        saver.triShapeParameter()->setValueNotifyingHost (static_cast<float> (stored));
        const auto v2 = saveState (saver);
        RoninAudioProcessor loader;
        loader.triShapeParameter()->setValueNotifyingHost (1.0f - static_cast<float> (stored));
        loader.setStateInformation (v2.getData(), static_cast<int> (v2.getSize()));
        check (loader.triShape() == (stored == 1 ? Vco::TriShape::Parabola : Vco::TriShape::Triangle),
               "format-2 state -> the stored value");
        check (loader.loadedFormat() == 2 && ! reportHas (loader, "TRI SHAPE set to PARABOLA"), "format 2 is not migrated");
    }
    finish ("testTriDefaultByOrigin");
}

static void testParabolaSelectable()
{
    RoninAudioProcessor p;
    p.triShapeParameter()->setValueNotifyingHost (1.0f);
    check (p.triShape() == Vco::TriShape::Parabola, "PARABOLA is selectable");
    check (p.triShapeParameter()->getCurrentChoiceName() == "PARABOLA (legacy)", "named PARABOLA (legacy)");
    const auto state = saveState (p);
    RoninAudioProcessor q;
    q.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    check (q.triShape() == Vco::TriShape::Parabola, "PARABOLA is saved and recalled");
    finish ("testParabolaSelectable");
}

static void testUserPatchCutoffCompensation()
{
    RoninAudioProcessor layout;
    const int vco = layout.vcoGraphIndex();
    const int vcf = layout.vcfGraphIndex();
    const int ext = layout.extInGraphIndex();
    {
        RoninAudioProcessor p;
        const auto v1 = makeV1State (p, { Cable { vco, Vco::kSaw, vcf, Vcf::kSigIn } }, { { "vcfCutoff", 0.45 } });
        p.setStateInformation (v1.getData(), static_cast<int> (v1.getSize()));
        const double now = static_cast<double> (param01 (p, "VCF", "CUTOFF"));
        const double oldHz = Vcf::effectiveHzFor (Vcf::knobHzFor (0.45), 2.5, Vcf::kLegacyInputPull);
        const double newHz = Vcf::effectiveHzFor (Vcf::knobHzFor (now), 2.5, Vcf::kInputPull);
        std::printf ("  saw: cutoff 0.45 -> %.4f, %.1f Hz vs %.1f Hz (%.3f c)\n", now, newHz, oldHz, cents (newHz, oldHz));
        check (std::fabs (now - 0.385) < 0.001, "VCO SAW -> VCF IN at 0.45 loads at 0.385");
        check (std::fabs (cents (newHz, oldHz)) < 1.0, "same effective cutoff within 1 cent");
        check (reportHas (p, "same cutoff for the VCO"), "compensation is reported");
    }
    {
        RoninAudioProcessor p;
        const auto v1 = makeV1State (p, { Cable { vco, Vco::kPulse, vcf, Vcf::kSigIn } }, { { "vcfCutoff", 0.45 } });
        p.setStateInformation (v1.getData(), static_cast<int> (v1.getSize()));
        const double now = static_cast<double> (param01 (p, "VCF", "CUTOFF"));
        check (std::fabs ((now - 0.45) + 0.130) < 0.002, "VCO PULSE -> VCF IN: delta c ~ -0.130");
    }
    {
        RoninAudioProcessor p;
        const auto v1 = makeV1State (p, { Cable { ext, 2, vcf, Vcf::kSigIn } }, { { "vcfCutoff", 0.45 } });
        p.setStateInformation (v1.getData(), static_cast<int> (v1.getSize()));
        check (std::fabs (param01 (p, "VCF", "CUTOFF") - 0.45f) < 1.0e-6f, "EXT IN -> VCF IN: cutoff unchanged");
        check (reportHas (p, "left as saved"), "... and listed in the load report");
    }
    {
        // A format-2 state is never compensated.
        RoninAudioProcessor saver;
        saver.connectJacks (vco, Vco::kSaw, vcf, Vcf::kSigIn);
        const auto v2 = saveState (saver);
        RoninAudioProcessor p;
        p.setStateInformation (v2.getData(), static_cast<int> (v2.getSize()));
        check (std::fabs (param01 (p, "VCF", "CUTOFF") - param01 (saver, "VCF", "CUTOFF")) < 1.0e-6f,
               "format 2 keeps its cutoff");
    }
    finish ("testUserPatchCutoffCompensation");
}

static void testFormat1MigrationEgAndLegacyInvert()
{
    RoninAudioProcessor p;
    const int eg2 = p.eg2GraphIndex();
    const int div = p.dividerGraphIndex();
    const int eg1 = p.eg1GraphIndex();
    const int ext = p.extInGraphIndex();
    const auto v1 = makeV1State (p,
                                 { Cable { eg2, Eg2::kDelayTrig, div, Divider::kIn },     // Gate -> V-trig: legacy
                                   Cable { eg2, Eg2::kDelayTrig, eg1, Eg1::kTrig },       // Gate -> S-trig: converted anyway
                                   Cable { ext, 3, eg1, Eg1::kTrig } },                   // S-trig source: never
                                 { { "eg1Attack", 0.5 }, { "eg1Decay", 0.5 }, { "eg1Release", 1.0 }, { "eg2Attack", 0.0 },
                                   { "eg2Release", 0.0 } });
    p.setStateInformation (v1.getData(), static_cast<int> (v1.getSize()));
    check (std::fabs (param01 (p, "EG 1", "ATTACK") - 0.5846f) < 2.0e-4f, "M-R1: attack 0.5 -> 0.5846 (0.6215 s)");
    check (std::fabs (param01 (p, "EG 1", "DECAY") - 0.5574f) < 2.0e-4f, "M-R1: decay 0.5 -> 0.5574");
    check (std::fabs (param01 (p, "EG 1", "RELEASE") - 0.976f) < 1.0e-3f, "M-R1: release 1.0 -> 0.976");
    check (std::fabs (param01 (p, "EG 2", "ATTACK") - 0.1661f) < 2.0e-4f, "M-R1: EG 2 attack 0.0 -> 0.1661");
    check (std::fabs (param01 (p, "EG 2", "RELEASE") - 0.1388f) < 2.0e-4f, "M-R1: EG 2 release 0.0 -> 0.1388");

    Cable cables[8];
    const int n = p.copyPublishedCables (cables, 8);
    check (n == 3, "three cables load");
    check (n == 3 && cables[0].legacyInvert && ! cables[1].legacyInvert && ! cables[2].legacyInvert,
           "M-R3: only the Gate -> non-S-trig cable gets legacyInvert");
    check (reportHas (p, "1 cable kept its old S-trig inversion"), "M-R3 is reported");

    // The flag survives a format-2 save and load.
    const auto v2 = saveState (p);
    RoninAudioProcessor q;
    q.setStateInformation (v2.getData(), static_cast<int> (v2.getSize()));
    Cable back[8];
    const int m = q.copyPublishedCables (back, 8);
    check (m == 3 && back[0].legacyInvert && ! back[1].legacyInvert, "legacyInvert round-trips in format 2");
    check (std::fabs (param01 (q, "EG 1", "ATTACK") - param01 (p, "EG 1", "ATTACK")) < 1.0e-6f, "format 2 is not re-migrated");
    finish ("testFormat1MigrationEgAndLegacyInvert");
}

static void testFormat2Xml()
{
    RoninAudioProcessor p;
    const auto state = saveState (p);
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    check (xml != nullptr && xml->getIntAttribute ("format") == 2 && xml->getStringAttribute ("unit") == "RONIN",
           "format=2 unit=RONIN");
    check (xml != nullptr && ! xml->hasAttribute ("graph"), "no index blob");
    auto* list = xml != nullptr ? xml->getChildByName ("CABLES") : nullptr;
    check (list != nullptr && list->getNumChildElements() == 8, "INIT's eight cables by jack id");
    if (list != nullptr && list->getNumChildElements() > 0)
    {
        auto* first = list->getChildElement (0);
        check (first->getStringAttribute ("from") == "EXT IN:MONO" && first->getStringAttribute ("to") == "VCF:IN",
               "cables are SECTION:LABEL ids, oldest first");
    }

    // Hand-written format-3 state: an unknown jack, a colour override, a prefixed id, an unknown attribute.
    juce::XmlElement future ("RONIN");
    future.setAttribute ("format", 3);
    future.setAttribute ("unit", "RONIN");
    future.setAttribute ("vcfCutoff", 0.25);
    future.setAttribute ("someFutureThing", 7);
    auto* cables = future.createNewChildElement ("CABLES");
    auto* a = cables->createNewChildElement ("CABLE");
    a->setAttribute ("from", "RONIN#2/VCO:SAW");
    a->setAttribute ("to", "VCF:IN");
    a->setAttribute ("colour", "ff112233");
    auto* b = cables->createNewChildElement ("CABLE");
    b->setAttribute ("from", "VCO:SINE");
    b->setAttribute ("to", "VCF:IN");
    auto* c = cables->createNewChildElement ("CABLE");
    c->setAttribute ("from", "VCF:IN");      // input -> input: not allowed
    c->setAttribute ("to", "VCA 1:IN");
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary (future, block);
    RoninAudioProcessor q;
    q.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    Cable loaded[8];
    const int n = q.copyPublishedCables (loaded, 8);
    check (n == 1 && loaded[0].colour == 0xff112233u, "the known cable loads with its colour");
    check (std::fabs (param01 (q, "VCF", "CUTOFF") - 0.25f) < 1.0e-6f, "known parameters load");
    check (reportHas (q, "newer RONIN"), "a newer format loads best-effort with a warning");
    check (reportHas (q, "unknown jack: VCO:SINE"), "an unknown jack is skipped and reported");
    check (reportHas (q, "does not allow"), "an illegal cable is skipped and reported");

    // Colour survives a save.
    const auto again = saveState (q);
    RoninAudioProcessor r;
    r.setStateInformation (again.getData(), static_cast<int> (again.getSize()));
    Cable rc[8];
    check (r.copyPublishedCables (rc, 8) == 1 && rc[0].colour == 0xff112233u, "colour override round-trips");

    // A corrupt format-1 blob is rejected and leaves the patch alone.
    RoninAudioProcessor s;
    juce::XmlElement bad ("RONIN");
    bad.setAttribute ("graph", "00ff");
    juce::MemoryBlock badBlock;
    juce::AudioProcessor::copyXmlToBinary (bad, badBlock);
    s.setStateInformation (badBlock.getData(), static_cast<int> (badBlock.getSize()));
    Cable sc[8];
    check (s.copyPublishedCables (sc, 8) == 8 && s.presetError().isNotEmpty(), "a corrupt v1 state is rejected");
    finish ("testFormat2Xml");
}

namespace {

bool sameCables (const Cable* a, int na, const Cable* b, int nb)
{
    if (na != nb)
        return false;
    for (int i = 0; i < na; ++i)
        if (a[i].sourceModule != b[i].sourceModule || a[i].sourcePort != b[i].sourcePort
            || a[i].destModule != b[i].destModule || a[i].destPort != b[i].destPort)
            return false;
    return true;
}

juce::MemoryBlock withPrefix (const juce::MemoryBlock& state, const juce::String& prefix)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    juce::MemoryBlock out;
    if (xml == nullptr)
        return out;
    if (auto* cables = xml->getChildByName ("CABLES"))
        for (auto* cable : cables->getChildIterator())
            for (const char* attr : { "from", "to" })
                cable->setAttribute (attr, prefix + cable->getStringAttribute (attr));
    juce::AudioProcessor::copyXmlToBinary (*xml, out);
    return out;
}

}

static void testSlashJackIdsStateRoundTrip()
{
    // JCS R6 ids whose LABEL contains '/' (VCO:HZ/V, VCO:V/OCT, DIV:/2, DIV:/4) save and load through the shared
    // jidai-common parser in the bare, RONIN/ and RONIN#N/ forms.
    RoninAudioProcessor p;
    const RackIndices rack = p.rackIndices();
    std::vector<std::string> slashIds;
    for (int jack = 0; jack < kPanelJackCount; ++jack)
    {
        const std::string id = std::string (kPanelJacks[jack].section) + ":" + kPanelJacks[jack].label;
        if (id.find ('/') == std::string::npos)
            continue;
        slashIds.push_back (id);
        int m = -1;
        int port = -1;
        check (patchstate::jackAddress (rack, id, m, port), "a slash id binds");
        bool connected = false;
        for (int other = 0; other < kPanelJackCount && ! connected; ++other)
        {
            if (other == jack)
                continue;
            const std::string otherId = std::string (kPanelJacks[other].section) + ":" + kPanelJacks[other].label;
            int m2 = -1;
            int p2 = -1;
            if (! patchstate::jackAddress (rack, otherId, m2, p2))
                continue;
            connected = p.connectJacks (m, port, m2, p2) == PatchGraph::ConnectResult::Ok
                        || p.connectJacks (m2, p2, m, port) == PatchGraph::ConnectResult::Ok;
        }
        check (connected, "every slash jack takes a cable");
    }
    check (slashIds.size() == 4, "four slash ids");

    const auto state = saveState (p);
    auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
    check (xml != nullptr, "state parses");
    if (xml != nullptr)
        for (const auto& id : slashIds)
        {
            bool found = false;
            if (auto* cables = xml->getChildByName ("CABLES"))
                for (auto* cable : cables->getChildIterator())
                    found = found || cable->getStringAttribute ("from").toStdString() == id
                            || cable->getStringAttribute ("to").toStdString() == id;
            check (found, "the saved state carries the slash id verbatim");
        }

    Cable original[kPatchBayMaxCables];
    const int n = p.copyPublishedCables (original, kPatchBayMaxCables);
    for (const char* prefix : { "", "RONIN/", "RONIN#3/" })
    {
        RoninAudioProcessor q;
        const auto block = withPrefix (state, prefix);
        q.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
        Cable loaded[kPatchBayMaxCables];
        const int nq = q.copyPublishedCables (loaded, kPatchBayMaxCables);
        check (sameCables (original, n, loaded, nq), "every cable, slash ids included, loads back to the same ports");
        check (q.presetError().isEmpty(), "no load error");
    }
    finish ("testSlashJackIdsStateRoundTrip");
}

// ---- Factory bank (docs/presets.md) ----

namespace {

bool usesExtIn (int index)
{
    // An effect program takes EXT IN MONO or GATE into the patch; the dry L and R cables alone do not count.
    const FactoryPreset& preset = kFactoryPresets[index];
    for (int i = 0; i < preset.cableCount; ++i)
    {
        const juce::String from (preset.cables[i].from);
        if (from == "EXT IN:MONO" || from == "EXT IN:GATE")
            return true;
    }
    return false;
}

// A representative effect input, 125 BPM: a decaying chord stab and a kick-like thump on every beat, a noise
// tick on the off-beats. Peaks near 0.6 (-4.4 dBFS); quiet between hits, so EXT IN GATE opens and closes.
void testInput (std::vector<float>& left, std::vector<float>& right, double rate)
{
    const double beat = 60.0 / 125.0;
    juce::Random random (12345);
    for (size_t n = 0; n < left.size(); ++n)
    {
        const double t = static_cast<double> (n) / rate;
        const double inBeat = std::fmod (t, beat);
        const double offBeat = std::fmod (t + beat * 0.5, beat);
        const double stab = std::exp (-inBeat / 0.15)
                          * (std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t)
                             + std::sin (2.0 * juce::MathConstants<double>::pi * 277.18 * t)
                             + std::sin (2.0 * juce::MathConstants<double>::pi * 329.63 * t)) * 0.10;
        const double kick = std::exp (-inBeat / 0.08) * std::sin (2.0 * juce::MathConstants<double>::pi * 55.0 * t) * 0.25;
        const double tick = std::exp (-offBeat / 0.02) * (static_cast<double> (random.nextFloat()) * 2.0 - 1.0) * 0.12;
        left[n] = static_cast<float> (stab + kick + tick);
        right[n] = static_cast<float> (stab * 0.9 + kick + tick);
    }
}

struct RenderStats {
    float peak = 0.0f;
    double rms = 0.0;
    bool finite = true;
};

RenderStats renderProgram (int index, bool hq, double seconds, juce::AudioBuffer<float>* keep)
{
    constexpr double rate = 48000.0;
    constexpr int block = 512;
    RoninAudioProcessor p;
    p.hqParameter()->setValueNotifyingHost (hq ? 1.0f : 0.0f);
    p.setCurrentProgram (index);
    p.prepareToPlay (rate, block);
    const int total = static_cast<int> (seconds * rate);
    std::vector<float> inL (static_cast<size_t> (total), 0.0f);
    std::vector<float> inR (static_cast<size_t> (total), 0.0f);
    if (usesExtIn (index))
        testInput (inL, inR, rate);   // effect programs get the test input; the rest play with silence at EXT IN

    if (keep != nullptr)
        keep->setSize (2, total);
    RenderStats stats;
    double sum = 0.0;
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;
    for (int start = 0; start < total; start += block)
    {
        const int n = std::min (block, total - start);
        buffer.setSize (2, n, false, false, true);
        for (int i = 0; i < n; ++i)
        {
            buffer.setSample (0, i, inL[static_cast<size_t> (start + i)]);
            buffer.setSample (1, i, inR[static_cast<size_t> (start + i)]);
        }
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < n; ++i)
            {
                const float y = buffer.getSample (ch, i);
                if (! std::isfinite (y))
                {
                    stats.finite = false;
                    continue;
                }
                stats.peak = std::max (stats.peak, std::fabs (y));
                sum += static_cast<double> (y) * static_cast<double> (y);
                if (keep != nullptr)
                    keep->setSample (ch, start + i, y);
            }
        }
    }
    stats.rms = std::sqrt (sum / (2.0 * static_cast<double> (total)));
    return stats;
}

juce::String knobDefaultsFromInit (const juce::XmlElement& init, const juce::String& id)
{
    return init.getStringAttribute (id);
}

}

// Every program loads, is on the true TRIANGLE with the effect on, saves its own knobs (by id) and cables
// (by jack id) in format 2, and the saved state round-trips byte for byte.
static void testFactoryBankStateRoundTrip()
{
    std::unique_ptr<juce::XmlElement> initXml;
    {
        RoninAudioProcessor init;
        init.setCurrentProgram (kInitPreset);
        const juce::MemoryBlock initState = saveState (init);
        initXml = juce::AudioProcessor::getXmlFromBinary (initState.getData(), static_cast<int> (initState.getSize()));
    }
    check (initXml != nullptr, "INIT state parses");
    if (initXml == nullptr)
    {
        finish ("testFactoryBankStateRoundTrip");
        return;
    }

    for (int index = 0; index < kFactoryPresetCount; ++index)
    {
        const FactoryPreset& preset = kFactoryPresets[index];
        const juce::String name (preset.name);
        auto fail = [&] (bool ok, const char* what)
        {
            if (! ok)
                std::printf ("  [%02d %s] %s\n", index + 1, preset.name, what);
            check (ok, what);
        };

        RoninAudioProcessor p;
        p.prepareToPlay (48000.0, 256);
        p.triShapeParameter()->setValueNotifyingHost (1.0f);   // start from PARABOLA: the program must switch it
        p.setCurrentProgram (index);
        fail (p.getCurrentProgram() == index, "the program is selected");
        fail (p.getProgramName (index) == name, "the host sees the program name");
        fail (p.triShape() == Vco::TriShape::Triangle, "the program is on the true TRIANGLE");
        fail (p.effectIsOn(), "the program is effect on");
        fail (p.presetError().isEmpty(), "no load error");

        const juce::MemoryBlock saved = saveState (p);
        const auto xml = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
        fail (xml != nullptr && xml->getIntAttribute ("format") == 2, "saved as state format 2");
        if (xml == nullptr)
            continue;
        fail (xml->getIntAttribute ("vcoTriShape", -1) == 0, "saved TRI SHAPE is TRIANGLE");

        // Knobs: the program's own values by id; every other knob is the INIT (default table) value.
        for (int k = 0; k < preset.knobCount; ++k)
        {
            const juce::String id (preset.knobs[k].id);
            fail (xml->hasAttribute (id), "every program knob id is a saved host parameter");
            fail (std::fabs (xml->getDoubleAttribute (id) - static_cast<double> (preset.knobs[k].value)) < 1.0e-6,
                  "the saved knob is the program value");
        }
        for (int a = 0; a < initXml->getNumAttributes(); ++a)
        {
            const juce::String id = initXml->getAttributeName (a);
            if (id == "effectOn" || id == "hqMode" || id == "vcoTriShape" || id.startsWith ("ui") || id == "format"
                || id == "unit")
                continue;
            if (factoryKnob (index, id.toRawUTF8(), -1.0f) >= 0.0f)
                continue;
            fail (xml->getStringAttribute (id) == knobDefaultsFromInit (*initXml, id),
                  "a knob the program does not set is the default table value");
        }

        // Cables: the program's jack ids, in order (oldest first).
        const auto* list = xml->getChildByName ("CABLES");
        fail (list != nullptr && list->getNumChildElements() == preset.cableCount, "every program cable is saved");
        if (list != nullptr)
        {
            int c = 0;
            for (auto* cable : list->getChildWithTagNameIterator ("CABLE"))
            {
                if (c < preset.cableCount)
                {
                    fail (cable->getStringAttribute ("from") == juce::String::fromUTF8 (preset.cables[c].from)
                              && cable->getStringAttribute ("to") == juce::String::fromUTF8 (preset.cables[c].to),
                          "saved cable matches the program, in order");
                }
                ++c;
            }
        }

        // Round trip: load the saved state into a fresh instance and save again, byte for byte.
        RoninAudioProcessor q;
        q.prepareToPlay (48000.0, 256);
        q.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
        const juce::MemoryBlock again = saveState (q);
        fail (again == saved, "the saved state round-trips byte for byte");
        fail (q.triShape() == Vco::TriShape::Triangle, "the reloaded program stays on TRIANGLE");
        fail (saveState (p) == saved, "saving twice gives the same bytes");
    }
    finish ("testFactoryBankStateRoundTrip");
}

// Every program sounds and stays in range: 4 s rendered at 48 kHz (HQ off and on). Effect programs get a
// representative drum-and-chord test input at EXT IN; the self-playing programs get silence, so they must play
// on their own. Peak above -60 dBFS, at or below 0 dBFS (1.0), no NaN or inf. Level checks only (no exact
// sample compare), so the result does not depend on FMA contraction or the compiler.
static void testFactoryBankRenders()
{
    const char* wavDir = std::getenv ("RONIN_PRESET_WAV_DIR");
    for (int index = 0; index < kFactoryPresetCount; ++index)
    {
        for (bool hq : { false, true })
        {
            juce::AudioBuffer<float> keep;
            const bool dump = wavDir != nullptr && ! hq;
            const RenderStats stats = renderProgram (index, hq, hq ? 2.0 : 4.0, dump ? &keep : nullptr);
            const double peakDb = 20.0 * std::log10 (std::max (1.0e-12, static_cast<double> (stats.peak)));
            const double rmsDb = 20.0 * std::log10 (std::max (1.0e-12, stats.rms));
            std::printf ("  %02d %-12s %s input %-5s peak %6.2f dBFS  rms %6.2f dBFS\n", index + 1,
                         kFactoryPresets[index].name, hq ? "HQ" : "1x", usesExtIn (index) ? "test" : "none", peakDb,
                         rmsDb);
            if (! stats.finite)
                std::printf ("  [%02d] NaN or inf in the output\n", index + 1);
            check (stats.finite, "no NaN or inf");
            check (stats.peak > 0.001f, "not silent: peak above -60 dBFS");
            check (stats.peak <= 1.0f, "no clipping: peak at or below 0 dBFS");
            if (dump)
            {
                const juce::File file = juce::File (wavDir).getChildFile (juce::String (index + 1).paddedLeft ('0', 2) + "_"
                                                                          + juce::String (kFactoryPresets[index].name).replaceCharacter (' ', '_')
                                                                          + ".wav");
                file.deleteFile();
                juce::WavAudioFormat format;
                if (auto stream = file.createOutputStream())
                {
                    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream.get(), 48000.0, 2, 24, {}, 0));
                    if (writer != nullptr)
                    {
                        stream.release();
                        writer->writeFromAudioSampleBuffer (keep, 0, keep.getNumSamples());
                    }
                }
            }
        }
    }
    finish ("testFactoryBankRenders");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    testHqDefaultOff();
    testHqLatencyIsTwentyThree();
    testTriDefaultFreshAndInit();
    testTriDefaultByOrigin();
    testParabolaSelectable();
    testUserPatchCutoffCompensation();
    testFormat1MigrationEgAndLegacyInvert();
    testFormat2Xml();
    testSlashJackIdsStateRoundTrip();
    testFactoryBankStateRoundTrip();
    testFactoryBankRenders();
    std::printf ("ProcessorTests: %d passed, %d failed\n", gPassed, gFailed);
    return gFailed == 0 ? 0 : 1;
}
