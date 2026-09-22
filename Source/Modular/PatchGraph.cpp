// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PatchGraph.h"

namespace {

// Step 2 connection table. Rows are the source type. Columns are the destination type.
// Audio into Gate is refused. CV into Gate is allowed. Gate into Audio is refused.
constexpr bool kAllowed[3][3] = {
    { true, true, false },
    { true, true, true },
    { false, true, true },
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

    for (int i = 0; i < editCableCount_; ++i)
    {
        if (editCables_[i].destModule == destModule && editCables_[i].destPort == destPort)
            return false;
    }

    Cable& cable = editCables_[editCableCount_];
    cable.sourceModule = sourceModule;
    cable.sourcePort = sourcePort;
    cable.destModule = destModule;
    cable.destPort = destPort;
    ++editCableCount_;
    publish();
    return true;
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
            if (desc.type == PortType::Gate)
                continue;
            if (! patched[moduleIndex][portIndex])
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
            module->portValue[cable.destPort] = modules_[cable.sourceModule]->portValue[cable.sourcePort];
        }

        module->processSample();
    }
}
