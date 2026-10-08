// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// RONIN_Redesign §4.0-4.2: the tab strip (MAIN · VOICE · ENV · PATCH · SETUP) above the unchanged panel art,
// and the four non-MAIN pages. A non-MAIN page replaces the whole face at the same size (1600 x 564 design
// units). Pages use the panel's plate, gold rules and labels in equal, mirrored blocks.

#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ronin_ui {

inline constexpr float kStripH = 36.0f;    // design px above the 1600 x 564 art (29 px at 1280 wide)
inline constexpr float kTabW = 120.0f;
inline constexpr int kTabCount = 5;
enum class Tab { Main = 0, Voice, Env, Patch, Setup };
const char* tabName (Tab tab) noexcept;

class TabStrip : public juce::Component
{
public:
    TabStrip();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setCurrent (Tab tab);
    Tab current() const noexcept { return current_; }
    // Local bounds of one tab (for tests and the probe).
    juce::Rectangle<float> tabBounds (Tab tab) const;
    std::function<void (Tab)> onChange;

private:
    Tab current_ = Tab::Main;
};

// Base page: design-unit layout, a 20 Hz refresh, simple click regions.
class Page : public juce::Component, private juce::Timer
{
public:
    explicit Page (RoninAudioProcessor& p);
    ~Page() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    // Click a design-unit point (probe and tests). True when a region took it.
    bool clickDesign (float x, float y, bool right = false);
    float scale() const noexcept;
    juce::Point<float> origin() const noexcept;
    juce::Rectangle<float> toLocal (juce::Rectangle<float> design) const noexcept;

protected:
    struct Region {
        juce::Rectangle<float> design;
        std::function<void (bool right)> action;
    };
    virtual void paintPage (juce::Graphics&) = 0;
    void addRegion (juce::Rectangle<float> design, std::function<void (bool right)> action);

    // Drawing helpers, all in design units.
    void block (juce::Graphics&, juce::Rectangle<float> design, const juce::String& title);
    void text (juce::Graphics&, juce::Rectangle<float> design, const juce::String& s, float size,
               juce::Colour colour, juce::Justification just = juce::Justification::centred);
    void lcd (juce::Graphics&, juce::Rectangle<float> design, const juce::String& s, float size = 15.0f);
    // Segmented toggle; returns nothing, registers the click regions.
    void toggle (juce::Graphics&, juce::Rectangle<float> design, const juce::StringArray& names, int selected,
                 std::function<void (int)> choose);
    void lamp (juce::Graphics&, juce::Point<float> centre, bool lit, juce::Colour colour);

    RoninAudioProcessor& processor;

private:
    void timerCallback() override;
    std::vector<Region> regions_;
};

class VoicePage : public Page
{
public:
    using Page::Page;
protected:
    void paintPage (juce::Graphics&) override;
};

class EnvPage : public Page
{
public:
    using Page::Page;
protected:
    void paintPage (juce::Graphics&) override;
private:
    void paintEg1 (juce::Graphics&, juce::Rectangle<float> area);
    void paintEg2 (juce::Graphics&, juce::Rectangle<float> area);
};

class PatchPage : public Page
{
public:
    using Page::Page;
    int selectedCable() const noexcept { return selected_; }
    void selectCable (int index) { selected_ = index; repaint(); }
    bool keyPressed (const juce::KeyPress&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    // Row text for tests: "FROM -> TO | ROLE | FLAG".
    juce::String rowText (int index) const;
    int rowCount() const;
    std::function<void()> onCablesChanged;
protected:
    void paintPage (juce::Graphics&) override;
private:
    void showMenu (int index);
    int selected_ = -1;
    int scroll_ = 0;
};

class SetupPage : public Page
{
public:
    using Page::Page;
    std::function<void (int percent)> onScale;
    std::function<void()> onCableColourMode;
protected:
    void paintPage (juce::Graphics&) override;
};

// R14 role of a graph cable's source port, and the colour it is drawn with (override wins).
jcs::Role cableRole (const RoninAudioProcessor& p, const Cable& c);
juce::Colour cableColour (const RoninAudioProcessor& p, const Cable& c);
// Per-cable override palette (PATCH right-click). Index 0 = by role.
inline constexpr int kOverrideCount = 8;
const char* overrideName (int index) noexcept;
std::uint32_t overrideArgb (int index) noexcept;

}
