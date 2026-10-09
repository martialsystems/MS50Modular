// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "UI/PatchBayView.h"

#include "Modular/EffectSwitch.h"
#include "Modular/FloatCompare.h"
#include "Modular/PatchState.h"
#include "Modular/Port.h"
#include "PanelAssets.h"
#include "UI/KnobUnits.h"
#include "UI/TabPages.h"

#include <algorithm>
#include <cmath>
#include <cstring>

struct CablePaint {
    juce::Colour base;
    juce::Colour dark;
    juce::Colour light;
};

namespace {

CablePaint paintFor (int color)
{
    switch (color)
    {
        case 1: return { juce::Colour (0xffece8da), juce::Colour (0xff86836f), juce::Colour (0xffffffff) };
        case 2: return { juce::Colour (0xffeabd2c), juce::Colour (0xff80600a), juce::Colour (0xfffff2a8) };
        case 3: return { juce::Colour (0xff33a352), juce::Colour (0xff10521f), juce::Colour (0xff98e6aa) };
        default: return { juce::Colour (0xffd23a30), juce::Colour (0xff6e100c), juce::Colour (0xffff9a8a) };
    }
}

// JCS R14 role colour or a PATCH-tab override, shaded like the palette cables.
CablePaint paintForColour (juce::Colour c)
{
    return { c, c.darker (0.9f), c.brighter (0.8f) };
}

// Momentary square key. The HOLD legend and the lamp bezel are already drawn on the plate.
// Mouse down holds the gate. Mouse up releases it. The cap sinks and the red lamp above it lights while down,
// the same lamp BUSHIDO shows under START.
void paintExtInHold (juce::Graphics& g, juce::Point<float> origin, float scale, bool held)
{
    const float lx = origin.x + kHoldLampCx * scale;
    const float ly = origin.y + kHoldLampCy * scale;
    const float lr = kHoldLampR * scale;
    if (held)
    {
        g.setColour (juce::Colour (0x55ff3b2b));
        g.fillEllipse (lx - lr * 2.2f, ly - lr * 2.2f, lr * 4.4f, lr * 4.4f);
    }
    g.setColour (held ? juce::Colour (0xffff4a36) : juce::Colour (0xff4a0c08));
    g.fillEllipse (lx - lr, ly - lr, 2.0f * lr, 2.0f * lr);

    const float sink = held ? 1.6f * scale : 0.0f;
    const float cx = origin.x + kHoldCx * scale;
    const float cy = origin.y + kHoldCy * scale + sink;

    const float cap = 26.0f * scale;
    const float x = cx - 13.0f * scale;
    const float y = cy - 13.0f * scale;
    juce::ColourGradient shell (juce::Colour (0xfff4eedc), x, y, juce::Colour (0xffb3ab94), x + cap, y + cap, false);
    g.setGradientFill (shell);
    g.fillRoundedRectangle (x, y, cap, cap, 3.0f * scale);
    g.setColour (juce::Colour (0xff6f6a5a));
    g.drawRoundedRectangle (x, y, cap, cap, 3.0f * scale, 0.9f * scale);

    g.setColour (juce::Colour (0xff7e7764).withAlpha (0.55f));
    g.fillRoundedRectangle (x, cy + 9.0f * scale, cap, 4.0f * scale, 2.0f * scale);

    const float faceX = cx - 9.5f * scale;
    const float faceY = cy - 10.0f * scale;
    const float faceW = 19.0f * scale;
    const float faceH = 17.0f * scale;
    const float gx = faceX + faceW * 0.45f;
    const float gy = faceY + faceH * 0.40f;
    juce::ColourGradient face (juce::Colour (0xfff8f3e4), gx, gy, juce::Colour (0xffd0c8b2), gx + 0.8f * faceW, gy, true);
    g.setGradientFill (face);
    g.fillRoundedRectangle (faceX, faceY, faceW, faceH, 2.5f * scale);
}

void paintEffectRocker (juce::Graphics& g, juce::Point<float> origin, float scale, bool on)
{
    const float x = origin.x + kPowerX * scale;
    const float y = origin.y + kPowerY * scale;
    const float w = kPowerW * scale;
    const float h = kPowerH * scale;
    const float half = w * 0.5f;
    const auto raised = juce::Colour (0xff3a3a3e);
    const auto pressed = juce::Colour (0xff101012);
    // EFFECT rocker, not power. Off is dry, on is wet. The graph keeps running either way.
    // Printed OFF is left of the rocker and ON is right of it.
    // The raised end points at the active word. Right half stays the ON click.
    const bool raisedOnRight = on;
    g.setColour (raisedOnRight ? pressed : raised);
    g.fillRoundedRectangle (x, y, half, h, 2.5f * scale);
    g.setColour (raisedOnRight ? raised : pressed);
    g.fillRoundedRectangle (x + half, y, half, h, 2.5f * scale);

    g.setColour (juce::Colour (0xff77777c));
    const float lineLeft = raisedOnRight ? x + half + 2.0f * scale : x + 2.0f * scale;
    const float lineRight = raisedOnRight ? x + w - 2.0f * scale : x + half - 2.0f * scale;
    g.drawLine (lineLeft, y + 1.2f * scale, lineRight, y + 1.2f * scale, 1.0f * scale);
    g.setColour (juce::Colours::black);
    g.drawLine (x + half, y + scale, x + half, y + h - scale, 1.0f * scale);
}

const int* lcdRows (juce::juce_wchar ch)
{
    struct Glyph
    {
        char ch;
        int row[7];
    };
    static constexpr int kBlank[7] = { 0, 0, 0, 0, 0, 0, 0 };
    static constexpr Glyph kFont[] = {
        { ' ', { 0, 0, 0, 0, 0, 0, 0 } },
        { '0', { 14, 17, 19, 21, 25, 17, 14 } },
        { '1', { 4, 12, 4, 4, 4, 4, 14 } },
        { '2', { 14, 17, 1, 2, 4, 8, 31 } },
        { '3', { 31, 2, 4, 2, 1, 17, 14 } },
        { '4', { 2, 6, 10, 18, 31, 2, 2 } },
        { '5', { 31, 16, 30, 1, 1, 17, 14 } },
        { '6', { 6, 8, 16, 30, 17, 17, 14 } },
        { '7', { 31, 1, 2, 4, 8, 8, 8 } },
        { '8', { 14, 17, 17, 14, 17, 17, 14 } },
        { '9', { 14, 17, 17, 15, 1, 2, 12 } },
        { 'A', { 14, 17, 17, 17, 31, 17, 17 } },
        { 'B', { 30, 17, 17, 30, 17, 17, 30 } },
        { 'C', { 14, 17, 16, 16, 16, 17, 14 } },
        { 'D', { 28, 18, 17, 17, 17, 18, 28 } },
        { 'E', { 31, 16, 16, 30, 16, 16, 31 } },
        { 'F', { 31, 16, 16, 30, 16, 16, 16 } },
        { 'G', { 14, 17, 16, 23, 17, 17, 15 } },
        { 'H', { 17, 17, 17, 31, 17, 17, 17 } },
        { 'I', { 14, 4, 4, 4, 4, 4, 14 } },
        { 'J', { 7, 2, 2, 2, 2, 18, 12 } },
        { 'K', { 17, 18, 20, 24, 20, 18, 17 } },
        { 'L', { 16, 16, 16, 16, 16, 16, 31 } },
        { 'M', { 17, 27, 21, 21, 17, 17, 17 } },
        { 'N', { 17, 17, 25, 21, 19, 17, 17 } },
        { 'O', { 14, 17, 17, 17, 17, 17, 14 } },
        { 'P', { 30, 17, 17, 30, 16, 16, 16 } },
        { 'Q', { 14, 17, 17, 17, 21, 18, 13 } },
        { 'R', { 30, 17, 17, 30, 20, 18, 17 } },
        { 'S', { 15, 16, 16, 14, 1, 1, 30 } },
        { 'T', { 31, 4, 4, 4, 4, 4, 4 } },
        { 'U', { 17, 17, 17, 17, 17, 17, 14 } },
        { 'V', { 17, 17, 17, 17, 17, 10, 4 } },
        { 'W', { 17, 17, 17, 21, 21, 21, 10 } },
        { 'X', { 17, 17, 10, 4, 10, 17, 17 } },
        { 'Y', { 17, 17, 17, 10, 4, 4, 4 } },
        { 'Z', { 31, 1, 2, 4, 8, 16, 31 } },
        { '&', { 12, 18, 20, 8, 21, 18, 13 } },
        { '-', { 0, 0, 0, 31, 0, 0, 0 } },
        { '+', { 0, 4, 4, 31, 4, 4, 0 } },
        { '/', { 0, 1, 2, 4, 8, 16, 0 } },
        { '.', { 0, 0, 0, 0, 0, 12, 12 } },
        { '>', { 8, 4, 2, 1, 2, 4, 8 } },
    };

    const char ascii = (ch >= 32 && ch < 127) ? static_cast<char> (ch) : ' ';
    for (const auto& glyph : kFont)
        if (glyph.ch == ascii)
            return glyph.row;
    return kBlank;
}

juce::String presetScreenLine (int index, const juce::String& hostName)
{
    // The program's own name, upper case. Factory names are written to fit the screen.
    const juce::String name = hostName.toUpperCase();
    return (juce::String (index + 1).paddedLeft ('0', 2) + " " + name).substring (0, kPresetChars);
}

void paintLcdDots (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& text,
                   juce::Colour ink, float ghostAlpha)
{
    const float pitch = juce::jmin (area.getWidth() / (static_cast<float> (kPresetChars) * 6.0f),
                                     area.getHeight() / 8.0f);
    const float dot = pitch * 0.86f;
    const float ox = area.getX() + (area.getWidth() - static_cast<float> (kPresetChars) * 6.0f * pitch) * 0.5f
                     + pitch * 0.5f;
    const float oy = area.getY() + (area.getHeight() - 7.0f * pitch) * 0.5f;
    for (int column = 0; column < kPresetChars; ++column)
    {
        const juce::juce_wchar ch = column < text.length() ? text[column] : static_cast<juce::juce_wchar> (' ');
        const int* rows = lcdRows (ch);
        for (int row = 0; row < 7; ++row)
        {
            for (int bit = 0; bit < 5; ++bit)
            {
                const bool on = ((rows[row] >> (4 - bit)) & 1) != 0;
                g.setColour (ink.withAlpha (on ? 0.9f : ghostAlpha));
                g.fillRect (ox + (static_cast<float> (column) * 6.0f + static_cast<float> (bit)) * pitch,
                            oy + static_cast<float> (row) * pitch,
                            dot, dot);
            }
        }
    }
}

// The PRESET list drops below the screen in rows of 21 design px. A bank longer than the bay can show splits
// into balanced columns side by side, so every program stays on screen and clickable.
constexpr int kPresetMenuMaxRows = 18;
constexpr float kPresetRowPitch = 21.0f;

int presetMenuColumns (int count)
{
    return count > kPresetMenuMaxRows ? (count + kPresetMenuMaxRows - 1) / kPresetMenuMaxRows : 1;
}

int presetMenuRows (int count)
{
    const int columns = presetMenuColumns (count);
    return (count + columns - 1) / columns;
}

float presetMenuColumnWidth()
{
    return (kPresetKeyX + kPresetKeyW) - kPresetBezelX;
}

juce::Rectangle<float> presetMenuDesign (int count)
{
    return { kPresetBezelX,
             kPresetBezelY + kPresetBezelH + 3.0f,
             presetMenuColumnWidth() * static_cast<float> (presetMenuColumns (count)),
             10.0f + static_cast<float> (presetMenuRows (count)) * kPresetRowPitch - 3.0f };
}

// Design-space rectangle of one program's row: column-major, top to bottom then left to right.
juce::Rectangle<float> presetRowDesign (int count, int index)
{
    const auto box = presetMenuDesign (count);
    const int rows = presetMenuRows (count);
    const int column = index / rows;
    const int row = index % rows;
    return { box.getX() + 5.0f + static_cast<float> (column) * presetMenuColumnWidth(),
             box.getY() + 5.0f + static_cast<float> (row) * kPresetRowPitch,
             presetMenuColumnWidth() - 10.0f,
             18.0f };
}

void paintKnobCap (juce::Graphics& g, juce::Point<float> centre, float radius, float scale, bool isSwitch, float value)
{
    // Ticks stay on the plate. The cap, its shadow, and the pointer turn as one piece.
    const float angleDeg = panelKnobAngleDegrees (isSwitch, value);
    const float angle = juce::degreesToRadians (angleDeg);
    const auto turn = juce::AffineTransform::rotation (angle, centre.x, centre.y);

    const float shadowR = radius + 4.0f * scale;
    juce::Point<float> shadowCentre (centre.x + scale, centre.y + 2.0f * scale);
    shadowCentre.applyTransform (turn);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (shadowCentre.x - shadowR, shadowCentre.y - shadowR, shadowR * 2.0f, shadowR * 2.0f);

    const float skirt = radius + 3.0f * scale;
    g.setColour (juce::Colour (0xff08080a));
    g.fillEllipse (centre.x - skirt, centre.y - skirt, skirt * 2.0f, skirt * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (centre.x - skirt, centre.y - skirt, skirt * 2.0f, skirt * 2.0f, 0.8f * scale);

    juce::Path ring;
    const float ringRadius = radius + 1.2f * scale;
    ring.addEllipse (centre.x - ringRadius, centre.y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f);
    const float dashes[] = { 0.8f * scale, 1.6f * scale };
    juce::Path dashed;
    juce::PathStrokeType (2.4f * scale).createDashedStroke (dashed, ring, dashes, 2);
    g.setColour (juce::Colour (0xff3c3c3f).withAlpha (0.75f));
    g.fillPath (dashed);

    const float body = radius - 0.3f * scale;
    juce::Point<float> metalHi (centre.x - body, centre.y - body);
    juce::Point<float> metalLo (centre.x + body, centre.y + body);
    metalHi.applyTransform (turn);
    metalLo.applyTransform (turn);
    juce::ColourGradient metal (juce::Colour (0xff4b4b4e), metalHi.x, metalHi.y,
                                juce::Colour (0xff060607), metalLo.x, metalLo.y, false);
    metal.addColour (0.5, juce::Colour (0xff1a1a1b));
    g.setGradientFill (metal);
    g.fillEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (centre.x - body, centre.y - body, body * 2.0f, body * 2.0f, 0.8f * scale);

    const float cap = radius * 0.8f;
    juce::Point<float> capHi (centre.x - cap, centre.y - cap);
    juce::Point<float> capLo (centre.x + cap, centre.y + cap);
    capHi.applyTransform (turn);
    capLo.applyTransform (turn);
    juce::ColourGradient top (juce::Colour (0xff2a2a2c), capHi.x, capHi.y,
                              juce::Colour (0xff131314), capLo.x, capLo.y, false);
    g.setGradientFill (top);
    g.fillEllipse (centre.x - cap, centre.y - cap, cap * 2.0f, cap * 2.0f);

    {
        juce::Graphics::ScopedSaveState clip (g);
        juce::Path capDisc;
        capDisc.addEllipse (centre.x - cap, centre.y - cap, cap * 2.0f, cap * 2.0f);
        g.reduceClipRegion (capDisc);
        const float blobR = cap * 0.85f;
        juce::Point<float> blob (centre.x, centre.y + cap * 0.62f);
        blob.applyTransform (turn);
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.fillEllipse (blob.x - blobR, blob.y - blobR, blobR * 2.0f, blobR * 2.0f);
    }

    g.setColour (juce::Colour (0xff050505));
    g.drawEllipse (centre.x - cap, centre.y - cap, cap * 2.0f, cap * 2.0f, 0.8f * scale);

    juce::Path shine;
    shine.addEllipse (-radius * 0.45f, -radius * 0.28f, radius * 0.90f, radius * 0.56f);
    shine.applyTransform (juce::AffineTransform::translation (centre.x - radius * 0.28f, centre.y - radius * 0.32f)
                              .followedBy (juce::AffineTransform::rotation (juce::degreesToRadians (-35.0f + angleDeg),
                                                                             centre.x, centre.y)));
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillPath (shine);

    const auto spin = juce::AffineTransform::rotation (angle, centre.x, centre.y);
    juce::Line<float> shade (centre.x + 1.3f * scale, centre.y - radius * 0.1f,
                             centre.x + 1.3f * scale, centre.y - (radius - 1.5f * scale));
    shade.applyTransform (spin);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (shade, 3.0f * scale);

    g.setColour (juce::Colour (0xfff1ede0));
    juce::Line<float> pointer (centre.x, centre.y - radius * 0.1f,
                               centre.x, centre.y - (radius - 1.5f * scale));
    pointer.applyTransform (spin);
    g.drawLine (pointer, 2.4f * scale);
}

std::unique_ptr<juce::Drawable> loadPanelBackground()
{
    if (auto drawn = juce::Drawable::createFromImageData (PanelAssets::panel_bg_svg, PanelAssets::panel_bg_svgSize))
        return drawn;

    // The grain filter is optional. Vector wear stays if the filter rejects the parse.
    juce::String svg (reinterpret_cast<const char*> (PanelAssets::panel_bg_svg),
                      static_cast<size_t> (PanelAssets::panel_bg_svgSize));
    for (;;)
    {
        const int open = svg.indexOfIgnoreCase ("<filter");
        if (open < 0)
            break;
        const int close = svg.indexOfIgnoreCase (open, "</filter>");
        if (close < 0)
            break;
        svg = svg.substring (0, open) + svg.substring (close + 9);
    }

    if (auto xml = juce::parseXML (svg))
        return juce::Drawable::createFromSVG (*xml);
    return {};
}

float clampf (float value, float low, float high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

void repel (float& px, float& py, float x, float y, float radius)
{
    const float dx = px - x;
    const float dy = py - y;
    const float distance = std::hypot (dx, dy);
    if (distance >= radius)
        return;

    const float force = (radius - distance) / (distance > 0.0f ? distance : 1.0f) * 0.6f;
    px += (distance > 0.0f ? dx : 0.0f) * force;
    py += distance > 0.0f ? dy * force : radius * 0.6f;
}

float segmentDistance (float px, float py, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax;
    const float dy = by - ay;
    const float length2 = dx * dx + dy * dy;
    float t = length2 > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / length2 : 0.0f;
    t = clampf (t, 0.0f, 1.0f);
    return std::hypot (px - (ax + t * dx), py - (ay + t * dy));
}

}

class StackMenu : public juce::Component
{
public:
    explicit StackMenu (PatchBayView& bay) : owner (bay)
    {
        setInterceptsMouseClicks (true, true);
    }

    void showFor (int jack)
    {
        if (jack < 0 || jack >= kPanelJackCount)
            return;

        jackIndex = jack;
        rowCount = 0;
        int plugs[kPatchBayMaxCables] {};
        const int n = owner.plugsOnJack (jack, plugs, kPatchBayMaxCables);
        for (int i = n - 1; i >= 0 && rowCount < kMaxRows - 1; --i)
        {
            Row& row = rows[rowCount];
            row.cable = plugs[i];
            row.isNew = false;
            int jackA = -1;
            int jackB = -1;
            owner.visualEnds (plugs[i], jackA, jackB);
            row.endA = jackB != jack;
            const int other = row.endA ? jackB : jackA;
            if (other >= 0 && other < kPanelJackCount)
                row.text = juce::String (kPanelJacks[other].section) + ": " + kPanelJacks[other].label;
            else
                row.text = "loose end";
            row.color = owner.visualColor (plugs[i]);
            ++rowCount;
        }

        rows[rowCount].cable = -1;
        rows[rowCount].isNew = true;
        rows[rowCount].endA = false;
        rows[rowCount].text = "+ New cable here";
        rows[rowCount].color = 0;
        ++rowCount;

        // Fill colors from the owner's visual cables via a second pass in show.
        refreshColors();

        const float scaleX = owner.designToLocal (kPanelJacks[jack].x, kPanelJacks[jack].y).x;
        const float scaleY = owner.designToLocal (kPanelJacks[jack].x, kPanelJacks[jack].y).y;
        const int width = 240;
        const int height = 26 + rowCount * 28;
        int x = static_cast<int> (scaleX);
        int y = static_cast<int> (scaleY) + 18;
        if (x + width > owner.getWidth() - 4)
            x = owner.getWidth() - width - 4;
        if (x < 4)
            x = 4;
        if (y + height > owner.getHeight() - 4)
            y = static_cast<int> (scaleY) - height - 18;
        if (y < 4)
            y = 4;
        setBounds (x, y, width, height);
        setVisible (true);
        toFront (false);
        repaint();
    }

    void refreshColors()
    {
        for (int i = 0; i < rowCount; ++i)
        {
            if (rows[i].isNew || rows[i].cable < 0)
            {
                rows[i].color = owner.selectedColor();
                continue;
            }
            rows[i].color = owner.visualColor (rows[i].cable);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff1b1b1e));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
        g.setColour (juce::Colour (0xff3a3a3e));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

        g.setColour (juce::Colour (0xffd8d2bd).withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        const juce::String title = juce::String (kPanelJacks[jackIndex].section) + ": "
                                    + kPanelJacks[jackIndex].label;
        g.drawText (title, 8, 4, getWidth() - 16, 18, juce::Justification::centredLeft, true);

        for (int i = 0; i < rowCount; ++i)
        {
            auto row = juce::Rectangle<int> (4, 24 + i * 28, getWidth() - 8, 26);
            if (i == dragRow)
                g.setColour (juce::Colour (0xff34343a));
            else
                g.setColour (juce::Colours::transparentBlack);
            g.fillRoundedRectangle (row.toFloat(), 5.0f);

            const auto paint = paintFor (rows[i].color);
            g.setColour (rows[i].isNew ? paint.base.withAlpha (0.5f) : paint.base);
            g.fillEllipse (static_cast<float> (row.getX()) + 8.0f, static_cast<float> (row.getCentreY()) - 6.0f, 12.0f, 12.0f);
            g.setColour (juce::Colour (0xffd8d2bd));
            g.drawText (rows[i].text, row.getX() + 28, row.getY(), row.getWidth() - 36, row.getHeight(),
                        juce::Justification::centredLeft, true);
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        dragRow = rowAt (event.y);
        dragMoved = false;
        dragY = static_cast<float> (event.y);
        if (dragRow >= 0 && rows[dragRow].isNew)
            dragRow = -2;
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (dragRow < 0)
            return;
        if (! dragMoved && std::abs (static_cast<float> (event.y) - dragY) < 4.0f)
            return;

        dragMoved = true;
        const int target = rowAt (static_cast<int> (event.y));
        if (target < 0 || target == dragRow || rows[target].isNew)
            return;

        const Row moving = rows[dragRow];
        if (target < dragRow)
        {
            for (int i = dragRow; i > target; --i)
                rows[i] = rows[i - 1];
        }
        else
        {
            for (int i = dragRow; i < target; ++i)
                rows[i] = rows[i + 1];
        }
        rows[target] = moving;
        dragRow = target;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        if (dragRow == -2)
        {
            dragRow = -1;
            const int jack = jackIndex;
            setVisible (false);
            owner.menuAdd (jack);
            return;
        }

        if (dragRow < 0)
            return;

        const int row = rowAt (static_cast<int> (event.y));
        const bool moved = dragMoved;
        const int jack = jackIndex;
        if (moved)
        {
            int top[kPatchBayMaxCables] {};
            int n = 0;
            for (int i = 0; i < rowCount; ++i)
            {
                if (! rows[i].isNew && rows[i].cable >= 0)
                    top[n++] = rows[i].cable;
            }
            dragRow = -1;
            dragMoved = false;
            owner.menuReorder (jack, top, n);
            return;
        }

        if (row < 0 || rows[row].isNew)
        {
            dragRow = -1;
            return;
        }

        const int cable = rows[row].cable;
        const bool endA = rows[row].endA;
        dragRow = -1;
        setVisible (false);
        owner.menuPickup (cable, endA);
    }

private:
    struct Row {
        int cable = -1;
        bool endA = false;
        bool isNew = false;
        int color = 0;
        juce::String text;
    };

    static constexpr int kMaxRows = 17;

    int rowAt (int y) const
    {
        const int index = (y - 24) / 28;
        if (index < 0 || index >= rowCount)
            return -1;
        return index;
    }

    PatchBayView& owner;
    Row rows[kMaxRows] {};
    int rowCount = 0;
    int jackIndex = -1;
    int dragRow = -1;
    float dragY = 0.0f;
    bool dragMoved = false;
};

PatchBayView::PatchBayView (RoninAudioProcessor& processor)
    : audioProcessor (processor)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);   // paint covers every pixel (the cached plate), so partial repaints skip the parent
    stroke_.preallocateSpace (768);
    menu_ = std::make_unique<StackMenu> (*this);
    addChildComponent (*menu_);
    for (int i = 0; i < kPanelKnobCount; ++i)
        knobValue_[i] = kPanelKnobs[i].valueDefault;

    panel_ = loadPanelBackground();
    reloadPublishedCables();
    for (int i = 0; i < kPanelKnobCount; ++i)
        paintedKnob_[i] = -1.0f;
    updateTimer();
}

void PatchBayView::visibilityChanged()
{
    updateTimer();
}

void PatchBayView::parentHierarchyChanged()
{
    updateTimer();
}

void PatchBayView::updateTimer()
{
    // 60 Hz while the MAIN face is visible (or not yet on a desktop, as in the probe); stopped on other tabs.
    const bool wanted = isVisible() && (getPeer() == nullptr ? true : isShowing());
    if (wanted && ! isTimerRunning())
    {
        paintedSignature_.clear();   // first frame after showing repaints everything
        startTimerHz (60);
    }
    else if (! wanted && isTimerRunning())
        stopTimer();
}

juce::Rectangle<int> PatchBayView::designRectToLocal (float x, float y, float w, float h) const
{
    const auto origin = panelOrigin();
    const float scale = panelScale();
    return juce::Rectangle<float> (origin.x + x * scale, origin.y + y * scale, w * scale, h * scale)
        .getSmallestIntegerContainer()
        .expanded (2);
}

juce::String PatchBayView::frameSignature() const
{
    // Everything the timer can change besides ropes, knobs and the needle. A change repaints the whole face.
    juce::String s;
    s << audioProcessor.getCurrentProgram() << '|' << (audioProcessor.effectIsOn() ? 1 : 0) << '|'
      << (audioProcessor.extInButtonHeld() ? 1 : 0) << '|' << audioProcessor.presetError() << '|' << status_ << '|'
      << hoverJack_ << '|' << count_ << '|' << (grabActive_ ? 1 : 0) << '|' << (presetMenu_ ? 1 : 0) << '|'
      << currentColor_ << '|' << getWidth() << 'x' << getHeight() << '|' << knobReadout_;
    Cable published[kPatchBayMaxCables] {};
    const int n = audioProcessor.copyPublishedCables (published, kPatchBayMaxCables);
    s << '|' << n << '|' << (audioProcessor.cableColourByRole() ? 1 : 0);
    for (int i = 0; i < n; ++i)
        s << ',' << static_cast<juce::int64> (published[i].colour) << (published[i].legacyInvert ? 'L' : 'n');
    return s;
}

void PatchBayView::paintBackdrop (juce::Graphics& g)
{
    // The plate fill and the panel art never change: draw them once into an image at the physical pixel size and
    // copy it. At a 1:1 pixel scale the copy is bit-identical to drawing them directly.
    const float physical = juce::jmax (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int w = juce::jmax (1, juce::roundToInt (static_cast<float> (getWidth()) * physical));
    const int h = juce::jmax (1, juce::roundToInt (static_cast<float> (getHeight()) * physical));
    if (! backdrop_.isValid() || backdrop_.getWidth() != w || backdrop_.getHeight() != h
        || ! ronin::exactlyEqual (backdropScale_, physical))
    {
        backdrop_ = juce::Image (juce::Image::ARGB, w, h, false, juce::SoftwareImageType());
        backdropScale_ = physical;
        juce::Graphics ig (backdrop_);
        ig.addTransform (juce::AffineTransform::scale (physical));
        ig.fillAll (juce::Colour (0xff101012));
        const auto origin = panelOrigin();
        const float scale = panelScale();
        const auto area = juce::Rectangle<float> (origin.x, origin.y, kPanelW * scale, kPanelH * scale);
        if (panel_ != nullptr)
        {
            // Wear sticks outside the viewBox. Fit the viewBox, not the drawable bounds, so the knobs land on the ticks.
            juce::Graphics::ScopedSaveState clip (ig);
            ig.reduceClipRegion (area.toNearestInt());
            panel_->draw (ig, 1.0f, juce::AffineTransform::scale (scale, scale).translated (origin.x, origin.y));
        }
        else
            ig.fillAll (juce::Colour (0xff1a1a1c));
    }
    if (ronin::exactlyEqual (physical, 1.0f))
        g.drawImageAt (backdrop_, 0, 0);
    else
        g.drawImageTransformed (backdrop_, juce::AffineTransform::scale (1.0f / physical));
}

PatchBayView::~PatchBayView()
{
    stopTimer();
    closeValueEditor();
}

float PatchBayView::panelScale() const
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return 1.0f;
    const float sx = static_cast<float> (getWidth()) / kPanelW;
    const float sy = static_cast<float> (getHeight()) / kPanelH;
    return sx < sy ? sx : sy;
}

juce::Point<float> PatchBayView::panelOrigin() const
{
    const float scale = panelScale();
    return { (static_cast<float> (getWidth()) - kPanelW * scale) * 0.5f,
             (static_cast<float> (getHeight()) - kPanelH * scale) * 0.5f };
}

juce::Point<float> PatchBayView::designToLocal (float x, float y) const
{
    const auto origin = panelOrigin();
    const float scale = panelScale();
    return { origin.x + x * scale, origin.y + y * scale };
}

juce::Point<float> PatchBayView::localToDesign (juce::Point<float> local) const
{
    const auto origin = panelOrigin();
    const float scale = panelScale();
    if (scale <= 0.0f)
        return {};
    return { (local.x - origin.x) / scale, (local.y - origin.y) / scale };
}

void PatchBayView::visualEnds (int index, int& jackA, int& jackB) const
{
    if (index < 0 || index >= count_)
    {
        jackA = -1;
        jackB = -1;
        return;
    }
    jackA = cables_[index].a;
    jackB = cables_[index].b;
}

bool PatchBayView::menuOpen() const
{
    return menu_ != nullptr && menu_->isVisible();
}

int PatchBayView::plugsOnJack (int jack, int* out, int capacity) const
{
    return plugsAtJack (cables_, count_, jack, out, capacity);
}

bool PatchBayView::reorderStack (int jack, const int* bottomToTop, int n)
{
    if (bottomToTop == nullptr || n <= 0)
        return false;

    int slots[kPatchBayMaxCables] {};
    const int onJack = plugsAtJack (cables_, count_, jack, slots, kPatchBayMaxCables);
    if (onJack != n)
        return false;

    Rope taken[kPatchBayMaxCables] {};
    for (int i = 0; i < n; ++i)
    {
        const int index = bottomToTop[i];
        if (index < 0 || index >= count_)
            return false;
        taken[i] = ropes_[index];
    }

    if (! reorderJackStack (cables_, count_, jack, bottomToTop, n))
        return false;

    for (int i = 0; i < n; ++i)
        ropes_[slots[i]] = taken[i];
    repaint();
    return true;
}

void PatchBayView::advanceCableFrame()
{
    prepareRopes (true);
    stepRopes();
    repaint();
}

void PatchBayView::menuReorder (int jack, const int* topFirst, int n)
{
    if (topFirst == nullptr || n <= 0 || n > kPatchBayMaxCables)
        return;

    int bottom[kPatchBayMaxCables] {};
    for (int i = 0; i < n; ++i)
        bottom[n - 1 - i] = topFirst[i];

    reorderStack (jack, bottom, n);
    if (menu_ != nullptr)
        menu_->showFor (jack);
}

void PatchBayView::menuPickup (int cableIndex, bool endIsA)
{
    if (cableIndex < 0 || cableIndex >= count_)
        return;
    startGrab (cableIndex, endIsA, false, true);
}

void PatchBayView::menuAdd (int jack)
{
    startNewCable (jack);
    grabCarry_ = true;
}

void PatchBayView::showStatus (const char* text)
{
    status_ = text != nullptr ? text : "";
    repaint();
}

void PatchBayView::stackLevels (int* levelA, int* levelB) const
{
    int used[kPanelJackCount] {};
    for (int i = 0; i < count_; ++i)
    {
        const int jackA = cables_[i].a;
        const int jackB = cables_[i].b;
        levelA[i] = 0;
        levelB[i] = 0;
        if (jackA >= 0 && jackA < kPanelJackCount)
        {
            levelA[i] = used[jackA];
            ++used[jackA];
        }
        if (jackB >= 0 && jackB < kPanelJackCount)
        {
            levelB[i] = used[jackB];
            ++used[jackB];
        }
    }
}

void PatchBayView::endPin (int jack, int level, float& x, float& y) const
{
    if (jack < 0 || jack >= kPanelJackCount)
    {
        x = pointerX_;
        y = pointerY_ + 4.0f;
        return;
    }

    x = kPanelJacks[jack].x - 2.0f * static_cast<float> (level);
    y = kPanelJacks[jack].y - 3.0f * static_cast<float> (level) + 4.0f;
}

void PatchBayView::prepareRopes (bool settleFresh)
{
    int levelA[kPatchBayMaxCables] {};
    int levelB[kPatchBayMaxCables] {};
    stackLevels (levelA, levelB);

    bool fresh = false;
    for (int i = 0; i < count_; ++i)
    {
        if (ropes_[i].ready)
            continue;

        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        endPin (cables_[i].a, levelA[i], ax, ay);
        endPin (cables_[i].b, levelB[i], bx, by);
        for (int n = 0; n < kNodes; ++n)
        {
            const float t = static_cast<float> (n) / static_cast<float> (kNodes - 1);
            const float sag = std::sin (3.14159265f * t) * 40.0f;
            ropes_[i].p[n][0] = ax + (bx - ax) * t;
            ropes_[i].p[n][1] = ay + (by - ay) * t + sag;
            ropes_[i].q[n][0] = ropes_[i].p[n][0];
            ropes_[i].q[n][1] = ropes_[i].p[n][1];
        }
        ropes_[i].ready = true;
        fresh = true;
    }

    if (fresh && settleFresh)
    {
        for (int i = 0; i < 160; ++i)
            stepRopes();
    }
}

void PatchBayView::stepRopes()
{
    int levelA[kPatchBayMaxCables] {};
    int levelB[kPatchBayMaxCables] {};
    stackLevels (levelA, levelB);

    for (int cable = 0; cable < count_; ++cable)
    {
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        endPin (cables_[cable].a, levelA[cable], ax, ay);
        endPin (cables_[cable].b, levelB[cable], bx, by);
        const float span = std::hypot (bx - ax, by - ay);
        const float slack = span * 0.3f + 40.0f;
        const float length = span + (slack < 140.0f ? slack : 140.0f);
        const float segment = length / static_cast<float> (kNodes - 1);
        Rope& rope = ropes_[cable];

        for (int n = 1; n < kNodes - 1; ++n)
        {
            const float x = rope.p[n][0];
            const float y = rope.p[n][1];
            rope.p[n][0] = x + (x - rope.q[n][0]) * 0.985f;
            rope.p[n][1] = y + (y - rope.q[n][1]) * 0.985f + 0.45f;
            rope.q[n][0] = x;
            rope.q[n][1] = y;
        }

        const bool grabbed = grabActive_ && grabIndex_ == cable;
        for (int iteration = 0; iteration < 10; ++iteration)
        {
            rope.p[0][0] = ax;
            rope.p[0][1] = ay;
            rope.p[kNodes - 1][0] = bx;
            rope.p[kNodes - 1][1] = by;

            for (int n = 0; n < kNodes - 1; ++n)
            {
                const float dx = rope.p[n + 1][0] - rope.p[n][0];
                const float dy = rope.p[n + 1][1] - rope.p[n][1];
                const float distance = std::hypot (dx, dy);
                const float safe = distance > 1.0e-3f ? distance : 1.0e-3f;
                const float correction = (distance - segment) / safe;
                const float weightA = n > 0 ? 1.0f : 0.0f;
                const float weightB = n < kNodes - 2 ? 1.0f : 0.0f;
                const float weight = weightA + weightB;
                if (weight <= 0.0f)
                    continue;
                rope.p[n][0] += dx * correction * weightA / weight;
                rope.p[n][1] += dy * correction * weightA / weight;
                rope.p[n + 1][0] -= dx * correction * weightB / weight;
                rope.p[n + 1][1] -= dy * correction * weightB / weight;
            }

            for (int n = 1; n < kNodes - 1; ++n)
            {
                if (hoverJack_ >= 0)
                    repel (rope.p[n][0], rope.p[n][1], kPanelJacks[hoverJack_].x, kPanelJacks[hoverJack_].y, 40.0f);
                if (pointerIn_ && ! grabbed)
                    repel (rope.p[n][0], rope.p[n][1], pointerX_, pointerY_, 28.0f);
                if (hoverLabel_ >= 0 && hoverLabel_ < kPanelLabelCount)
                {
                    const PanelLabelRec& label = kPanelLabels[hoverLabel_];
                    const float x0 = label.x - 10.0f;
                    const float y0 = label.y - 10.0f;
                    const float x1 = label.x + label.w + 10.0f;
                    const float y1 = label.y + label.h + 10.0f;
                    if (rope.p[n][0] > x0 && rope.p[n][0] < x1 && rope.p[n][1] > y0 && rope.p[n][1] < y1)
                    {
                        const float left = rope.p[n][0] - x0;
                        const float right = x1 - rope.p[n][0];
                        const float top = rope.p[n][1] - y0;
                        const float bottom = y1 - rope.p[n][1];
                        const bool exitLeft = left <= right && left <= top && left <= bottom;
                        const bool exitRight = right < left && right <= top && right <= bottom;
                        const bool exitTop = top < left && top < right && top <= bottom;
                        if (exitLeft)
                            rope.p[n][0] -= left * 0.6f;
                        else if (exitRight)
                            rope.p[n][0] += right * 0.6f;
                        else if (exitTop)
                            rope.p[n][1] -= top * 0.6f;
                        else
                            rope.p[n][1] += bottom * 0.6f;
                    }
                }
                if (rope.p[n][1] > kPanelH - 8.0f)
                    rope.p[n][1] = kPanelH - 8.0f;
            }
        }
    }
}

int PatchBayView::jackAt (float x, float y) const
{
    int best = -1;
    float bestDistance = 17.0f;
    for (int i = 0; i < kPanelJackCount; ++i)
    {
        const float distance = std::hypot (kPanelJacks[i].x - x, kPanelJacks[i].y - y);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

int PatchBayView::labelAt (float x, float y) const
{
    for (int i = 0; i < kPanelLabelCount; ++i)
    {
        const PanelLabelRec& label = kPanelLabels[i];
        if (x >= label.x - 3.0f && x <= label.x + label.w + 3.0f
            && y >= label.y - 3.0f && y <= label.y + label.h + 3.0f)
            return i;
    }
    return -1;
}

int PatchBayView::cableNear (float x, float y) const
{
    int hit = -1;
    for (int cable = 0; cable < count_; ++cable)
    {
        if (! ropes_[cable].ready)
            continue;
        for (int n = 0; n < kNodes - 1; ++n)
        {
            const float distance = segmentDistance (x, y,
                                                     ropes_[cable].p[n][0], ropes_[cable].p[n][1],
                                                     ropes_[cable].p[n + 1][0], ropes_[cable].p[n + 1][1]);
            if (distance < 8.0f)
                hit = cable;
        }
    }
    return hit;
}

int PatchBayView::powerHalfAt (float x, float y) const
{
    const float pad = 8.0f;
    if (x < kPowerX - pad || x > kPowerX + kPowerW + pad || y < kPowerY - pad || y > kPowerY + kPowerH + pad)
        return -1;
    return x < kPowerX + kPowerW * 0.5f ? 0 : 1;
}

bool PatchBayView::extInButtonAt (float x, float y) const
{
    const bool key = x >= kHoldHitX - 4.0f && x <= kHoldHitX + kHoldHitW + 4.0f
                     && y >= kHoldHitY - 4.0f && y <= kHoldHitY + kHoldHitH + 4.0f;
    if (key)
        return true;

    // The printed HOLD word sits under the key. The EG 2 HOLD label is higher on the plate.
    for (int i = 0; i < kPanelLabelCount; ++i)
    {
        const PanelLabelRec& label = kPanelLabels[i];
        if (std::strcmp (label.text, "HOLD") != 0 || label.y < kHoldCy)
            continue;
        if (x >= label.x - 4.0f && x <= label.x + label.w + 4.0f
            && y >= label.y - 8.0f && y <= label.y + label.h + 4.0f)
            return true;
    }
    return false;
}

bool PatchBayView::presetAt (float x, float y) const
{
    const float left = std::min (kPresetBezelX, kPresetKeyX) - 6.0f;
    const float top = std::min (kPresetBezelY, kPresetKeyY) - 6.0f;
    const float right = std::max (kPresetBezelX + kPresetBezelW, kPresetKeyX + kPresetKeyW) + 6.0f;
    const float bottom = std::max (kPresetBezelY + kPresetBezelH, kPresetKeyY + kPresetKeyH) + 8.0f;
    return x >= left && x <= right && y >= top && y <= bottom;
}

int PatchBayView::presetRowAt (float x, float y) const
{
    const int count = audioProcessor.getNumPrograms();
    const auto box = presetMenuDesign (count);
    if (x < box.getX() || x > box.getRight() || y < box.getY() || y > box.getBottom())
        return -1;
    const int rows = presetMenuRows (count);
    const int row = static_cast<int> (std::floor ((y - box.getY() - 5.0f) / kPresetRowPitch));
    const int column = static_cast<int> (std::floor ((x - box.getX()) / presetMenuColumnWidth()));
    if (row < 0 || row >= rows || column < 0 || column >= presetMenuColumns (count))
        return -1;
    const int index = column * rows + row;
    return index < count ? index : -1;
}

void PatchBayView::reloadPublishedCables()
{
    for (int i = 0; i < kPatchBayMaxCables; ++i)
        ropes_[i].ready = false;

    Cable published[kPatchBayMaxCables] {};
    const int publishedCount = audioProcessor.copyPublishedCables (published, kPatchBayMaxCables);
    count_ = loadPublishedCables (cables_, kPatchBayMaxCables, published, publishedCount,
                                  audioProcessor.extInGraphIndex(),
                                  audioProcessor.outputGraphIndex(),
                                  audioProcessor.noiseGraphIndex(),
                                  audioProcessor.vcfGraphIndex(),
                                  audioProcessor.vca1GraphIndex(),
                                  audioProcessor.vca2GraphIndex(),
                                  audioProcessor.eg1GraphIndex(),
                                  audioProcessor.mgGraphIndex(),
                                  audioProcessor.vcoGraphIndex(),
                                  audioProcessor.eg2GraphIndex(),
                                  audioProcessor.ringGraphIndex(),
                                  audioProcessor.dividerGraphIndex(),
                                  audioProcessor.inverterGraphIndex(),
                                  audioProcessor.integratorGraphIndex(),
                                  audioProcessor.mixerGraphIndex(),
                                  audioProcessor.sampleHoldGraphIndex());
}

void PatchBayView::choosePreset (int index)
{
    if (index < 0 || index >= audioProcessor.getNumPrograms())
        return;

    presetMenu_ = false;
    if (menu_ != nullptr && menu_->isVisible())
        menu_->setVisible (false);
    endGesture();
    effectPress_ = false;
    cancelGrab();

    audioProcessor.setCurrentProgram (index);
    if (audioProcessor.getCurrentProgram() != index)
    {
        repaint();
        return;
    }

    shownProgram_ = index;
    showProgramKnobs();
    reloadPublishedCables();
    prepareRopes (true);
    repaint();
}

float PatchBayView::outputMix() const noexcept
{
    return audioProcessor.effectiveOutputMix();
}

juce::AudioProcessorParameter* PatchBayView::parameterForKnob (int index) const
{
    if (index < 0 || index >= kPanelKnobCount)
        return nullptr;
    return audioProcessor.parameterForPanelKnob (kPanelKnobs[index].section, kPanelKnobs[index].label);
}

void PatchBayView::endGesture()
{
    if (gestureParam_ != nullptr)
        gestureParam_->endChangeGesture();
    gestureParam_ = nullptr;
}

void PatchBayView::syncHostKnobs()
{
    for (int i = 0; i < kPanelKnobCount; ++i)
    {
        auto* parameter = parameterForKnob (i);
        if (parameter == nullptr || parameter == gestureParam_)
            continue;
        knobValue_[i] = parameter->getValue();
    }
}

void PatchBayView::showProgramKnobs()
{
    for (int i = 0; i < kPanelKnobCount; ++i)
        knobValue_[i] = kPanelKnobs[i].valueDefault;
    syncHostKnobs();
}

int PatchBayView::swatchAt (float x, float y) const
{
    for (int i = 0; i < 4; ++i)
    {
        const float cx = 200.0f + static_cast<float> (i) * 22.0f;
        if (std::hypot (x - cx, y - 22.0f) <= 11.0f)
            return i;
    }
    return -1;
}



void PatchBayView::removeCable (int index)
{
    if (index < 0 || index >= count_)
        return;
    for (int i = index; i < count_ - 1; ++i)
    {
        cables_[i] = cables_[i + 1];
        ropes_[i] = ropes_[i + 1];
    }
    --count_;
    ropes_[count_] = {};
    cables_[count_] = {};
}

void PatchBayView::restoreGrabbedEnd()
{
    if (grabIndex_ < 0 || grabIndex_ >= count_)
        return;
    if (grabEndA_)
        cables_[grabIndex_].a = grabFrom_;
    else
        cables_[grabIndex_].b = grabFrom_;
}

void PatchBayView::clearGrab()
{
    grabActive_ = false;
    grabCarry_ = false;
    grabIsNew_ = false;
    grabIndex_ = -1;
    grabFrom_ = -1;
}

void PatchBayView::cancelGrab()
{
    if (! grabActive_)
        return;
    if (grabIsNew_)
        removeCable (grabIndex_);
    else
        restoreGrabbedEnd();
    clearGrab();
    showStatus ("");
}

void PatchBayView::unplugIndex (int index)
{
    if (index < 0 || index >= count_)
        return;
    if (grabActive_)
        cancelGrab();

    if (cables_[index].sounding)
    {
        audioProcessor.disconnectJacks (cables_[index].sourceModule, cables_[index].sourcePort,
                                        cables_[index].destModule, cables_[index].destPort);
    }
    removeCable (index);
    showStatus ("");
}

void PatchBayView::startGrab (int index, bool endIsA, bool isNew, bool carry)
{
    if (index < 0 || index >= count_)
        return;

    grabActive_ = true;
    grabIndex_ = index;
    grabEndA_ = endIsA;
    grabIsNew_ = isNew;
    grabCarry_ = carry;
    grabFrom_ = endIsA ? cables_[index].a : cables_[index].b;
    if (endIsA)
        cables_[index].a = -1;
    else
        cables_[index].b = -1;
    prepareRopes (true);
    repaint();
}

void PatchBayView::startNewCable (int jack)
{
    if (count_ >= kPatchBayMaxCables)
    {
        showStatus ("that jack does not take this cable");
        return;
    }

    VisualCable& cable = cables_[count_];
    cable = {};
    cable.a = jack;
    cable.b = -1;
    cable.color = currentColor_;
    ropes_[count_] = {};
    const int index = count_;
    ++count_;
    startGrab (index, false, true, false);
}

void PatchBayView::dropAt (int targetJack)
{
    if (! grabActive_ || grabIndex_ < 0 || grabIndex_ >= count_)
    {
        clearGrab();
        return;
    }

    const int index = grabIndex_;
    const bool endA = grabEndA_;
    const bool isNew = grabIsNew_;
    const int from = grabFrom_;
    const int other = endA ? cables_[index].b : cables_[index].a;

    if (targetJack >= 0 && targetJack == other)
    {
        if (isNew)
            removeCable (index);
        else
            restoreGrabbedEnd();
        clearGrab();
        showStatus ("");
        return;
    }

    if (targetJack < 0)
    {
        if (cables_[index].sounding)
        {
            audioProcessor.disconnectJacks (cables_[index].sourceModule, cables_[index].sourcePort,
                                            cables_[index].destModule, cables_[index].destPort);
        }
        removeCable (index);
        clearGrab();
        showStatus ("");
        return;
    }

    if (endA)
        cables_[index].a = targetJack;
    else
        cables_[index].b = targetJack;

    PanelLink link;
    const PanelLinkResult oriented = orientPanelJacks (cables_[index].a, cables_[index].b,
                                                        audioProcessor.extInGraphIndex(),
                                                        audioProcessor.outputGraphIndex(),
                                                        audioProcessor.noiseGraphIndex(),
                                                        link,
                                                        audioProcessor.vcfGraphIndex(),
                                                        audioProcessor.vca1GraphIndex(),
                                                        audioProcessor.vca2GraphIndex(),
                                                        audioProcessor.eg1GraphIndex(),
                                                        audioProcessor.mgGraphIndex(),
                                                        audioProcessor.vcoGraphIndex(),
                                                        audioProcessor.eg2GraphIndex(),
                                                        audioProcessor.ringGraphIndex(),
                                                        audioProcessor.dividerGraphIndex(),
                                                        audioProcessor.inverterGraphIndex(),
                                                        audioProcessor.integratorGraphIndex(),
                                                        audioProcessor.mixerGraphIndex(),
                                                        audioProcessor.sampleHoldGraphIndex());
    if (oriented != PanelLinkResult::Ok)
    {
        if (isNew)
            removeCable (index);
        else
        {
            if (endA)
                cables_[index].a = from;
            else
                cables_[index].b = from;
        }
        clearGrab();
        showStatus ("that jack does not take this cable");
        return;
    }

    const bool had = cables_[index].sounding;
    const int oldSourceModule = cables_[index].sourceModule;
    const int oldSourcePort = cables_[index].sourcePort;
    const int oldDestModule = cables_[index].destModule;
    const int oldDestPort = cables_[index].destPort;
    const bool same = had
                      && oldSourceModule == link.sourceModule
                      && oldSourcePort == link.sourcePort
                      && oldDestModule == link.destModule
                      && oldDestPort == link.destPort;
    if (same)
    {
        clearGrab();
        showStatus ("");
        return;
    }

    if (had)
        audioProcessor.disconnectJacks (oldSourceModule, oldSourcePort, oldDestModule, oldDestPort);

    const PatchGraph::ConnectResult connected = audioProcessor.connectJacks (link.sourceModule, link.sourcePort,
                                                                              link.destModule, link.destPort);
    if (connected != PatchGraph::ConnectResult::Ok)
    {
        if (had)
            audioProcessor.connectJacks (oldSourceModule, oldSourcePort, oldDestModule, oldDestPort);
        if (isNew)
            removeCable (index);
        else if (endA)
            cables_[index].a = from;
        else
            cables_[index].b = from;
        clearGrab();
        const char* text = PatchGraph::connectResultText (connected);
        if (text == nullptr || text[0] == '\0')
            text = "that jack does not take this cable";
        showStatus (text);
        return;
    }

    cables_[index].sounding = true;
    cables_[index].sourceModule = link.sourceModule;
    cables_[index].sourcePort = link.sourcePort;
    cables_[index].destModule = link.destModule;
    cables_[index].destPort = link.destPort;
    clearGrab();
    showStatus ("");
}

void PatchBayView::selectMeter (int jack)
{
    int module = -1;
    int port = -1;
    if (! panelJackAddress (jack,
                            audioProcessor.extInGraphIndex(),
                            audioProcessor.outputGraphIndex(),
                            audioProcessor.noiseGraphIndex(),
                            audioProcessor.vcfGraphIndex(),
                            audioProcessor.vca1GraphIndex(),
                            audioProcessor.vca2GraphIndex(),
                            audioProcessor.eg1GraphIndex(),
                            audioProcessor.mgGraphIndex(),
                            audioProcessor.vcoGraphIndex(),
                            audioProcessor.eg2GraphIndex(),
                            audioProcessor.ringGraphIndex(),
                            audioProcessor.dividerGraphIndex(),
                            audioProcessor.inverterGraphIndex(),
                            audioProcessor.integratorGraphIndex(),
                            audioProcessor.mixerGraphIndex(),
                            audioProcessor.sampleHoldGraphIndex(),
                            module, port))
        return;

    audioProcessor.meter().setSource (module, port);
}

void PatchBayView::timerCallback()
{
    if (! knobDrag_ && audioProcessor.getCurrentProgram() != shownProgram_)
    {
        shownProgram_ = audioProcessor.getCurrentProgram();
        showProgramKnobs();
    }
    syncHostKnobs();
    prepareRopes (true);
    stepRopes();

    // Repaint only what changed: moving ropes (old and new extent), turned knobs, the meter needle.
    const juce::String signature = frameSignature();
    if (signature != paintedSignature_)
    {
        paintedSignature_ = signature;
        for (int i = 0; i < kPanelKnobCount; ++i)
            paintedKnob_[i] = knobValue_[i];
        paintedNeedle_ = Meter::needle (audioProcessor.meterVolts());
        for (int c = 0; c < count_; ++c)
            std::memcpy (prevNodes_[c], ropes_[c].p, sizeof (ropes_[c].p));
        lastDirty_ = getLocalBounds();
        repaint();
        return;
    }
    juce::RectangleList<int> dirty;
    constexpr float kRopeMargin = 30.0f;   // plug radius, stack offset, shadow
    for (int c = 0; c < count_; ++c)
    {
        if (! ropes_[c].ready)
            continue;
        bool moved = false;
        float x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
        for (int n = 0; n < kNodes; ++n)
        {
            const float ax = prevNodes_[c][n][0], ay = prevNodes_[c][n][1];
            const float bx = ropes_[c].p[n][0], by = ropes_[c].p[n][1];
            // prevNodes_ holds where the rope was last painted: a settling rope that creeps by less than a
            // twentieth of a design pixel is not repainted every frame.
            moved = moved || std::fabs (ax - bx) > 0.05f || std::fabs (ay - by) > 0.05f;
            x0 = std::min ({ x0, ax, bx });
            y0 = std::min ({ y0, ay, by });
            x1 = std::max ({ x1, ax, bx });
            y1 = std::max ({ y1, ay, by });
        }
        if (moved)
        {
            dirty.add (designRectToLocal (x0 - kRopeMargin, y0 - kRopeMargin, x1 - x0 + 2.0f * kRopeMargin,
                                          y1 - y0 + 2.0f * kRopeMargin));
            std::memcpy (prevNodes_[c], ropes_[c].p, sizeof (ropes_[c].p));
        }
    }
    for (int i = 0; i < kPanelKnobCount; ++i)
    {
        if (ronin::exactlyEqual (paintedKnob_[i], knobValue_[i]))
            continue;
        paintedKnob_[i] = knobValue_[i];
        const PanelKnobRec& k = kPanelKnobs[i];
        const float r = k.radius + 10.0f;
        dirty.add (designRectToLocal (k.cx - r, k.cy - r, 2.0f * r, 2.0f * r));
    }
    const float needle = Meter::needle (audioProcessor.meterVolts());
    if (! ronin::exactlyEqual (needle, paintedNeedle_))
    {
        paintedNeedle_ = needle;
        dirty.add (designRectToLocal (kMeterFaceX, kMeterFaceY, kMeterFaceW, kMeterFaceH));
    }
    lastDirty_ = dirty.getBounds();
    for (const auto& r : dirty)
        repaint (r);
}

void PatchBayView::paint (juce::Graphics& g)
{
    paintBackdrop (g);
    const auto origin = panelOrigin();
    const float scale = panelScale();

    int levelA[kPatchBayMaxCables] {};
    int levelB[kPatchBayMaxCables] {};
    stackLevels (levelA, levelB);

    auto screenPoint = [origin, scale] (float x, float y)
    {
        return juce::Point<float> (origin.x + x * scale, origin.y + y * scale);
    };

    {
        // Printed ticks are the VU face, from -20 to +3. The needle is the selected jack: ±5 V across that arc.
        const float unit = Meter::needle (audioProcessor.meterVolts());
        const float angleDeg = kMeterMinAngle + (kMeterMaxAngle - kMeterMinAngle) * (unit + 1.0f) * 0.5f;
        const float angle = juce::degreesToRadians (angleDeg);
        const float hub = 17.0f * kMeterScale;
        const float reach = kMeterRadius + 5.0f * kMeterScale;
        const auto face = juce::Rectangle<float> (origin.x + kMeterFaceX * scale,
                                                  origin.y + kMeterFaceY * scale,
                                                  kMeterFaceW * scale,
                                                  kMeterFaceH * scale);
        auto polar = [&] (float radius)
        {
            return screenPoint (kMeterPivotX + radius * std::sin (angle),
                                kMeterPivotY - radius * std::cos (angle));
        };
        const auto start = polar (hub);
        const auto tip = polar (reach);
        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (face.toNearestInt());
            g.setColour (juce::Colour (0xff141414));
            g.drawLine (start.x, start.y, tip.x, tip.y, 1.3f * kMeterScale * scale);
            juce::ColourGradient glass (juce::Colour (0xfffff8e0).withAlpha (0.18f), face.getX(), face.getY(),
                                        juce::Colour (0xffe8c870).withAlpha (0.04f), face.getRight(), face.getBottom(),
                                        false);
            g.setGradientFill (glass);
            g.fillRect (face);
        }
    }

    for (int i = 0; i < kPanelKnobCount; ++i)
    {
        const PanelKnobRec& knob = kPanelKnobs[i];
        paintKnobCap (g, screenPoint (knob.cx, knob.cy), knob.radius * scale, scale,
                      knob.kind == 1, knobValue_[i]);
    }

    paintExtInHold (g, origin, scale, audioProcessor.extInButtonHeld());

    Cable published[kPatchBayMaxCables] {};
    const int publishedCount = audioProcessor.copyPublishedCables (published, kPatchBayMaxCables);
    for (int cable = 0; cable < count_; ++cable)
    {
        if (! ropes_[cable].ready)
            continue;

        stroke_.clear();
        stroke_.startNewSubPath (screenPoint (ropes_[cable].p[0][0], ropes_[cable].p[0][1]));
        for (int n = 1; n < kNodes - 1; ++n)
        {
            const float midX = (ropes_[cable].p[n][0] + ropes_[cable].p[n + 1][0]) * 0.5f;
            const float midY = (ropes_[cable].p[n][1] + ropes_[cable].p[n + 1][1]) * 0.5f;
            stroke_.quadraticTo (screenPoint (ropes_[cable].p[n][0], ropes_[cable].p[n][1]),
                                 screenPoint (midX, midY));
        }
        stroke_.lineTo (screenPoint (ropes_[cable].p[kNodes - 1][0], ropes_[cable].p[kNodes - 1][1]));

        const CablePaint colors = cablePaintFor (cable, published, publishedCount);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (stroke_, juce::PathStrokeType (7.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (3.0f * scale, 5.0f * scale));
        g.setColour (colors.dark);
        g.strokePath (stroke_, juce::PathStrokeType (7.0f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colors.base);
        g.strokePath (stroke_, juce::PathStrokeType (5.2f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (colors.light.withAlpha (0.55f));
        g.strokePath (stroke_, juce::PathStrokeType (1.6f * scale, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-0.8f * scale, -1.2f * scale));

        auto drawPlug = [&] (int jack, int level)
        {
            const float radius = 12.0f * (1.0f + 0.04f * static_cast<float> (level)) * scale;
            float x = pointerX_;
            float y = pointerY_;
            if (jack >= 0 && jack < kPanelJackCount)
            {
                x = kPanelJacks[jack].x;
                y = kPanelJacks[jack].y;
            }
            x -= 2.0f * static_cast<float> (level);
            y -= 3.0f * static_cast<float> (level);
            const auto centre = screenPoint (x, y);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillEllipse (centre.x + 2.0f * scale - radius - 1.5f * scale,
                           centre.y + 3.0f * scale - radius - 1.5f * scale,
                           (radius + 1.5f * scale) * 2.0f,
                           (radius + 1.5f * scale) * 2.0f);
            g.setColour (colors.base);
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (juce::Colour (0xffd0d0d0));
            const float collar = radius * 0.62f;
            g.fillEllipse (centre.x - collar, centre.y - collar, collar * 2.0f, collar * 2.0f);
            g.setColour (juce::Colour (0xff050505));
            const float hole = radius * 0.36f;
            g.fillEllipse (centre.x - hole, centre.y - hole, hole * 2.0f, hole * 2.0f);
        };
        drawPlug (cables_[cable].a, levelA[cable]);
        drawPlug (cables_[cable].b, levelB[cable]);
    }

    if (hoverJack_ >= 0)
    {
        const auto centre = screenPoint (kPanelJacks[hoverJack_].x, kPanelJacks[hoverJack_].y);
        // Transient hover ring in the jack's JCS R14 role colour (RONIN_Redesign §5 item 13c).
        int ringModule = -1;
        int ringPort = -1;
        juce::Colour ring = juce::Colours::white;
        if (jackGraphPort (hoverJack_, ringModule, ringPort))
            ring = juce::Colour (jcs::roleArgb (portRole (audioProcessor.portDesc (ringModule, ringPort))));
        g.setColour (ring.withAlpha (0.8f));
        g.drawEllipse (centre.x - 16.0f * scale, centre.y - 16.0f * scale, 32.0f * scale, 32.0f * scale, 1.5f * scale);
    }

    for (int i = 0; i < 4; ++i)
    {
        const auto centre = screenPoint (200.0f + static_cast<float> (i) * 22.0f, 22.0f);
        const float radius = 8.0f * scale;
        const CablePaint colors = paintFor (i);
        g.setColour (colors.base);
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        if (i == currentColor_)
        {
            g.setColour (juce::Colours::white);
            g.drawEllipse (centre.x - radius - 2.0f * scale, centre.y - radius - 2.0f * scale,
                           (radius + 2.0f * scale) * 2.0f, (radius + 2.0f * scale) * 2.0f, 1.5f * scale);
        }
    }

    const bool effectOn = audioProcessor.effectIsOn();
    paintEffectRocker (g, origin, scale, effectOn);
    {
        const int program = audioProcessor.getCurrentProgram();
        const auto lcd = juce::Rectangle<float> (origin.x + (kPresetLcdX + 2.0f) * scale,
                                                 origin.y + (kPresetLcdY + 1.0f) * scale,
                                                 (kPresetLcdW - 4.0f) * scale,
                                                 (kPresetLcdH - 2.0f) * scale);
        paintLcdDots (g, lcd, presetScreenLine (program, audioProcessor.getProgramName (program)),
                      juce::Colour (0xff1e2419), 0.09f);
    }

    juce::String line = status_;
    const bool presetAlert = line.isEmpty() && ! grabActive_ && audioProcessor.presetError().isNotEmpty();
    if (line.isEmpty())
    {
        if (presetAlert)
            line = audioProcessor.presetError();
        else if (grabActive_)
            line = "Drop on a jack to plug in. Empty space unplugs. Esc cancels.";
        else if (hoverJack_ >= 0)
            line = jackHoverText (hoverJack_);
        else
            line = knobReadout_;
    }
    if (line.isNotEmpty())
    {
        g.setColour (status_.isNotEmpty() || presetAlert ? juce::Colour (0xffffe08a) : juce::Colour (0xffd8d2bd));
        g.setFont (juce::Font (juce::FontOptions (13.0f * scale)));
        const auto textOrigin = screenPoint (24.0f, 600.0f);
        g.drawText (line, juce::Rectangle<float> (textOrigin.x, textOrigin.y, 1000.0f * scale, 20.0f * scale),
                    juce::Justification::centredLeft, true);
    }

    if (presetMenu_)
    {
        const int programs = audioProcessor.getNumPrograms();
        const auto box = presetMenuDesign (programs);
        const auto menu = juce::Rectangle<float> (origin.x + box.getX() * scale,
                                                  origin.y + box.getY() * scale,
                                                  box.getWidth() * scale,
                                                  box.getHeight() * scale);
        g.setColour (juce::Colour (0xff0a0a0b));
        g.fillRoundedRectangle (menu, 4.0f * scale);
        g.setColour (juce::Colour (0xffc29f4c));
        g.drawRoundedRectangle (menu, 4.0f * scale, 1.2f * scale);

        const int current = audioProcessor.getCurrentProgram();
        for (int row = 0; row < programs; ++row)
        {
            const bool hi = row == presetHi_;
            const auto rowDesign = presetRowDesign (programs, row);
            const auto rowRect = juce::Rectangle<float> (origin.x + rowDesign.getX() * scale,
                                                         origin.y + rowDesign.getY() * scale,
                                                         rowDesign.getWidth() * scale,
                                                         rowDesign.getHeight() * scale);
            if (hi)
            {
                g.setColour (juce::Colour (0xff1e2419));
                g.fillRoundedRectangle (rowRect, 1.5f * scale);
            }
            else
            {
                juce::ColourGradient glass (juce::Colour (0xff8f9a7c), rowRect.getX(), rowRect.getY(),
                                            juce::Colour (0xff94a083), rowRect.getX(), rowRect.getBottom(), false);
                glass.addColour (0.5, juce::Colour (0xffa6b192));
                g.setGradientFill (glass);
                g.fillRoundedRectangle (rowRect, 1.5f * scale);
            }
            const juce::String shown = (juce::String (row == current ? ">" : " ")
                                        + presetScreenLine (row, audioProcessor.getProgramName (row)))
                                           .substring (0, kPresetChars);
            // Lists use a plain font (the screen itself keeps its dot matrix).
            const auto rowText = rowRect.reduced (6.0f * scale, 0.0f);
            g.setColour (hi ? juce::Colour (0xffa6b192) : juce::Colour (0xff1e2419));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (12.0f, rowRect.getHeight() * 0.72f))).boldened());
            g.drawFittedText (shown, rowText.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
        }
    }
}

void PatchBayView::resized()
{
    backdrop_ = juce::Image();   // the static layer is re-drawn at the new size
    paintedSignature_.clear();
}

void PatchBayView::mouseMove (const juce::MouseEvent& event)
{
    const auto design = localToDesign (event.position);
    pointerX_ = design.x;
    pointerY_ = design.y;
    pointerIn_ = true;
    if (! knobDrag_)
    {
        knobReadout_.clear();
        // RONIN_Redesign §4.1: transient hover read-out in real units.
        const int knob = knobAt (design.x, design.y);
        if (knob >= 0)
            knobReadout_ = kPanelKnobs[knob].kind == 1
                               ? juce::String (kPanelKnobs[knob].section) + juce::String::fromUTF8 (" · ÷ ")
                                     + (knobValue_[knob] < 0.5f ? "2" : "4")
                               : knobText (knob);
    }
    hoverJack_ = jackAt (design.x, design.y);
    hoverLabel_ = labelAt (design.x, design.y);
    repaint();
}

void PatchBayView::mouseExit (const juce::MouseEvent&)
{
    pointerIn_ = false;
    if (! grabActive_)
    {
        hoverJack_ = -1;
        hoverLabel_ = -1;
    }
    repaint();
}

void PatchBayView::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();
    const auto design = localToDesign (event.position);
    pointerX_ = design.x;
    pointerY_ = design.y;
    pointerIn_ = true;

    if (grabActive_ && grabCarry_)
    {
        dropAt (jackAt (design.x, design.y));
        downActive_ = false;
        return;
    }

    if (presetMenu_)
    {
        if (menuOpen())
            menu_->setVisible (false);
        const int row = presetRowAt (design.x, design.y);
        if (! event.mods.isRightButtonDown() && row >= 0)
            choosePreset (row);
        else
        {
            presetMenu_ = false;
            repaint();
        }
        return;
    }

    if (menuOpen())
        menu_->setVisible (false);

    if (event.mods.isRightButtonDown())
    {
        if (extInButtonAt (design.x, design.y))
            return;
        // A list control opens its whole list (current ticked).
        if (presetAt (design.x, design.y))
        {
            listRightClick (1);
            return;
        }
        const int typedKnob = knobAt (design.x, design.y);
        if (typedKnob >= 0 && kPanelKnobs[typedKnob].kind == 1)
        {
            listRightClick (0);
            return;
        }
        if (typedKnob >= 0 && kPanelKnobs[typedKnob].kind != 1 && parameterForKnob (typedKnob) != nullptr)
        {
            openValueEditor (typedKnob);
            return;
        }
        if (knobAt (design.x, design.y) < 0 && jackAt (design.x, design.y) < 0)
            unplugIndex (cableNear (design.x, design.y));
        return;
    }

    const int swatch = swatchAt (design.x, design.y);
    if (swatch >= 0)
    {
        currentColor_ = swatch;
        repaint();
        return;
    }

    if (presetAt (design.x, design.y))
    {
        cancelGrab();
        presetMenu_ = true;
        presetHi_ = audioProcessor.getCurrentProgram();
        repaint();
        return;
    }

    const int half = powerHalfAt (design.x, design.y);
    if (half >= 0)
    {
        endGesture();
        effectPress_ = true;
        if (half == 0)
            presetMenu_ = false;
        if (auto* parameter = audioProcessor.effectParameter())
        {
            gestureParam_ = parameter;
            parameter->beginChangeGesture();
            // Left half is printed OFF (dry). Right half is printed ON (wet).
            parameter->setValueNotifyingHost (half == 1 ? 1.0f : 0.0f);
        }
        repaint();
        return;
    }

    if (extInButtonAt (design.x, design.y))
    {
        extInPress_ = true;
        audioProcessor.setExtInButtonHeld (true);
        repaint();
        return;
    }

    const int knob = knobAt (design.x, design.y);
    if (knob >= 0)
    {
        endGesture();
        knobDrag_ = true;
        knobDragMoved_ = false;
        knobSuppressSwitchStep_ = false;
        knobDragIndex_ = knob;
        knobDragStartY_ = event.position.y;
        knobDragStartValue_ = knobValue_[knob];
        knobShift_ = event.mods.isShiftDown();
        gestureParam_ = parameterForKnob (knob);
        if (gestureParam_ != nullptr)
            gestureParam_->beginChangeGesture();
        return;
    }

    const int jack = jackAt (design.x, design.y);
    if (jack < 0)
        return;

    selectMeter (jack);
    downActive_ = true;
    downMoved_ = false;
    downShift_ = event.mods.isShiftDown();
    downJack_ = jack;
    downX_ = design.x;
    downY_ = design.y;
}

void PatchBayView::mouseDrag (const juce::MouseEvent& event)
{
    const auto design = localToDesign (event.position);
    pointerX_ = design.x;
    pointerY_ = design.y;
    pointerIn_ = true;

    if (knobDrag_ && knobDragIndex_ >= 0)
    {
        const float deltaUp = knobDragStartY_ - event.position.y;
        if (std::fabs (deltaUp) > 3.0f)
            knobDragMoved_ = true;
        const bool isSwitch = kPanelKnobs[knobDragIndex_].kind == 1;
        const float next = panelKnobDrag (knobDragStartValue_, deltaUp, event.mods.isShiftDown(), isSwitch);
        setKnobValue (knobDragIndex_, next);
        if (gestureParam_ != nullptr)
            gestureParam_->setValueNotifyingHost (clampf (next, 0.0f, 1.0f));
        return;
    }

    hoverJack_ = jackAt (design.x, design.y);
    hoverLabel_ = labelAt (design.x, design.y);

    if (effectPress_ || extInPress_)
        return;

    if (downActive_ && ! grabActive_ && std::hypot (design.x - downX_, design.y - downY_) > 6.0f)
    {
        downMoved_ = true;
        int plugs[kPatchBayMaxCables] {};
        const int n = plugsAtJack (cables_, count_, downJack_, plugs, kPatchBayMaxCables);
        if (n > 0 && ! downShift_)
        {
            const int top = plugs[n - 1];
            const bool endA = cables_[top].b != downJack_;
            startGrab (top, endA, false, false);
        }
        else
        {
            startNewCable (downJack_);
        }
    }

    if (grabActive_)
        prepareRopes (true);
    repaint();
}

void PatchBayView::mouseUp (const juce::MouseEvent& event)
{
    const auto design = localToDesign (event.position);
    pointerX_ = design.x;
    pointerY_ = design.y;

    if (knobDrag_)
    {
        const int index = knobDragIndex_;
        const bool moved = knobDragMoved_;
        const bool suppress = knobSuppressSwitchStep_;
        knobDrag_ = false;
        knobDragIndex_ = -1;
        knobSuppressSwitchStep_ = false;
        if (! suppress && ! moved && index >= 0 && index < kPanelKnobCount && kPanelKnobs[index].kind == 1)
        {
            endGesture();
            switchClick (index, knobShift_);
            return;
        }
        endGesture();
        return;
    }

    if (effectPress_)
    {
        effectPress_ = false;
        endGesture();
        return;
    }

    if (extInPress_)
    {
        extInPress_ = false;
        audioProcessor.setExtInButtonHeld (false);
        repaint();
        return;
    }

    if (grabActive_ && ! grabCarry_)
    {
        dropAt (jackAt (design.x, design.y));
        downActive_ = false;
        return;
    }

    if (downActive_ && ! downMoved_)
    {
        int plugs[kPatchBayMaxCables] {};
        const int n = plugsAtJack (cables_, count_, downJack_, plugs, kPatchBayMaxCables);
        if (n > 0 && menu_ != nullptr)
            menu_->showFor (downJack_);
    }
    downActive_ = false;
}

void PatchBayView::mouseDoubleClick (const juce::MouseEvent& event)
{
    const auto design = localToDesign (event.position);
    const int index = knobAt (design.x, design.y);
    if (index < 0)
        return;

    knobSuppressSwitchStep_ = true;
    const float reset = kPanelKnobs[index].valueDefault;
    setKnobValue (index, reset);
    if (auto* parameter = parameterForKnob (index))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (clampf (reset, 0.0f, 1.0f));
        parameter->endChangeGesture();
    }
}

void PatchBayView::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (knobDrag_)
        return;

    const auto design = localToDesign (event.position);
    const int index = knobAt (design.x, design.y);
    if (index < 0)
        return;

    const float next = panelKnobFromWheel (knobValue_[index], wheel.deltaY, wheel.isReversed,
                                           event.mods.isShiftDown(), kPanelKnobs[index].kind == 1);
    setKnobValue (index, next);
    if (auto* parameter = parameterForKnob (index))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (clampf (next, 0.0f, 1.0f));
        parameter->endChangeGesture();
    }
}

