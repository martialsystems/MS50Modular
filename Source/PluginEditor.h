// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "PluginProcessor.h"
#include "UI/RackView.h"

#include <juce_gui_basics/juce_gui_basics.h>

class MS50ModularAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private PatchStatusTarget
{
public:
    explicit MS50ModularAudioProcessorEditor (MS50ModularAudioProcessor&);
    ~MS50ModularAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void showPatchStatus (const char* text) override;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label mixLabel;
    juce::Slider mixSlider;
    RackView rack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS50ModularAudioProcessorEditor)
};
