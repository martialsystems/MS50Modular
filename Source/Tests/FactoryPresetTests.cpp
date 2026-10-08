// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "Tests/TestSuite.h"
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
#include "UI/FaceKnobs.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
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


static std::string repoFile (const char* relative)
{
    std::string path = RONIN_PROCESSOR_SOURCE;
    const auto source = path.rfind ("/Source/");
    if (source == std::string::npos)
        return {};
    path.replace (source, std::string::npos, relative);
    return readFile (path.c_str());
}

// The factory bank is INIT only. INIT is the fresh-instance patch: the eight connectFactoryCables cables.
int testFactoryPresetCount()
{
    check (kFactoryPresetCount == 1, "one factory preset");
    check (kDefaultFactoryPreset == 0 && kInitPreset == 0, "default program is INIT at index 0");
    check (std::strcmp (factoryPresetName (0), "INIT") == 0, "the preset is named INIT");
    check (factoryPresetEffect (0), "INIT is effect on");
    check (kFactoryPresets[0].cableCount == 8, "INIT has eight cables");
    check (std::strcmp (factoryPresetName (-1), "") == 0 && std::strcmp (factoryPresetName (1), "") == 0,
           "no name outside the bank");
    check (! factoryPresetEffect (1), "no effect flag outside the bank");

    Rack fresh;
    fresh.addAll();
    check (connectFactoryCables (fresh.graph, 0, 1, 3, 4, 6), "eight factory cables");
    check (cablesMatch (fresh.graph, kPresetInit, 8), "INIT is the eight factory cables, in order");
    check (fresh.graph.delayedCableCount() == 0, "the factory eight have no delayed cable");

    Rack loaded;
    loaded.addAll();
    check (loaded.graph.connect (2, 0, 5, Vca2::kIn), "a cable that INIT must replace");
    check (loaded.graph.connect (FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vcf, Vcf::kCutoff), "a feedback cable");
    check (loaded.graph.delayedCableCount() == 1, "the planted loop has a delayed cable");
    loaded.vca1.setKnob (Vca1::kKnobInitial, 0.7f);
    check (loadFactoryPreset (loaded.graph, kDefaultFactoryPreset), "INIT loads");
    check (loaded.graph.cableCount() == 8, "INIT replaces the old cables");
    check (cablesMatch (loaded.graph, kPresetInit, 8), "INIT cables");
    check (loaded.graph.delayedCableCount() == 0, "INIT clears the delayed cable");
    check (std::fabs (loaded.vca1.initial()) < 1.0e-6f, "INIT clears VCA 1 Initial");
    check (ronin::exactlyEqual (factoryVca1Initial (kDefaultFactoryPreset), 0.0f), "INIT does not set VCA 1 Initial");

    check (! loadFactoryPreset (loaded.graph, -1), "index -1 does not load");
    check (! loadFactoryPreset (loaded.graph, kFactoryPresetCount), "an index past the bank does not load");
    check (cablesMatch (loaded.graph, kPresetInit, 8), "a refused load keeps the cables");

    const std::string processor = readFile (RONIN_PROCESSOR_SOURCE);
    const std::string programs = functionBody (processor, "int RoninAudioProcessor::getNumPrograms()");
    const std::string ctor = functionBody (processor, "RoninAudioProcessor::RoninAudioProcessor()");
    const std::string choose = functionBody (processor, "void RoninAudioProcessor::setCurrentProgram");
    check (programs.find ("kFactoryPresetCount") != std::string::npos, "the host list reports the factory count");
    const std::string programKnobs = functionBody (processor, "void RoninAudioProcessor::applyProgramParameters");
    check (programKnobs.find ("factoryProgramKnobs") != std::string::npos, "a program restores its own host knobs");
    check (ctor.find ("setCurrentProgram (kDefaultFactoryPreset)") != std::string::npos,
           "a fresh instance loads INIT the same way choosing it does");
    check (ctor.find ("connectFactoryCables") == std::string::npos, "construction does not patch its own cable list");
    check (choose.find ("loadFactoryPreset") != std::string::npos, "choosing a program loads that preset");
    check (processor.find ("vca1Initial") != std::string::npos, "session state keeps VCA 1 Initial");
    // Format-1 sessions are restored (and migrated) in loadFormat1 (state format 2, JCS R7).
    const std::string restore = functionBody (processor, "void RoninAudioProcessor::loadFormat1");
    check (restore.find ("vca1Initial") != std::string::npos, "session restore reads VCA 1 Initial");
    check (restore.find ("kKnobInitial, 0.0f") != std::string::npos, "an old session leaves VCA 1 Initial at 0");

    std::string header = RONIN_PROCESSOR_SOURCE;
    const auto dot = header.rfind ('.');
    check (dot != std::string::npos, "processor path");
    if (dot != std::string::npos)
        header.replace (dot, std::string::npos, ".h");
    check (readFile (header.c_str()).find ("currentProgram_ = kDefaultFactoryPreset") != std::string::npos,
           "the current program starts on INIT");

    const std::string text = repoFile ("/docs/presets.md");
    check (! text.empty(), "presets.md is readable");
    check (text.find ("\xE2\x80\x94") == std::string::npos, "presets.md has no em dash");
    check (text.find ("INIT") != std::string::npos, "presets.md names INIT");
    check (text.find ("Ext In L to Output L") != std::string::npos, "INIT cables are written down");
    return finish ("testFactoryPresetCount");
}

