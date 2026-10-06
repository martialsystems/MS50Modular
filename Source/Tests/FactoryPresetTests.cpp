// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/DefaultPatch.h"
#include "Modular/Divider.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/ExtIn.h"
#include "Modular/FactoryPresets.h"
#include "Modular/Integrator.h"
#include "Modular/Inverter.h"
#include "Modular/Mg.h"
#include "Modular/Mixer.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/Ring.h"
#include "Modular/SampleHold.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "Modular/Vco.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

namespace {

int gChecks = 0;

void check (bool ok, const char* message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message);
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

struct Rack {
    ExtIn ext;
    OutputModule output;
    NoiseModule noise;
    Vcf vcf;
    Vca1 vca1;
    Vca2 vca2;
    Eg1 eg1;
    MgModule mg;
    Vco vco;
    Eg2 eg2;
    Ring ring;
    Divider divider;
    Inverter inverter;
    Integrator integrator;
    Mixer mixer;
    SampleHold sampleHold;
    PatchGraph graph;

    void addAll()
    {
        graph.addModule (ext);
        graph.addModule (output);
        graph.addModule (noise);
        graph.addModule (vcf);
        graph.addModule (vca1);
        graph.addModule (vca2);
        graph.addModule (eg1);
        graph.addModule (mg);
        graph.addModule (vco);
        graph.addModule (eg2);
        graph.addModule (ring);
        graph.addModule (divider);
        graph.addModule (inverter);
        graph.addModule (integrator);
        graph.addModule (mixer);
        graph.addModule (sampleHold);
    }
};

bool sameCable (const Cable& cable, int sourceModule, int sourcePort, int destModule, int destPort)
{
    return cable.sourceModule == sourceModule && cable.sourcePort == sourcePort
        && cable.destModule == destModule && cable.destPort == destPort;
}

bool cablesMatch (const PatchGraph& graph, const FactoryCable* expected, int count)
{
    Cable live[PatchGraph::kMaxCables] {};
    const int liveCount = graph.copyPublishedCables (live, PatchGraph::kMaxCables);
    if (liveCount != count)
        return false;
    for (int i = 0; i < count; ++i)
    {
        if (! sameCable (live[i], expected[i].sourceModule, expected[i].sourcePort,
                         expected[i].destModule, expected[i].destPort))
            return false;
    }
    return true;
}

std::string readFile (const char* path)
{
    std::ifstream input (path);
    if (! input)
        return {};
    return std::string ((std::istreambuf_iterator<char> (input)), std::istreambuf_iterator<char>());
}

std::string functionBody (const std::string& text, const std::string& marker)
{
    const auto start = text.find (marker);
    if (start == std::string::npos)
        return {};
    const auto body = text.find ('{', start);
    if (body == std::string::npos)
        return {};
    int depth = 0;
    std::size_t end = body;
    for (; end < text.size(); ++end)
    {
        if (text[end] == '{')
            ++depth;
        else if (text[end] == '}')
        {
            --depth;
            if (depth == 0)
                break;
        }
    }
    if (end >= text.size())
        return {};
    return text.substr (start, end - start + 1);
}

}

int testFactoryPresetCount()
{
    check (kFactoryPresetCount == 6, "six factory presets");
    check (kDefaultFactoryPreset == 2, "default program is Voice");
    const char* names[] = { "Dry", "Noise to mixer", "Voice", "Ring", "S&H", "Feedback" };
    const bool effect[] = { false, true, true, true, true, true };
    const int counts[] = { 2, 2, 8, 3, 4, 4 };
    for (int i = 0; i < kFactoryPresetCount; ++i)
    {
        check (std::strcmp (factoryPresetName (i), names[i]) == 0, "preset name");
        check (factoryPresetEffect (i) == effect[i], "preset effect");
        check (kFactoryPresets[i].cableCount == counts[i], "preset cable count");
    }

    Rack voice;
    voice.addAll();
    check (connectFactoryCables (voice.graph, 0, 1, 3, 4, 6), "eight factory cables");
    check (cablesMatch (voice.graph, kPresetVoice, 8), "Voice is the eight factory cables");
    check (voice.graph.delayedCableCount() == 0, "the factory eight have no delayed cable");

    Rack loaded;
    loaded.addAll();
    check (loaded.graph.connect (2, 0, 5, Vca2::kIn), "a cable that Voice must replace");
    check (loadFactoryPreset (loaded.graph, kDefaultFactoryPreset), "Voice loads");
    check (loaded.graph.cableCount() == 8, "Voice replaces the old cable");
    check (cablesMatch (loaded.graph, kPresetVoice, 8), "Voice cables");
    check (loaded.graph.delayedCableCount() == 0, "Voice adds no delay");

    check (loadFactoryPreset (loaded.graph, 5), "Feedback loads");
    check (loaded.graph.cableCount() == 4, "Feedback replaces Voice");
    check (cablesMatch (loaded.graph, kPresetFeedback, 4), "Feedback cables");
    check (loaded.graph.delayedCableCount() == 1, "only the newest feedback cable is delayed");
    check (! loaded.graph.cableIsDelayed (0) && ! loaded.graph.cableIsDelayed (1) && ! loaded.graph.cableIsDelayed (2),
           "the older feedback cables stay zero-delay");
    check (loaded.graph.cableIsDelayed (3), "VCF SigOut to VCF Cutoff is the delayed cable");

    check (loadFactoryPreset (loaded.graph, 0), "Dry loads");
    check (cablesMatch (loaded.graph, kPresetDry, 2), "Dry keeps only the left and right cables");
    check (! factoryPresetEffect (0), "Dry is effect off");

    const std::string processor = readFile (MS50_PROCESSOR_SOURCE);
    const std::string programs = functionBody (processor, "int MS50ModularAudioProcessor::getNumPrograms()");
    const std::string ctor = functionBody (processor, "MS50ModularAudioProcessor::MS50ModularAudioProcessor()");
    const std::string choose = functionBody (processor, "void MS50ModularAudioProcessor::setCurrentProgram");
    check (programs.find ("kFactoryPresetCount") != std::string::npos, "the host list reports six programs");
    check (ctor.find ("connectFactoryCables") != std::string::npos, "the opening patch is the factory cables");
    check (ctor.find ("loadFactoryPreset") == std::string::npos, "construction does not swap in another preset");
    check (choose.find ("loadFactoryPreset") != std::string::npos, "choosing a program loads that preset");

    std::string header = MS50_PROCESSOR_SOURCE;
    const auto dot = header.rfind ('.');
    check (dot != std::string::npos, "processor path");
    if (dot != std::string::npos)
        header.replace (dot, std::string::npos, ".h");
    check (readFile (header.c_str()).find ("currentProgram_ = kDefaultFactoryPreset") != std::string::npos,
           "the current program starts on Voice");

    std::string doc = MS50_PROCESSOR_SOURCE;
    const auto source = doc.rfind ("/Source/");
    check (source != std::string::npos, "docs path");
    if (source != std::string::npos)
    {
        doc.replace (source, std::string::npos, "/docs/presets.md");
        const std::string text = readFile (doc.c_str());
        check (text.find ("\xE2\x80\x94") == std::string::npos, "presets.md has no em dash");
        for (int i = 0; i < kFactoryPresetCount; ++i)
            check (text.find (names[i]) != std::string::npos, "presets.md names the program");
        check (text.find ("Ext In L to Output L") != std::string::npos, "Dry cables are written down");
        check (text.find ("VCF SigOut to VCF Cutoff") != std::string::npos, "Feedback cables are written down");
    }
    return finish ("testFactoryPresetCount");
}

