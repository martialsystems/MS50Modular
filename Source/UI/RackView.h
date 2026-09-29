// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "UI/CableView.h"

#include <juce_gui_basics/juce_gui_basics.h>

class MS50ModularAudioProcessor;

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

private:
    CableEnd jackCentre (int graphModule, int port) const override;

    struct Faceplate;
    MS50ModularAudioProcessor& processor;
    juce::OwnedArray<Faceplate> plates;
    CableView cables;
};
