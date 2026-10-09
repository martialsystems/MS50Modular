// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PatchGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

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

constexpr char kMagic[4] = { 'R', 'N', 'I', 'N' };

struct ByteCursor {
    const std::uint8_t* cursor = nullptr;
    const std::uint8_t* end = nullptr;
    bool ok = true;

    std::int32_t readI32()
    {
        if (! ok || cursor == nullptr || end - cursor < 4)
        {
            ok = false;
            return 0;
        }
        std::int32_t value = 0;
        std::memcpy (&value, cursor, 4);
        cursor += 4;
        return value;
    }

    float readF32()
    {
        if (! ok || cursor == nullptr || end - cursor < 4)
        {
            ok = false;
            return 0.0f;
        }
        float value = 0.0f;
        std::memcpy (&value, cursor, 4);
        cursor += 4;
        return value;
    }
};

struct ByteWriter {
    std::uint8_t* cursor = nullptr;
    std::uint8_t* end = nullptr;
    bool ok = true;

    void writeI32 (std::int32_t value)
    {
        if (! ok || cursor == nullptr || end - cursor < 4)
        {
            ok = false;
            return;
        }
        std::memcpy (cursor, &value, 4);
        cursor += 4;
    }

    void writeF32 (float value)
    {
        if (! ok || cursor == nullptr || end - cursor < 4)
        {
            ok = false;
            return;
        }
        std::memcpy (cursor, &value, 4);
        cursor += 4;
    }
};

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