int PatchBayView::knobAt (float x, float y) const
{
    for (int i = 0; i < kPanelKnobCount; ++i)
    {
        const PanelKnobRec& knob = kPanelKnobs[i];
        if (x >= knob.hitX && x <= knob.hitX + knob.hitW
            && y >= knob.hitY && y <= knob.hitY + knob.hitH)
            return i;
    }
    return -1;
}

// JCS R14: BY ROLE (default) colours a cable by its source port's role, a PATCH-tab override wins.
// MANUAL keeps the four-swatch palette. A cable still in the hand has no source port yet: palette.
CablePaint PatchBayView::cablePaintFor (int index, const Cable* published, int publishedCount) const
{
    const VisualCable& v = cables_[index];
    if (! audioProcessor.cableColourByRole() || v.sourceModule < 0 || v.destModule < 0)
        return paintFor (v.color);
    for (int i = 0; i < publishedCount; ++i)
    {
        const Cable& c = published[i];
        if (c.sourceModule == v.sourceModule && c.sourcePort == v.sourcePort
            && c.destModule == v.destModule && c.destPort == v.destPort)
            return paintForColour (ronin_ui::cableColour (audioProcessor, c));
    }
    return paintForColour (juce::Colour (jcs::roleArgb (portRole (audioProcessor.portDesc (v.sourceModule, v.sourcePort)))));
}

