// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginEditor.h"

MS50ModularAudioProcessorEditor::MS50ModularAudioProcessorEditor (MS50ModularAudioProcessor& audioProcessor)
    : AudioProcessorEditor (audioProcessor)
{
    titleLabel.setText ("MS-50 Modular", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (rack);

    setResizable (true, true);
    setResizeLimits (720, 480, 1800, 1200);
    setSize (1100, 720);
}

MS50ModularAudioProcessorEditor::~MS50ModularAudioProcessorEditor() = default;

void MS50ModularAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));
}

void MS50ModularAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds (area.removeFromTop (28));
    rack.setBounds (area);
}
