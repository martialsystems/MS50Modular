// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "PluginProcessor.h"
#include "UI/ListControl.h"
#include "UI/PatchBayLogic.h"

#include <juce_gui_basics/juce_gui_basics.h>

class StackMenu;
struct CablePaint;

// Landscape panel and the patch-bay cable gestures.
// Graph edits run on the message thread through the processor.
class PatchBayView : public juce::Component,
                     private juce::Timer
{
public:
    explicit PatchBayView (RoninAudioProcessor&);
    ~PatchBayView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    // Editor performance: the animation timer runs only while the bay is on screen.
    bool animating() const { return isTimerRunning(); }
    // The area the last timer frame asked to repaint (tests); empty when nothing moved.
    juce::Rectangle<int> lastDirtyBounds() const noexcept { return lastDirty_; }
    void runFrame() { timerCallback(); }
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
    bool presetMenuOpen() const noexcept { return presetMenu_; }
    const juce::String& statusText() const noexcept { return status_; }
    float outputMix() const noexcept;
    int knobCount() const noexcept { return kPanelKnobCount; }
    float knobValue (int index) const;
    const juce::String& knobReadout() const noexcept { return knobReadout_; }
    // MAIN right-click on a knob -> type a value in real units (RONIN_Redesign §4.1). Transient overlay.
    bool valueEditorOpen() const noexcept { return valueEditor_ != nullptr; }
    juce::String valueEditorText() const;
    bool typeKnobValue (int knob, const juce::String& text);
    // The PATCH tab unplugged or edited a cable: redraw the bay from the published graph.
    void reloadCablesFromGraph() { reloadPublishedCables(); repaint(); }
    juce::String hoverTextForJack (int jack) const { return jack >= 0 && jack < kPanelJackCount ? jackHoverText (jack) : juce::String(); }
    bool panelLoaded() const noexcept { return panel_ != nullptr; }
    int cableNearDesign (float x, float y) const { return cableNear (x, y); }
    int plugsOnJack (int jack, int* out, int capacity) const;
    int visualColor (int index) const;
    int selectedColor() const noexcept { return currentColor_; }
    bool reorderStack (int jack, const int* bottomToTop, int n);
    void advanceCableFrame();

    // MAIN list controls (DIV RATIO SWITCH steps; PRESET opens its list), the same paths the mouse takes.
    std::vector<ronin_ui::ListControl> listControls() const;
    void switchClick (int knob, bool back);      // left-click (Shift = back) on a stepping switch
    void listRightClick (int control);           // right-click: the full list, current ticked

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
    void updateTimer();
    void paintBackdrop (juce::Graphics&);
    juce::Rectangle<int> designRectToLocal (float x, float y, float w, float h) const;
    juce::String frameSignature() const;
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
    int powerHalfAt (float x, float y) const;
    bool extInButtonAt (float x, float y) const;
    bool presetAt (float x, float y) const;
    int presetRowAt (float x, float y) const;
    void choosePreset (int index);
    void reloadPublishedCables();
    int swatchAt (float x, float y) const;
    CablePaint cablePaintFor (int index, const Cable* published, int publishedCount) const;
    bool jackGraphPort (int jack, int& module, int& port) const;
    juce::String jackHoverText (int jack) const;
    juce::String knobText (int index) const;
    void openValueEditor (int knob);
    void closeValueEditor();
    juce::AudioProcessorParameter* parameterForKnob (int index) const;
    void endGesture();
    void syncHostKnobs();
    void showProgramKnobs();
    void selectMeter (int jack);
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

    RoninAudioProcessor& audioProcessor;
    std::unique_ptr<juce::Drawable> panel_;
    std::unique_ptr<StackMenu> menu_;
    std::unique_ptr<juce::TextEditor> valueEditor_;
    int valueKnob_ = -1;
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
    bool presetMenu_ = false;
    int presetHi_ = 0;
    bool knobDrag_ = false;
    bool knobDragMoved_ = false;
    bool knobSuppressSwitchStep_ = false;
    bool knobShift_ = false;   // Shift held on a switch click: step back
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
    int shownProgram_ = kDefaultFactoryPreset;

    // Static layer (plate fill + panel art) cached at the physical pixel size; redrawn only on resize.
    juce::Image backdrop_;
    float backdropScale_ = 0.0f;
    // What the last frame showed, so the timer repaints only what changed.
    float prevNodes_[kPatchBayMaxCables][kNodes][2] {};   // rope nodes as last painted
    float paintedKnob_[kPanelKnobCount] {};
    float paintedNeedle_ = -9.0f;
    juce::String paintedSignature_;
    juce::Rectangle<int> lastDirty_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchBayView)
};
