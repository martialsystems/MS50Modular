// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Fourteen faceplates. Slot index is the rack order, not a PatchGraph index.
// Ext In is slot 0. Output is slot 5. The graph currently stores Output at 1.
class RackView : public juce::Component
{
public:
    RackView();
    ~RackView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Faceplate;
    juce::OwnedArray<Faceplate> plates;
};
