// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/FloatCompare.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "UI/PatchBayLogic.h"
#include "UI/PatchBayView.h"
#include "UI/TabPages.h"

#include <cmath>
#include <cstdlib>
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

void doubleClickKnob (PatchBayView& bay, int knob)
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
    const auto time = juce::Time::getCurrentTime();
    const auto at = bay.designToLocal (kPanelKnobs[knob].cx, kPanelKnobs[knob].cy);
    juce::MouseEvent click (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            &bay, &bay, time, at, time, 2, false);
    bay.mouseDoubleClick (click);
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

// Pixels in the HOLD lamp's disc that turned red between two shots. Cables and their shadows may cross the lamp,
// so count over the disc rather than sample one point.
int holdLampPixelsLit (PatchBayView& bay, const juce::Image& up, const juce::Image& down)
{
    const auto topLeft = bay.designToLocal (kHoldLampCx - kHoldLampR, kHoldLampCy - kHoldLampR);
    const auto bottomRight = bay.designToLocal (kHoldLampCx + kHoldLampR, kHoldLampCy + kHoldLampR);
    int lit = 0;
    for (int y = juce::jmax (0, (int) topLeft.y); y <= juce::jmin (down.getHeight() - 1, (int) bottomRight.y); ++y)
        for (int x = juce::jmax (0, (int) topLeft.x); x <= juce::jmin (down.getWidth() - 1, (int) bottomRight.x); ++x)
        {
            const auto before = up.getPixelAt (x, y);
            const auto after = down.getPixelAt (x, y);
            if (after.getRed() > before.getRed() + 50 && after.getGreen() * 2 < after.getRed() && after.getBlue() * 2 < after.getRed())
                ++lit;
        }
    return lit;
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

float rockerBrightness (PatchBayView& bay, float designX, float designY)
{
    const auto image = paintBay (bay);
    const auto local = bay.designToLocal (designX, designY);
    const int x = juce::jlimit (0, image.getWidth() - 1, static_cast<int> (local.x));
    const int y = juce::jlimit (0, image.getHeight() - 1, static_cast<int> (local.y));
    const auto pixel = image.getPixelAt (x, y);
    return static_cast<float> (pixel.getRed() + pixel.getGreen() + pixel.getBlue());
}

int publishedCount (RoninAudioProcessor& processor)
{
    Cable cables[kPatchBayMaxCables] {};
    return processor.copyPublishedCables (cables, kPatchBayMaxCables);
}

float hostLeftAfter (RoninAudioProcessor& processor, float left, float right)
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
    const juce::String getApplicationName() override { return "RoninPanelProbe"; }
    const juce::String getApplicationVersion() override { return "1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        processor = std::make_unique<RoninAudioProcessor>();
        auto* created = processor->createEditor();
        window = std::make_unique<juce::DocumentWindow> ("RONIN panel probe",
                                                          juce::Colours::black,
                                                          juce::DocumentWindow::allButtons);
        window->setUsingNativeTitleBar (true);
        window->setContentOwned (created, true);
        window->centreWithSize (1280, 480);
        window->setVisible (true);
        window->toFront (true);
        if (std::getenv ("RONIN_PROBE_MANUAL") != nullptr)
            processor->setCableColourByRole (false);   // pixel check: the pre-redesign palette
        if (const char* dumpPath = std::getenv ("RONIN_PROBE_DUMP"))
        {
            // Panel pixel check (RONIN_Redesign §4.0): MAIN face with no cables and every knob at 0.5, bay at
            // 1280 x 451. The same block runs against origin/main; the PNGs must match.
            const juce::String path (dumpPath);
            juce::Timer::callAfterDelay (150, [this]
            {
                Cable published[64] {};
                const int n = processor->copyPublishedCables (published, 64);
                for (int i = 0; i < n; ++i)
                    processor->disconnectJacks (published[i].sourceModule, published[i].sourcePort,
                                                published[i].destModule, published[i].destPort);
                for (int i = 0; i < kPanelKnobCount; ++i)
                    if (auto* p = processor->parameterForPanelKnob (kPanelKnobs[i].section, kPanelKnobs[i].label))
                        p->setValueNotifyingHost (0.5f);
                window->setContentOwned (processor->createEditor(), true);   // fresh bay: no cables loaded
            });
            juce::Timer::callAfterDelay (900, [this, path]
            {
                auto* editor = dynamic_cast<juce::Component*> (window->getContentComponent());
                auto* bay = editor->getChildComponent (0);
                const int extra = editor->getHeight() - bay->getHeight();
                editor->setSize (1280, 451 + extra);
                juce::Timer::callAfterDelay (300, [this, path]
                {
                    auto* bayNow = window->getContentComponent()->getChildComponent (0);
                    const auto shot = bayNow->createComponentSnapshot (bayNow->getLocalBounds(), true, 1.0f);
                    juce::File file (path);
                    file.deleteFile();
                    juce::FileOutputStream out (file);
                    juce::PNGImageFormat().writeImageToStream (shot, out);
                    out.flush();
                    std::printf ("DUMP %dx%d %s\n", shot.getWidth(), shot.getHeight(), path.toRawUTF8());
                    quit();
                });
            });
            return;
        }
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
        auto* editor = dynamic_cast<RoninAudioProcessorEditor*> (window->getContentComponent());
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
        // Startup is the INIT program: every face knob starts on the default table, cap and parameter alike.
        for (int i = 0; i < kPanelKnobCount; ++i)
        {
            auto* parameter = processor->parameterForPanelKnob (kPanelKnobs[i].section, kPanelKnobs[i].label);
            if (parameter == nullptr || ! near (parameter->getValue(), kPanelKnobs[i].valueDefault)
                || ! near (bay->knobValue (i), kPanelKnobs[i].valueDefault))
            {
                std::printf ("FAIL %s %s does not start on the default table\n", kPanelKnobs[i].section, kPanelKnobs[i].label);
                ++gFails;
            }
        }

        const int levelKnob = panelKnobIndex ("OUTPUT", "LEVEL");
        auto* levelParam = processor->parameterForPanelKnob ("OUTPUT", "LEVEL");
        auto* effect = processor->effectParameter();
        expect (levelKnob >= 0 && levelParam != nullptr && effect != nullptr, "output level knob is wired");
        if (levelParam != nullptr && effect != nullptr && levelKnob >= 0)
        {
            expect (levelParam->getName (64) == "Output Level", "output level parameter name");
            expect (near (levelParam->getValue(), 0.7f), "output level starts at 0.7");
            expect (near (bay->knobValue (levelKnob), 0.7f), "output level cap starts at 0.7");
            // A fresh instance is the INIT program, so Effect starts on. The dry checks below turn it off.
            expect (processor->effectIsOn(), "a fresh instance starts with Effect on");
            expect (processor->getCurrentProgram() == kDefaultFactoryPreset, "a fresh instance is INIT");
            effect->setValueNotifyingHost (0.0f);
            const float dryUnity = hostLeftAfter (*processor, 0.5f, 0.0f);
            expect (near (dryUnity, 0.5f), "default output level is unity on the dry path");

            dragKnobLocal (*bay, kPanelKnobs[levelKnob].cx, kPanelKnobs[levelKnob].cy, -40.0f, false);
            const float raised = bay->knobValue (levelKnob);
            const float dryRaised = hostLeftAfter (*processor, 0.5f, 0.0f);
            expect (raised > 0.85f, "drag raises output level");
            expect (dryRaised > dryUnity + 0.2f, "effect off, raising level makes the dry track louder");
            expect (near (dryRaised, 0.5f * outputLevelGain (raised)), "dry host buffer uses the output level law");

            dragKnobLocal (*bay, kPanelKnobs[levelKnob].cx, kPanelKnobs[levelKnob].cy, 5000.0f, false);
            expect (near (bay->knobValue (levelKnob), 0.0f), "output level drags to silence");
            expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 0.0f), "output level at 0 mutes the dry track");

            levelParam->setValueNotifyingHost (1.0f);
            expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 1.0f), "full output level is twice as loud");

            effect->setValueNotifyingHost (1.0f);
            expect (std::fabs (hostLeftAfter (*processor, 0.02f, 0.02f)) < 1.0e-3f,
                    "effect on, gate released, full level stays silent");

            auto* attack = processor->parameterForPanelKnob ("EG 1", "ATTACK");
            auto* cutoff = processor->parameterForPanelKnob ("VCF", "CUTOFF");
            auto* amount = processor->parameterForPanelKnob ("VCF", "MOD");
            auto* lowCut = processor->parameterForPanelKnob ("VCA 1", "LOW CUT");
            expect (attack != nullptr && cutoff != nullptr && amount != nullptr && lowCut != nullptr,
                    "voice knobs stay parameters");
            if (attack != nullptr && cutoff != nullptr && amount != nullptr && lowCut != nullptr)
            {
                const float attackWas = attack->getValue();
                const float cutoffWas = cutoff->getValue();
                const float amountWas = amount->getValue();
                const float lowCutWas = lowCut->getValue();
                attack->setValueNotifyingHost (0.0f);
                cutoff->setValueNotifyingHost (1.0f);
                amount->setValueNotifyingHost (0.0f);
                lowCut->setValueNotifyingHost (0.0f);
                processor->setExtInButtonHeld (true);

                auto voiceRms = [&] (float level) -> float
                {
                    levelParam->setValueNotifyingHost (level);
                    processor->prepareToPlay (48000.0, 128);
                    juce::AudioBuffer<float> buffer (2, 128);
                    juce::MidiBuffer midi;
                    double energy = 0.0;
                    int count = 0;
                    for (int block = 0; block < 40; ++block)
                    {
                        for (int i = 0; i < 128; ++i)
                        {
                            const int t = block * 128 + i;
                            const float sample = 0.02f * std::sin (2.0f * 3.14159265f * 220.0f
                                                                   * static_cast<float> (t) / 48000.0f);
                            buffer.setSample (0, i, sample);
                            buffer.setSample (1, i, sample);
                        }
                        processor->processBlock (buffer, midi);
                        if (block < 20)
                            continue;
                        for (int i = 0; i < 128; ++i)
                        {
                            const double x = static_cast<double> (buffer.getSample (0, i));
                            energy += x * x;
                        }
                        count += 128;
                    }
                    return static_cast<float> (std::sqrt (energy / static_cast<double> (count)));
                };

                const float unityVoice = voiceRms (0.7f);
                const float fullVoice = voiceRms (1.0f);
                expect (unityVoice > 1.0e-4f, "effect on, gate held, the voice is audible");
                expect (fullVoice > unityVoice * 1.9f && fullVoice < unityVoice * 2.1f,
                        "raising output level makes the held voice louder");

                processor->setExtInButtonHeld (false);
                attack->setValueNotifyingHost (attackWas);
                cutoff->setValueNotifyingHost (cutoffWas);
                amount->setValueNotifyingHost (amountWas);
                lowCut->setValueNotifyingHost (lowCutWas);
            }

            processor->setExtInButtonHeld (false);
            effect->setValueNotifyingHost (0.0f);
            levelParam->setValueNotifyingHost (0.7f);
            {
                auto source = juce::Desktop::getInstance().getMainMouseSource();
                const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
                const auto time = juce::Time::getCurrentTime();
                const auto at = bay->designToLocal (kPanelKnobs[levelKnob].cx, kPanelKnobs[levelKnob].cy);
                juce::MouseEvent click (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        bay, bay, time, at, time, 2, false);
                bay->mouseDoubleClick (click);
            }
            expect (near (bay->knobValue (levelKnob), 0.7f), "double-click resets output level");
            expect (! processor->effectIsOn(), "effect is off after the output level check");
            expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 0.5f), "restored output level is unity");
            expect (publishedCount (*processor) == factoryCount, "output level does not change the eight cables");
        }

        const int mixKnob = panelKnobIndex ("OUTPUT", "MIX");
        auto* mixParam = processor->parameterForPanelKnob ("OUTPUT", "MIX");
        expect (mixKnob >= 0 && mixParam != nullptr, "output mix knob is wired");
        if (mixKnob >= 0 && mixParam != nullptr)
        {
            expect (mixParam->getName (64) == "Output Mix", "output mix parameter name");
            expect (near (mixParam->getValue(), 1.0f), "output mix starts at 1");
            expect (near (bay->knobValue (mixKnob), 1.0f), "output mix cap starts at 1");
            expect (! processor->effectIsOn(), "mix check starts with the effect off");
            const float levelBefore = levelParam != nullptr ? levelParam->getValue() : -1.0f;
            const float mixX = kPanelKnobs[mixKnob].cx;
            const float mixY = kPanelKnobs[mixKnob].cy;

            dragKnobLocal (*bay, mixX, mixY, 200.0f, false);
            expect (near (bay->knobValue (mixKnob), 0.0f), "output mix drags to 0");
            expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 0.5f), "effect off, mix 0 stays dry");
            dragKnobLocal (*bay, mixX, mixY, -200.0f, false);
            expect (near (bay->knobValue (mixKnob), 1.0f), "output mix drags back to 1");
            expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 0.5f), "effect off, mix 1 stays dry");

            clickAt (*bay, kPowerX + kPowerW * 0.75f, kPowerY + kPowerH * 0.5f);
            expect (processor->effectIsOn(), "printed ON turns the effect on");
            dragKnobLocal (*bay, mixX, mixY, 200.0f, false);
            expect (near (bay->knobValue (mixKnob), 0.0f), "effect on, mix drags to 0");
            expect (near (bay->outputMix(), 0.0f), "effect on, mix 0 is dry");
            expect (near (hostLeftAfter (*processor, 0.02f, 0.02f), 0.02f), "effect on, mix 0 passes the dry cable");
            dragKnobLocal (*bay, mixX, mixY, -100.0f, false);
            expect (near (bay->knobValue (mixKnob), 0.5f), "output mix lands on 0.5");
            const float blended = hostLeftAfter (*processor, 0.02f, 0.02f);
            expect (blended > 0.005f && blended < 0.015f, "effect on, mix 0.5 blends dry with the patch");
            dragKnobLocal (*bay, mixX, mixY, -100.0f, false);
            expect (near (bay->knobValue (mixKnob), 1.0f), "output mix returns to the patch");
            expect (std::fabs (hostLeftAfter (*processor, 0.02f, 0.02f)) < 1.0e-3f, "effect on, mix 1 is the patch");
            expect (publishedCount (*processor) == factoryCount, "output mix does not change the eight cables");
            if (levelParam != nullptr)
                expect (near (levelParam->getValue(), levelBefore), "output mix leaves output level");

            {
                auto source = juce::Desktop::getInstance().getMainMouseSource();
                const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
                const auto time = juce::Time::getCurrentTime();
                const auto at = bay->designToLocal (mixX, mixY);
                juce::MouseEvent click (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        bay, bay, time, at, time, 2, false);
                bay->mouseDoubleClick (click);
            }
            expect (near (bay->knobValue (mixKnob), 1.0f), "double-click resets output mix");
            clickAt (*bay, kPowerX + kPowerW * 0.25f, kPowerY + kPowerH * 0.5f);
            expect (! processor->effectIsOn(), "printed OFF turns the effect off");
            expect (near (bay->outputMix(), 0.0f), "printed OFF ignores mix");
            expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 0.5f), "printed OFF passes the dry cable");
        }

        const auto there = bay->designToLocal (100.0f, 80.0f);
        const auto back = bay->localToDesign (there);
        expect (std::fabs (back.x - 100.0f) < 0.6f && std::fabs (back.y - 80.0f) < 0.6f, "design mapping at 1280x451");

        const int cutoff = panelKnobIndex ("VCF", "CUTOFF");
        const int ratio = panelKnobIndex ("DIV", "RATIO SWITCH");
        expect (bay->knobCount() == 33 && cutoff >= 0 && ratio >= 0, "live knob table");
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
        expect (ronin::exactlyEqual (bay->knobValue (cutoff), 1.0f), "cutoff clamps at 1");
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
        {
            const auto midPaint = paintBay (*bay);
            dragKnobLocal (*bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, 100.0f, false);
            expect (near (bay->knobValue (cutoff), 0.0f), "drag down parks cutoff at 0");
            const auto lowPaint = paintBay (*bay);
            const float cx = kPanelKnobs[cutoff].cx;
            const float cy = kPanelKnobs[cutoff].cy;
            const float ox = 1.0f;
            const float oy = 2.0f;
            const float olen = std::sqrt (ox * ox + oy * oy);
            const float dist = kPanelKnobs[cutoff].radius + 3.0f + 1.6f;
            const auto shadowAt = bay->designToLocal (cx + ox / olen * dist, cy + oy / olen * dist);
            const int sx = juce::jlimit (0, midPaint.getWidth() - 1, static_cast<int> (shadowAt.x));
            const int sy = juce::jlimit (0, midPaint.getHeight() - 1, static_cast<int> (shadowAt.y));
            const auto midShadow = midPaint.getPixelAt (sx, sy);
            const auto lowShadow = lowPaint.getPixelAt (sx, sy);
            expect (midShadow != lowShadow, "knob shadow moves when the cap turns");
            const auto highCentre = bay->designToLocal (cx, cy);
            const int box = 36;
            const juce::Rectangle<int> knobBox (static_cast<int> (highCentre.x) - box,
                                                static_cast<int> (highCentre.y) - box,
                                                box * 2, box * 2);
            auto writeCrop = [&] (const juce::Image& image, const char* path)
            {
                juce::File file (path);
                file.deleteFile();
                juce::FileOutputStream stream (file);
                if (! stream.openedOk())
                    return;
                juce::PNGImageFormat format;
                format.writeImageToStream (image.getClippedImage (knobBox.getIntersection (image.getBounds())), stream);
            };
            writeCrop (lowPaint, "/tmp/ronin_knob_low.png");
            dragKnobLocal (*bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, -100.0f, false);
            expect (near (bay->knobValue (cutoff), 0.5f), "drag up 100px from 0 is half travel");
            writeCrop (paintBay (*bay), "/tmp/ronin_knob_mid.png");
        }
        clickAt (*bay, kPanelKnobs[ratio].cx, kPanelKnobs[ratio].cy);
        expect (ronin::exactlyEqual (bay->knobValue (ratio), 1.0f), "divider click steps from /2 to /4");
        expect (bay->knobReadout().contains ("4") && ! bay->knobReadout().contains ("16"), "switch readout shows 4");
        if (auto* ratioParam = processor->parameterForPanelKnob ("DIV", "RATIO SWITCH"))
            expect (near (ratioParam->getValue(), 1.0f), "the switch is saved as a host setting");
        else
            expect (false, "the switch is saved as a host setting");
        dragKnobLocal (*bay, kPanelKnobs[ratio].cx, kPanelKnobs[ratio].cy, 120.0f, false);
        expect (ronin::exactlyEqual (bay->knobValue (ratio), 0.0f), "divider drag snaps back to /2");
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

        editor->setSize (1600, 600);   // art 1600 x 564 + 36 px tab strip
        const auto wide = bay->designToLocal (kPanelJacks[outL].x, kPanelJacks[outL].y);
        const auto wideBack = bay->localToDesign (wide);
        expect (std::fabs (wideBack.x - kPanelJacks[outL].x) < 0.6f, "mapping at 1600 wide");
        editor->setSize (1100, 413);
        const auto narrow = bay->designToLocal (kPanelJacks[white].x, kPanelJacks[white].y);
        const auto narrowBack = bay->localToDesign (narrow);
        expect (std::fabs (narrowBack.x - kPanelJacks[white].x) < 0.6f
                && std::fabs (narrowBack.y - kPanelJacks[white].y) < 0.6f,
                "mapping at 1100 wide");

        editor->setSize (1280, 480);
        const float powerY = kPowerY + kPowerH * 0.5f;
        const float onX = kPowerX + kPowerW * 0.75f;
        const float offX = kPowerX + kPowerW * 0.25f;
        clickAt (*bay, onX, powerY);
        expect (ronin::exactlyEqual (bay->outputMix(), 1.0f), "power rocker turns on");
        expect (processor->effectIsOn(), "right half is wet");
        expect (rockerBrightness (*bay, onX, powerY) > rockerBrightness (*bay, offX, powerY) + 20.0f,
                "raised end points at ON");
        clickAt (*bay, onX, powerY);
        expect (ronin::exactlyEqual (bay->outputMix(), 1.0f), "power rocker on stays on");
        clickAt (*bay, offX, powerY);
        expect (ronin::exactlyEqual (bay->outputMix(), 0.0f), "power rocker turns off");
        expect (! processor->effectIsOn(), "left half is dry");
        expect (rockerBrightness (*bay, offX, powerY) > rockerBrightness (*bay, onX, powerY) + 20.0f,
                "raised end points at OFF");
        expect (near (hostLeftAfter (*processor, 0.5f, 0.0f), 0.5f), "printed OFF stays a dry pass");
        {
            const auto offPaint = paintBay (*bay);
            const auto lcd0 = bay->designToLocal (kPresetLcdX, kPresetLcdY);
            const auto lcd1 = bay->designToLocal (kPresetLcdX + kPresetLcdW, kPresetLcdY + kPresetLcdH);
            const juce::Rectangle<int> lcdBox (static_cast<int> (lcd0.x), static_cast<int> (lcd0.y),
                                              static_cast<int> (lcd1.x - lcd0.x), static_cast<int> (lcd1.y - lcd0.y));
            juce::File lcdFile ("/tmp/ronin_lcd_off.png");
            lcdFile.deleteFile();
            juce::FileOutputStream lcdStream (lcdFile);
            if (lcdStream.openedOk())
            {
                juce::PNGImageFormat format;
                format.writeImageToStream (offPaint.getClippedImage (lcdBox.getIntersection (offPaint.getBounds())), lcdStream);
            }
        }
        clickAt (*bay, kPresetBezelX + 12.0f, kPresetBezelY + kPresetBezelH * 0.5f);
        expect (bay->presetMenuOpen(), "preset screen opens while power is off");
        expect (bay->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey)), "escape closes the preset list while power is off");
        expect (! bay->presetMenuOpen(), "preset list is closed while power is off");
        clickAt (*bay, onX, powerY);
        expect (ronin::exactlyEqual (bay->outputMix(), 1.0f), "power rocker turns back on");

        for (int i = 0; i < 20; ++i)
            bay->advanceCableFrame();

        juce::Image snapshot (juce::Image::ARGB, bay->getWidth(), bay->getHeight(), true);
        juce::Graphics graphics (snapshot);
        bay->paintEntireComponent (graphics, true);
        juce::File png ("/tmp/ronin_panel_probe.png");
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
            juce::File upFile ("/tmp/ronin_hold_up.png");
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
            juce::File downFile ("/tmp/ronin_hold_down.png");
            downFile.deleteFile();
            juce::FileOutputStream downStream (downFile);
            if (downStream.openedOk())
            {
                juce::PNGImageFormat format;
                format.writeImageToStream (downClip, downStream);
            }
            expect (knobPixelsDiffer (snapshot, heldShot, crop), "holding the button repaints the cap");
            expect (holdLampPixelsLit (*bay, snapshot, heldShot) >= 3, "the red HOLD lamp lights while the key is held");
            expect (processor->extInButtonHeld(), "mouse down holds the key");

            const auto dragged = bay->designToLocal (kHoldCx + 80.0f, kHoldCy + 40.0f);
            juce::MouseEvent drag (source, dragged, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                   bay, bay, time, centre, time, 1, true);
            bay->mouseDrag (drag);
            expect (processor->extInButtonHeld(), "dragging off the key keeps it held");

            juce::MouseEvent up (source, centre, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 bay, bay, time, centre, time, 1, false);
            bay->mouseUp (up);
            expect (! processor->extInButtonHeld(), "mouse up releases the hold");

            clickAt (*bay, offX, powerY);
            expect (ronin::exactlyEqual (bay->outputMix(), 0.0f), "hold check turns the effect off");
            bay->mouseDown (down);
            juce::Image offHeld (juce::Image::ARGB, bay->getWidth(), bay->getHeight(), true);
            juce::Graphics offHeldGraphics (offHeld);
            bay->paintEntireComponent (offHeldGraphics, true);
            expect (processor->extInButtonHeld(), "mouse down holds the key while the effect is off");
            expect (holdLampPixelsLit (*bay, snapshot, offHeld) >= 3, "the HOLD lamp lights while the effect is off");
            bay->mouseUp (up);
            expect (! processor->extInButtonHeld(), "mouse up releases the hold while the effect is off");
            clickAt (*bay, onX, powerY);
            expect (ronin::exactlyEqual (bay->outputMix(), 1.0f), "hold check turns the effect back on");

            const auto legend = bay->designToLocal (1325.6f + 10.0f, 287.4f + 4.0f);
            juce::MouseEvent legendDown (source, legend, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         bay, bay, time, legend, time, 1, false);
            bay->mouseDown (legendDown);
            expect (processor->extInButtonHeld(), "the HOLD legend holds the key");
            juce::MouseEvent legendUp (source, legend, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                       bay, bay, time, legend, time, 1, false);
            bay->mouseUp (legendUp);
            expect (! processor->extInButtonHeld(), "the HOLD legend releases the key");

            const juce::ModifierKeys right (juce::ModifierKeys::rightButtonModifier);
            juce::MouseEvent rightDown (source, centre, right, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                        bay, bay, time, centre, time, 1, false);
            bay->mouseDown (rightDown);
            expect (! processor->extInButtonHeld(), "a right-click does not hold the key");
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
        expect (processor->getCurrentProgram() == kInitPreset, "the first preset is INIT");
        checkVcaKnobs (*bay, row0Y);
        clickAt (*bay, kPresetBezelX + 12.0f, kPresetBezelY + kPresetBezelH * 0.5f);
        clickAt (*bay, kPresetBezelX + 20.0f, row0Y);
        expect (processor->getCurrentProgram() == kInitPreset, "the first preset is INIT again");
        expect (! ronin::exactlyEqual (bay->outputMix(), 0.0f), "INIT leaves the effect on");
        expect (publishedCount (*processor) == 8, "INIT replaces the cables with its eight");
        if (auto* restoredLevel = processor->parameterForPanelKnob ("OUTPUT", "LEVEL"))
            expect (near (restoredLevel->getValue(), 0.7f), "preset restore puts output level back at 0.7");
        if (auto* restoredMix = processor->parameterForPanelKnob ("OUTPUT", "MIX"))
            expect (near (restoredMix->getValue(), 1.0f), "preset restore puts output mix back at 1");

        checkTabs (*editor, *bay);
        finish();
    }

    // VCA 1 and VCA 2 Initial and Mod: bound to the module, double-click returns the module default,
    // and loading INIT puts a raised VCA 1 Initial back at 0 on the knob.
    void checkVcaKnobs (PatchBayView& bay, float row0Y)
    {
        struct VcaKnob { const char* section; const char* label; float moduleDefault; };
        const VcaKnob vcaKnobs[] = { { "VCA 1", "INITIAL", 0.0f }, { "VCA 1", "MOD", 0.85f },
                                     { "VCA 2", "INITIAL", 0.0f }, { "VCA 2", "MOD", 1.0f } };
        for (const auto& v : vcaKnobs)
        {
            const int knob = panelKnobIndex (v.section, v.label);
            auto* parameter = processor->parameterForPanelKnob (v.section, v.label);
            if (knob < 0 || parameter == nullptr)
            {
                std::printf ("FAIL %s %s is not a host knob\n", v.section, v.label);
                ++gFails;
                continue;
            }
            expect (near (kPanelKnobs[knob].valueDefault, v.moduleDefault), "panel default is the module default");
            expect (near (bay.knobValue (knob), v.moduleDefault), "INIT shows the module default");
            dragKnobLocal (bay, kPanelKnobs[knob].cx, kPanelKnobs[knob].cy, v.moduleDefault > 0.5f ? 60.0f : -60.0f, false);
            expect (! near (parameter->getValue(), v.moduleDefault), "dragging the knob moves its parameter");
            doubleClickKnob (bay, knob);
            expect (near (bay.knobValue (knob), v.moduleDefault), "double-click returns the module default");
            expect (near (parameter->getValue(), v.moduleDefault), "double-click resets the parameter");
        }

        const int initial = panelKnobIndex ("VCA 1", "INITIAL");
        auto* initialParam = processor->parameterForPanelKnob ("VCA 1", "INITIAL");
        expect (near (factoryVca1Initial (kInitPreset), 0.0f), "INIT leaves VCA 1 Initial at 0");
        if (initial >= 0 && initialParam != nullptr)
        {
            dragKnobLocal (bay, kPanelKnobs[initial].cx, kPanelKnobs[initial].cy, -60.0f, false);
            expect (initialParam->getValue() > 0.05f, "dragging raises VCA 1 Initial");
            clickAt (bay, kPresetBezelX + 12.0f, kPresetBezelY + kPresetBezelH * 0.5f);
            clickAt (bay, kPresetBezelX + 20.0f, row0Y);
            expect (processor->getCurrentProgram() == kInitPreset, "INIT loads from the list");
            expect (near (initialParam->getValue(), 0.0f), "INIT puts the VCA 1 Initial parameter back at 0");
            expect (near (bay.knobValue (initial), 0.0f), "INIT shows 0 on the VCA 1 Initial knob");
        }
    }

    // RONIN_Redesign §4.0 / §5.16: tab strip above the unchanged art, VOICE / ENV / PATCH / SETUP click-through,
    // MAIN overlays (hover read-outs, role colours).
    void checkTabs (RoninAudioProcessorEditor& editor, PatchBayView& bay)
    {
        using ronin_ui::Tab;
        editor.applyScale (100);
        expect (editor.getWidth() == 1280 && editor.getHeight() == 480, "editor default 1280 x 480");
        expect (bay.getWidth() == 1280 && bay.getHeight() == 451, "MAIN art keeps 1280 x 451 under the strip");
        expect (bay.getY() == editor.tabStrip().getHeight() && editor.tabStrip().getY() == 0, "strip sits above the art");
        expect (editor.getChildComponent (0) == &bay, "bay stays child 0");

        auto clickStrip = [&editor] (Tab tab)
        {
            auto& strip = editor.tabStrip();
            const auto at = strip.tabBounds (tab).getCentre();
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
            const auto time = juce::Time::getCurrentTime();
            juce::MouseEvent down (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &strip, &strip, time, at, time, 1, false);
            strip.mouseDown (down);
        };
        auto clickPage = [] (ronin_ui::Page& page, float x, float y)
        {
            // A real mouse event at the design point, so the page's local mapping is exercised too.
            const auto at = page.origin() + juce::Point<float> (x, y) * page.scale();
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            const juce::ModifierKeys mods (juce::ModifierKeys::leftButtonModifier);
            const auto time = juce::Time::getCurrentTime();
            juce::MouseEvent down (source, at, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &page, &page, time, at, time, 1, false);
            juce::Image scratch (juce::Image::ARGB, page.getWidth(), page.getHeight(), true);
            juce::Graphics g (scratch);
            page.paint (g);   // regions come from paint
            page.mouseDown (down);
        };

        for (auto tab : { Tab::Voice, Tab::Env, Tab::Patch, Tab::Setup, Tab::Main })
        {
            clickStrip (tab);
            expect (editor.currentTab() == tab, "tab click selects the tab");
            expect (bay.isVisible() == (tab == Tab::Main), "MAIN art shows only on MAIN");
            if (auto* page = editor.page (tab))
            {
                expect (page->isVisible() && page->getBounds() == bay.getBounds(), "page replaces the art at its size");
                const auto shot = page->createComponentSnapshot (page->getLocalBounds(), true, 1.0f);
                expect (shot.isValid(), "page paints");
                if (const char* dir = std::getenv ("RONIN_PROBE_PAGES"))
                {
                    const auto whole = editor.createComponentSnapshot (editor.getLocalBounds(), true, 1.0f);
                    juce::File file (juce::File (juce::String (dir)).getChildFile (juce::String (ronin_ui::tabName (tab)) + ".png"));
                    file.deleteFile();
                    juce::FileOutputStream out (file);
                    juce::PNGImageFormat().writeImageToStream (whole, out);
                }
            }
        }

        // VOICE: TRI SHAPE (M-R2) and HQ 2x (default OFF, latency 0 / 23).
        clickStrip (Tab::Voice);
        auto& voice = *editor.page (Tab::Voice);
        expect (processor->triShape() == Vco::TriShape::Triangle, "fresh instance is TRIANGLE");
        clickPage (voice, 160.0f + 180.0f, 257.0f);
        expect (processor->triShape() == Vco::TriShape::Parabola, "VOICE selects PARABOLA (legacy)");
        clickPage (voice, 160.0f + 60.0f, 257.0f);
        expect (processor->triShape() == Vco::TriShape::Triangle, "VOICE selects TRIANGLE");
        expect (! processor->hqParameter()->get(), "HQ defaults OFF");
        clickPage (voice, 720.0f + 120.0f, 127.0f);
        expect (processor->hqParameter()->get(), "VOICE turns HQ on");
        clickPage (voice, 720.0f + 40.0f, 127.0f);
        expect (! processor->hqParameter()->get(), "VOICE turns HQ off");

        // SETUP: cable colour mode, EG time display, UI scale.
        clickStrip (Tab::Setup);
        auto& setup = *editor.page (Tab::Setup);
        expect (processor->cableColourByRole(), "cable colour defaults BY ROLE");
        clickPage (setup, 420.0f + 195.0f, 223.0f);
        expect (! processor->cableColourByRole(), "SETUP selects MANUAL colours");
        clickPage (setup, 420.0f + 65.0f, 223.0f);
        expect (processor->cableColourByRole(), "SETUP selects BY ROLE colours");
        clickPage (setup, 420.0f + 195.0f, 303.0f);
        expect (processor->egTimeInMs(), "SETUP selects ms EG display");
        clickPage (setup, 420.0f + 65.0f, 303.0f);
        expect (! processor->egTimeInMs(), "SETUP selects s EG display");
        clickPage (setup, 110.0f + 120.0f * 2.0f + 60.0f, 118.0f);   // 125 %
        expect (editor.getWidth() == 1600 && editor.getHeight() == 600, "125 % scale is 1600 x 600");
        expect (processor->uiScalePercent() == 125, "scale is saved with the patch");
        clickPage (*editor.page (Tab::Setup), 110.0f + 120.0f + 60.0f, 118.0f);      // 100 %
        expect (editor.getWidth() == 1280 && editor.getHeight() == 480, "100 % scale is back to 1280 x 480");

        // PATCH: one row per published cable; Del unplugs and the MAIN bay follows.
        clickStrip (Tab::Patch);
        auto* patch = dynamic_cast<ronin_ui::PatchPage*> (editor.page (Tab::Patch));
        const int cablesBefore = publishedCount (*processor);
        expect (patch != nullptr && patch->rowCount() == cablesBefore, "PATCH lists every cable");
        if (patch != nullptr && cablesBefore > 0)
        {
            expect (patch->rowText (0).contains (":"), "PATCH rows name jacks SECTION:LABEL");
            clickPage (*patch, 300.0f, 76.0f + 15.0f);
            expect (patch->selectedCable() == 0, "clicking a row selects it");
            const int visualBefore = bay.visualCount();
            patch->keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
            expect (publishedCount (*processor) == cablesBefore - 1, "Del unplugs the selected cable");
            expect (bay.visualCount() == visualBefore - 1, "MAIN bay drops the unplugged cable");
        }

        clickStrip (Tab::Main);
        // MAIN overlays: transient hover text with role, volts and far end; MIX is the inverting mixer.
        const int saw = panelJackIndex ("VCO", "SAW");
        const auto sawText = bay.hoverTextForJack (saw);
        expect (sawText.startsWith ("VCO:SAW") && sawText.contains ("AUDIO") && sawText.contains (" V"),
                "jack hover shows id, role and volts");
        const auto mixText = bay.hoverTextForJack (panelJackIndex ("MIX", "OUT"));
        expect (mixText.contains ("inverting mixer"), "MIX hover explains the inverting sum");
        const auto trigText = bay.hoverTextForJack (panelJackIndex ("EG 1", "TRIG"));
        expect (trigText.contains ("S-TRIG"), "EG 1 TRIG reads as an S-trig input");

        // Right-click a knob -> type a value in real units.
        const int cutoff = panelKnobIndex ("VCF", "CUTOFF");
        auto* cutoffParam = processor->parameterForPanelKnob ("VCF", "CUTOFF");
        if (cutoff >= 0 && cutoffParam != nullptr)
        {
            gesture (bay, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy, kPanelKnobs[cutoff].cx, kPanelKnobs[cutoff].cy,
                     false, true);
            expect (bay.valueEditorOpen(), "right-click on a knob opens the value box");
            expect (bay.valueEditorText().endsWith ("Hz"), "the value box shows real units");
            expect (bay.typeKnobValue (cutoff, "1 kHz"), "typing 1 kHz is accepted");
            expect (std::fabs (Vcf::knobHzFor (static_cast<double> (cutoffParam->getValue())) - 1000.0) < 1.0, "1 kHz lands on the knob law");
            expect (bay.knobReadout().contains ("1.00 kHz"), "the read-out follows the typed value");
            expect (! bay.typeKnobValue (cutoff, "fast"), "nonsense is refused");
        }
        const auto egKnob = panelKnobIndex ("EG 1", "ATTACK");
        if (egKnob >= 0 && processor->parameterForPanelKnob ("EG 1", "ATTACK") != nullptr)
        {
            expect (bay.typeKnobValue (egKnob, "621.5 ms"), "typing an EG time is accepted");
            expect (bay.knobReadout().contains ("621 ms") || bay.knobReadout().contains ("622 ms"), "EG reads real time");
        }
        std::printf ("tabs: %s\n", gFails == 0 ? "clicked through" : "failures above");
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

    std::unique_ptr<RoninAudioProcessor> processor;
    std::unique_ptr<juce::DocumentWindow> window;
};

START_JUCE_APPLICATION (ProbeApp)
