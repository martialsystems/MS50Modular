// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/PatchGraph.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
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

int testOneCablePerInput()
{
    PatchGraph graph;
    ConstantModule a (1.0f, PortType::Audio);
    GainModule b;
    ConstantModule c (2.0f, PortType::Audio);
    const int ia = graph.addModule (a);
    const int ib = graph.addModule (b);
    const int ic = graph.addModule (c);

    check (graph.connect (ia, 1, ib, 0), "first cable");
    const int cables = graph.cableCount();
    check (! graph.connect (ic, 1, ib, 0), "second cable into B.in");
    check (graph.cableCount() == cables, "first cable remains");

    graph.prepare (48000.0);
    graph.process();
    check (b.portValue[1] == 1.0f, "B still follows A");
    return finish ("testOneCablePerInput");
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

    check (! graph.connect (iAudio, 1, iGate, 0), "Audio into Gate");
    check (graph.cableCount() == 0, "Audio reject changed nothing");
    check (graph.connect (iCv, 1, iGate, 0), "CV into Gate");

    ConstantModule gateSource (1.0f, PortType::Gate);
    SinkModule audioSink (PortType::Audio);
    SinkModule gateSink (PortType::Gate);
    const int iGateOut = graph.addModule (gateSource);
    const int iAudioIn = graph.addModule (audioSink);
    const int iGateIn = graph.addModule (gateSink);
    check (! graph.connect (iGateOut, 1, iAudioIn, 0), "Gate into Audio");
    check (graph.connect (iGateOut, 1, iGateIn, 0), "Gate into Gate");
    return finish ("testRejectSignalIntoGate");
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

}

int main()
{
    int failed = 0;
    failed += testOneCablePerInput();
    failed += testFanOutAllowed();
    failed += testRejectSignalIntoGate();
    failed += testRejectCycle();
    failed += testSnapshotSwapDoesNotAllocate();
    failed += testDisconnectMissingIsNoop();
    return failed == 0 ? 0 : 1;
}
