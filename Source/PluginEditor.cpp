// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginEditor.h"

RoninAudioProcessorEditor::RoninAudioProcessorEditor (RoninAudioProcessor& audioProcessor)
    : AudioProcessorEditor (audioProcessor),
      roninProcessor (audioProcessor),
      bay (audioProcessor),
      voice (audioProcessor),
      env (audioProcessor),
      patch (audioProcessor),
      setup (audioProcessor)
{
    addAndMakeVisible (bay);
    addAndMakeVisible (strip);
    addChildComponent (voice);
    addChildComponent (env);
    addChildComponent (patch);
    addChildComponent (setup);
    strip.onChange = [this] (ronin_ui::Tab tab) { showTab (tab); };
    setup.onScale = [this] (int percent) { applyScale (percent); };
    setup.onCableColourMode = [this] { bay.repaint(); };
    patch.onCablesChanged = [this] { bay.reloadCablesFromGraph(); };

    // Art 1600 x 564 plus the 36 px strip: 1600 x 600 design. Scale steps 75 .. 200 % of 1280.
    constrainer.setFixedAspectRatio (1600.0 / (564.0 + ronin_ui::kStripH));
    constrainer.setSizeLimits (960, heightForWidth (960), 2560, heightForWidth (2560));
    setConstrainer (&constrainer);
    setResizable (true, true);
    const int width = kDefaultWidth * juce::jlimit (75, 200, audioProcessor.uiScalePercent()) / 100;
    setSize (width, heightForWidth (width));
}

RoninAudioProcessorEditor::~RoninAudioProcessorEditor() = default;

ronin_ui::Page* RoninAudioProcessorEditor::page (ronin_ui::Tab tab) noexcept
{
    switch (tab)
    {
        case ronin_ui::Tab::Voice: return &voice;
        case ronin_ui::Tab::Env: return &env;
        case ronin_ui::Tab::Patch: return &patch;
        case ronin_ui::Tab::Setup: return &setup;
        case ronin_ui::Tab::Main: break;
    }
    return nullptr;
}

void RoninAudioProcessorEditor::showTab (ronin_ui::Tab tab)
{
    if (strip.current() != tab)
    {
        strip.setCurrent (tab);   // calls back into showTab
        return;
    }
    bay.setVisible (tab == ronin_ui::Tab::Main);
    for (auto t : { ronin_ui::Tab::Voice, ronin_ui::Tab::Env, ronin_ui::Tab::Patch, ronin_ui::Tab::Setup })
        page (t)->setVisible (t == tab);
    if (tab == ronin_ui::Tab::Main)
        bay.repaint();
}

void RoninAudioProcessorEditor::applyScale (int percent)
{
    roninProcessor.setUiScalePercent (percent);
    const int width = kDefaultWidth * percent / 100;
    setSize (width, heightForWidth (width));
}

void RoninAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff101012));
}

void RoninAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    const int stripH = juce::roundToInt (static_cast<float> (getWidth()) * ronin_ui::kStripH / 1600.0f);
    strip.setBounds (area.removeFromTop (stripH));
    bay.setBounds (area);
    voice.setBounds (area);
    env.setBounds (area);
    patch.setBounds (area);
    setup.setBounds (area);
}
