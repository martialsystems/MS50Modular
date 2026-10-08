// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "PluginProcessor.h"
#include "UI/PatchBayView.h"

#include <juce_gui_basics/juce_gui_basics.h>

class RoninAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoninAudioProcessorEditor (RoninAudioProcessor&);
    ~RoninAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::ComponentBoundsConstrainer constrainer;
    PatchBayView bay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoninAudioProcessorEditor)
};