bool PatchBayView::jackGraphPort (int jack, int& module, int& port) const
{
    return panelJackAddress (jack,
                             audioProcessor.extInGraphIndex(), audioProcessor.outputGraphIndex(),
                             audioProcessor.noiseGraphIndex(), audioProcessor.vcfGraphIndex(),
                             audioProcessor.vca1GraphIndex(), audioProcessor.vca2GraphIndex(),
                             audioProcessor.eg1GraphIndex(), audioProcessor.mgGraphIndex(),
                             audioProcessor.vcoGraphIndex(), audioProcessor.eg2GraphIndex(),
                             audioProcessor.ringGraphIndex(), audioProcessor.dividerGraphIndex(),
                             audioProcessor.inverterGraphIndex(), audioProcessor.integratorGraphIndex(),
                             audioProcessor.mixerGraphIndex(), audioProcessor.sampleHoldGraphIndex(),
                             module, port);
}

// RONIN_Redesign §4.1 jack hover: id (SECTION:LABEL), role glyph and name, live volts, far end(s).
juce::String PatchBayView::jackHoverText (int jack) const
{
    const PanelJackRec& rec = kPanelJacks[jack];
    juce::String line = juce::String::fromUTF8 (rec.section) + ":" + juce::String::fromUTF8 (rec.label);
    int module = -1;
    int port = -1;
    if (jackGraphPort (jack, module, port))
    {
        const auto desc = audioProcessor.portDesc (module, port);
        const auto& role = jcs::roleInfo (portRole (desc));
        const float volts = audioProcessor.jackVolts (module, port);
        line << juce::String::fromUTF8 ("  ") << juce::String::fromUTF8 (role.glyph) << " " << role.name
             << (rec.dir == 0 ? " in" : " out") << "  " << (volts >= 0.0f ? "+" : "") << juce::String (volts, 2) << " V";
        if (audioProcessor.jackOverRange (module, port))   // JCS R15
            line << " OVER";
    }
    juce::StringArray far;
    for (int i = 0; i < count_; ++i)
    {
        const int other = cables_[i].a == jack ? cables_[i].b : (cables_[i].b == jack ? cables_[i].a : -2);
        if (other >= 0 && other < kPanelJackCount)
            far.add (juce::String::fromUTF8 (kPanelJacks[other].section) + ":" + juce::String::fromUTF8 (kPanelJacks[other].label));
    }
    if (! far.isEmpty())
        line << juce::String::fromUTF8 ("  \u2194 ") << far.joinIntoString (", ");
    if (std::strcmp (rec.section, "MIX") == 0)
        line << juce::String::fromUTF8 ("  (\u2212\u03a3 inverting mixer: OUT = \u2212(L1\u00b7IN1 + L2\u00b7IN2 + L3\u00b7IN3))");
    return line;
}

