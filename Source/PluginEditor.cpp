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

    statusLabel.setText ("no patch yet", juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    setResizable (false, false);
    setSize (420, 180);
}

MS50ModularAudioProcessorEditor::~MS50ModularAudioProcessorEditor() = default;

void MS50ModularAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c1c));
}

void MS50ModularAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (24);
    titleLabel.setBounds (area.removeFromTop (36));
    area.removeFromTop (8);
    statusLabel.setBounds (area.removeFromTop (28));
}
