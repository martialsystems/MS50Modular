// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "JackView.h"

JackView::JackView (int moduleIndex, int portIndex, PortType type, PortDir dir, juce::String displayName)
    : moduleIndex_ (moduleIndex),
      portIndex_ (portIndex),
      type_ (type),
      dir_ (dir),
      name_ (std::move (displayName))
{
    setInterceptsMouseClicks (false, false);
}

juce::Rectangle<float> JackView::circleBounds() const
{
    const float side = juce::jmin (static_cast<float> (getWidth()) - 4.0f,
                                    static_cast<float> (getHeight()) * 0.46f);
    const float diameter = juce::jmax (8.0f, side);
    return { (static_cast<float> (getWidth()) - diameter) * 0.5f, 2.0f, diameter, diameter };
}

juce::Rectangle<int> JackView::getHitBounds() const
{
    return circleBounds().getSmallestIntegerContainer().translated (getX(), getY());
}

void JackView::paint (juce::Graphics& g)
{
    juce::Colour fill (0xffe0a020);
    if (type_ == PortType::CV)
        fill = juce::Colour (0xff4c8dff);
    else if (type_ == PortType::Gate)
        fill = juce::Colours::white;

    const auto circle = circleBounds();
    g.setColour (fill);
    g.fillEllipse (circle);
    g.setColour (juce::Colour (0xff141414));
    g.drawEllipse (circle, 1.2f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    const auto text = getLocalBounds().withTrimmedTop (static_cast<int> (circle.getBottom()) + 1);
    g.drawFittedText (name_, text, juce::Justification::centredTop, 1);
}
