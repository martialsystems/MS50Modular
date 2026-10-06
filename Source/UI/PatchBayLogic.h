// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Modular/Cable.h"
#include "Modular/Port.h"
#include "UI/PanelGeometry.inc"

#include <cstddef>

// Visual cables for the panel. The graph stores electrical links only.
// Color and the order of plugs on a jack are not fields on Cable.

inline constexpr int kPatchBayMaxCables = 64;

struct VisualCable {
    int a = -1;
    int b = -1;
    int color = 0;
    bool sounding = false;
    int sourceModule = -1;
    int sourcePort = -1;
    int destModule = -1;
    int destPort = -1;
};

struct PanelLink {
    int sourceModule = -1;
    int sourcePort = -1;
    int destModule = -1;
    int destPort = -1;
};

enum class PanelLinkResult {
    Ok,
    BadType,
    Unmapped
};

int panelJackIndex (const char* section, const char* label);

// Graph module and port for a panel jack. False when that hole is not on the graph.
bool panelJackAddress (int jack,
                       int extIndex, int outputIndex, int noiseIndex,
                       int vcfIndex, int vca1Index, int vca2Index, int eg1Index,
                       int mgIndex, int vcoIndex, int eg2Index, int ringIndex,
                       int dividerIndex, int inverterIndex, int integratorIndex,
                       int mixerIndex, int sampleHoldIndex,
                       int& module, int& port);

int jackForGraphPort (int module, int port, int extIndex, int outputIndex, int noiseIndex,
                      int vcfIndex = -1, int vca1Index = -1, int vca2Index = -1, int eg1Index = -1,
                      int mgIndex = -1, int vcoIndex = -1, int eg2Index = -1, int ringIndex = -1, int dividerIndex = -1, int inverterIndex = -1, int integratorIndex = -1, int mixerIndex = -1, int sampleHoldIndex = -1);

// Either jack may be the output. The link runs output to input.
// Same direction, a jack with no graph port, or both ends on one jack: not Ok.
PanelLinkResult orientPanelJacks (int jackA, int jackB,
                                  int extIndex, int outputIndex, int noiseIndex,
                                  PanelLink& link,
                                  int vcfIndex = -1, int vca1Index = -1, int vca2Index = -1, int eg1Index = -1,
                                  int mgIndex = -1, int vcoIndex = -1, int eg2Index = -1, int ringIndex = -1, int dividerIndex = -1, int inverterIndex = -1, int integratorIndex = -1, int mixerIndex = -1, int sampleHoldIndex = -1);

// Plugs on this jack, bottom to top, which is array order.
int plugsAtJack (const VisualCable* cables, int count, int jack, int* out, int capacity);

// bottomToTop is a permutation of the plugs on this jack.
// The write stays inside those array slots. It does not publish a graph.
bool reorderJackStack (VisualCable* cables, int count, int jack, const int* bottomToTop, int n);

// One visual cable per published link that has both jacks on this panel.
int loadPublishedCables (VisualCable* dest, int capacity,
                         const Cable* published, int publishedCount,
                         int extIndex, int outputIndex, int noiseIndex,
                         int vcfIndex = -1, int vca1Index = -1, int vca2Index = -1, int eg1Index = -1,
                         int mgIndex = -1, int vcoIndex = -1, int eg2Index = -1, int ringIndex = -1, int dividerIndex = -1, int inverterIndex = -1, int integratorIndex = -1, int mixerIndex = -1, int sampleHoldIndex = -1);

int panelKnobIndex (const char* section, const char* label);

// Column knobs are 0..1. The divider switch snaps to 0, 0.5, and 1.
// deltaUpPx is the pointer travel in component pixels, positive when the drag moves up.
float panelKnobClamp (float value, bool isSwitch);
float panelKnobDrag (float start, float deltaUpPx, bool shift, bool isSwitch);
float panelKnobWheel (float current, float htmlDeltaY, bool shift, bool isSwitch);
float panelKnobFromWheel (float current, float wheelDeltaY, bool reversed, bool shift, bool isSwitch);
float panelKnobSwitchClick (float current);
float panelKnobAngleDegrees (bool isSwitch, float value);