juce::String PatchBayView::knobText (int index) const
{
    const PanelKnobRec& knob = kPanelKnobs[index];
    juce::String line = juce::String::fromUTF8 (knob.section) + juce::String::fromUTF8 (" · ")
                        + juce::String::fromUTF8 (knob.label) + " "
                        + juce::String::fromUTF8 (knobunits::realUnits (knob.section, knob.label, knobValue_[index],
                                                                        audioProcessor.egTimeInMs()).c_str());
    if (std::strcmp (knob.section, "MIX") == 0)
        line << juce::String::fromUTF8 ("  (\u2212\u03a3 inverting mixer)");
    return line;
}

void PatchBayView::openValueEditor (int knob)
{
    closeValueEditor();
    valueKnob_ = knob;
    valueEditor_ = std::make_unique<juce::TextEditor>();
    auto& editor = *valueEditor_;
    const float scale = panelScale();
    const auto at = designToLocal (kPanelKnobs[knob].cx, kPanelKnobs[knob].cy);
    const float w = 120.0f * scale;
    const float h = 24.0f * scale;
    editor.setBounds (juce::Rectangle<float> (at.x - w * 0.5f, at.y + 20.0f * scale, w, h).toNearestInt());
    editor.setFont (juce::Font (juce::FontOptions (13.0f * scale)));
    editor.setJustification (juce::Justification::centred);
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xffb9c39c));
    editor.setColour (juce::TextEditor::textColourId, juce::Colour (0xff1e2419));
    editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff0a0a0b));
    editor.setText (juce::String::fromUTF8 (knobunits::realUnits (kPanelKnobs[knob].section, kPanelKnobs[knob].label,
                                                                  knobValue_[knob], audioProcessor.egTimeInMs()).c_str()),
                    false);
    juce::Component::SafePointer<PatchBayView> safe (this);
    editor.onReturnKey = [safe]
    {
        if (safe == nullptr || safe->valueEditor_ == nullptr)
            return;
        if (! safe->typeKnobValue (safe->valueKnob_, safe->valueEditor_->getText()))
            safe->showStatus ("Type a value such as 1.2 kHz, 250 ms, 2.5 s, +10 c or 40 %.");
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->closeValueEditor(); });
    };
    auto cancel = [safe] { juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->closeValueEditor(); }); };
    editor.onEscapeKey = cancel;
    editor.onFocusLost = cancel;
    addAndMakeVisible (editor);
    editor.selectAll();
    if (isShowing())
        editor.grabKeyboardFocus();
}

