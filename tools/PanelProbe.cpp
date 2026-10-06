// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "UI/PatchBayLogic.h"
#include "UI/PatchBayView.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

int gFails = 0;

void expect (bool ok, const char* message)
{
    if (! ok)
    {
        std::printf ("FAIL %s\n", message);
        ++gFails;
    }
}

void gesture (PatchBayView& bay, float x0, float y0, float x1, float y1, bool shift, bool right)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    int flags = right ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier;
    if (shift)
        flags |= juce::ModifierKeys::shiftModifier;
    const juce::ModifierKeys mods (flags);
    const auto time = juce::Time::getCurrentTime();
    const auto start = bay.designToLocal (x0, y0);
    const auto end = bay.designToLocal (x1, y1);
    juce::MouseEvent down (source, start, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &bay, &bay, time, start, time, 1, false);
    bay.mouseDown (down);
    if (right)
        return;

    const auto mid = bay.designToLocal (x0 + 24.0f, y0 + 24.0f);
    juce::MouseEvent drag (source, mid, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &bay, &bay, time, start, time, 1, true);
    bay.mouseDrag (drag);
    juce::MouseEvent dragEnd (source, end, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                              &bay, &bay, time, start, time, 1, true);
    bay.mouseDrag (dragEnd);
    juce::MouseEvent up (source, end, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                         &bay, &bay, time, start, time, 1, true);
    bay.mouseUp (up);
}

void dragKnobLocal (PatchBayView& bay, float designX, float designY, float localDy, bool shift)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    int flags = juce::ModifierKeys::leftButtonModifier;
    if (shift)
        flags |= juce::ModifierKeys::shiftModifier;
    const juce::ModifierKeys mods (flags);
    const auto time = juce::Time::getCurrentTime();
    const auto start = bay.designToLocal (designX, designY);
    const auto end = start + juce::Point<float> (0.0f, localDy);
    juce::MouseEvent down (source, start, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &bay, &bay, time, start, time, 1, false);
    bay.mouseDown (down);
    juce::MouseEvent drag (source, end, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &bay, &bay, time, start, time, 1, true);
    bay.mouseDrag (drag);
    juce::MouseEvent up (source, end, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                         &bay, &bay, time, start, time, 1, true);
    bay.mouseUp (up);
}

void wheelOnKnob (PatchBayView& bay, float designX, float designY, float deltaY)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const juce::ModifierKeys mods (juce::ModifierKeys::noModifiers);
    const auto time = juce::Time::getCurrentTime();
    const auto start = bay.designToLocal (designX, designY);
    juce::MouseEvent move (source, start, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &bay, &bay, time, start, time, 0, false);
    juce::MouseWheelDetails wheel {};
    wheel.deltaY = deltaY;
    bay.mouseWheelMove (move, wheel);
}

juce::Image paintBay (PatchBayView& bay)
{
    juce::Image image (juce::Image::ARGB, bay.getWidth(), bay.getHeight(), true);
    juce::Graphics graphics (image);
    bay.paintEntireComponent (graphics, true);
    return image;
}

bool knobPixelsDiffer (const juce::Image& before, const juce::Image& after, juce::Rectangle<int> box)
{
    const int x0 = juce::jlimit (0, before.getWidth() - 1, box.getX());
    const int y0 = juce::jlimit (0, before.getHeight() - 1, box.getY());
    const int x1 = juce::jlimit (0, before.getWidth(), box.getRight());
    const int y1 = juce::jlimit (0, before.getHeight(), box.getBottom());
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (before.getPixelAt (x, y) != after.getPixelAt (x, y))
                return true;
    return false;
}

void clickAt (PatchBayView& bay, float x, float y)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
    const auto time = juce::Time::getCurrentTime();
    const auto start = bay.designToLocal (x, y);
    juce::MouseEvent down (source, start, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           &bay, &bay, time, start, time, 1, false);
    bay.mouseDown (down);
    juce::MouseEvent up (source, start, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                         &bay, &bay, time, start, time, 1, false);
    bay.mouseUp (up);
}

bool near (float actual, float expected)
{
    return std::fabs (actual - expected) < 1.0e-3f;
}

int publishedCount (MS50ModularAudioProcessor& processor)
{
    Cable cables[kPatchBayMaxCables] {};
    return processor.copyPublishedCables (cables, kPatchBayMaxCables);
}

