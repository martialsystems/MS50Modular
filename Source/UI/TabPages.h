// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// RONIN_Redesign §4.0-4.2: the tab strip (MAIN · VOICE · ENV · PATCH · MIDI · SETUP) above the unchanged panel
// art, and the five non-MAIN pages. A non-MAIN page replaces the whole face at the same size (1600 x 564 design
// units). Pages use the panel's plate, gold rules and labels in equal, mirrored blocks.

#pragma once

#include "PluginProcessor.h"
#include "UI/ListControl.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ronin_ui {

inline constexpr float kStripH = 36.0f;    // design px above the 1600 x 564 art (29 px at 1280 wide)
inline constexpr float kTabW = 120.0f;
inline constexpr int kTabCount = 6;
enum class Tab { Main = 0, Voice, Env, Patch, Midi, Setup };
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
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void visibilityChanged() override;

    // Click a design-unit point (probe and tests). True when a region took it.
    bool clickDesign (float x, float y, bool right = false, bool shift = false);
    // Every list control on the page, as last painted (tests and the probe).
    std::vector<ListControl> listControls();
    // Help text under the pointer for half a second is shown enlarged (tests: the zoomed note, or empty).
    juce::String zoomedNote() const { return zoomNote_; }
    void hoverDesign (float x, float y, double heldSeconds);
    // On-screen pixel size of help text drawn at a design size (never under 12 px, 9 pt).
    float noteSize (float designSize) const noexcept;
    float scale() const noexcept;
    juce::Point<float> origin() const noexcept;
    juce::Rectangle<float> toLocal (juce::Rectangle<float> design) const noexcept;

protected:
    struct Region {
        juce::Rectangle<float> design;
        std::function<void (bool right, bool shift)> action;
    };
    virtual void paintPage (juce::Graphics&) = 0;
    void addRegion (juce::Rectangle<float> design, std::function<void (bool right, bool shift)> action);

    // Drawing helpers, all in design units.
    void block (juce::Graphics&, juce::Rectangle<float> design, const juce::String& title);
    void text (juce::Graphics&, juce::Rectangle<float> design, const juce::String& s, float size,
               juce::Colour colour, juce::Justification just = juce::Justification::centred);
    void lcd (juce::Graphics&, juce::Rectangle<float> design, const juce::String& s, float size = 15.0f);
    // Help and description text (hints, explanations, footers, the load report): at least 9 pt on screen, and
    // enlarged after the pointer rests on it for half a second.
    void note (juce::Graphics&, juce::Rectangle<float> design, const juce::String& s,
               juce::Justification just = juce::Justification::centred);
    // Segmented toggle (direct pick); right-click opens the list. Registers the click regions.
    void toggle (juce::Graphics&, juce::Rectangle<float> design, const juce::StringArray& names, int selected,
                 std::function<void (int)> choose, const juce::String& name = {});
    // A list stepper: left-click forward, Shift-left-click back, right-click the list.
    void stepper (juce::Graphics&, juce::Rectangle<float> design, const juce::String& name,
                  const juce::StringArray& items, int current, std::function<void (int)> choose);
    void lamp (juce::Graphics&, juce::Point<float> centre, bool lit, juce::Colour colour);

    RoninAudioProcessor& processor;

    struct NoteRegion { juce::Rectangle<float> design; juce::String text; };
    std::vector<NoteRegion> notes_;

private:
    void timerCallback() override;
    void ensurePainted();
    void paintZoom (juce::Graphics&);
    std::vector<Region> regions_;
    std::vector<ListControl> lists_;
    juce::Point<float> hover_ { -1.0f, -1.0f };
    double hoverSince_ = 0.0;
    juce::String zoomNote_;
    juce::Rectangle<float> zoomAnchor_;
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

// MIDI IN jacks (patched to panel inputs from here) and MG host sync.
class MidiPage : public Page
{
public:
    using Page::Page;
    std::function<void()> onCablesChanged;
    // The panel inputs a MIDI jack can be patched to, as "SECTION:LABEL" (tests and the probe).
    static juce::StringArray patchTargets();
    // Toggle a cable from MIDI jack port (MidiIn::kNote ..) to a panel input id. True when the graph changed.
    bool togglePatch (int midiPort, const juce::String& target);
protected:
    void paintPage (juce::Graphics&) override;
private:
    void showPatchMenu (int midiPort);
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
