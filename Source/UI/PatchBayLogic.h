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

int jackForGraphPort (int module, int port, int extIndex, int outputIndex, int noiseIndex);

// Either jack may be the output. The link runs output to input.
// Same direction, a jack with no graph port, or both ends on one jack: not Ok.
PanelLinkResult orientPanelJacks (int jackA, int jackB,
                                  int extIndex, int outputIndex, int noiseIndex,
                                  PanelLink& link);

// Plugs on this jack, bottom to top, which is array order.
int plugsAtJack (const VisualCable* cables, int count, int jack, int* out, int capacity);

// bottomToTop is a permutation of the plugs on this jack.
// The write stays inside those array slots. It does not publish a graph.
bool reorderJackStack (VisualCable* cables, int count, int jack, const int* bottomToTop, int n);

// One visual cable per published link that has both jacks on this panel.
int loadPublishedCables (VisualCable* dest, int capacity,
                         const Cable* published, int publishedCount,
                         int extIndex, int outputIndex, int noiseIndex);