float hostLeftAfter (MS50ModularAudioProcessor& processor, float left, float right)
{
    processor.prepareToPlay (48000.0, 64);
    juce::AudioBuffer<float> buffer (2, 64);
    juce::MidiBuffer midi;
    for (int i = 0; i < 64; ++i)
    {
        buffer.setSample (0, i, left);
        buffer.setSample (1, i, right);
    }
    processor.processBlock (buffer, midi);
    return buffer.getSample (0, 63);
}

bool cablePixelBright (juce::Component& component, juce::Point<float> local)
{
    const auto image = component.createComponentSnapshot (component.getLocalBounds(), true, 1.0f);
    if (! image.isValid() || image.getWidth() < 1 || image.getHeight() < 1)
        return false;

    const int px = juce::jlimit (0, image.getWidth() - 1, static_cast<int> (local.x));
    const int py = juce::jlimit (0, image.getHeight() - 1, static_cast<int> (local.y));
    for (int dy = -3; dy <= 3; ++dy)
    {
        for (int dx = -3; dx <= 3; ++dx)
        {
            const int x = juce::jlimit (0, image.getWidth() - 1, px + dx);
            const int y = juce::jlimit (0, image.getHeight() - 1, py + dy);
            const auto pixel = image.getPixelAt (x, y);
            if (pixel.getRed() + pixel.getGreen() + pixel.getBlue() > 180)
                return true;
        }
    }
    return false;
}

}

class ProbeApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "MS50PanelProbe"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        processor = std::make_unique<MS50ModularAudioProcessor>();
        auto* created = processor->createEditor();
        window = std::make_unique<juce::DocumentWindow> ("MS-50 panel probe",
                                                          juce::Colours::black,
                                                          juce::DocumentWindow::allButtons);
        window->setUsingNativeTitleBar (true);
        window->setContentOwned (created, true);
        window->centreWithSize (1280, 512);
        window->setVisible (true);
        window->toFront (true);
        juce::Timer::callAfterDelay (150, [this] { run(); });
    }

    void shutdown() override
    {
        window = nullptr;
        processor = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

private:
    void run()
    {
        auto* editor = dynamic_cast<MS50ModularAudioProcessorEditor*> (window->getContentComponent());
        auto* bay = editor != nullptr ? dynamic_cast<PatchBayView*> (editor->getChildComponent (0)) : nullptr;
        expect (editor != nullptr && bay != nullptr, "editor hosts the patch bay");
        if (bay == nullptr || processor == nullptr)
        {
            finish();
            return;
        }

        expect (bay->panelLoaded(), "panel SVG parsed");
        const int factoryCount = publishedCount (*processor);
        expect (factoryCount == 8, "default patch has eight cables");
        expect (bay->visualCount() == factoryCount, "default cables are drawn");

        const auto there = bay->designToLocal (100.0f, 80.0f);
        const auto back = bay->localToDesign (there);
        expect (std::fabs (back.x - 100.0f) < 0.6f && std::fabs (back.y - 80.0f) < 0.6f, "design mapping at 1280x512");

        const int cutoff = panelKnobIndex ("VCF", "CUTOFF");
        const int ratio = panelKnobIndex ("DIV", "RATIO SWITCH");
        expect (bay->knobCount() == 31 && cutoff >= 0 && ratio >= 0, "live knob table");
        const float cutoffDefault = bay->knobValue (cutoff);
        const float mixBefore = bay->outputMix();
        const int cablesBefore = publishedCount (*processor);
        const auto beforePaint = paintBay (*bay);
        dragKnobLocal (*bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, -40.0f, false);
        expect (near (bay->knobValue (cutoff), cutoffDefault + 0.2f), "drag up 40px turns cutoff");
        expect (bay->knobReadout().contains ("CUTOFF"), "knob readout names cutoff");
        dragKnobLocal (*bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, -200.0f, true);
        expect (near (bay->knobValue (cutoff), cutoffDefault + 0.4f), "shift drag is finer");
        wheelOnKnob (*bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, 1.0f);
        expect (near (bay->knobValue (cutoff), cutoffDefault + 0.5f), "wheel up turns cutoff");
        dragKnobLocal (*bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, -5000.0f, false);
        expect (bay->knobValue (cutoff) == 1.0f, "cutoff clamps at 1");
        const auto turnedPaint = paintBay (*bay);
        const auto capCentre = bay->designToLocal (kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy);
        const auto capEdge = bay->designToLocal (kPanelKnobs[cutoff].cx + kPanelKnobs[cutoff].radius + 8.0f,
                                                  kPanelKnobs[cutoff].cy + kPanelKnobs[cutoff].radius + 8.0f);
        const juce::Rectangle<int> capBox (static_cast<int> (capCentre.x - (capEdge.x - capCentre.x)),
                                            static_cast<int> (capCentre.y - (capEdge.y - capCentre.y)),
                                            static_cast<int> ((capEdge.x - capCentre.x) * 2.0f),
                                            static_cast<int> ((capEdge.y - capCentre.y) * 2.0f));
        expect (knobPixelsDiffer (beforePaint, turnedPaint, capBox), "turning a knob repaints its cap");
        {
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
            const auto time = juce::Time::getCurrentTime();
            const auto at = bay->designToLocal (kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy);
            juce::MouseEvent click (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    bay, bay, time, at, time, 2, false);
            bay->mouseDoubleClick (click);
        }
        expect (near (bay->knobValue (cutoff), cutoffDefault), "double-click resets cutoff");
        clickAt (*bay, kPanelKnobs[ratio].cx, kPanelKnobs[ratio].cy);
        expect (bay->knobValue (ratio) == 1.0f, "divider click steps to 16");
        expect (bay->knobReadout().contains ("16"), "switch readout shows 16");
        dragKnobLocal (*bay, kPanelKnobs[ratio].cx, kPanelKnobs[ratio].cy, 40.0f, false);
        expect (bay->knobValue (ratio) == 0.5f, "divider drag snaps");
        expect (publishedCount (*processor) == cablesBefore, "turning knobs does not publish");
        expect (std::fabs (bay->outputMix() - mixBefore) < 1.0e-6f, "column knobs leave Output mix alone");
        expect (! bay->menuOpen(), "turning knobs does not open the chooser");

        const int extMono = panelJackIndex ("EXT IN", "MONO");
        const int outL = panelJackIndex ("OUTPUT", "L");
        const int outWet = panelJackIndex ("OUTPUT", "WET");
        const int vco = panelJackIndex ("VCO", "HZ/V");
        const int white = panelJackIndex ("NOISE", "WHITE");
        // Mono already feeds the filter. Shift stacks a second cable on Output L.
        gesture (*bay, kPanelJacks[extMono].x, kPanelJacks[extMono].y, kPanelJacks[outL].x, kPanelJacks[outL].y, true, false);
        expect (publishedCount (*processor) == factoryCount + 1, "second cable stacks on Output L");
        expect (near (hostLeftAfter (*processor, 1.0f, 0.0f), 1.5f), "stacked Output L sums to 1.5");

        gesture (*bay, kPanelJacks[outL].x, kPanelJacks[outL].y, 800.0f, 600.0f, false, false);
        expect (publishedCount (*processor) == factoryCount, "drop on empty unplugs the top plug");
        expect (near (hostLeftAfter (*processor, 1.0f, 0.0f), 1.0f), "dry left returns after unplug");

        gesture (*bay, kPanelJacks[vco].x, kPanelJacks[vco].y, kPanelJacks[outWet].x, kPanelJacks[outWet].y, false, false);
        expect (publishedCount (*processor) == factoryCount, "unmapped jack does not connect");
        expect (bay->statusText() == "that jack does not take this cable", "refusal uses the type status");

        int wetBeforePlugs[8] {};
        const int wetBefore = bay->plugsOnJack (outWet, wetBeforePlugs, 8);
        gesture (*bay, kPanelJacks[white].x, kPanelJacks[white].y, kPanelJacks[outWet].x, kPanelJacks[outWet].y, true, false);
        gesture (*bay, kPanelJacks[extMono].x, kPanelJacks[extMono].y, kPanelJacks[outWet].x, kPanelJacks[outWet].y, true, false);
        expect (publishedCount (*processor) == factoryCount + 2, "two cables stack on Output Wet");

        clickAt (*bay, kPanelJacks[outWet].x, kPanelJacks[outWet].y);
        expect (bay->menuOpen(), "occupied jack opens the stack chooser");
        expect (bay->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "escape handles the chooser");
        expect (! bay->menuOpen(), "escape closes the stack chooser");

        Cable before[16] {};
        const int beforeCount = processor->copyPublishedCables (before, 16);
        int plugs[8] {};
        const int onWet = bay->plugsOnJack (outWet, plugs, 8);
        expect (onWet == wetBefore + 2, "wet jack shows the added plugs");
        if (onWet >= 2 && onWet <= 8)
        {
            int reversed[8] {};
            for (int i = 0; i < onWet; ++i)
                reversed[i] = plugs[onWet - 1 - i];
            expect (bay->reorderStack (outWet, reversed, onWet), "chooser reorder");
        }
        Cable after[16] {};
        const int afterCount = processor->copyPublishedCables (after, 16);
        expect (afterCount == beforeCount, "reorder count");
        bool same = beforeCount == afterCount;
        for (int i = 0; i < beforeCount && same; ++i)
        {
            same = before[i].sourceModule == after[i].sourceModule
                   && before[i].sourcePort == after[i].sourcePort
                   && before[i].destModule == after[i].destModule
                   && before[i].destPort == after[i].destPort;
        }
        expect (same, "reordering the stack does not change the graph");

        editor->setSize (1600, 640);
        const auto wide = bay->designToLocal (kPanelJacks[outL].x, kPanelJacks[outL].y);
        const auto wideBack = bay->localToDesign (wide);
        expect (std::fabs (wideBack.x - kPanelJacks[outL].x) < 0.6f, "mapping at 1600 wide");
        editor->setSize (1100, 440);
        const auto narrow = bay->designToLocal (kPanelJacks[white].x, kPanelJacks[white].y);
        const auto narrowBack = bay->localToDesign (narrow);
        expect (std::fabs (narrowBack.x - kPanelJacks[white].x) < 0.6f
                && std::fabs (narrowBack.y - kPanelJacks[white].y) < 0.6f,
                "mapping at 1100 wide");

        editor->setSize (1280, 512);
        const float powerY = kPowerY + kPowerH * 0.5f;
        const float onX = kPowerX + kPowerW * 0.75f;
        const float offX = kPowerX + kPowerW * 0.25f;
        clickAt (*bay, onX, powerY);
        expect (bay->outputMix() == 1.0f, "power rocker turns on");
        clickAt (*bay, onX, powerY);
        expect (bay->outputMix() == 1.0f, "power rocker on stays on");
        clickAt (*bay, offX, powerY);
        expect (bay->outputMix() == 0.0f, "power rocker turns off");
        clickAt (*bay, kPresetBezelX + 12.0f, kPresetBezelY + kPresetBezelH * 0.5f);
        expect (! bay->presetMenuOpen(), "preset screen stays closed while power is off");
        clickAt (*bay, onX, powerY);
        expect (bay->outputMix() == 1.0f, "power rocker turns back on");

        for (int i = 0; i < 20; ++i)
            bay->advanceCableFrame();

        juce::Image snapshot (juce::Image::ARGB, bay->getWidth(), bay->getHeight(), true);
        juce::Graphics graphics (snapshot);
        bay->paintEntireComponent (graphics, true);
        juce::File png ("/tmp/ms50_panel_probe.png");
        png.deleteFile();
        juce::FileOutputStream stream (png);
        if (stream.openedOk())
        {
            juce::PNGImageFormat format;
            format.writeImageToStream (snapshot, stream);
        }

        {
            const auto centre = bay->designToLocal (kHoldCx, kHoldCy);
            const auto topLeft = bay->designToLocal (kHoldCx - 80.0f, kHoldCy - 90.0f);
            const auto bottomRight = bay->designToLocal (kHoldCx + 80.0f, kHoldCy + 70.0f);
            const juce::Rectangle<int> crop (static_cast<int> (topLeft.x),
                                              static_cast<int> (topLeft.y),
                                              static_cast<int> (bottomRight.x - topLeft.x),
                                              static_cast<int> (bottomRight.y - topLeft.y));
            const auto upClip = snapshot.getClippedImage (crop.getIntersection (snapshot.getBounds()));
            const int capX = juce::jlimit (0, snapshot.getWidth() - 1, static_cast<int> (centre.x));
            const int capY = juce::jlimit (0, snapshot.getHeight() - 1, static_cast<int> (centre.y));
            const auto capPixel = snapshot.getPixelAt (capX, capY);
            expect (capPixel.getRed() > 180 && capPixel.getGreen() > 170 && capPixel.getBlue() > 140,
                    "hold key is the cream cap");
            juce::File upFile ("/tmp/ms50_hold_up.png");
            upFile.deleteFile();
            juce::FileOutputStream upStream (upFile);
            if (upStream.openedOk())
            {
                juce::PNGImageFormat format;
                format.writeImageToStream (upClip, upStream);
            }

            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
            const auto time = juce::Time::getCurrentTime();
            juce::MouseEvent down (source, centre, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                   bay, bay, time, centre, time, 1, false);
            bay->mouseDown (down);
            juce::Image heldShot (juce::Image::ARGB, bay->getWidth(), bay->getHeight(), true);
            juce::Graphics heldGraphics (heldShot);
            bay->paintEntireComponent (heldGraphics, true);
            const auto downClip = heldShot.getClippedImage (crop.getIntersection (heldShot.getBounds()));
            juce::File downFile ("/tmp/ms50_hold_down.png");
            downFile.deleteFile();
            juce::FileOutputStream downStream (downFile);
            if (downStream.openedOk())
            {
                juce::PNGImageFormat format;
                format.writeImageToStream (downClip, downStream);
            }
            expect (knobPixelsDiffer (snapshot, heldShot, crop), "holding the button repaints the cap");
            const auto litPoint = bay->designToLocal (kHoldCx + 8.0f, kHoldCy + 8.0f);
            const int litX = juce::jlimit (0, heldShot.getWidth() - 1, static_cast<int> (litPoint.x));
            const int litY = juce::jlimit (0, heldShot.getHeight() - 1, static_cast<int> (litPoint.y));
            const auto unlitCorner = snapshot.getPixelAt (litX, litY);
            const auto heldPixel = heldShot.getPixelAt (litX, litY);
            expect (heldPixel.getRed() > unlitCorner.getRed() + 20
                        && heldPixel.getBlue() + 20 < unlitCorner.getBlue(),
                    "latched hold key is amber");
            expect (processor->extInButtonHeld(), "hold latches");

            juce::MouseEvent up (source, centre, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 bay, bay, time, centre, time, 1, false);
            bay->mouseUp (up);
            expect (processor->extInButtonHeld(), "mouse up leaves the hold latched");
            bay->mouseDown (down);
            expect (! processor->extInButtonHeld(), "a second press releases the hold");
            bay->mouseUp (up);
        }

        bool foundRope = false;
        float ropeX = 0.0f;
        float ropeY = 0.0f;
        for (float y = 360.0f; y < 620.0f && ! foundRope; y += 6.0f)
        {
            for (float x = 80.0f; x < 1520.0f && ! foundRope; x += 6.0f)
            {
                bool nearJack = false;
                for (int jack = 0; jack < kPanelJackCount; ++jack)
                {
                    const float dx = kPanelJacks[jack].x - x;
                    const float dy = kPanelJacks[jack].y - y;
                    if (dx * dx + dy * dy < 22.0f * 22.0f)
                        nearJack = true;
                }
                if (nearJack)
                    continue;
                if (bay->cableNearDesign (x, y) >= 0)
                {
                    foundRope = true;
                    ropeX = x;
                    ropeY = y;
                }
            }
        }
        expect (foundRope, "a settled cable can be hit");
        if (foundRope)
            expect (cablePixelBright (*bay, bay->designToLocal (ropeX, ropeY)), "the hit cable paints");
        const int beforeUnplug = publishedCount (*processor);
        if (foundRope)
            gesture (*bay, ropeX, ropeY, ropeX, ropeY, false, true);
        expect (publishedCount (*processor) == beforeUnplug - 1, "right-click unplugs that cable");

        clickAt (*bay, kPresetBezelX + 12.0f, kPresetBezelY + kPresetBezelH * 0.5f);
        expect (bay->presetMenuOpen(), "preset screen opens the list");
        expect (bay->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "escape closes the preset list");
        expect (! bay->presetMenuOpen(), "preset list is closed");
        clickAt (*bay, kPresetBezelX + 12.0f, kPresetBezelY + kPresetBezelH * 0.5f);
        const float row0Y = kPresetBezelY + kPresetBezelH + 3.0f + 5.0f + 10.5f;
        clickAt (*bay, kPresetBezelX + 20.0f, row0Y);
        expect (! bay->presetMenuOpen(), "choosing a preset closes the list");
        expect (processor->getCurrentProgram() == 0, "the first preset is Dry");
        expect (bay->outputMix() == 0.0f, "dry preset turns the effect off");
        expect (publishedCount (*processor) == 2, "dry replaces the cables");

        finish();
    }

    void finish()
    {
        if (gFails == 0)
            std::printf ("PROBE PASS\n");
        else
            std::printf ("PROBE FAIL %d\n", gFails);
        setApplicationReturnValue (gFails == 0 ? 0 : 1);
        quit();
    }

    std::unique_ptr<MS50ModularAudioProcessor> processor;
    std::unique_ptr<juce::DocumentWindow> window;
};

START_JUCE_APPLICATION (ProbeApp)
