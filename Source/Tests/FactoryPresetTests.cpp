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
    check (kFactoryPresetCount == 13, "thirteen factory presets");
    check (kDefaultFactoryPreset == 2, "default program is Voice");
    const char* names[] = {
        "Dry", "Noise to mixer", "Voice", "Ring", "S&H", "Feedback", "Hold",
        "Filter loop", "MG into filter", "Stepped cutoff", "Ring drone", "Delayed bounce", "Self ring"
    };
    const bool effect[] = {
        false, true, true, true, true, true, true,
        true, true, true, true, true, true
    };
    const int counts[] = { 2, 2, 8, 3, 4, 4, 6, 4, 4, 5, 5, 5, 5 };
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

void checkNear (float value, float want, const char* message)
{
    if (std::fabs (value - want) > 1.0e-6f)
    {
        std::printf ("  FAIL %s (got %.6g, want %.6g)\n", message, static_cast<double> (value), static_cast<double> (want));
        ++gChecks;
    }
}

void applyHostRow (Rack& rack, int index)
{
    const FactoryProgramKnobs knobs = factoryProgramKnobs (index);
    rack.vcf.setKnob (Vcf::kKnobCutoff, knobs.vcfCutoff);
    rack.vcf.setKnob (Vcf::kKnobPeak, knobs.vcfPeak);
    rack.vcf.setKnob (Vcf::kKnobAmount, 0.68f);
    rack.vca1.setKnob (Vca1::kKnobLowCut, 0.68f);
    rack.vca1.setKnob (Vca1::kKnobIntensity, 0.85f);
    rack.eg1.setKnob (Eg1::kKnobAttack, knobs.eg1Attack);
    rack.eg1.setKnob (Eg1::kKnobDecay, knobs.eg1Decay);
    rack.eg1.setKnob (Eg1::kKnobSustain, knobs.eg1Sustain);
    rack.eg1.setKnob (Eg1::kKnobRelease, knobs.eg1Release);
    rack.mg.setKnob (MgModule::kKnobFrequency, factoryMgRate (index));
    rack.mg.setKnob (MgModule::kKnobPw, 0.30f);
    rack.vco.setKnob (Vco::kKnobScale, knobs.vcoRange);
    rack.vco.setKnob (Vco::kKnobFine, 0.30f);
    rack.vco.setKnob (Vco::kKnobPw, 0.68f);
    rack.vco.setKnob (Vco::kKnobAmountA, 0.42f);
    rack.vco.setKnob (Vco::kKnobAmountB, 0.78f);
    rack.integrator.setKnob (Integrator::kKnobTime, factoryIntegratorTime (index));
    rack.sampleHold.setKnob (SampleHold::kKnobRate, factorySampleHoldRate (index));
}

void armWet (Rack& rack)
{
    rack.ext.setButtonHeld (false);
    rack.output.setMix (1.0f);
    rack.output.setLevel (1.0f);
    rack.output.setOutputLevel (0.70f);
    rack.graph.prepare (48000.0);
}

int onlyDelayedIndex (const PatchGraph& graph)
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

bool presetFeedsGateOrExt (Rack& rack)
{
    Cable live[PatchGraph::kMaxCables] {};
    const int count = rack.graph.copyPublishedCables (live, PatchGraph::kMaxCables);
    for (int i = 0; i < count; ++i)
    {
        if (live[i].sourceModule == FactoryModule::Ext || live[i].destModule == FactoryModule::Ext)
            return true;
        Module* dest = rack.graph.moduleAt (live[i].destModule);
        if (dest == nullptr)
            return true;
        if (dest->port (live[i].destPort).type == PortType::Gate)
            return true;
    }
    return false;
}

bool loadReplacesPlanted (Rack& rack, int index, const FactoryCable* expected, int count)
{
    const bool planted = rack.graph.connect (FactoryModule::Ext, 0, FactoryModule::Output, 0);
    const bool loaded = loadFactoryPreset (rack.graph, index);
    return planted && loaded && cablesMatch (rack.graph, expected, count) && ! presetFeedsGateOrExt (rack);
}

