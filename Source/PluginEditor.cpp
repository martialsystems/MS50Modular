// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginEditor.h"

RoninAudioProcessorEditor::RoninAudioProcessorEditor (RoninAudioProcessor& audioProcessor)
    : AudioProcessorEditor (audioProcessor),
      bay (audioProcessor)
{
    addAndMakeVisible (bay);
    constrainer.setFixedAspectRatio (1600.0 / 564.0);
    constrainer.setSizeLimits (960, 338, 2400, 846);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (1280, 451);
}

RoninAudioProcessorEditor::~RoninAudioProcessorEditor() = default;

void RoninAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101012));
}

void RoninAudioProcessorEditor::resized()
{
    bay.setBounds (getLocalBounds());
}
