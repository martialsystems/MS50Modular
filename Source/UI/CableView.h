// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Modular/PatchGraph.h"
#include "Modular/Port.h"

#include <juce_gui_basics/juce_gui_basics.h>

class MS50ModularAudioProcessor;

// One jack centre in CableView coordinates. found is false when the jack is absent.
struct CableEnd {
    float x = 0.0f;
    float y = 0.0f;
    PortType type = PortType::Audio;
    bool found = false;
};

// RackView resolves graph module indices to jack circles.
class CableJackLookup {
public:
    virtual ~CableJackLookup() = default;
    virtual CableEnd jackCentre (int graphModule, int port) const = 0;
};

// Cubic cables for the published snapshot. The 30 Hz timer runs on the message thread.
// The audio callback is PluginProcessor::processBlock. The editor constructs this view.
class CableView : public juce::Component,
                  private juce::Timer
{
public:
    CableView (CableJackLookup& jacks, MS50ModularAudioProcessor& processor);
    ~CableView() override;

    void paint (juce::Graphics&) override;
    void timerCallback() override;

    bool isAnimationTimerRunning() const noexcept { return isTimerRunning(); }
    float dashPhase() const noexcept { return dashPhase_; }
    int drawnCableCount() const noexcept { return cubicCount_; }
    int pathRebuildCount() const noexcept { return rebuilds_; }

    bool drawnCable (int index, float& x0, float& y0, float& x1, float& y1, PortType& type) const noexcept;

private:
    struct Cubic {
        float x0 = 0.0f;
        float y0 = 0.0f;
        float c1x = 0.0f;
        float c1y = 0.0f;
        float c2x = 0.0f;
        float c2y = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        PortType type = PortType::Audio;
    };

    static bool sameCubic (const Cubic& a, const Cubic& b);
    static juce::Point<float> pointOnCubic (const Cubic& cubic, float t);

    void refreshGeometry();
    void rebuildPaths();
    void strokeBody (juce::Graphics& g) const;
    void strokeDashes (juce::Graphics& g) const;

    CableJackLookup& jacks;
    MS50ModularAudioProcessor& processor;
    Cubic cubics_[PatchGraph::kMaxCables] {};
    int cubicCount_ = 0;
    int rebuilds_ = 0;
    float dashPhase_ = 0.0f;
    juce::Path curves_[3];
};
