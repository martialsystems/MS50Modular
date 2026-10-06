// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "UI/PatchBayLogic.h"

#include <cmath>
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

int moduleIndex (const PanelJackRec& jack, int extIndex, int outputIndex, int noiseIndex,
                 int vcfIndex, int vca1Index, int vca2Index, int eg1Index)
{
    if (jack.module == 1 && extIndex >= 0)
        return extIndex;
    if (jack.module == 2 && outputIndex >= 0)
        return outputIndex;
    if (jack.module == 3 && noiseIndex >= 0)
        return noiseIndex;
    if (jack.module == 4 && vcfIndex >= 0)
        return vcfIndex;
    if (jack.module == 5 && vca1Index >= 0)
        return vca1Index;
    if (jack.module == 6 && vca2Index >= 0)
        return vca2Index;
    if (jack.module == 7 && eg1Index >= 0)
        return eg1Index;
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

int jackForGraphPort (int module, int port, int extIndex, int outputIndex, int noiseIndex,
                      int vcfIndex, int vca1Index, int vca2Index, int eg1Index)
{
    int which = 0;
    if (extIndex >= 0 && module == extIndex)
        which = 1;
    else if (outputIndex >= 0 && module == outputIndex)
        which = 2;
    else if (noiseIndex >= 0 && module == noiseIndex)
        which = 3;
    else if (vcfIndex >= 0 && module == vcfIndex)
        which = 4;
    else if (vca1Index >= 0 && module == vca1Index)
        which = 5;
    else if (vca2Index >= 0 && module == vca2Index)
        which = 6;
    else if (eg1Index >= 0 && module == eg1Index)
        which = 7;
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
                                  PanelLink& link,
                                  int vcfIndex, int vca1Index, int vca2Index, int eg1Index)
{
    link = {};
    if (jackA < 0 || jackB < 0 || jackA >= kPanelJackCount || jackB >= kPanelJackCount || jackA == jackB)
        return PanelLinkResult::BadType;

    const PanelJackRec& a = kPanelJacks[jackA];
    const PanelJackRec& b = kPanelJacks[jackB];
    if (a.module == 0 || b.module == 0 || a.dir < 0 || b.dir < 0)
        return PanelLinkResult::Unmapped;

    const int moduleA = moduleIndex (a, extIndex, outputIndex, noiseIndex, vcfIndex, vca1Index, vca2Index, eg1Index);
    const int moduleB = moduleIndex (b, extIndex, outputIndex, noiseIndex, vcfIndex, vca1Index, vca2Index, eg1Index);
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
                         int extIndex, int outputIndex, int noiseIndex,
                         int vcfIndex, int vca1Index, int vca2Index, int eg1Index)
{
    if (dest == nullptr || published == nullptr || capacity <= 0 || publishedCount <= 0)
        return 0;

    int count = 0;
    const int limit = publishedCount < kPatchBayMaxCables ? publishedCount : kPatchBayMaxCables;
    for (int i = 0; i < limit && count < capacity; ++i)
    {
        const int jackA = jackForGraphPort (published[i].sourceModule, published[i].sourcePort,
                                             extIndex, outputIndex, noiseIndex,
                                             vcfIndex, vca1Index, vca2Index, eg1Index);
        const int jackB = jackForGraphPort (published[i].destModule, published[i].destPort,
                                             extIndex, outputIndex, noiseIndex,
                                             vcfIndex, vca1Index, vca2Index, eg1Index);
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

int panelKnobIndex (const char* section, const char* label)
{
    if (section == nullptr || label == nullptr)
        return -1;

    for (int i = 0; i < kPanelKnobCount; ++i)
    {
        if (std::strcmp (kPanelKnobs[i].section, section) == 0
            && std::strcmp (kPanelKnobs[i].label, label) == 0)
            return i;
    }
    return -1;
}

float panelKnobClamp (float value, bool isSwitch)
{
    if (value < 0.0f)
        value = 0.0f;
    if (value > 1.0f)
        value = 1.0f;
    if (isSwitch)
        value = std::round (value * 2.0f) / 2.0f;
    return value;
}

float panelKnobDrag (float start, float deltaUpPx, bool shift, bool isSwitch)
{
    const float divisor = isSwitch ? 60.0f : (shift ? 1000.0f : 200.0f);
    return panelKnobClamp (start + deltaUpPx / divisor, isSwitch);
}

float panelKnobWheel (float current, float htmlDeltaY, bool shift, bool isSwitch)
{
    if (isSwitch)
    {
        float sign = 0.0f;
        if (htmlDeltaY > 0.0f)
            sign = 1.0f;
        else if (htmlDeltaY < 0.0f)
            sign = -1.0f;
        return panelKnobClamp (current - sign * 0.5f, true);
    }

    const float rate = shift ? 0.0002f : 0.001f;
    return panelKnobClamp (current - htmlDeltaY * rate, false);
}

float panelKnobFromWheel (float current, float wheelDeltaY, bool reversed, bool shift, bool isSwitch)
{
    // One notch matches the prototype's 100px wheel line.
    // Positive wheelDeltaY is a physical upward push when reversed is false.
    const float htmlDeltaY = (reversed ? wheelDeltaY : -wheelDeltaY) * 100.0f;
    return panelKnobWheel (current, htmlDeltaY, shift, isSwitch);
}

float panelKnobSwitchClick (float current)
{
    float stepped = std::fmod (current + 0.5f, 1.5f);
    if (stepped < 0.0f)
        stepped += 1.5f;
    return panelKnobClamp (stepped, true);
}

float panelKnobAngleDegrees (bool isSwitch, float value)
{
    return isSwitch ? (-48.0f + 96.0f * value) : (-135.0f + 270.0f * value);
}
