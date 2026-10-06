// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PatchGraph.h"

namespace {

// Rows are the source type (Audio, CV, Gate). Columns are the destination type.
// SCHEMATICS.md: Audio and CV may feed Audio or CV. Gate may feed Audio, CV, or Gate.
// Audio or CV into a Gate input is refused.
constexpr bool kAllowed[3][3] = {
    { true, true, false },
    { true, true, false },
    { true, true, true },
};

int typeIndex (PortType type)
{
    if (type == PortType::Audio)
        return 0;
    if (type == PortType::CV)
        return 1;
    return 2;
}

bool typesAllowed (PortType from, PortType to)
{
    return kAllowed[typeIndex (from)][typeIndex (to)];
}

}

int PatchGraph::addModule (Module& module)
{
    if (moduleCount_ >= kMaxModules)
        return -1;

    modules_[moduleCount_] = &module;
    const int index = moduleCount_;
    ++moduleCount_;
    publish();
    return index;
}

bool PatchGraph::indicesLegal (int sourceModule, int sourcePort, int destModule, int destPort) const
{
    if (sourceModule < 0 || destModule < 0 || sourceModule >= moduleCount_ || destModule >= moduleCount_)
        return false;
    if (sourcePort < 0 || destPort < 0 || sourcePort >= kMaxPorts || destPort >= kMaxPorts)
        return false;

    const Module* source = modules_[sourceModule];
    const Module* dest = modules_[destModule];
    if (sourcePort >= source->numPorts() || destPort >= dest->numPorts())
        return false;

    const PortDesc sourceDesc = source->port (sourcePort);
    const PortDesc destDesc = dest->port (destPort);
    if (sourceDesc.dir != PortDir::Out || destDesc.dir != PortDir::In)
        return false;

    return typesAllowed (sourceDesc.type, destDesc.type);
}

bool PatchGraph::closesCycle (int sourceModule, int destModule) const
{
    if (sourceModule == destModule)
        return true;

    bool seen[kMaxModules] {};
    int queue[kMaxModules] {};
    int head = 0;
    int tail = 0;
    queue[tail++] = destModule;
    seen[destModule] = true;

    while (head < tail)
    {
        const int module = queue[head++];
        for (int i = 0; i < editCableCount_; ++i)
        {
            if (editCables_[i].sourceModule != module)
                continue;
            const int next = editCables_[i].destModule;
            if (next == sourceModule)
                return true;
            if (! seen[next])
            {
                seen[next] = true;
                queue[tail++] = next;
            }
        }
    }
    return false;
}

bool PatchGraph::connect (int sourceModule, int sourcePort, int destModule, int destPort)
{
    if (sourceModule == destModule && sourcePort == destPort)
        return false;
    if (! indicesLegal (sourceModule, sourcePort, destModule, destPort))
        return false;
    if (closesCycle (sourceModule, destModule))
        return false;
    if (editCableCount_ >= kMaxCables)
        return false;

    Cable& cable = editCables_[editCableCount_];
    cable.sourceModule = sourceModule;
    cable.sourcePort = sourcePort;
    cable.destModule = destModule;
    cable.destPort = destPort;
    ++editCableCount_;
    publish();
    return true;
}

const char* PatchGraph::connectResultText (ConnectResult result) noexcept
{
    switch (result)
    {
        case ConnectResult::BadType:
            return "that jack does not take this cable";
        case ConnectResult::Cycle:
            return "feedback is not available until step 19";
        case ConnectResult::Ok:
        case ConnectResult::Rejected:
            return "";
    }
    return "";
}

PatchGraph::ConnectResult PatchGraph::attemptConnect (int sourceModule, int sourcePort, int destModule, int destPort)
{
    if (sourceModule < 0 || destModule < 0 || sourceModule >= moduleCount_ || destModule >= moduleCount_)
        return ConnectResult::Rejected;
    if (sourcePort < 0 || destPort < 0 || sourcePort >= kMaxPorts || destPort >= kMaxPorts)
        return ConnectResult::BadType;

    const Module* source = modules_[sourceModule];
    const Module* dest = modules_[destModule];
    if (source == nullptr || dest == nullptr)
        return ConnectResult::Rejected;
    if (sourcePort >= source->numPorts() || destPort >= dest->numPorts())
        return ConnectResult::BadType;

    const PortDesc sourceDesc = source->port (sourcePort);
    const PortDesc destDesc = dest->port (destPort);
    if (sourceDesc.dir != PortDir::Out || destDesc.dir != PortDir::In)
        return ConnectResult::BadType;
    if (! typesAllowed (sourceDesc.type, destDesc.type))
        return ConnectResult::BadType;

    if (closesCycle (sourceModule, destModule))
        return ConnectResult::Cycle;
    if (editCableCount_ >= kMaxCables)
        return ConnectResult::Rejected;
    if (! connect (sourceModule, sourcePort, destModule, destPort))
        return ConnectResult::Rejected;
    return ConnectResult::Ok;
}