int testSelfModPresets()
{
    const float closed = 0.0f;
    checkNear (factoryVca1Initial (0), closed, "Dry leaves VCA 1 Initial at 0");
    checkNear (factoryVca1Initial (1), closed, "Noise to mixer leaves VCA 1 Initial at 0");
    checkNear (factoryVca1Initial (kDefaultFactoryPreset), closed, "Voice leaves VCA 1 Initial at 0");
    checkNear (factoryVca1Initial (3), closed, "Ring leaves VCA 1 Initial at 0");
    checkNear (factoryVca1Initial (4), closed, "S&H leaves VCA 1 Initial at 0");
    checkNear (factoryVca1Initial (kFeedbackPreset), kFeedbackVca1Initial, "Feedback keeps VCA 1 Initial at 0.7");
    checkNear (factoryVca1Initial (kHoldPreset), closed, "Hold leaves VCA 1 Initial at 0");
    for (int index = kFilterLoopPreset; index <= kSelfRingPreset; ++index)
        checkNear (factoryVca1Initial (index), kFeedbackVca1Initial, "self-mod preset sets VCA 1 Initial to 0.7");

    const FactoryProgramKnobs voice = factoryProgramKnobs (kDefaultFactoryPreset);
    const FactoryProgramKnobs feedback = factoryProgramKnobs (kFeedbackPreset);
    const FactoryProgramKnobs hold = factoryProgramKnobs (kHoldPreset);
    const FactoryProgramKnobs filterLoop = factoryProgramKnobs (kFilterLoopPreset);
    check (voice.vcfCutoff == 0.50f && voice.vcfPeak == 0.30f && voice.eg1Attack == 0.50f
               && voice.eg1Decay == 0.30f && voice.eg1Sustain == 0.68f && voice.eg1Release == 0.42f
               && voice.vcoRange == 0.50f,
           "Voice keeps the shared knob row");
    check (feedback.vcfCutoff == voice.vcfCutoff && feedback.vcfPeak == voice.vcfPeak
               && feedback.eg1Attack == voice.eg1Attack && feedback.vcoRange == voice.vcoRange,
           "Feedback keeps the shared knob row");
    checkNear (hold.vcfCutoff, kHoldVcfCutoff, "Hold cutoff stays 0.45");
    checkNear (hold.vcfPeak, kHoldVcfPeak, "Hold peak stays 0.20");
    checkNear (hold.eg1Attack, kHoldEg1Attack, "Hold attack stays 0.10");
    checkNear (hold.vcoRange, kHoldVcoRange, "Hold range stays 8'");
    checkNear (filterLoop.vcfCutoff, kFilterLoopCutoff, "Filter loop cutoff is 0.4");
    checkNear (filterLoop.vcfPeak, kFilterLoopPeak, "Filter loop peak is 0.7");
    check (filterLoop.eg1Attack == voice.eg1Attack && filterLoop.vcoRange == voice.vcoRange,
           "Filter loop keeps the shared envelope and range");
    checkNear (factoryMgRate (kDefaultFactoryPreset), 0.50f, "Voice MG rate stays 0.50");
    checkNear (factoryMgRate (kFeedbackPreset), 0.50f, "Feedback MG rate stays 0.50");
    checkNear (factoryMgRate (kHoldPreset), 0.50f, "Hold MG rate stays 0.50");
    checkNear (factoryMgRate (kMgFilterPreset), kMgFilterRate, "MG into filter rate is 0.3");
    checkNear (factoryMgRate (kRingDronePreset), kRingDroneRate, "Ring drone rate is 0.25");
    checkNear (factorySampleHoldRate (4), 0.50f, "S&H rate stays 0.50");
    checkNear (factorySampleHoldRate (kSteppedCutoffPreset), kSteppedCutoffRate, "Stepped cutoff rate is 0.4");
    checkNear (factoryIntegratorTime (kDefaultFactoryPreset), 0.50f, "Voice integrator time stays 0.50");
    checkNear (factoryIntegratorTime (kFeedbackPreset), 0.50f, "Feedback integrator time stays 0.50");
    checkNear (factoryIntegratorTime (kHoldPreset), 0.50f, "Hold integrator time stays 0.50");
    checkNear (factoryIntegratorTime (kDelayedBouncePreset), kDelayedBounceTime, "Delayed bounce time is 0.6");

    const std::string processor = readFile (MS50_PROCESSOR_SOURCE);
    const std::string restore = functionBody (processor, "void MS50ModularAudioProcessor::applyProgramParameters");
    check (restore.find ("factoryProgramKnobs") != std::string::npos, "programs still restore the shared knob row");
    check (restore.find ("factoryMgRate") != std::string::npos, "programs restore the MG rate");
    check (restore.find ("factorySampleHoldRate") != std::string::npos, "programs restore the S&H rate");
    check (restore.find ("factoryIntegratorTime") != std::string::npos, "programs restore the integrator time");
    check (restore.find ("outputLevel_, 0.70f") != std::string::npos, "Output Level stays 0.70");
    check (restore.find ("outputMix_, 1.0f") != std::string::npos, "Output Mix stays 1");
    std::string presetHeader = MS50_PROCESSOR_SOURCE;
    const auto presetSlash = presetHeader.rfind ('/');
    check (presetSlash != std::string::npos, "factory header path");
    if (presetSlash != std::string::npos)
    {
        presetHeader.replace (presetSlash, std::string::npos, "/Modular/FactoryPresets.h");
        const std::string load = functionBody (readFile (presetHeader.c_str()), "inline bool loadFactoryPreset");
        const auto sharedRate = load.find ("SampleHold::kKnobRate, 0.50f");
        const auto namedKnobs = load.find ("writeFactoryProgramKnobs");
        check (sharedRate != std::string::npos && namedKnobs != std::string::npos && sharedRate < namedKnobs,
               "the shared S&H rate is written before the preset rate");
    }

    Rack rack;
    rack.addAll();

    check (loadReplacesPlanted (rack, kFilterLoopPreset, kPresetFilterLoop, 4), "Filter loop replaces cables");
    check (factoryPresetEffect (kFilterLoopPreset), "Filter loop is effect on");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "Filter loop sets VCA 1 Initial");
    checkNear (rack.vcf.presetKnob (Vcf::kKnobCutoff), kFilterLoopCutoff, "Filter loop writes cutoff");
    checkNear (rack.vcf.presetKnob (Vcf::kKnobPeak), kFilterLoopPeak, "Filter loop writes peak");
    check (rack.graph.delayedCableCount() == 1, "Filter loop delays one cable");
    check (onlyDelayedIndex (rack.graph) == 3, "Filter loop delays the newest cable");
    check (rack.graph.cableIsDelayed (3), "VCF SigOut to VCF Cutoff is delayed in Filter loop");
    check (! rack.ext.buttonHeld(), "Filter loop does not press Hold");
    applyHostRow (rack, kFilterLoopPreset);
    armWet (rack);
    skipSamples (rack, 64);
    float cutoffMin = 1.0e9f;
    float cutoffMax = -1.0e9f;
    for (int i = 0; i < 4096; ++i)
    {
        rack.graph.process();
        const float cutoff = rack.vcf.portValue[Vcf::kCutoff];
        if (cutoff < cutoffMin)
            cutoffMin = cutoff;
        if (cutoff > cutoffMax)
            cutoffMax = cutoff;
    }
    const double filterRms = takeRms (rack, 2400, false);
    const double filterHost = takeRms (rack, 2400, true);
    check (cutoffMax - cutoffMin > 0.5f, "Filter loop cutoff jack moves");
    check (filterRms > 0.02, "Filter loop is audible at VCA 1 with no Hold press");
    check (filterHost > 0.002, "Filter loop reaches Output Wet");

    unsigned char blob[4096];
    const int filterBytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (filterBytes > 16 && blob[0] == 'M' && blob[1] == 'S' && blob[2] == '5' && blob[3] == '0',
           "Filter loop state magic stays MS50");
    check (blob[4] == 1, "Filter loop saves as version 1");
    Rack filterAgain;
    filterAgain.addAll();
    check (filterAgain.graph.setState (blob, filterBytes), "Filter loop round trip");
    check (cablesMatch (filterAgain.graph, kPresetFilterLoop, 4), "Filter loop round trip kept the cables");
    check (filterAgain.graph.cableIsDelayed (3), "Filter loop round trip kept the delayed cutoff cable");
    checkNear (filterAgain.vcf.presetKnob (Vcf::kKnobCutoff), kFilterLoopCutoff, "Filter loop round trip kept cutoff");

    check (loadReplacesPlanted (rack, kMgFilterPreset, kPresetMgFilter, 4), "MG into filter replaces cables");
    check (rack.graph.delayedCableCount() == 0, "MG into filter has no delayed cable");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "MG into filter sets VCA 1 Initial");
    checkNear (rack.mg.presetKnob (MgModule::kKnobFrequency), kMgFilterRate, "MG into filter writes the rate");
    check (! rack.ext.buttonHeld(), "MG into filter does not press Hold");
    applyHostRow (rack, kMgFilterPreset);
    armWet (rack);
    skipSamples (rack, 200);
    double earlySweep = 0.0;
    for (int i = 0; i < 2000; ++i)
    {
        rack.graph.process();
        earlySweep += static_cast<double> (rack.vcf.portValue[Vcf::kCutoff]);
    }
    earlySweep /= 2000.0;
    skipSamples (rack, 48000);
    double lateSweep = 0.0;
    for (int i = 0; i < 2000; ++i)
    {
        rack.graph.process();
        lateSweep += static_cast<double> (rack.vcf.portValue[Vcf::kCutoff]);
    }
    lateSweep /= 2000.0;
    const double sweepRms = takeRms (rack, 2400, false);
    check (std::fabs (lateSweep - earlySweep) > 0.4, "MG into filter sweeps the cutoff");
    check (sweepRms > 0.02, "MG into filter is audible with no Hold press");

    check (loadReplacesPlanted (rack, kSteppedCutoffPreset, kPresetSteppedCutoff, 5), "Stepped cutoff replaces cables");
    check (rack.graph.delayedCableCount() == 0, "Stepped cutoff has no delayed cable");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "Stepped cutoff sets VCA 1 Initial");
    checkNear (rack.sampleHold.presetKnob (SampleHold::kKnobRate), kSteppedCutoffRate, "Stepped cutoff writes S&H rate after 0.50");
    check (! rack.ext.buttonHeld(), "Stepped cutoff does not press Hold");
    applyHostRow (rack, kSteppedCutoffPreset);
    armWet (rack);
    int jumps = 0;
    int held = 0;
    float previousCutoff = 0.0f;
    bool haveCutoff = false;
    double steppedSum = 0.0;
    constexpr int kStepListen = 48000 * 3;
    for (int i = 0; i < kStepListen; ++i)
    {
        rack.graph.process();
        const float cutoff = rack.vcf.portValue[Vcf::kCutoff];
        const double vca = static_cast<double> (rack.vca1.portValue[Vca1::kOut]);
        steppedSum += vca * vca;
        if (haveCutoff)
        {
            const float step = std::fabs (cutoff - previousCutoff);
            if (step > 0.25f)
                ++jumps;
            else if (step < 0.02f)
                ++held;
        }
        previousCutoff = cutoff;
        haveCutoff = true;
    }
    const double steppedRms = std::sqrt (steppedSum / static_cast<double> (kStepListen));
    check (jumps >= 2 && jumps <= 12, "Stepped cutoff jumps a few times");
    check (held > (kStepListen * 9) / 10, "Stepped cutoff holds between jumps");
    check (steppedRms > 0.02, "Stepped cutoff is audible with no Hold press");

    check (loadReplacesPlanted (rack, kRingDronePreset, kPresetRingDrone, 5), "Ring drone replaces cables");
    check (rack.graph.delayedCableCount() == 0, "Ring drone has no delayed cable");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "Ring drone sets VCA 1 Initial");
    checkNear (rack.mg.presetKnob (MgModule::kKnobFrequency), kRingDroneRate, "Ring drone writes the MG rate");
    check (! rack.ext.buttonHeld(), "Ring drone does not press Hold");
    applyHostRow (rack, kRingDronePreset);
    armWet (rack);
    skipSamples (rack, 1000);
    const double droneEarly = takeRms (rack, 2400, false);
    // Host MG pulse width is 0.30, so the triangle crosses 0 halfway through the rise.
    const double mgHz = 0.01 * std::pow (20000.0, static_cast<double> (kRingDroneRate));
    const int zeroSample = static_cast<int> ((0.30 * 0.5 / mgHz) * 48000.0);
    const int skipToZero = zeroSample - 1000 - 2400 - 1200;
    check (skipToZero > 0, "Ring drone quiet window is after the loud window");
    skipSamples (rack, skipToZero);
    const double droneLate = takeRms (rack, 2400, false);
    if (! (droneEarly > 0.02))
        std::printf ("  FAIL Ring drone early rms %.6g\n", droneEarly);
    if (! (droneLate * 4.0 < droneEarly))
        std::printf ("  FAIL Ring drone move early %.6g late %.6g\n", droneEarly, droneLate);
    check (droneEarly > 0.02, "Ring drone is audible with no Hold press");
    check (droneLate * 4.0 < droneEarly, "Ring drone moves with the MG triangle");

    check (loadReplacesPlanted (rack, kDelayedBouncePreset, kPresetDelayedBounce, 5), "Delayed bounce replaces cables");
    check (rack.graph.delayedCableCount() == 0, "Delayed bounce has no graph delay");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "Delayed bounce sets VCA 1 Initial");
    checkNear (rack.integrator.presetKnob (Integrator::kKnobTime), kDelayedBounceTime, "Delayed bounce writes integrator time");
    check (! rack.ext.buttonHeld(), "Delayed bounce does not press Hold");
    applyHostRow (rack, kDelayedBouncePreset);
    armWet (rack);
    double noiseStep = 0.0;
    double lagStep = 0.0;
    double lagSum = 0.0;
    double lagMean = 0.0;
    float previousNoise = 0.0f;
    float previousLag = 0.0f;
    constexpr int kLag = 8000;
    for (int i = 0; i < kLag; ++i)
    {
        rack.graph.process();
        const float noise = rack.noise.portValue[NoiseModule::kWhite];
        const float lag = rack.vcf.portValue[Vcf::kCutoff];
        lagSum += static_cast<double> (lag) * static_cast<double> (lag);
        lagMean += static_cast<double> (lag);
        if (i > 0)
        {
            noiseStep += std::fabs (static_cast<double> (noise - previousNoise));
            lagStep += std::fabs (static_cast<double> (lag - previousLag));
        }
        previousNoise = noise;
        previousLag = lag;
    }
    lagMean /= static_cast<double> (kLag);
    double lagVariance = (lagSum / static_cast<double> (kLag)) - lagMean * lagMean;
    if (lagVariance < 0.0)
        lagVariance = 0.0;
    const double lagDeviation = std::sqrt (lagVariance);
    const double bounceRms = takeRms (rack, 2400, false);
    // Time 0.6 is about 96 ms, so white noise on the cutoff moves by a few millivolts.
    if (! (lagDeviation > 0.003))
        std::printf ("  FAIL Delayed bounce cutoff deviation %.6g lagStep %.6g noiseStep %.6g\n",
                     lagDeviation, lagStep, noiseStep);
    check (noiseStep > lagStep * 20.0, "Delayed bounce cutoff moves slower than the noise");
    check (lagDeviation > 0.003, "Delayed bounce cutoff still moves");
    check (bounceRms > 0.02, "Delayed bounce is audible with no Hold press");

    check (loadReplacesPlanted (rack, kSelfRingPreset, kPresetSelfRing, 5), "Self ring replaces cables and keeps the cycle");
    check (factoryPresetEffect (kSelfRingPreset), "Self ring is effect on");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "Self ring sets VCA 1 Initial");
    check (rack.graph.delayedCableCount() == 1, "Self ring delays one cable");
    Cable selfCables[PatchGraph::kMaxCables] {};
    const int selfCount = rack.graph.copyPublishedCables (selfCables, PatchGraph::kMaxCables);
    const int selfDelayed = onlyDelayedIndex (rack.graph);
    check (selfCount == 5 && selfDelayed >= 0, "Self ring has one delayed index");
    check (selfDelayed >= 0 && sameCable (selfCables[selfDelayed], FactoryModule::Ring, Ring::kOut,
                                           FactoryModule::Ring, Ring::kB),
           "Ring Out to Ring B is the delayed cable");
    check (! rack.ext.buttonHeld(), "Self ring does not press Hold");
    applyHostRow (rack, kSelfRingPreset);
    armWet (rack);
    const double selfRms = takeRms (rack, 4800, false);
    const double selfHost = takeRms (rack, 2400, true);
    check (selfRms < 1.0e-5, "Self ring stays quiet at VCA 1 with Initial 0.7");
    check (selfHost < 1.0e-6, "Self ring stays quiet at Output Wet");
    const int selfBytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (selfBytes > 16 && blob[4] == 1, "Self ring saves as version 1");
    Rack selfAgain;
    selfAgain.addAll();
    check (selfAgain.graph.setState (blob, selfBytes), "Self ring round trip");
    check (cablesMatch (selfAgain.graph, kPresetSelfRing, 5), "Self ring round trip kept the cycle");
    Cable selfAgainCables[PatchGraph::kMaxCables] {};
    selfAgain.graph.copyPublishedCables (selfAgainCables, PatchGraph::kMaxCables);
    const int selfAgainDelayed = onlyDelayedIndex (selfAgain.graph);
    check (selfAgainDelayed >= 0 && sameCable (selfAgainCables[selfAgainDelayed], FactoryModule::Ring, Ring::kOut,
                                                FactoryModule::Ring, Ring::kB),
           "Self ring round trip kept Ring Out to Ring B delayed");

    check (loadFactoryPreset (rack.graph, 4), "S&H loads after Stepped cutoff");
    check (cablesMatch (rack.graph, kPresetSampleHold, 4), "S&H cables stay");
    checkNear (rack.sampleHold.presetKnob (SampleHold::kKnobRate), 0.50f, "S&H rate returns to 0.50");
    checkNear (rack.vca1.initial(), 0.0f, "S&H clears VCA 1 Initial");

    check (loadFactoryPreset (rack.graph, kDefaultFactoryPreset), "Voice loads after the self-mod presets");
    check (cablesMatch (rack.graph, kPresetVoice, 8), "Voice cables stay the eight");
    check (rack.graph.delayedCableCount() == 0, "Voice still adds no delay");
    checkNear (rack.vca1.initial(), 0.0f, "Voice still clears VCA 1 Initial");

    check (loadFactoryPreset (rack.graph, kFeedbackPreset), "Feedback loads after the self-mod presets");
    check (cablesMatch (rack.graph, kPresetFeedback, 4), "Feedback cables stay");
    check (rack.graph.delayedCableCount() == 1 && rack.graph.cableIsDelayed (3), "Feedback still delays VCF SigOut to VCF Cutoff");
    checkNear (rack.vca1.initial(), kFeedbackVca1Initial, "Feedback still sets VCA 1 Initial to 0.7");
    checkNear (factoryProgramKnobs (kFeedbackPreset).vcfCutoff, 0.50f, "Feedback cutoff travel stays 0.50");
    checkNear (factoryProgramKnobs (kFeedbackPreset).vcfPeak, 0.30f, "Feedback peak travel stays 0.30");

    check (loadFactoryPreset (rack.graph, kHoldPreset), "Hold loads after the self-mod presets");
    check (cablesMatch (rack.graph, kPresetHold, 6), "Hold cables stay");
    check (rack.graph.delayedCableCount() == 0, "Hold still has no delayed cable");
    check (! holdHasExtAudio (rack.graph), "Hold still has no Ext In audio cable");
    checkNear (rack.vca1.initial(), 0.0f, "Hold still leaves VCA 1 Initial at 0");
    checkNear (rack.eg1.presetKnob (Eg1::kKnobAttack), kHoldEg1Attack, "Hold still writes attack");
    checkNear (rack.vcf.presetKnob (Vcf::kKnobCutoff), kHoldVcfCutoff, "Hold still writes cutoff");
    checkNear (rack.vcf.presetKnob (Vcf::kKnobPeak), kHoldVcfPeak, "Hold still writes peak");
    checkNear (rack.vco.presetKnob (Vco::kKnobScale), kHoldVcoRange, "Hold still writes range");

    std::string doc = MS50_PROCESSOR_SOURCE;
    const auto source = doc.rfind ("/Source/");
    check (source != std::string::npos, "presets doc path");
    if (source != std::string::npos)
    {
        doc.replace (source, std::string::npos, "/docs/presets.md");
        const std::string text = readFile (doc.c_str());
        check (text.find ("thirteen programs") != std::string::npos, "presets.md counts thirteen programs");
        int heard = 0;
        for (std::size_t pos = 0; (pos = text.find ("With no Hold press", pos)) != std::string::npos; pos += 1)
            ++heard;
        check (heard == 6, "each self-mod line says what is heard with no Hold press");
        doc = MS50_PROCESSOR_SOURCE;
        doc.replace (source, std::string::npos, "/Source/UI/PatchBayView.cpp");
        const std::string view = readFile (doc.c_str());
        check (view.find ("\"FILTER LOOP\"") != std::string::npos, "the preset screen has FILTER LOOP");
        check (view.find ("\"MG FILTER\"") != std::string::npos, "the preset screen has MG FILTER");
        check (view.find ("\"STEP CUTOFF\"") != std::string::npos, "the preset screen has STEP CUTOFF");
        check (view.find ("\"RING DRONE\"") != std::string::npos, "the preset screen has RING DRONE");
        check (view.find ("\"DELAY BOUNCE\"") != std::string::npos, "the preset screen has DELAY BOUNCE");
        check (view.find ("\"SELF RING\"") != std::string::npos, "the preset screen has SELF RING");
    }
    return finish ("testSelfModPresets");
}
