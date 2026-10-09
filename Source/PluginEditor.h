// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "PluginProcessor.h"
#include "UI/PatchBayView.h"
#include "UI/TabPages.h"

#include <juce_gui_basics/juce_gui_basics.h>

// RONIN_Redesign §4.0: the MAIN tab is the unchanged panel art (PatchBayView). The tab strip sits in its own
// 36-design-px band above it; a non-MAIN tab replaces the face at the same size. Default 1280 x 480.
class RoninAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoninAudioProcessorEditor (RoninAudioProcessor&);
    ~RoninAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kDefaultWidth = 1280;
    static int heightForWidth (int width) noexcept { return juce::roundToInt (width * (564.0 + static_cast<double> (ronin_ui::kStripH)) / 1600.0); }

    void showTab (ronin_ui::Tab tab);
    ronin_ui::Tab currentTab() const noexcept { return strip.current(); }
    ronin_ui::TabStrip& tabStrip() noexcept { return strip; }
    PatchBayView& patchBay() noexcept { return bay; }
    ronin_ui::Page* page (ronin_ui::Tab tab) noexcept;
    void applyScale (int percent);

private:
    RoninAudioProcessor& roninProcessor;
    juce::ComponentBoundsConstrainer constrainer;
    PatchBayView bay;   // child 0: the MAIN face
    ronin_ui::TabStrip strip;
    ronin_ui::VoicePage voice;
    ronin_ui::EnvPage env;
    ronin_ui::PatchPage patch;
    ronin_ui::MidiPage midi;
    ronin_ui::SetupPage setup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoninAudioProcessorEditor)
};