void PatchBayView::closeValueEditor()
{
    if (valueEditor_ != nullptr)
        removeChildComponent (valueEditor_.get());
    valueEditor_ = nullptr;
    valueKnob_ = -1;
}

juce::String PatchBayView::valueEditorText() const
{
    return valueEditor_ != nullptr ? valueEditor_->getText() : juce::String();
}

bool PatchBayView::typeKnobValue (int knob, const juce::String& text)
{
    if (knob < 0 || knob >= kPanelKnobCount || kPanelKnobs[knob].kind == 1)
        return false;
    auto* parameter = parameterForKnob (knob);
    double value = 0.0;
    if (parameter == nullptr
        || ! knobunits::parseKnob (kPanelKnobs[knob].section, kPanelKnobs[knob].label, text.toStdString(),
                                   audioProcessor.egTimeInMs(), value))
        return false;
    const float v = static_cast<float> (value);
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (v);
    parameter->endChangeGesture();
    setKnobValue (knob, v);
    return true;
}

namespace {

int ratioSwitchKnob() noexcept
{
    for (int i = 0; i < kPanelKnobCount; ++i)
        if (kPanelKnobs[i].kind == 1)
            return i;
    return -1;
}

juce::StringArray ratioItems()
{
    return { juce::String::fromUTF8 ("\xc3\xb7 2"), juce::String::fromUTF8 ("\xc3\xb7 4") };
}

}

