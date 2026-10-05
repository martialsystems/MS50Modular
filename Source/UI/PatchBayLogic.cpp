// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "UI/PatchBayLogic.h"

#include <cstring>

int panelJackIndex (const char* section, const char* label)
{
    if (section == nullptr || label == nullptr)
        return -1;

    for (int i = 0; i < kPanelJackCount; ++i)
    {
        if (std::strcmp (kPanelJacks[i].section, section) == 0
            && std::strcmp (kPanelJacks[i].label, label) == 0)
            return i;
    }
    return -1;
}

namespace {

int moduleIndex (const PanelJackRec& jack, int extIndex, int outputIndex, int noiseIndex)
{
    if (jack.module == 1)
        return extIndex;
    if (jack.module == 2)
        return outputIndex;
    if (jack.module == 3)
        return noiseIndex;
    return -1;
}

bool samePlugSet (const int* slots, int n, const int* listed)
{
    bool used[kPatchBayMaxCables] {};
    for (int i = 0; i < n; ++i)
    {
        const int index = listed[i];
        if (index < 0 || index >= kPatchBayMaxCables || used[index])
            return false;

        bool found = false;
        for (int s = 0; s < n; ++s)
        {
            if (slots[s] == index)
                found = true;
        }
        if (! found)
            return false;
        used[index] = true;
    }
    return true;
}

}

int jackForGraphPort (int module, int port, int extIndex, int outputIndex, int noiseIndex)
{
    int which = 0;
    if (module == extIndex)
        which = 1;
    else if (module == outputIndex)
        which = 2;
    else if (module == noiseIndex)
        which = 3;
    else
        return -1;

    for (int i = 0; i < kPanelJackCount; ++i)
    {
        if (kPanelJacks[i].module == which && kPanelJacks[i].port == port)
            return i;
    }
    return -1;
}

PanelLinkResult orientPanelJacks (int jackA, int jackB,
                                  int extIndex, int outputIndex, int noiseIndex,
                                  PanelLink& link)
{
    link = {};
    if (jackA < 0 || jackB < 0 || jackA >= kPanelJackCount || jackB >= kPanelJackCount || jackA == jackB)
        return PanelLinkResult::BadType;

    const PanelJackRec& a = kPanelJacks[jackA];
    const PanelJackRec& b = kPanelJacks[jackB];
    if (a.module == 0 || b.module == 0 || a.dir < 0 || b.dir < 0)
        return PanelLinkResult::Unmapped;

    const int moduleA = moduleIndex (a, extIndex, outputIndex, noiseIndex);
    const int moduleB = moduleIndex (b, extIndex, outputIndex, noiseIndex);
    if (moduleA < 0 || moduleB < 0)
        return PanelLinkResult::Unmapped;
    if (a.dir == b.dir)
        return PanelLinkResult::BadType;

    const bool aIsOut = a.dir == 1;
    link.sourceModule = aIsOut ? moduleA : moduleB;
    link.sourcePort = aIsOut ? a.port : b.port;
    link.destModule = aIsOut ? moduleB : moduleA;
    link.destPort = aIsOut ? b.port : a.port;
    return PanelLinkResult::Ok;
}

int plugsAtJack (const VisualCable* cables, int count, int jack, int* out, int capacity)
{
    if (cables == nullptr || out == nullptr || capacity <= 0 || jack < 0)
        return 0;

    int found = 0;
    const int limit = count < kPatchBayMaxCables ? count : kPatchBayMaxCables;
    for (int i = 0; i < limit; ++i)
    {
        if (cables[i].a != jack && cables[i].b != jack)
            continue;
        if (found < capacity)
            out[found] = i;
        ++found;
    }
    return found < capacity ? found : capacity;
}

bool reorderJackStack (VisualCable* cables, int count, int jack, const int* bottomToTop, int n)
{
    if (cables == nullptr || bottomToTop == nullptr || n <= 0 || count <= 0 || count > kPatchBayMaxCables)
        return false;

    int slots[kPatchBayMaxCables] {};
    const int onJack = plugsAtJack (cables, count, jack, slots, kPatchBayMaxCables);
    if (onJack != n || ! samePlugSet (slots, n, bottomToTop))
        return false;

    VisualCable moved[kPatchBayMaxCables] {};
    for (int i = 0; i < n; ++i)
        moved[i] = cables[bottomToTop[i]];
    for (int i = 0; i < n; ++i)
        cables[slots[i]] = moved[i];
    return true;
}

int loadPublishedCables (VisualCable* dest, int capacity,
                         const Cable* published, int publishedCount,
                         int extIndex, int outputIndex, int noiseIndex)
{
    if (dest == nullptr || published == nullptr || capacity <= 0 || publishedCount <= 0)
        return 0;

    int count = 0;
    const int limit = publishedCount < kPatchBayMaxCables ? publishedCount : kPatchBayMaxCables;
    for (int i = 0; i < limit && count < capacity; ++i)
    {
        const int jackA = jackForGraphPort (published[i].sourceModule, published[i].sourcePort,
                                             extIndex, outputIndex, noiseIndex);
        const int jackB = jackForGraphPort (published[i].destModule, published[i].destPort,
                                             extIndex, outputIndex, noiseIndex);
        if (jackA < 0 || jackB < 0)
            continue;

        VisualCable& cable = dest[count];
        cable.a = jackA;
        cable.b = jackB;
        cable.color = count % 4;
        cable.sounding = true;
        cable.sourceModule = published[i].sourceModule;
        cable.sourcePort = published[i].sourcePort;
        cable.destModule = published[i].destModule;
        cable.destPort = published[i].destPort;
        ++count;
    }
    return count;
}