int testInitPresetRoundTrip()
{
    Rack rack;
    rack.addAll();
    check (connectFactoryCables (rack.graph, 0, 1, 3, 4, 6), "starts from the factory cables");
    check (rack.graph.connect (8, Vco::kSaw, 1, 2), "an extra cable is stacked on wet");
    check (rack.graph.cableCount() == 9, "nine cables before the preset");
    rack.mixer.setKnob (Mixer::kKnobLevel1, 0.2f);
    rack.mixer.setKnob (Mixer::kKnobLevel2, 0.1f);
    rack.mixer.setKnob (Mixer::kKnobLevel3, 0.3f);
    rack.sampleHold.setKnob (SampleHold::kKnobRate, 0.9f);

    check (loadFactoryPreset (rack.graph, kInitPreset), "INIT loads");
    check (rack.graph.cableCount() == 8, "INIT replaces the extra cable");
    check (cablesMatch (rack.graph, kPresetInit, 8), "INIT cables");
    check (std::fabs (rack.mixer.presetKnob (Mixer::kKnobLevel1) - PanelDefault::kMixerLevel) < 1.0e-6f, "level 1 returns to 0.8");
    check (std::fabs (rack.mixer.presetKnob (Mixer::kKnobLevel2) - PanelDefault::kMixerLevel) < 1.0e-6f, "level 2 returns to 0.8");
    check (std::fabs (rack.mixer.presetKnob (Mixer::kKnobLevel3) - PanelDefault::kMixerLevel) < 1.0e-6f, "level 3 returns to 0.8");
    check (std::fabs (rack.sampleHold.presetKnob (SampleHold::kKnobRate) - PanelDefault::kSampleHoldRate) < 1.0e-6f,
           "S&H rate returns to 0.5");

    unsigned char blob[4096];
    const int bytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (bytes > 16, "version 1 state saved");
    check (blob[0] == 'R' && blob[1] == 'N' && blob[2] == 'I' && blob[3] == 'N', "INIT state magic is RNIN");
    check (blob[4] == 1, "saved version is 1");
    Rack again;
    again.addAll();
    check (again.graph.connect (0, 0, 1, 0), "destination starts with a different cable");
    check (again.graph.setState (blob, bytes), "version 1 round trip");
    check (cablesMatch (again.graph, kPresetInit, 8), "the loaded state replaced the destination cables");
    return finish ("testInitPresetRoundTrip");
}