bool PatchGraph::keptReaches (int from, int target, const Snapshot& snapshot, const bool* kept) const
{
    if (from < 0 || target < 0 || from >= moduleCount_ || target >= moduleCount_ || kept == nullptr)
        return false;

    bool seen[kMaxModules] {};
    int queue[kMaxModules] {};
    int head = 0;
    int tail = 0;
    queue[tail] = from;
    ++tail;
    seen[from] = true;

    while (head < tail)
    {
        const int module = queue[head];
        ++head;
        for (int i = 0; i < snapshot.cableCount; ++i)
        {
            if (! kept[i] || snapshot.cables[i].sourceModule != module)
                continue;
            const int next = snapshot.cables[i].destModule;
            if (next == target)
                return true;
            if (next < 0 || next >= moduleCount_ || seen[next])
                continue;
            seen[next] = true;
            queue[tail] = next;
            ++tail;
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
    if (editCableCount_ >= kMaxCables)
        return false;

    Cable& cable = editCables_[editCableCount_];
    cable = Cable {};   // a new cable: colour by role, never legacyInvert (JCS M3)
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
            return "";
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
    preparedRate_ = sampleRate;
    overHoldSamples_ = std::max (1, static_cast<int> (std::lround (0.1 * sampleRate)));
    for (int m = 0; m < kMaxModules; ++m)
        for (int p = 0; p < kMaxPorts; ++p)
        {
            overLed_[m][p].prepare (sampleRate);
            overHold_[m][p] = 0;
            overFlag_[m][p].store (false, std::memory_order_relaxed);
        }
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
        if (snapshot.feedback[i])
            continue;
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

    if (snapshot.orderCount >= moduleCount_)
        return;

    bool placed[kMaxModules] {};
    for (int i = 0; i < snapshot.orderCount; ++i)
        placed[snapshot.order[i]] = true;
    for (int module = 0; module < moduleCount_; ++module)
    {
        if (placed[module])
            continue;
        snapshot.order[snapshot.orderCount] = module;
        ++snapshot.orderCount;
    }
}

void PatchGraph::publish()
{
    const int front = published_.load (std::memory_order_relaxed);
    const int frontIndex = (front == 1) ? 1 : 0;
    const int back = 1 - frontIndex;
    Snapshot& snapshot = snapshots_[back];
    const Snapshot& previous = snapshots_[frontIndex];

    snapshot.cableCount = editCableCount_;
    for (int i = 0; i < editCableCount_; ++i)
        snapshot.cables[i] = editCables_[i];

    // JCS R9: walk oldest to newest. A cable is feedback when its destination already reaches its source over
    // the cables kept so far, or when it is a self-patch. Every feedback cable is delayed exactly one sample;
    // all other cables are zero-delay. The kept edges stay a DAG, so no module ever runs twice per sample.
    bool kept[kMaxCables] {};
    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        const int source = snapshot.cables[i].sourceModule;
        const int dest = snapshot.cables[i].destModule;
        const bool closes = source == dest || keptReaches (dest, source, snapshot, kept);
        snapshot.feedback[i] = closes;
        snapshot.delayed[i] = closes;
        kept[i] = ! closes;
    }
    for (int i = snapshot.cableCount; i < kMaxCables; ++i)
    {
        snapshot.feedback[i] = false;
        snapshot.delayed[i] = false;
        snapshot.held[i] = 0.0f;
    }

    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        snapshot.held[i] = 0.0f;
        if (! snapshot.delayed[i])
            continue;
        const Cable& cable = snapshot.cables[i];
        for (int j = 0; j < previous.cableCount; ++j)
        {
            if (! previous.delayed[j])
                continue;
            const Cable& old = previous.cables[j];
            if (old.sourceModule != cable.sourceModule || old.sourcePort != cable.sourcePort)
                continue;
            if (old.destModule != cable.destModule || old.destPort != cable.destPort)
                continue;
            snapshot.held[i] = previous.held[j];
            break;
        }
    }

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

void PatchGraph::clearModuleInputs (int moduleIndex, const bool patched[kMaxModules][kMaxPorts]) const
{
    Module* module = modules_[moduleIndex];
    const int ports = module->numPorts();
    for (int portIndex = 0; portIndex < 8; ++portIndex)
        module->inputConnected[portIndex] = false;
    for (int portIndex = 0; portIndex < ports; ++portIndex)
    {
        const PortDesc desc = module->port (portIndex);
        if (desc.dir != PortDir::In)
            continue;
        module->inputConnected[portIndex] = patched[moduleIndex][portIndex];
        // JCS R10: an unpatched input sits at its typed rest every sample (Gate inputs too). Nothing latches.
        module->portValue[portIndex] = patched[moduleIndex][portIndex] ? 0.0f : desc.rest;
    }
}

float PatchGraph::cableVolts (float raw, const PortDesc& sourceDesc, const PortDesc& destDesc, bool legacyInvert) noexcept
{
    // JCS R3s: only a cable that lands on an S-trig input (EG 1/EG 2 TRIG) is polarity-converted. A Gate source
    // that is not S-trig writes high ? 0 V : 5 V; an S-trig source (EXT IN GATE) passes as written.
    // Every other input receives the raw volts, except a migrated format-1 cable flagged legacyInvert (JCS M3),
    // which keeps the old S-15 inversion into any non-Gate input.
    const bool gateSource = sourceDesc.type == PortType::Gate && ! sourceDesc.strigVolts;
    if (! gateSource)
        return raw;
    if (destDesc.strigInput || (legacyInvert && destDesc.type != PortType::Gate))
        return raw >= 0.5f ? 0.0f : jcs::kStrigRest;
    return raw;
}

bool PatchGraph::convertsToStrig (const PortDesc& sourceDesc, const PortDesc& destDesc) noexcept
{
    return sourceDesc.type == PortType::Gate && ! sourceDesc.strigVolts && destDesc.strigInput;
}

jcs::Badge PatchGraph::cableBadge (const PortDesc& sourceDesc, const PortDesc& destDesc) noexcept
{
    if (convertsToStrig (sourceDesc, destDesc))
        return jcs::Badge::GateToStrig;
    const jcs::Badge badge = jcs::cableBadge (portRole (sourceDesc), portRole (destDesc));
    return badge == jcs::Badge::GateToStrig ? jcs::Badge::None : badge;
}

void PatchGraph::contributeCables (int moduleIndex, const Snapshot& snapshot) const
{
    Module* module = modules_[moduleIndex];
    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        const Cable& cable = snapshot.cables[i];
        if (cable.destModule != moduleIndex)
            continue;

        const Module* source = modules_[cable.sourceModule];
        const float raw = snapshot.delayed[i] ? snapshot.held[i] : source->portValue[cable.sourcePort];
        // JCS R8: per-cable conversion before the sum.
        module->portValue[cable.destPort] += cableVolts (raw, source->port (cable.sourcePort),
                                                         module->port (cable.destPort), cable.legacyInvert);
    }
}

int PatchGraph::delayedCableCount() const
{
    const int published = published_.load (std::memory_order_acquire);
    const int index = (published == 1) ? 1 : 0;
    const Snapshot& snapshot = snapshots_[index];
    int count = 0;
    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        if (snapshot.delayed[i])
            ++count;
    }
    return count;
}

bool PatchGraph::cableIsDelayed (int index) const
{
    const int published = published_.load (std::memory_order_acquire);
    const int slot = (published == 1) ? 1 : 0;
    const Snapshot& snapshot = snapshots_[slot];
    if (index < 0 || index >= snapshot.cableCount)
        return false;
    return snapshot.delayed[index];
}

float PatchGraph::portVolts (int module, int port) const noexcept
{
    if (module < 0 || module >= moduleCount_ || modules_[module] == nullptr || port < 0 || port >= kMaxPorts)
        return 0.0f;
    const float value = modules_[module]->portValue[port];
    return std::isfinite (value) ? value : 0.0f;
}