std::vector<ronin_ui::ListControl> PatchBayView::listControls() const
{
    std::vector<ronin_ui::ListControl> lists;
    const int knob = ratioSwitchKnob();
    if (knob >= 0)
    {
        const PanelKnobRec& rec = kPanelKnobs[knob];
        juce::Component::SafePointer<PatchBayView> safe (const_cast<PatchBayView*> (this));
        lists.push_back ({ "DIV RATIO SWITCH", ratioItems(), knobValue_[knob] < 0.5f ? 0 : 1, true,
                           { rec.hitX, rec.hitY, rec.hitW, rec.hitH },
                           [safe, knob] (int i) {
                               if (safe == nullptr)
                                   return;
                               auto* parameter = safe->parameterForKnob (knob);
                               const float value = i == 0 ? 0.0f : 1.0f;
                               safe->setKnobValue (knob, value);
                               if (parameter != nullptr)
                               {
                                   parameter->beginChangeGesture();
                                   parameter->setValueNotifyingHost (value);
                                   parameter->endChangeGesture();
                               }
                               safe->repaint();
                           } });
    }
    juce::StringArray programs;
    for (int i = 0; i < audioProcessor.getNumPrograms(); ++i)
        programs.add (presetScreenLine (i, audioProcessor.getProgramName (i)));
    juce::Component::SafePointer<PatchBayView> safe (const_cast<PatchBayView*> (this));
    lists.push_back ({ "PRESET", programs, audioProcessor.getCurrentProgram(), false,
                       { kPresetBezelX, kPresetBezelY, kPresetBezelW, kPresetBezelH },
                       [safe] (int i) { if (safe != nullptr) safe->choosePreset (i); } });
    return lists;
}

