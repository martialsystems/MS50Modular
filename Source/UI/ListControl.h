// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
//
// The one rule for every control that steps through a list (MAIN and every tab): left-click steps forward,
// Shift-left-click steps back, right-click opens the whole list with the current item ticked. The scroll wheel
// keeps its own behaviour.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace ronin_ui {

struct ListControl {
    juce::String name;
    juce::StringArray items;
    int current = 0;
    bool steps = true;                  // false: a direct-pick control (segments) that also offers the list
    juce::Rectangle<float> design;      // where it sits, in its view's design units (tests and the probe)
    std::function<void (int)> choose;
};

// Menu item ids are index + 1; the current item is ticked.
juce::PopupMenu buildListMenu (const juce::StringArray& items, int current);
// The next index: forward, or back with Shift. Wraps.
int stepIndex (int current, int count, bool back) noexcept;
// Opens the list as a popup at the mouse (plain menu font). Picking an item calls choose.
void showListMenu (const ListControl& control, juce::Component* owner);
// Tests: receives the menu instead of showing it, with the callback a pick would run.
extern std::function<void (const juce::PopupMenu&, std::function<void (int)>)> listMenuSpy;

}