void PatchGraph::disconnect (int sourceModule, int sourcePort, int destModule, int destPort)
{
    for (int i = 0; i < editCableCount_; ++i)
    {
        const Cable& cable = editCables_[i];
        if (cable.sourceModule != sourceModule || cable.sourcePort != sourcePort)
            continue;
        if (cable.destModule != destModule || cable.destPort != destPort)
            continue;

        for (int j = i; j < editCableCount_ - 1; ++j)
            editCables_[j] = editCables_[j + 1];
        --editCableCount_;
        publish();
        return;
    }
}

void PatchGraph::prepare (double sampleRate)
{
    for (int i = 0; i < moduleCount_; ++i)
    {
        modules_[i]->sampleRate = sampleRate;
        modules_[i]->prepare (sampleRate);
    }
    publish();
}

void PatchGraph::fillOrder (Snapshot& snapshot) const
{
    int indegree[kMaxModules] {};
    int adjacent[kMaxModules][kMaxModules] {};
    int adjacentCount[kMaxModules] {};

    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        const int source = snapshot.cables[i].sourceModule;
        const int dest = snapshot.cables[i].destModule;
        if (source == dest)
            continue;

        bool already = false;
        for (int k = 0; k < adjacentCount[source]; ++k)
        {
            if (adjacent[source][k] == dest)
                already = true;
        }
        if (already)
            continue;

        adjacent[source][adjacentCount[source]] = dest;
        ++adjacentCount[source];
        ++indegree[dest];
    }

    int queue[kMaxModules] {};
    int head = 0;
    int tail = 0;
    for (int module = 0; module < moduleCount_; ++module)
    {
        if (indegree[module] == 0)
        {
            queue[tail] = module;
            ++tail;
        }
    }

    snapshot.orderCount = 0;
    while (head < tail)
    {
        const int module = queue[head];
        ++head;
        snapshot.order[snapshot.orderCount] = module;
        ++snapshot.orderCount;

        for (int k = 0; k < adjacentCount[module]; ++k)
        {
            const int dest = adjacent[module][k];
            --indegree[dest];
            if (indegree[dest] == 0)
            {
                queue[tail] = dest;
                ++tail;
            }
        }
    }
}

void PatchGraph::publish()
{
    const int back = 1 - published_.load (std::memory_order_relaxed);
    Snapshot& snapshot = snapshots_[back];
    snapshot.cableCount = editCableCount_;
    for (int i = 0; i < editCableCount_; ++i)
        snapshot.cables[i] = editCables_[i];
    fillOrder (snapshot);
    published_.store (back, std::memory_order_release);
}

int PatchGraph::copyPublishedCables (Cable* dest, int capacity) const
{
    if (dest == nullptr || capacity <= 0)
        return 0;

    const int published = published_.load (std::memory_order_acquire);
    const int index = (published == 1) ? 1 : 0;
    const Snapshot& snapshot = snapshots_[index];
    int count = snapshot.cableCount;
    if (count > capacity)
        count = capacity;
    if (count > kMaxCables)
        count = kMaxCables;
    if (count < 0)
        count = 0;
    for (int i = 0; i < count; ++i)
        dest[i] = snapshot.cables[i];
    return count;
}

void PatchGraph::process()
{
    const Snapshot& snapshot = snapshots_[published_.load (std::memory_order_acquire)];

    bool patched[kMaxModules][kMaxPorts] {};
    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        const Cable& cable = snapshot.cables[i];
        patched[cable.destModule][cable.destPort] = true;
    }

    for (int moduleIndex = 0; moduleIndex < moduleCount_; ++moduleIndex)
    {
        Module* module = modules_[moduleIndex];
        const int ports = module->numPorts();
        for (int portIndex = 0; portIndex < ports; ++portIndex)
        {
            const PortDesc desc = module->port (portIndex);
            if (desc.dir != PortDir::In)
                continue;
            if (! patched[moduleIndex][portIndex])
            {
                if (desc.type == PortType::Gate)
                    continue;
                module->portValue[portIndex] = desc.rest;
                continue;
            }
            module->portValue[portIndex] = 0.0f;
        }
    }

    for (int orderIndex = 0; orderIndex < snapshot.orderCount; ++orderIndex)
    {
        const int moduleIndex = snapshot.order[orderIndex];
        Module* module = modules_[moduleIndex];

        for (int i = 0; i < snapshot.cableCount; ++i)
        {
            const Cable& cable = snapshot.cables[i];
            if (cable.destModule != moduleIndex)
                continue;
            const Module* source = modules_[cable.sourceModule];
            float contributed = source->portValue[cable.sourcePort];
            const PortDesc sourceDesc = source->port (cable.sourcePort);
            const PortDesc destDesc = module->port (cable.destPort);
            // S-15: a held gate contributes 0 V, a released gate contributes +5 V.
            // Gate-to-gate stays the raw 0 or 1 level.
            if (sourceDesc.type == PortType::Gate && destDesc.type != PortType::Gate)
                contributed = contributed >= 0.5f ? 0.0f : 5.0f;
            module->portValue[cable.destPort] += contributed;
        }

        module->processSample();
    }
}
