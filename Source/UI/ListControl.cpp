// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "UI/ListControl.h"

namespace ronin_ui {

std::function<void (const juce::PopupMenu&, std::function<void (int)>)> listMenuSpy;

juce::PopupMenu buildListMenu (const juce::StringArray& items, int current)
{
    juce::PopupMenu menu;
    for (int i = 0; i < items.size(); ++i)
        menu.addItem (i + 1, items[i], true, i == current);
    return menu;
}

int stepIndex (int current, int count, bool back) noexcept
{
    if (count <= 0)
        return 0;
    const int next = back ? current - 1 : current + 1;
    return ((next % count) + count) % count;
}

void showListMenu (const ListControl& control, juce::Component* owner)
{
    auto menu = buildListMenu (control.items, control.current);
    auto choose = control.choose;
    const int count = control.items.size();
    std::function<void (int)> pick = [choose, count] (int result) {
        if (result >= 1 && result <= count && choose)
            choose (result - 1);
    };
    if (listMenuSpy)
    {
        listMenuSpy (menu, pick);
        return;
    }
    juce::Component::SafePointer<juce::Component> safe (owner);
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safe, pick] (int result) {
        pick (result);
        if (safe != nullptr)
            safe->repaint();
    });
}

}
