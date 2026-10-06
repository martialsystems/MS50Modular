// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "PluginProcessor.h"
#include "UI/PatchBayLogic.h"

#include <juce_gui_basics/juce_gui_basics.h>

class StackMenu;

// Landscape panel and the patch-bay cable gestures.
// Graph edits run on the message thread through the processor.
class PatchBayView : public juce::Component,
                     private juce::Timer
{
public:
    explicit PatchBayView (MS50ModularAudioProcessor&);
    ~PatchBayView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

    juce::Point<float> designToLocal (float x, float y) const;
    juce::Point<float> localToDesign (juce::Point<float> local) const;
    int visualCount() const noexcept { return count_; }
    void visualEnds (int index, int& jackA, int& jackB) const;
    bool menuOpen() const;
    const juce::String& statusText() const noexcept { return status_; }
    float outputMix() const noexcept { return audioProcessor.effectIsOn() ? 1.0f : 0.0f; }
    int knobCount() const noexcept { return kPanelKnobCount; }
    float knobValue (int index) const;
    const juce::String& knobReadout() const noexcept { return knobReadout_; }
    bool panelLoaded() const noexcept { return panel_ != nullptr; }
    int cableNearDesign (float x, float y) const { return cableNear (x, y); }
    int plugsOnJack (int jack, int* out, int capacity) const;
    int visualColor (int index) const;
    int selectedColor() const noexcept { return currentColor_; }
    bool reorderStack (int jack, const int* bottomToTop, int n);
    void advanceCableFrame();

    void menuReorder (int jack, const int* topFirst, int n);
    void menuPickup (int cableIndex, bool endIsA);
    void menuAdd (int jack);

private:
    static constexpr int kNodes = 36;

    struct Rope {
        float p[kNodes][2] {};
        float q[kNodes][2] {};
        bool ready = false;
    };

    void timerCallback() override;
    void showStatus (const char* text);
    void prepareRopes (bool settleFresh);
    void stepRopes();
    void stackLevels (int* levelA, int* levelB) const;
    void endPin (int jack, int level, float& x, float& y) const;
    int jackAt (float x, float y) const;
    int knobAt (float x, float y) const;
    void setKnobValue (int index, float value);
    int labelAt (float x, float y) const;
    int cableNear (float x, float y) const;
    bool switchAt (float x, float y) const;
    bool extInButtonAt (float x, float y) const;
    int swatchAt (float x, float y) const;
    juce::AudioProcessorParameter* parameterForKnob (int index) const;
    void endGesture();
    void syncHostKnobs();
    void removeCable (int index);
    void restoreGrabbedEnd();
    void clearGrab();
    void cancelGrab();
    void dropAt (int targetJack);
    void unplugIndex (int index);
    void startGrab (int index, bool endIsA, bool isNew, bool carry);
    void startNewCable (int jack);
    float panelScale() const;
    juce::Point<float> panelOrigin() const;

    MS50ModularAudioProcessor& audioProcessor;
    std::unique_ptr<juce::Drawable> panel_;
    std::unique_ptr<StackMenu> menu_;
    VisualCable cables_[kPatchBayMaxCables] {};
    Rope ropes_[kPatchBayMaxCables] {};
    juce::Path stroke_;
    int count_ = 0;
    int currentColor_ = 0;
    float knobValue_[kPanelKnobCount] {};
    juce::AudioProcessorParameter* gestureParam_ = nullptr;
    juce::String status_;
    juce::String knobReadout_;

    bool grabActive_ = false;
    bool grabEndA_ = false;
    bool grabIsNew_ = false;
    bool grabCarry_ = false;
    int grabIndex_ = -1;
    int grabFrom_ = -1;

    bool downActive_ = false;
    bool downMoved_ = false;
    bool downShift_ = false;
    bool effectPress_ = false;
    bool extInPress_ = false;
    bool knobDrag_ = false;
    bool knobDragMoved_ = false;
    bool knobSuppressSwitchStep_ = false;
    int knobDragIndex_ = -1;
    float knobDragStartY_ = 0.0f;
    float knobDragStartValue_ = 0.0f;
    int downJack_ = -1;
    float downX_ = 0.0f;
    float downY_ = 0.0f;
    float pointerX_ = 0.0f;
    float pointerY_ = 0.0f;
    bool pointerIn_ = false;
    int hoverJack_ = -1;
    int hoverLabel_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBayView)
};