void PatchBayView::switchClick (int knob, bool back)
{
    if (knob < 0 || knob >= kPanelKnobCount || kPanelKnobs[knob].kind != 1)
        return;
    const auto lists = listControls();
    const auto& list = lists[0];
    list.choose (ronin_ui::stepIndex (list.current, list.items.size(), back));
}

void PatchBayView::listRightClick (int control)
{
    const auto lists = listControls();
    if (control < 0 || control >= static_cast<int> (lists.size()))
        return;
    endGesture();
    presetMenu_ = false;
    ronin_ui::showListMenu (lists[static_cast<size_t> (control)], this);
    repaint();
}

void PatchBayView::setKnobValue (int index, float value)
{
    if (index < 0 || index >= kPanelKnobCount)
        return;

    const PanelKnobRec& knob = kPanelKnobs[index];
    const bool isSwitch = knob.kind == 1;
    knobValue_[index] = panelKnobClamp (value, isSwitch);
    const float shown = knobValue_[index];
    if (isSwitch)
    {
        const char* ratio = shown < 0.5f ? "2" : "4";
        knobReadout_ = juce::String (knob.section) + juce::String::fromUTF8 (" · ÷ ") + ratio;
    }
    else
    {
        knobReadout_ = knobText (index);
    }
    repaint();
}

