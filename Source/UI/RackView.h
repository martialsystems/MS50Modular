// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "UI/CableView.h"

#include <juce_gui_basics/juce_gui_basics.h>

class JackView;
class MS50ModularAudioProcessor;

// Editor status line. The rack calls this on the message thread after a gesture.
class PatchStatusTarget {
public:
    virtual ~PatchStatusTarget() = default;
    virtual void showPatchStatus (const char* text) = 0;
};

// Fourteen faceplates. Slot index is the rack order, not a PatchGraph index.
// Ext In is slot 0. Output is slot 5. The graph stores those two modules at the
// indices returned by addModule, which the processor keeps.
class RackView : public juce::Component,
                 private CableJackLookup
{
public:
    explicit RackView (MS50ModularAudioProcessor&);
    ~RackView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setStatusTarget (PatchStatusTarget* target) noexcept { statusTarget = target; }

private:
    CableEnd jackCentre (int graphModule, int port) const override;

    struct Faceplate;
    struct JackHit {
        JackView* jack = nullptr;
        Faceplate* plate = nullptr;
    };

    JackHit jackAt (juce::Point<float> rackPoint) const;
    bool graphEndpoint (const JackView& jack, int& module, int& port) const;
    juce::Point<float> jackCentreInRack (const Faceplate& plate, const JackView& jack) const;
    void showStatus (const char* text);

    MS50ModularAudioProcessor& processor;
    juce::OwnedArray<Faceplate> plates;
    CableView cables;
    PatchStatusTarget* statusTarget = nullptr;
    bool dragging = false;
    bool dragMapped = false;
    int dragModule = -1;
    int dragPort = -1;
    float dragX = 0.0f;
    float dragY = 0.0f;
};
