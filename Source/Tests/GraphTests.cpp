// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

static std::atomic<long> gAllocations { 0 };

void* operator new (std::size_t size)
{
    gAllocations.fetch_add (1, std::memory_order_relaxed);
    if (void* memory = std::malloc (size))
        return memory;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t size)
{
    return ::operator new (size);
}

void operator delete (void* memory) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

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

class GainModule : public Module {
public:
    int numPorts() const override { return 2; }

    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", PortType::Audio, PortDir::In };
        return { "Out", PortType::Audio, PortDir::Out };
    }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override { portValue[1] = portValue[0] * 1.0f; }
};

class ConstantModule : public Module {
public:
    explicit ConstantModule (float value, PortType type)
        : value_ (value), type_ (type)
    {
    }

    int numPorts() const override { return 2; }

    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", PortType::Audio, PortDir::In };
        return { "Out", type_, PortDir::Out };
    }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override { portValue[1] = value_; }

private:
    float value_;
    PortType type_;
};

class SinkModule : public Module {
public:
    explicit SinkModule (PortType inputType)
        : inputType_ (inputType)
    {
    }

    int numPorts() const override { return 2; }

    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", inputType_, PortDir::In };
        return { "Out", PortType::Audio, PortDir::Out };
    }

    void setKnob (int, float) override {}

    void prepare (double rate) override { sampleRate = rate; }

    void processSample() override { portValue[1] = portValue[0]; }

private:
    PortType inputType_;
};

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

int testInputSumsTwoCables()
{
    PatchGraph graph;
    ConstantModule a (1.0f, PortType::Audio);
    ConstantModule b (0.5f, PortType::Audio);
    GainModule dest;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int id = graph.addModule (dest);

    check (graph.attemptConnect (ia, 1, id, 0) == PatchGraph::ConnectResult::Ok, "first into dest");
    check (graph.attemptConnect (ib, 1, id, 0) == PatchGraph::ConnectResult::Ok, "second into dest");
    graph.prepare (48000.0);
    graph.process();
    check (dest.portValue[0] == 1.5f, "dest input is 1.5");
    return finish ("testInputSumsTwoCables");
}

int testSecondCableDoesNotReplaceFirst()
{
    PatchGraph graph;
    ConstantModule a (1.0f, PortType::Audio);
    ConstantModule b (0.5f, PortType::Audio);
    OutputModule output;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int iOut = graph.addModule (output);

    check (graph.connect (ia, 1, iOut, 2), "first wet cable");
    check (graph.connect (ib, 1, iOut, 2), "second wet cable");

    Cable published[8];
    const int count = graph.copyPublishedCables (published, 8);
    int wet = 0;
    bool sawA = false;
    bool sawB = false;
    for (int i = 0; i < count; ++i)
    {
        if (published[i].destModule != iOut || published[i].destPort != 2)
            continue;
        ++wet;
        if (published[i].sourceModule == ia && published[i].sourcePort == 1)
            sawA = true;
        if (published[i].sourceModule == ib && published[i].sourcePort == 1)
            sawB = true;
    }
    check (count == 2, "snapshot has both cables");
    check (wet == 2, "both land on Output Wet");
    check (sawA && sawB, "neither source was replaced");
    return finish ("testSecondCableDoesNotReplaceFirst");
}

int testFanOutAllowed()
{
    PatchGraph graph;
    ConstantModule a (0.25f, PortType::Audio);
    GainModule b;
    GainModule c;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int ic = graph.addModule (c);

    check (graph.connect (ia, 1, ib, 0), "A to B");
    check (graph.connect (ia, 1, ic, 0), "A to C");

    graph.prepare (48000.0);
    graph.process();
    check (b.portValue[1] == 0.25f, "B received fan-out");
    check (c.portValue[1] == 0.25f, "C received fan-out");
    return finish ("testFanOutAllowed");
}

int testRejectSignalIntoGate()
{
    PatchGraph graph;
    ConstantModule audio (1.0f, PortType::Audio);
    ConstantModule cv (1.0f, PortType::CV);
    SinkModule gate (PortType::Gate);
    const int iAudio = graph.addModule (audio);
    const int iCv = graph.addModule (cv);
    const int iGate = graph.addModule (gate);

    // SCHEMATICS.md: Audio and CV into a Gate input are refused. Gate may feed Audio, CV, or Gate.
    check (! graph.connect (iAudio, 1, iGate, 0), "Audio into Gate");
    check (graph.cableCount() == 0, "Audio reject changed nothing");
    check (! graph.connect (iCv, 1, iGate, 0), "CV into Gate");
    check (graph.cableCount() == 0, "CV reject changed nothing");

    ConstantModule gateSource (1.0f, PortType::Gate);
    SinkModule audioSink (PortType::Audio);
    SinkModule cvSink (PortType::CV);
    SinkModule gateSink (PortType::Gate);
    const int iGateOut = graph.addModule (gateSource);
    const int iAudioIn = graph.addModule (audioSink);
    const int iCvIn = graph.addModule (cvSink);
    const int iGateIn = graph.addModule (gateSink);
    check (graph.connect (iGateOut, 1, iAudioIn, 0), "Gate into Audio");
    check (graph.connect (iGateOut, 1, iCvIn, 0), "Gate into CV");
    check (graph.connect (iGateOut, 1, iGateIn, 0), "Gate into Gate");
    return finish ("testRejectSignalIntoGate");
}