void PatchGraph::process()
{
    const int published = published_.load (std::memory_order_acquire);
    const int index = (published == 1) ? 1 : 0;
    Snapshot& snapshot = snapshots_[index];

    bool patched[kMaxModules][kMaxPorts] {};
    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        const Cable& cable = snapshot.cables[i];
        patched[cable.destModule][cable.destPort] = true;
    }

    for (int moduleIndex = 0; moduleIndex < moduleCount_; ++moduleIndex)
        clearModuleInputs (moduleIndex, patched);

    // JCS R9: one pass. Each module runs exactly once; feedback cables read last sample's held value.
    for (int orderIndex = 0; orderIndex < snapshot.orderCount; ++orderIndex)
    {
        const int moduleIndex = snapshot.order[orderIndex];
        contributeCables (moduleIndex, snapshot);
        modules_[moduleIndex]->processSample();
    }

    for (int i = 0; i < snapshot.cableCount; ++i)
    {
        if (! snapshot.delayed[i])
            continue;
        const Cable& cable = snapshot.cables[i];
        snapshot.held[i] = modules_[cable.sourceModule]->portValue[cable.sourcePort];
    }

    // JCS R15: over-range when |V| > 5.5 V for more than 10 ms. No allocation, no lock.
    for (int m = 0; m < moduleCount_; ++m)
    {
        const Module* module = modules_[m];
        const int ports = std::min (module->numPorts(), static_cast<int> (kMaxPorts));
        for (int p = 0; p < ports; ++p)
        {
            if (overLed_[m][p].process (module->portValue[p]))
                overHold_[m][p] = overHoldSamples_;
            else if (overHold_[m][p] > 0)
                --overHold_[m][p];
            const bool lit = overHold_[m][p] > 0;
            if (overFlag_[m][p].load (std::memory_order_relaxed) != lit)
                overFlag_[m][p].store (lit, std::memory_order_relaxed);
        }
    }
}

bool PatchGraph::portOverRange (int module, int port) const noexcept
{
    if (module < 0 || module >= kMaxModules || port < 0 || port >= kMaxPorts)
        return false;
    return overFlag_[module][port].load (std::memory_order_relaxed);
}

int PatchGraph::getState (void* dest, int capacity) const
{
    if (dest == nullptr || capacity <= 0)
        return 0;

    int size = 16;
    for (int module = 0; module < moduleCount_; ++module)
    {
        const int knobs = modules_[module]->presetKnobCount();
        if (knobs < 0 || knobs > kMaxPresetKnobs)
            return 0;
        size += 8 + knobs * 4;
    }
    size += editCableCount_ * 16;
    if (capacity < size)
        return 0;

    auto* bytes = static_cast<std::uint8_t*> (dest);
    ByteWriter writer { bytes, bytes + capacity, true };
    writer.cursor[0] = static_cast<std::uint8_t> (kMagic[0]);
    writer.cursor[1] = static_cast<std::uint8_t> (kMagic[1]);
    writer.cursor[2] = static_cast<std::uint8_t> (kMagic[2]);
    writer.cursor[3] = static_cast<std::uint8_t> (kMagic[3]);
    writer.cursor += 4;
    writer.writeI32 (kStateVersion);
    writer.writeI32 (moduleCount_);
    writer.writeI32 (editCableCount_);

    for (int module = 0; module < moduleCount_; ++module)
    {
        const Module* item = modules_[module];
        const int knobs = item->presetKnobCount();
        writer.writeI32 (knobs);
        for (int knob = 0; knob < knobs; ++knob)
            writer.writeF32 (item->presetKnob (knob));
        writer.writeI32 (item->presetScaleIndex());
    }

    for (int cable = 0; cable < editCableCount_; ++cable)
    {
        writer.writeI32 (editCables_[cable].sourceModule);
        writer.writeI32 (editCables_[cable].sourcePort);
        writer.writeI32 (editCables_[cable].destModule);
        writer.writeI32 (editCables_[cable].destPort);
    }

    if (! writer.ok || writer.cursor != bytes + size)
        return 0;
    return size;
}