int testPresetNoiseToMixerRoundTrip()
{
    Rack rack;
    rack.addAll();
    check (connectFactoryCables (rack.graph, 0, 1, 3, 4, 6), "starts from the factory voice");
    check (rack.graph.connect (8, Vco::kSaw, 1, 2), "an extra cable is stacked on wet");
    check (rack.graph.cableCount() == 9, "nine cables before the preset");
    rack.mixer.setKnob (Mixer::kKnobLevel1, 0.2f);
    rack.mixer.setKnob (Mixer::kKnobLevel2, 0.1f);

    check (loadFactoryPreset (rack.graph, 1), "Noise to mixer loads");
    check (rack.graph.cableCount() == 2, "the preset replaces the old cables");
    check (cablesMatch (rack.graph, kPresetNoiseToMixer, 2), "white into In 1 and the mix into wet");
    check (std::fabs (rack.mixer.presetKnob (Mixer::kKnobLevel1) - 0.8f) < 1.0e-6f, "level 1 is 0.8");
    check (std::fabs (rack.mixer.presetKnob (Mixer::kKnobLevel2) - 0.8f) < 1.0e-6f, "level 2 returns to 0.8");
    check (factoryPresetEffect (1), "Noise to mixer is effect on");

    check (loadFactoryPreset (rack.graph, 3), "Ring replaces Noise to mixer");
    check (rack.graph.cableCount() == 3, "Ring does not keep the mixer cables");
    check (cablesMatch (rack.graph, kPresetRing, 3), "Ring cables");

    unsigned char blob[4096];
    const int bytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (bytes > 16, "version 1 state saved");
    check (blob[4] == 1, "saved version is 1");
    Rack again;
    again.addAll();
    check (again.graph.connect (0, 0, 1, 0), "destination starts with a different cable");
    check (again.graph.setState (blob, bytes), "version 1 round trip");
    check (cablesMatch (again.graph, kPresetRing, 3), "the loaded state replaced the destination cables");
    return finish ("testPresetNoiseToMixerRoundTrip");
}

int testPresetBadVersionStillRejected()
{
    Rack rack;
    rack.addAll();
    check (loadFactoryPreset (rack.graph, 1), "a good preset loads");
    const int cables = rack.graph.cableCount();
    const float level = rack.mixer.presetKnob (Mixer::kKnobLevel1);

    unsigned char blob[4096];
    const int bytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (bytes > 16 && blob[4] == 1, "saved version 1");
    blob[4] = 2;
    blob[5] = 0;
    blob[6] = 0;
    blob[7] = 0;

    check (! rack.graph.setState (blob, bytes), "version 2 is rejected");
    check (std::strcmp (rack.graph.stateError(), "preset version is not supported") == 0, "version error");
    check (rack.graph.cableCount() == cables, "rejected load keeps the cables");
    check (std::fabs (rack.mixer.presetKnob (Mixer::kKnobLevel1) - level) < 1.0e-6f, "rejected load keeps the level");
    check (cablesMatch (rack.graph, kPresetNoiseToMixer, 2), "the noise-to-mixer cables stay");
    return finish ("testPresetBadVersionStillRejected");
}