int testPresetBadVersionStillRejected()
{
    Rack rack;
    rack.addAll();
    check (loadFactoryPreset (rack.graph, kInitPreset), "INIT loads");
    rack.mixer.setKnob (Mixer::kKnobLevel1, 0.25f);
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
    check (cablesMatch (rack.graph, kPresetInit, 8), "the INIT cables stay");

    blob[4] = 1;
    blob[0] = 'M';
    check (! rack.graph.setState (blob, bytes), "a blob with another magic is rejected");
    check (cablesMatch (rack.graph, kPresetInit, 8), "the INIT cables stay after a bad magic");
    return finish ("testPresetBadVersionStillRejected");
}

double takeRms (Rack& rack, int samples, bool host)
{
    double sum = 0.0;
    for (int i = 0; i < samples; ++i)
    {
        rack.graph.process();
        const double y = host ? static_cast<double> (rack.output.hostLeft())
                              : static_cast<double> (rack.vca1.portValue[Vca1::kOut]);
        sum += y * y;
    }
    if (samples <= 0)
        return 0.0;
    return std::sqrt (sum / static_cast<double> (samples));
}

// INIT is an effect: the host input runs through the VCF and VCA 1, and EG 1 opens them on the Ext In gate.
int testInitPlaysTheInput()
{
    Rack rack;
    rack.addAll();
    check (loadFactoryPreset (rack.graph, kInitPreset), "INIT loads");
    rack.output.setMix (1.0f);
    rack.output.setLevel (1.0f);
    rack.output.setOutputLevel (0.70f);
    rack.graph.prepare (48000.0);
    rack.ext.setButtonHeld (false);

    double quiet = 0.0;
    for (int i = 0; i < 4800; ++i)
    {
        rack.ext.setHostSample (0.0f, 0.0f);
        rack.graph.process();
        const double y = static_cast<double> (rack.vca1.portValue[Vca1::kOut]);
        quiet += y * y;
    }
    check (std::sqrt (quiet / 4800.0) < 1.0e-6, "no input and no gate is silence at VCA 1");

    rack.ext.setButtonHeld (true);
    double open = 0.0;
    double host = 0.0;
    constexpr int kListen = 9600;
    for (int i = 0; i < kListen; ++i)
    {
        const float x = 0.5f * static_cast<float> (std::sin (2.0 * 3.14159265358979 * 220.0 * i / 48000.0));
        rack.ext.setHostSample (x, x);
        rack.graph.process();
        if (i < kListen / 2)
            continue;
        const double y = static_cast<double> (rack.vca1.portValue[Vca1::kOut]);
        open += y * y;
        const double h = static_cast<double> (rack.output.hostLeft());
        host += h * h;
    }
    check (std::sqrt (open / (kListen / 2)) > 0.05, "HOLD opens EG 1 and the input reaches VCA 1");
    check (std::sqrt (host / (kListen / 2)) > 0.005, "the input reaches the host output");

    const std::string view = repoFile ("/Source/UI/PatchBayView.cpp");
    const std::string line = functionBody (view, "juce::String presetScreenLine");
    check (line.find ("hostName.toUpperCase()") != std::string::npos, "the preset screen shows the program's own name");
    check (line.find ("\"DRY\"") == std::string::npos, "the preset screen has no table of removed program names");
    return finish ("testInitPlaysTheInput");
}

static void checkNear (float value, float want, const char* message)
{
    if (std::fabs (value - want) > 1.0e-6f)
    {
        std::printf ("  FAIL %s (got %.6g, want %.6g)\n", message, static_cast<double> (value), static_cast<double> (want));
        ++gChecks;
    }
}