bool PatchGraph::setState (const void* data, int size)
{
    auto fail = [this] (const char* message)
    {
        stateError_ = message;
        return false;
    };

    if (data == nullptr || size < 16)
        return fail ("preset state is corrupt");

    const auto* bytes = static_cast<const std::uint8_t*> (data);
    if (std::memcmp (bytes, kMagic, 4) != 0)
        return fail ("preset state is corrupt");

    ByteCursor reader { bytes + 4, bytes + size, true };
    const std::int32_t version = reader.readI32();
    const std::int32_t modules = reader.readI32();
    const std::int32_t cables = reader.readI32();
    if (! reader.ok)
        return fail ("preset state is corrupt");
    if (version != kStateVersion)
        return fail ("preset version is not supported");
    // A blob from before a module was appended (MIDI IN) stores fewer modules: the appended ones keep their state.
    if (modules < 1 || modules > moduleCount_ || cables < 0 || cables > kMaxCables)
        return fail ("preset state is corrupt");

    float knobs[kMaxModules][kMaxPresetKnobs] {};
    int knobCount[kMaxModules] {};
    int scaleIndex[kMaxModules] {};
    Cable loaded[kMaxCables] {};

    for (int module = 0; module < modules; ++module)
    {
        const int count = reader.readI32();
        if (! reader.ok || count != modules_[module]->presetKnobCount() || count < 0 || count > kMaxPresetKnobs)
            return fail ("preset state is corrupt");
        knobCount[module] = count;
        for (int knob = 0; knob < count; ++knob)
        {
            const float value = reader.readF32();
            if (! reader.ok || ! std::isfinite (value) || value < 0.0f || value > 1.0f)
                return fail ("preset state is corrupt");
            knobs[module][knob] = value;
        }
        const int scale = reader.readI32();
        if (! reader.ok)
            return fail ("preset state is corrupt");
        const int liveScale = modules_[module]->presetScaleIndex();
        if (liveScale < 0)
        {
            if (scale != -1)
                return fail ("preset state is corrupt");
        }
        else if (scale < 0 || scale > 3)
        {
            return fail ("preset state is corrupt");
        }
        scaleIndex[module] = scale;
    }

    for (int cable = 0; cable < cables; ++cable)
    {
        loaded[cable].sourceModule = reader.readI32();
        loaded[cable].sourcePort = reader.readI32();
        loaded[cable].destModule = reader.readI32();
        loaded[cable].destPort = reader.readI32();
        if (! reader.ok || ! indicesLegal (loaded[cable].sourceModule, loaded[cable].sourcePort,
                                            loaded[cable].destModule, loaded[cable].destPort))
            return fail ("preset state is corrupt");
    }

    if (! reader.ok || reader.cursor != reader.end)
        return fail ("preset state is corrupt");

    float previous[kMaxModules][kMaxPresetKnobs] {};
    for (int module = 0; module < moduleCount_; ++module)
    {
        for (int knob = 0; knob < knobCount[module]; ++knob)
            previous[module][knob] = modules_[module]->presetKnob (knob);
        for (int knob = 0; knob < knobCount[module]; ++knob)
            modules_[module]->setKnob (knob, knobs[module][knob]);
    }

    for (int module = 0; module < modules; ++module)
    {
        if (modules_[module]->presetScaleIndex() == scaleIndex[module])
            continue;
        for (int restore = 0; restore < moduleCount_; ++restore)
        {
            for (int knob = 0; knob < knobCount[restore]; ++knob)
                modules_[restore]->setKnob (knob, previous[restore][knob]);
        }
        return fail ("preset state is corrupt");
    }

    editCableCount_ = cables;
    for (int cable = 0; cable < cables; ++cable)
        editCables_[cable] = loaded[cable];

    prepare (preparedRate_ > 0.0 ? preparedRate_ : 48000.0);
    stateError_ = "";
    return true;
}

bool PatchGraph::setCables (const Cable* cables, int count)
{
    if (count < 0 || count > kMaxCables || (count > 0 && cables == nullptr))
        return false;

    for (int i = 0; i < count; ++i)
    {
        if (cables[i].sourceModule == cables[i].destModule && cables[i].sourcePort == cables[i].destPort)
            return false;
        if (! indicesLegal (cables[i].sourceModule, cables[i].sourcePort, cables[i].destModule, cables[i].destPort))
            return false;
    }

    editCableCount_ = count;
    for (int i = 0; i < count; ++i)
        editCables_[i] = cables[i];
    publish();
    stateError_ = "";
    return true;
}

bool PatchGraph::setCableColour (int index, std::uint32_t argb)
{
    if (index < 0 || index >= editCableCount_)
        return false;
    editCables_[index].colour = argb;
    publish();
    return true;
}

bool PatchGraph::setCableLegacyInvert (int index, bool legacy)
{
    if (index < 0 || index >= editCableCount_)
        return false;
    editCables_[index].legacyInvert = legacy;
    publish();
    return true;
}

bool PatchGraph::cableIsLegal (const Cable& cable) const
{
    if (cable.sourceModule == cable.destModule && cable.sourcePort == cable.destPort)
        return false;
    return indicesLegal (cable.sourceModule, cable.sourcePort, cable.destModule, cable.destPort);
}

Module* PatchGraph::moduleAt (int index) noexcept
{
    if (index < 0 || index >= moduleCount_)
        return nullptr;
    return modules_[index];
}

bool PatchGraph::writePresetKnob (int module, int knob, float value)
{
    if (module < 0 || module >= moduleCount_ || modules_[module] == nullptr)
        return false;
    if (knob < 0 || knob >= modules_[module]->presetKnobCount())
        return false;
    modules_[module]->setKnob (knob, value);
    return true;
}