float PatchBayView::knobValue (int index) const
{
    if (index < 0 || index >= kPanelKnobCount)
        return 0.0f;
    return knobValue_[index];
}

bool PatchBayView::keyPressed (const juce::KeyPress& key)
{
    if (presetMenu_)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            presetMenu_ = false;
            repaint();
            return true;
        }
        const int count = audioProcessor.getNumPrograms();
        if (key == juce::KeyPress::upKey)
        {
            if (presetHi_ > 0)
                --presetHi_;
            repaint();
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            if (presetHi_ + 1 < count)
                ++presetHi_;
            repaint();
            return true;
        }
        if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey)
        {
            const int step = presetMenuRows (count) * (key == juce::KeyPress::leftKey ? -1 : 1);
            if (presetHi_ + step >= 0 && presetHi_ + step < count)
                presetHi_ += step;
            repaint();
            return true;
        }
        if (key == juce::KeyPress::returnKey)
        {
            choosePreset (presetHi_);
            return true;
        }
        return true;
    }

    if (key != juce::KeyPress::escapeKey)
        return false;

    if (menuOpen())
        menu_->setVisible (false);
    cancelGrab();
    return true;
}

int PatchBayView::visualColor (int index) const
{
    if (index < 0 || index >= count_)
        return 0;
    return cables_[index].color;
}