// FaceKnobs, layout.json (through PanelGeometry.inc), double-click reset and the INIT program
// all read Source/Modular/PanelDefaults.h. A fresh instance is INIT, so they must agree.
int testOneDefaultTable()
{
    const std::string processor = readFile (RONIN_PROCESSOR_SOURCE);
    const std::string restore = functionBody (processor, "void RoninAudioProcessor::applyProgramParameters");
    for (int i = 0; i < kPanelKnobCount; ++i)
    {
        const PanelKnobRec& knob = kPanelKnobs[i];
        const FaceKnobBinding binding = faceKnobBinding (knob.section, knob.label);
        if (binding.knob == FaceKnob::None)
        {
            std::printf ("  FAIL %s %s is not bound\n", knob.section, knob.label);
            ++gChecks;
            continue;
        }
        if (! ronin::exactlyEqual (binding.fallback, knob.valueDefault))
        {
            std::printf ("  FAIL %s %s: FaceKnobs %.4f, layout %.4f\n", knob.section, knob.label,
                         static_cast<double> (binding.fallback), static_cast<double> (knob.valueDefault));
            ++gChecks;
        }
        const std::string restored = std::string ("restore (") + binding.parameterId + "_,";
        if (restore.find (restored) == std::string::npos)
        {
            std::printf ("  FAIL %s is not restored by a program load\n", binding.parameterId);
            ++gChecks;
        }
    }

    auto fallback = [] (const char* section, const char* label) { return faceKnobBinding (section, label).fallback; };
    const int init = kDefaultFactoryPreset;
    const FactoryProgramKnobs row = factoryProgramKnobs (init);
    check (ronin::exactlyEqual (row.vcfCutoff, fallback ("VCF", "CUTOFF")) && ronin::exactlyEqual (row.vcfPeak, fallback ("VCF", "PEAK")), "INIT filter is the table");
    check (ronin::exactlyEqual (row.eg1Attack, fallback ("EG 1", "ATTACK")) && ronin::exactlyEqual (row.eg1Decay, fallback ("EG 1", "DECAY"))
               && ronin::exactlyEqual (row.eg1Sustain, fallback ("EG 1", "SUSTAIN")) && ronin::exactlyEqual (row.eg1Release, fallback ("EG 1", "RELEASE")),
           "INIT envelope is the table");
    check (ronin::exactlyEqual (row.vcoRange, fallback ("VCO", "RANGE")), "INIT range is the table");
    check (ronin::exactlyEqual (factoryMgRate (init), fallback ("MG", "RATE")), "INIT MG rate is the table");
    check (ronin::exactlyEqual (factorySampleHoldRate (init), fallback ("S&H", "RATE")), "INIT S&H rate is the table");
    check (ronin::exactlyEqual (factoryIntegratorTime (init), fallback ("INT", "TIME")), "INIT integrator time is the table");
    check (ronin::exactlyEqual (factoryVca1Initial (init), fallback ("VCA 1", "INITIAL")), "INIT VCA 1 Initial is the table");
    check (factoryPresetEffect (init), "INIT is effect on");

    // The old 0.05 / 0.3 / 0.6 / 0.3 defaults through M-R1 (real-time law, RONIN_Redesign §6).
    check (ronin::exactlyEqual (fallback ("EG 1", "ATTACK"), 0.2079f) && ronin::exactlyEqual (fallback ("EG 1", "DECAY"), 0.39f)
               && ronin::exactlyEqual (fallback ("EG 1", "SUSTAIN"), 0.60f) && ronin::exactlyEqual (fallback ("EG 1", "RELEASE"), 0.39f),
           "fresh EG 1 is 0.2079 / 0.39 / 0.6 / 0.39");
    check (ronin::exactlyEqual (fallback ("VCF", "CUTOFF"), 0.45f) && ronin::exactlyEqual (fallback ("VCF", "PEAK"), 0.20f) && ronin::exactlyEqual (fallback ("VCF", "MOD"), 0.40f),
           "fresh VCF is 0.45 / 0.2 / 0.4");
    check (ronin::exactlyEqual (fallback ("MIX", "LEVEL 1"), 0.80f) && ronin::exactlyEqual (fallback ("MIX", "LEVEL 2"), 0.80f) && ronin::exactlyEqual (fallback ("MIX", "LEVEL 3"), 0.80f),
           "mixer levels are 0.8");

    // The old placeholder cycle repeated 0.50, 0.30, 0.68, 0.42, 0.78 down every column.
    int cycle = 0;
    for (int i = 0; i + 3 < kPanelKnobCount; ++i)
    {
        if (ronin::exactlyEqual (kPanelKnobs[i].valueDefault, 0.50f) && ronin::exactlyEqual (kPanelKnobs[i + 1].valueDefault, 0.30f)
            && ronin::exactlyEqual (kPanelKnobs[i + 2].valueDefault, 0.68f) && ronin::exactlyEqual (kPanelKnobs[i + 3].valueDefault, 0.42f))
            ++cycle;
    }
    check (cycle == 0, "no 0.50 / 0.30 / 0.68 / 0.42 cycle");

    ExtIn ext;
    check (std::fabs (ext.presetKnob (ExtIn::kKnobThreshold) - fallback ("EXT IN", "THRESHOLD")) < 1.0e-4f,
           "Ext In threshold default is the module's 0.2 V");
    check (std::fabs (ext.presetKnob (ExtIn::kKnobRelease) - fallback ("EXT IN", "RELEASE")) < 1.0e-4f,
           "Ext In release default is the module's 80 ms");

    std::string layout = RONIN_PROCESSOR_SOURCE;
    const auto source = layout.rfind ("/Source/");
    if (source != std::string::npos)
    {
        layout.replace (source, std::string::npos, "/panel/assets/layout.json");
        const std::string text = readFile (layout.c_str());
        check (text.find ("\"law\": \"linear") != std::string::npos, "layout.json meter law is linear");
        check (text.find ("dB/20") == std::string::npos, "layout.json has no dB meter law");
    }
    return finish ("testOneDefaultTable");
}