int testConnectStatusStrings()
{
    PatchGraph graph;
    ConstantModule a (1.0f, PortType::Audio);
    GainModule b;
    ConstantModule c (2.0f, PortType::Audio);
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int ic = graph.addModule (c);

    const auto first = graph.attemptConnect (ia, 1, ib, 0);
    check (first == PatchGraph::ConnectResult::Ok, "status path first cable");
    check (PatchGraph::connectResultText (first)[0] == '\0', "success clears the status");
    const auto stacked = graph.attemptConnect (ic, 1, ib, 0);
    check (stacked == PatchGraph::ConnectResult::Ok, "second cable stacks");
    check (PatchGraph::connectResultText (stacked)[0] == '\0', "stack clears the status");
    check (graph.cableCount() == 2, "second cable appended");

    check (graph.attemptConnect (ia, 0, ib, 0) == PatchGraph::ConnectResult::BadType, "in to in");
    check (graph.attemptConnect (ia, 1, ic, 1) == PatchGraph::ConnectResult::BadType, "out to out");
    check (graph.attemptConnect (ia, 1, ia, 1) == PatchGraph::ConnectResult::BadType, "port to itself");

    PatchGraph cycle;
    GainModule left;
    GainModule right;
    const int iLeft = cycle.addModule (left);
    const int iRight = cycle.addModule (right);
    check (cycle.attemptConnect (iLeft, 1, iRight, 0) == PatchGraph::ConnectResult::Ok, "cycle setup");
    const auto closed = cycle.attemptConnect (iRight, 1, iLeft, 0);
    check (closed == PatchGraph::ConnectResult::Cycle, "cycle result");
    check (std::strcmp (PatchGraph::connectResultText (closed), "feedback is not available until step 19") == 0,
           "cycle string");
    check (cycle.cableCount() == 1, "cycle connect added nothing");
    check (cycle.attemptConnect (iLeft, 1, iLeft, 0) == PatchGraph::ConnectResult::Cycle, "module into itself");

    PatchGraph types;
    ConstantModule audio (1.0f, PortType::Audio);
    SinkModule gate (PortType::Gate);
    const int iAudio = types.addModule (audio);
    const int iGate = types.addModule (gate);
    const auto badType = types.attemptConnect (iAudio, 1, iGate, 0);
    check (badType == PatchGraph::ConnectResult::BadType, "audio into gate result");
    check (std::strcmp (PatchGraph::connectResultText (badType), "that jack does not take this cable") == 0,
           "type string");
    check (types.cableCount() == 0, "type reject added nothing");

    PatchGraph both;
    GainModule src;
    GainModule mid;
    ConstantModule other (0.0f, PortType::Audio);
    const int iSrc = both.addModule (src);
    const int iMid = both.addModule (mid);
    const int iOther = both.addModule (other);
    check (both.attemptConnect (iSrc, 1, iMid, 0) == PatchGraph::ConnectResult::Ok, "path for both");
    check (both.attemptConnect (iOther, 1, iSrc, 0) == PatchGraph::ConnectResult::Ok, "src input cabled");
    const auto takenAndCycle = both.attemptConnect (iMid, 1, iSrc, 0);
    check (takenAndCycle == PatchGraph::ConnectResult::Cycle, "cycle still refused on a patched input");
    check (std::strcmp (PatchGraph::connectResultText (takenAndCycle), "feedback is not available until step 19") == 0,
           "cycle string on a patched input");
    check (both.cableCount() == 2, "cycle added nothing");

    check (graph.attemptConnect (-1, 0, 0, 0) == PatchGraph::ConnectResult::Rejected, "missing module");
    check (PatchGraph::connectResultText (PatchGraph::ConnectResult::Rejected)[0] == '\0', "rejected has no string");
    return finish ("testConnectStatusStrings");
}

int testRejectCycle()
{
    PatchGraph graph;
    GainModule a;
    GainModule b;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);

    check (graph.connect (ia, 1, ib, 0), "A to B");
    const int cables = graph.cableCount();
    check (! graph.connect (ib, 1, ia, 0), "B to A closes a cycle");
    check (graph.cableCount() == cables, "cycle reject changed nothing");
    check (! graph.connect (ia, 1, ia, 1), "a port connected to itself");
    check (! graph.connect (ia, 1, ia, 0), "module output into its own input");
    return finish ("testRejectCycle");
}

