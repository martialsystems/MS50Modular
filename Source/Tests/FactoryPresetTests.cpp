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

#include <cmath>
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

double vcaOutRms (Rack& rack, int samples)
{
    double sum = 0.0;
    const int skip = samples / 2;
    int used = 0;
    for (int i = 0; i < samples; ++i)
    {
        rack.graph.process();
        if (i < skip)
            continue;
        const double y = static_cast<double> (rack.vca1.portValue[Vca1::kOut]);
        sum += y * y;
        ++used;
    }
    if (used <= 0)
        return 0.0;
    return std::sqrt (sum / static_cast<double> (used));
}

}

int testFactoryPresetCount()
{
    check (kFactoryPresetCount == 7, "seven factory presets");
    check (kDefaultFactoryPreset == 2, "default program is Voice");
    const char* names[] = { "Dry", "Noise to mixer", "Voice", "Ring", "S&H", "Feedback", "Hold" };
    const bool effect[] = { false, true, true, true, true, true, true };
    const int counts[] = { 2, 2, 8, 3, 4, 4, 6 };
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
    check (std::fabs (loaded.vca1.initial() - kFeedbackVca1Initial) < 1.0e-6f, "Feedback sets VCA 1 Initial to 0.7");
    check (factoryVca1Initial (kDefaultFactoryPreset) == 0.0f, "Voice does not set VCA 1 Initial");
    loaded.graph.prepare (48000.0);
    check (vcaOutRms (loaded, 4800) > 0.02, "Feedback saw is audible with no gate");
    loaded.vca1.setKnob (Vca1::kKnobInitial, 0.0f);
    check (vcaOutRms (loaded, 4800) < 1.0e-6, "Initial 0 closes the Feedback VCA");

    check (loadFactoryPreset (loaded.graph, kDefaultFactoryPreset), "Voice loads after Feedback");
    check (std::fabs (loaded.vca1.initial()) < 1.0e-6f, "Voice clears VCA 1 Initial");
    check (cablesMatch (loaded.graph, kPresetVoice, 8), "Voice cables stay the eight");
    check (loaded.graph.delayedCableCount() == 0, "Voice still adds no delay");

    check (loadFactoryPreset (loaded.graph, 0), "Dry loads");
    check (cablesMatch (loaded.graph, kPresetDry, 2), "Dry keeps only the left and right cables");
    check (! factoryPresetEffect (0), "Dry is effect off");

    const std::string processor = readFile (MS50_PROCESSOR_SOURCE);
    const std::string programs = functionBody (processor, "int MS50ModularAudioProcessor::getNumPrograms()");
    const std::string ctor = functionBody (processor, "MS50ModularAudioProcessor::MS50ModularAudioProcessor()");
    const std::string choose = functionBody (processor, "void MS50ModularAudioProcessor::setCurrentProgram");
    check (programs.find ("kFactoryPresetCount") != std::string::npos, "the host list reports the factory count");
    const std::string programKnobs = functionBody (processor, "void MS50ModularAudioProcessor::applyProgramParameters");
    check (programKnobs.find ("factoryProgramKnobs") != std::string::npos, "a program restores its own host knobs");
    check (ctor.find ("connectFactoryCables") != std::string::npos, "the opening patch is the factory cables");
    check (ctor.find ("loadFactoryPreset") == std::string::npos, "construction does not swap in another preset");
    check (choose.find ("loadFactoryPreset") != std::string::npos, "choosing a program loads that preset");
    check (processor.find ("vca1Initial") != std::string::npos, "session state keeps VCA 1 Initial");
    const std::string restore = functionBody (processor, "void MS50ModularAudioProcessor::setStateInformation");
    check (restore.find ("vca1Initial") != std::string::npos, "session restore reads VCA 1 Initial");
    check (restore.find ("kKnobInitial, 0.0f") != std::string::npos, "an old session leaves VCA 1 Initial at 0");

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

void skipSamples (Rack& rack, int samples)
{
    for (int i = 0; i < samples; ++i)
        rack.graph.process();
}

bool holdHasExtAudio (const PatchGraph& graph)
{
    Cable live[PatchGraph::kMaxCables] {};
    const int count = graph.copyPublishedCables (live, PatchGraph::kMaxCables);
    for (int i = 0; i < count; ++i)
    {
        if (live[i].sourceModule == FactoryModule::Ext && live[i].sourcePort >= 0 && live[i].sourcePort <= 2)
            return true;
    }
    return false;
}

int testHoldPreset()
{
    const FactoryProgramKnobs voice = factoryProgramKnobs (kDefaultFactoryPreset);
    const FactoryProgramKnobs feedback = factoryProgramKnobs (kFeedbackPreset);
    const FactoryProgramKnobs hold = factoryProgramKnobs (kHoldPreset);
    check (voice.eg1Attack == 0.50f && voice.eg1Decay == 0.30f && voice.eg1Sustain == 0.68f
               && voice.eg1Release == 0.42f && voice.vcfCutoff == 0.50f && voice.vcfPeak == 0.30f
               && voice.vcoRange == 0.50f,
           "Voice keeps the shared knob row");
    check (feedback.eg1Attack == voice.eg1Attack && feedback.eg1Sustain == voice.eg1Sustain
               && feedback.vcfCutoff == voice.vcfCutoff && feedback.vcoRange == voice.vcoRange,
           "Feedback keeps the shared knob row");
    check (std::fabs (hold.eg1Attack - kHoldEg1Attack) < 1.0e-6f, "Hold attack is 0.10");
    check (std::fabs (hold.eg1Decay - kHoldEg1Decay) < 1.0e-6f, "Hold decay is 0.30");
    check (std::fabs (hold.eg1Sustain - kHoldEg1Sustain) < 1.0e-6f, "Hold sustain is 0.70");
    check (std::fabs (hold.eg1Release - kHoldEg1Release) < 1.0e-6f, "Hold release is 0.40");
    check (std::fabs (hold.vcfCutoff - kHoldVcfCutoff) < 1.0e-6f, "Hold cutoff is 0.45");
    check (std::fabs (hold.vcfPeak - kHoldVcfPeak) < 1.0e-6f, "Hold peak is 0.20");
    check (std::fabs (hold.vcoRange - kHoldVcoRange) < 1.0e-6f, "Hold range is the 8' footage");

    Rack rack;
    rack.addAll();
    check (rack.graph.connect (0, 0, 1, 0), "a cable that Hold must replace");
    check (loadFactoryPreset (rack.graph, kHoldPreset), "Hold loads");
    check (rack.graph.cableCount() == 6, "Hold replaces the old cable");
    check (cablesMatch (rack.graph, kPresetHold, 6), "Hold cables");
    check (rack.graph.delayedCableCount() == 0, "Hold has no delayed cable");
    check (! holdHasExtAudio (rack.graph), "Hold has no Ext In audio cable");
    check (factoryPresetEffect (kHoldPreset), "Hold is effect on");
    check (std::fabs (rack.vca1.initial()) < 1.0e-6f, "Hold leaves VCA 1 Initial at 0");
    check (std::fabs (rack.eg1.presetKnob (Eg1::kKnobAttack) - kHoldEg1Attack) < 1.0e-6f, "Hold writes EG 1 Attack");
    check (std::fabs (rack.eg1.presetKnob (Eg1::kKnobDecay) - kHoldEg1Decay) < 1.0e-6f, "Hold writes EG 1 Decay");
    check (std::fabs (rack.eg1.presetKnob (Eg1::kKnobSustain) - kHoldEg1Sustain) < 1.0e-6f, "Hold writes EG 1 Sustain");
    check (std::fabs (rack.eg1.presetKnob (Eg1::kKnobRelease) - kHoldEg1Release) < 1.0e-6f, "Hold writes EG 1 Release");
    check (std::fabs (rack.vcf.presetKnob (Vcf::kKnobCutoff) - kHoldVcfCutoff) < 1.0e-6f, "Hold writes VCF Cutoff");
    check (std::fabs (rack.vcf.presetKnob (Vcf::kKnobPeak) - kHoldVcfPeak) < 1.0e-6f, "Hold writes VCF Peak");
    check (std::fabs (rack.vco.presetKnob (Vco::kKnobScale) - kHoldVcoRange) < 1.0e-6f, "Hold writes VCO Range");
    check (rack.vco.presetScaleIndex() == 2, "Hold range is 8', a mid note");

    rack.output.setMix (1.0f);
    rack.output.setLevel (1.0f);
    rack.output.setOutputLevel (0.70f);
    rack.graph.prepare (48000.0);
    rack.ext.setButtonHeld (false);

    double silentSum = 0.0;
    double silentHost = 0.0;
    int rises = 0;
    float previous = 0.0f;
    constexpr int kListen = 24000;
    for (int i = 0; i < kListen; ++i)
    {
        rack.graph.process();
        const double vca = static_cast<double> (rack.vca1.portValue[Vca1::kOut]);
        const double host = static_cast<double> (rack.output.hostLeft());
        silentSum += vca * vca;
        silentHost += host * host;
        const float saw = rack.vco.portValue[Vco::kSaw];
        if (previous <= 0.0f && saw > 0.0f)
            ++rises;
        previous = saw;
    }
    const double silentRms = std::sqrt (silentSum / static_cast<double> (kListen));
    const double silentHostRms = std::sqrt (silentHost / static_cast<double> (kListen));
    const double sawHz = static_cast<double> (rises) * (48000.0 / static_cast<double> (kListen));
    check (silentRms < 1.0e-5, "button up is silence at VCA 1");
    check (silentHostRms < 1.0e-6, "button up is silence at the output");
    check (std::fabs (rack.vcf.portValue[Vcf::kCutoff]) < 1.0e-3f, "button up leaves the filter cutoff closed");
    check (sawHz > 110.0 && sawHz < 160.0, "the saw is a mid note near 130 Hz");

    rack.ext.setButtonHeld (true);
    const double early = takeRms (rack, 48, false);
    skipSamples (rack, 8000);
    const double open = takeRms (rack, 2400, false);
    const double openHost = takeRms (rack, 2400, true);
    const float openCutoff = rack.vcf.portValue[Vcf::kCutoff];
    check (open > 0.05, "button down makes the saw audible");
    check (early < open, "button down fades the saw in");
    check (openHost > 0.005, "button down reaches Output Wet");
    check (openCutoff > 3.0f, "button down opens the filter");

    rack.ext.setButtonHeld (false);
    const double releasing = takeRms (rack, 4800, false);
    skipSamples (rack, 24000);
    const double released = takeRms (rack, 2400, false);
    check (releasing < open, "button up starts the release");
    check (released < 1.0e-4, "button up returns to silence");
    check (std::fabs (rack.vcf.portValue[Vcf::kCutoff]) < 0.05f, "button up closes the filter");

    unsigned char blob[4096];
    const int bytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (bytes > 16 && blob[4] == 1, "Hold saves as version 1");
    Rack again;
    again.addAll();
    check (again.graph.connect (0, 0, 1, 0), "round trip starts from another cable");
    check (again.graph.setState (blob, bytes), "Hold round trip");
    check (cablesMatch (again.graph, kPresetHold, 6), "the round trip kept the Hold cables");
    check (std::fabs (again.eg1.presetKnob (Eg1::kKnobAttack) - kHoldEg1Attack) < 1.0e-6f, "the round trip kept Hold attack");

    check (loadFactoryPreset (rack.graph, kDefaultFactoryPreset), "Voice loads after Hold");
    check (cablesMatch (rack.graph, kPresetVoice, 8), "Voice cables stay the eight");
    check (std::fabs (rack.vca1.initial()) < 1.0e-6f, "Voice still clears VCA 1 Initial");
    check (loadFactoryPreset (rack.graph, kFeedbackPreset), "Feedback loads after Hold");
    check (cablesMatch (rack.graph, kPresetFeedback, 4), "Feedback cables stay");
    check (std::fabs (rack.vca1.initial() - kFeedbackVca1Initial) < 1.0e-6f, "Feedback still opens VCA 1 Initial to 0.7");
    check (rack.graph.delayedCableCount() == 1, "Feedback still delays one cable");
    check (loadFactoryPreset (rack.graph, kHoldPreset), "Hold loads after Feedback");
    check (std::fabs (rack.vca1.initial()) < 1.0e-6f, "Hold clears the Feedback initial");

    std::string view = MS50_PROCESSOR_SOURCE;
    const auto source = view.rfind ("/Source/");
    check (source != std::string::npos, "panel path");
    if (source != std::string::npos)
    {
        view.replace (source, std::string::npos, "/Source/UI/PatchBayView.cpp");
        check (readFile (view.c_str()).find ("\"FEEDBACK\", \"HOLD\"") != std::string::npos, "the preset screen has HOLD");
    }
    return finish ("testHoldPreset");
}
