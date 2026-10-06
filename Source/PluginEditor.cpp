// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginEditor.h"

MS50ModularAudioProcessorEditor::MS50ModularAudioProcessorEditor (MS50ModularAudioProcessor& audioProcessor)
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

MS50ModularAudioProcessorEditor::~MS50ModularAudioProcessorEditor() = default;

void MS50ModularAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101012));
}

void MS50ModularAudioProcessorEditor::resized()
{
    bay.setBounds (getLocalBounds());
}
