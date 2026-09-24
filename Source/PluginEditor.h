// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "PluginProcessor.h"
#include "UI/RackView.h"

#include <juce_gui_basics/juce_gui_basics.h>

class MS50ModularAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MS50ModularAudioProcessorEditor (MS50ModularAudioProcessor&);
    ~MS50ModularAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel;
    RackView rack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MS50ModularAudioProcessorEditor)
};
