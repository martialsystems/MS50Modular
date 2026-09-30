// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginEditor.h"

MS50ModularAudioProcessorEditor::MS50ModularAudioProcessorEditor (MS50ModularAudioProcessor& audioProcessor)
    : AudioProcessorEditor (audioProcessor),
      rack (audioProcessor)
{
    titleLabel.setText ("MS-50 Modular", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    statusLabel.setName ("patchStatus");
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffe08a));
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centredRight);
    mixLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    mixLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    mixLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (mixLabel);

    mixSlider.setName ("outputMix");
    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    mixSlider.setRange (0.0, 1.0, 0.0);
    mixSlider.setValue (0.0, juce::dontSendNotification);
    mixSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
    mixSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1c1c1c));
    mixSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xff5a5a5a));
    mixSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xffffe08a));
    mixSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xff8a8a8a));
    mixSlider.onValueChange = [this]
    {
        static_cast<MS50ModularAudioProcessor&> (processor).setOutputMix (static_cast<float> (mixSlider.getValue()));
    };
    addAndMakeVisible (mixSlider);

    rack.setStatusTarget (this);
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

void MS50ModularAudioProcessorEditor::showPatchStatus (const char* text)
{
    statusLabel.setText (text != nullptr ? text : "", juce::dontSendNotification);
}

void MS50ModularAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds (area.removeFromTop (28));
    auto tools = area.removeFromTop (26).reduced (8, 2);
    mixSlider.setBounds (tools.removeFromRight (200));
    mixLabel.setBounds (tools.removeFromRight (40));
    statusLabel.setBounds (tools);
    rack.setBounds (area);
}