bool processSourceIsFixed()
{
    const char* path = MS50_PATCH_GRAPH_SOURCE;
    std::ifstream input (path);
    if (! input)
    {
        std::printf ("  FAIL could not read %s\n", path);
        return false;
    }
    const std::string text ((std::istreambuf_iterator<char> (input)), std::istreambuf_iterator<char>());
    const std::string marker = "void PatchGraph::process()";
    const auto start = text.find (marker);
    if (start == std::string::npos)
        return false;
    const auto body = text.find ('{', start);
    if (body == std::string::npos)
        return false;

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
    const std::string function = text.substr (start, end - start + 1);
    if (function.find ("push_back") != std::string::npos)
        return false;
    for (std::size_t i = 0; i + 3 <= function.size(); ++i)
    {
        const bool word = (i == 0 || ! ((function[i - 1] >= 'a' && function[i - 1] <= 'z')
                                         || (function[i - 1] >= 'A' && function[i - 1] <= 'Z')
                                         || function[i - 1] == '_'));
        if (word && function.compare (i, 3, "new") == 0)
        {
            const char after = (i + 3 < function.size()) ? function[i + 3] : ' ';
            const bool tail = ! ((after >= 'a' && after <= 'z') || (after >= 'A' && after <= 'Z') || after == '_');
            if (tail)
                return false;
        }
    }
    return true;
}

int testSnapshotSwapDoesNotAllocate()
{
    check (processSourceIsFixed(), "process source has no new or push_back");

    PatchGraph graph;
    ConstantModule a (0.5f, PortType::Audio);
    GainModule b;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    check (graph.connect (ia, 1, ib, 0), "cable");
    graph.prepare (48000.0);

    gAllocations.store (0, std::memory_order_relaxed);
    for (int sample = 0; sample < 1000; ++sample)
        graph.process();
    check (gAllocations.load (std::memory_order_relaxed) == 0, "allocation counter stayed 0");
    check (b.portValue[1] == 0.5f, "GainModule copied the value");
    return finish ("testSnapshotSwapDoesNotAllocate");
}

int testDisconnectMissingIsNoop()
{
    PatchGraph graph;
    GainModule a;
    GainModule b;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    graph.disconnect (ia, 1, ib, 0);
    graph.disconnect (99, 0, 99, 0);
    check (graph.cableCount() == 0, "still empty");
    check (graph.connect (ia, 1, ib, 0), "connect after missing disconnect");
    return finish ("testDisconnectMissingIsNoop");
}

int testPublishedSnapshotCopy()
{
    PatchGraph graph;
    GainModule a;
    GainModule b;
    GainModule c;
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int ic = graph.addModule (c);

    Cable scratch[4];
    check (graph.copyPublishedCables (nullptr, 4) == 0, "null dest");
    check (graph.copyPublishedCables (scratch, 0) == 0, "zero capacity");
    check (graph.copyPublishedCables (scratch, 4) == 0, "empty snapshot");

    check (graph.connect (ia, 1, ib, 0), "first cable");
    check (graph.connect (ia, 1, ic, 0), "fan-out cable");
    check (graph.copyPublishedCables (scratch, 4) == 2, "two published cables");
    check (scratch[0].sourceModule == ia && scratch[0].sourcePort == 1, "first source");
    check (scratch[0].destModule == ib && scratch[0].destPort == 0, "first dest");
    check (scratch[1].sourceModule == ia && scratch[1].destModule == ic, "second cable");

    Cable onlyFirst[1];
    check (graph.copyPublishedCables (onlyFirst, 1) == 1, "capacity truncates");
    check (onlyFirst[0].destModule == ib, "truncation keeps the first cable");
    return finish ("testPublishedSnapshotCopy");
}

}

int testDryMixPassesStereo();
int testExtInMonoAveragesStereo();
int testWetMixIgnoresDry();
int testLevelZeroIsSilence();
int testLeftOnlyStaysLeft();
int testSineDryStereoPasses();
int testSineLeftOnlyStaysLeft();
int testSineWetUnpatchedIsSilence();
int testSineRmsInRange();

int main()
{
    int failed = 0;
    failed += testInputSumsTwoCables();
    failed += testSecondCableDoesNotReplaceFirst();
    failed += testFanOutAllowed();
    failed += testRejectSignalIntoGate();
    failed += testRejectCycle();
    failed += testConnectStatusStrings();
    failed += testSnapshotSwapDoesNotAllocate();
    failed += testDisconnectMissingIsNoop();
    failed += testPublishedSnapshotCopy();
    failed += testDryMixPassesStereo();
    failed += testExtInMonoAveragesStereo();
    failed += testWetMixIgnoresDry();
    failed += testLevelZeroIsSilence();
    failed += testLeftOnlyStaysLeft();
    failed += testSineDryStereoPasses();
    failed += testSineLeftOnlyStaysLeft();
    failed += testSineWetUnpatchedIsSilence();
    failed += testSineRmsInRange();
    return failed == 0 ? 0 : 1;
}
