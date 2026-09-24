// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Modular/Port.h"

#include <juce_gui_basics/juce_gui_basics.h>

// A jack circle. Clicks do not patch. Step 6 owns the drag.
class JackView : public juce::Component
{
public:
    JackView (int moduleIndex, int portIndex, PortType type, PortDir dir, juce::String displayName);

    void paint (juce::Graphics&) override;

    int getModuleIndex() const { return moduleIndex_; }
    int getPortIndex() const { return portIndex_; }
    PortType getPortType() const { return type_; }
    PortDir getPortDir() const { return dir_; }
    const juce::String& getDisplayName() const { return name_; }

    // Circle bounds in parent coordinates.
    juce::Rectangle<int> getHitBounds() const;

private:
    juce::Rectangle<float> circleBounds() const;

    int moduleIndex_;
    int portIndex_;
    PortType type_;
    PortDir dir_;
    juce::String name_;
};