static int onlyDelayedIndex (const PatchGraph& graph)
{
    const int count = graph.cableCount();
    int found = -1;
    for (int i = 0; i < count; ++i)
    {
        if (! graph.cableIsDelayed (i))
            continue;
        if (found >= 0)
            return -2;
        found = i;
    }
    return found;
}

// The cleared factory programs pinned these graph rules. A user patch still relies on them,
// so they are checked here with the same cable lists, built by hand.
int testSelfPatchFeedbackRules()
{
    const Cable filterLoop[] = {
        { FactoryModule::Vco, Vco::kSaw, FactoryModule::Vcf, Vcf::kSigIn },
        { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
        { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
        { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vcf, Vcf::kCutoff },
    };
    Rack rack;
    rack.addAll();
    check (rack.graph.setCables (filterLoop, 4), "a filter loop patch loads");
    check (rack.graph.delayedCableCount() == 1, "the filter loop delays one cable");
    check (onlyDelayedIndex (rack.graph) == 3, "the newest cable that closes the loop is the delayed one");
    rack.vca1.setKnob (Vca1::kKnobInitial, 0.7f);
    rack.output.setMix (1.0f);
    rack.output.setLevel (1.0f);
    rack.output.setOutputLevel (0.70f);
    rack.graph.prepare (48000.0);
    float cutoffMin = 1.0e9f;
    float cutoffMax = -1.0e9f;
    for (int i = 0; i < 4096; ++i)
    {
        rack.graph.process();
        const float cutoff = rack.vcf.portValue[Vcf::kCutoff];
        cutoffMin = std::min (cutoffMin, cutoff);
        cutoffMax = std::max (cutoffMax, cutoff);
    }
    check (cutoffMax - cutoffMin > 0.5f, "the filter output moves its own cutoff");
    check (takeRms (rack, 2400, false) > 0.02, "VCA 1 Initial 0.7 makes the loop audible with no gate");

    unsigned char blob[4096];
    const int bytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    Rack again;
    again.addAll();
    check (again.graph.setState (blob, bytes), "the filter loop round trips");
    check (again.graph.cableIsDelayed (3), "the round trip keeps the delayed cutoff cable");

    // Ring Out to Ring B is listed before the forward path. A same-module edge closes at once,
    // the later edges close nothing, so the self cable stays the delayed one.
    const Cable selfRing[] = {
        { FactoryModule::Vco, Vco::kSaw, FactoryModule::Ring, Ring::kA },
        { FactoryModule::Ring, Ring::kOut, FactoryModule::Ring, Ring::kB },
        { FactoryModule::Ring, Ring::kOut, FactoryModule::Vcf, Vcf::kSigIn },
        { FactoryModule::Vcf, Vcf::kSigOut, FactoryModule::Vca1, Vca1::kSigIn },
        { FactoryModule::Vca1, Vca1::kOut, FactoryModule::Output, 2 },
    };
    Rack ring;
    ring.addAll();
    check (ring.graph.setCables (selfRing, 5), "a self ring patch loads");
    check (ring.graph.delayedCableCount() == 1, "the self ring delays one cable");
    Cable live[PatchGraph::kMaxCables] {};
    const int count = ring.graph.copyPublishedCables (live, PatchGraph::kMaxCables);
    const int delayed = onlyDelayedIndex (ring.graph);
    check (count == 5 && delayed >= 0
               && sameCable (live[delayed], FactoryModule::Ring, Ring::kOut, FactoryModule::Ring, Ring::kB),
           "Ring Out to Ring B is the delayed cable");
    ring.vca1.setKnob (Vca1::kKnobInitial, 0.7f);
    ring.output.setMix (1.0f);
    ring.output.setLevel (1.0f);
    ring.graph.prepare (48000.0);
    check (takeRms (ring, 4800, false) < 1.0e-5, "an unseeded self ring stays quiet");
    const int ringBytes = ring.graph.getState (blob, static_cast<int> (sizeof blob));
    Rack ringAgain;
    ringAgain.addAll();
    check (ringAgain.graph.setState (blob, ringBytes), "the self ring round trips");
    Cable again2[PatchGraph::kMaxCables] {};
    ringAgain.graph.copyPublishedCables (again2, PatchGraph::kMaxCables);
    const int againDelayed = onlyDelayedIndex (ringAgain.graph);
    check (againDelayed >= 0 && sameCable (again2[againDelayed], FactoryModule::Ring, Ring::kOut, FactoryModule::Ring, Ring::kB),
           "the round trip keeps Ring Out to Ring B delayed");

    // Program loads restore the shared row, and the shared S&H rate is written before any program knob.
    const std::string processor = readFile (RONIN_PROCESSOR_SOURCE);
    const std::string restore = functionBody (processor, "void RoninAudioProcessor::applyProgramParameters");
    check (restore.find ("factoryProgramKnobs") != std::string::npos, "programs restore the shared knob row");
    check (restore.find ("factoryMgRate") != std::string::npos, "programs restore the MG rate");
    check (restore.find ("factorySampleHoldRate") != std::string::npos, "programs restore the S&H rate");
    check (restore.find ("factoryIntegratorTime") != std::string::npos, "programs restore the integrator time");
    check (restore.find ("outputLevel_, PanelDefault::kOutputLevel") != std::string::npos, "Output Level is the default table");
    check (restore.find ("outputMix_, PanelDefault::kOutputMix") != std::string::npos, "Output Mix is the default table");
    const std::string load = functionBody (repoFile ("/Source/Modular/FactoryPresets.h"), "inline bool loadFactoryPreset");
    const auto sharedRate = load.find ("SampleHold::kKnobRate, PanelDefault::kSampleHoldRate");
    const auto namedKnobs = load.find ("writeFactoryProgramKnobs");
    check (sharedRate != std::string::npos && namedKnobs != std::string::npos && sharedRate < namedKnobs,
           "the shared S&H rate is written before the program's own knobs");
    checkNear (factoryMgRate (kInitPreset), PanelDefault::kMgRate, "INIT MG rate is the table");
    checkNear (factorySampleHoldRate (kInitPreset), PanelDefault::kSampleHoldRate, "INIT S&H rate is the table");
    checkNear (factoryIntegratorTime (kInitPreset), PanelDefault::kIntegratorTime, "INIT integrator time is the table");
    return finish ("testSelfPatchFeedbackRules");
}
