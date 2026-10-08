// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "UI/TabPages.h"

#include "Modular/EgLaw.h"
#include "Modular/PatchState.h"
#include "UI/KnobUnits.h"

#include <cmath>

namespace ronin_ui {

namespace {

constexpr float kW = 1600.0f;
constexpr float kH = 564.0f;
const juce::Colour kPlateTop (0xff1f1f21);
const juce::Colour kPlateBottom (0xff141416);
const juce::Colour kGold (0xffc29f4c);
const juce::Colour kLabel (0xffece8dc);
const juce::Colour kDim (0xff8f8a7c);
const juce::Colour kLcdBack (0xffb9c39c);
const juce::Colour kLcdInk (0xff1e2419);
const juce::Colour kStrip (0xff0c0c0d);

juce::String utf8 (const char* s) { return juce::String::fromUTF8 (s); }
juce::String str (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

float knob01 (RoninAudioProcessor& p, const char* section, const char* label)
{
    auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (p.parameterForPanelKnob (section, label));
    return parameter != nullptr ? parameter->getValue() : 0.0f;
}

juce::String jackName (const RoninAudioProcessor& p, int module, int port)
{
    const std::string id = patchstate::jackId (p.rackIndices(), module, port);
    return id.empty() ? juce::String ("?") : str (id);
}

}

const char* tabName (Tab tab) noexcept
{
    static const char* names[kTabCount] = { "MAIN", "VOICE", "ENV", "PATCH", "SETUP" };
    const int i = static_cast<int> (tab);
    return names[i >= 0 && i < kTabCount ? i : 0];
}

// ---- strip -----------------------------------------------------------------------------------

TabStrip::TabStrip()
{
    setOpaque (true);
}

juce::Rectangle<float> TabStrip::tabBounds (Tab tab) const
{
    const float s = static_cast<float> (getWidth()) / kW;
    const float x0 = (kW - kTabW * kTabCount) * 0.5f;
    const float i = static_cast<float> (static_cast<int> (tab));
    return { (x0 + i * kTabW + 4.0f) * s, 6.0f * s, (kTabW - 8.0f) * s, (kStripH - 12.0f) * s };
}

void TabStrip::paint (juce::Graphics& g)
{
    const float s = static_cast<float> (getWidth()) / kW;
    g.fillAll (kStrip);
    g.setColour (kDim);
    g.setFont (juce::Font (juce::FontOptions (12.0f * s)).boldened());
    g.drawText ("RONIN", juce::Rectangle<float> (14.0f * s, 0.0f, 200.0f * s, static_cast<float> (getHeight())),
                juce::Justification::centredLeft);
    for (int i = 0; i < kTabCount; ++i)
    {
        const auto tab = static_cast<Tab> (i);
        const auto r = tabBounds (tab);
        const bool on = tab == current_;
        g.setColour (on ? kGold : juce::Colour (0xff1c1c1f));
        g.fillRoundedRectangle (r, 3.0f * s);
        g.setColour (on ? kGold.brighter (0.3f) : juce::Colour (0xff3c3c3f));
        g.drawRoundedRectangle (r, 3.0f * s, 1.0f);
        g.setColour (on ? juce::Colour (0xff1a1508) : kLabel);
        g.setFont (juce::Font (juce::FontOptions (11.5f * s)).boldened());
        g.drawText (tabName (tab), r, juce::Justification::centred);
    }
}

void TabStrip::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < kTabCount; ++i)
    {
        if (tabBounds (static_cast<Tab> (i)).contains (e.position))
        {
            setCurrent (static_cast<Tab> (i));
            return;
        }
    }
}

void TabStrip::setCurrent (Tab tab)
{
    if (tab == current_)
        return;
    current_ = tab;
    repaint();
    if (onChange)
        onChange (tab);
}

// ---- page base -------------------------------------------------------------------------------

Page::Page (RoninAudioProcessor& p) : processor (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

Page::~Page()
{
    stopTimer();
}

void Page::visibilityChanged()
{
    if (isVisible())
        startTimerHz (20);
    else
        stopTimer();
}

void Page::timerCallback()
{
    repaint();
}

float Page::scale() const noexcept
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return 1.0f;
    return juce::jmin (static_cast<float> (getWidth()) / kW, static_cast<float> (getHeight()) / kH);
}

juce::Point<float> Page::origin() const noexcept
{
    const float s = scale();
    return { (static_cast<float> (getWidth()) - kW * s) * 0.5f, (static_cast<float> (getHeight()) - kH * s) * 0.5f };
}

juce::Rectangle<float> Page::toLocal (juce::Rectangle<float> d) const noexcept
{
    const float s = scale();
    const auto o = origin();
    return { o.x + d.getX() * s, o.y + d.getY() * s, d.getWidth() * s, d.getHeight() * s };
}

void Page::addRegion (juce::Rectangle<float> design, std::function<void (bool)> action)
{
    regions_.push_back ({ design, std::move (action) });
}

void Page::paint (juce::Graphics& g)
{
    regions_.clear();
    g.fillAll (juce::Colour (0xff101012));
    const auto plate = toLocal ({ 0.0f, 0.0f, kW, kH });
    g.setGradientFill (juce::ColourGradient (kPlateTop, plate.getX(), plate.getY(), kPlateBottom, plate.getX(),
                                             plate.getBottom(), false));
    g.fillRect (plate);
    // Corner screws, as on the panel.
    g.setColour (juce::Colour (0xff6a6a66));
    for (auto c : { juce::Point<float> (10, 10), juce::Point<float> (1590, 10), juce::Point<float> (10, 554),
                    juce::Point<float> (1590, 554) })
    {
        const auto r = toLocal ({ c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f });
        g.fillEllipse (r);
    }
    paintPage (g);
}

bool Page::clickDesign (float x, float y, bool right)
{
    // Regions are rebuilt by paint; make sure they exist for a click that arrives before the first paint.
    if (regions_.empty() && getWidth() > 0)
    {
        juce::Image scratch (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
        juce::Graphics g (scratch);
        paint (g);
    }
    for (auto it = regions_.rbegin(); it != regions_.rend(); ++it)
    {
        if (it->design.contains (x, y))
        {
            auto action = it->action;
            action (right);
            repaint();
            return true;
        }
    }
    return false;
}

void Page::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const float s = scale();
    const auto o = origin();
    clickDesign ((e.position.x - o.x) / s, (e.position.y - o.y) / s, e.mods.isPopupMenu());
}

void Page::block (juce::Graphics& g, juce::Rectangle<float> d, const juce::String& title)
{
    const float s = scale();
    g.setColour (kGold);
    g.drawRect (toLocal (d), juce::jmax (1.0f, 1.2f * s));
    text (g, { d.getX(), d.getY() + 6.0f, d.getWidth(), 18.0f }, title, 12.0f, kLabel);
}

void Page::text (juce::Graphics& g, juce::Rectangle<float> d, const juce::String& s, float size, juce::Colour colour,
                 juce::Justification just)
{
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (size * scale())).boldened());
    g.drawFittedText (s, toLocal (d).toNearestInt(), just, 2, 0.8f);
}

void Page::lcd (juce::Graphics& g, juce::Rectangle<float> d, const juce::String& s, float size)
{
    const auto r = toLocal (d);
    g.setColour (kLcdBack);
    g.fillRect (r);
    g.setColour (juce::Colour (0xff0a0a0b));
    g.drawRect (r, juce::jmax (1.0f, scale()));
    g.setColour (kLcdInk);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size * scale(), juce::Font::bold)));
    g.drawFittedText (s, r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void Page::toggle (juce::Graphics& g, juce::Rectangle<float> d, const juce::StringArray& names, int selected,
                   std::function<void (int)> choose)
{
    const int n = names.size();
    const float w = d.getWidth() / static_cast<float> (juce::jmax (1, n));
    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> cell (d.getX() + w * static_cast<float> (i), d.getY(), w, d.getHeight());
        const auto r = toLocal (cell);
        const bool on = i == selected;
        g.setColour (on ? kLabel : juce::Colour (0xff050506));
        g.fillRect (r);
        g.setColour (juce::Colour (0xff3c3c3f));
        g.drawRect (r, 1.0f);
        g.setColour (on ? juce::Colour (0xff101010) : kLabel);
        g.setFont (juce::Font (juce::FontOptions (10.5f * scale())).boldened());
        g.drawFittedText (names[i], r.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        addRegion (cell, [choose, i] (bool) { choose (i); });
    }
}

void Page::lamp (juce::Graphics& g, juce::Point<float> c, bool lit, juce::Colour colour)
{
    const auto r = toLocal ({ c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f });
    if (lit)
    {
        g.setColour (colour.withAlpha (0.35f));
        g.fillEllipse (r.expanded (3.0f * scale()));
    }
    g.setColour (lit ? colour : colour.withBrightness (0.18f));
    g.fillEllipse (r);
}

// ---- cables: role and colour -----------------------------------------------------------------

jcs::Role cableRole (const RoninAudioProcessor& p, const Cable& c)
{
    return portRole (p.portDesc (c.sourceModule, c.sourcePort));
}

const char* overrideName (int index) noexcept
{
    static const char* names[kOverrideCount] = { "By role", "Red", "Orange", "Yellow", "Green", "Blue", "Purple", "White" };
    return names[index >= 0 && index < kOverrideCount ? index : 0];
}

std::uint32_t overrideArgb (int index) noexcept
{
    static const std::uint32_t argb[kOverrideCount] = { 0u, 0xffd23a2e, 0xffe58a2f, 0xffe8d24a, 0xff3fae4f,
                                                        0xff3f73d6, 0xff9a4fd0, 0xffe8e4d8 };
    return argb[index >= 0 && index < kOverrideCount ? index : 0];
}

juce::Colour cableColour (const RoninAudioProcessor& p, const Cable& c)
{
    if (c.colour != 0)
        return juce::Colour (c.colour);
    return juce::Colour (jcs::roleArgb (cableRole (p, c)));
}

// ---- VOICE -----------------------------------------------------------------------------------

void VoicePage::paintPage (juce::Graphics& g)
{
    const juce::Rectangle<float> vco (40, 40, 480, 484), quality (560, 40, 480, 484), vcf (1080, 40, 480, 484);
    block (g, vco, "VCO");
    block (g, quality, "QUALITY");
    block (g, vcf, "VCF");

    // VCO: tuner, TRI SHAPE, footage reference.
    const auto& osc = processor.vcoModule();
    const double hz = static_cast<double> (osc.lastHz());
    lcd (g, { 140, 90, 280, 40 }, str (knobunits::noteName (hz)), 20.0f);
    lcd (g, { 180, 140, 200, 26 }, juce::String (hz, 2) + " Hz", 13.0f);
    text (g, { 60, 170, 440, 18 }, "TUNER (HZ/V LIN or V/OCT path)", 10.0f, kDim);

    text (g, { 60, 220, 440, 18 }, "TRI SHAPE", 11.0f, kLabel);
    auto* shape = processor.triShapeParameter();
    toggle (g, { 160, 244, 240, 26 }, { "TRIANGLE", "PARABOLA (legacy)" }, shape != nullptr ? shape->getIndex() : 0,
            [shape] (int i) { if (shape != nullptr) shape->setValueNotifyingHost (static_cast<float> (i)); });
    text (g, { 60, 274, 440, 30 }, utf8 ("new patches: TRIANGLE (anti-aliased)  \xc2\xb7  PARABOLA: legacy, for older saved patches"),
          9.5f, kDim);

    text (g, { 60, 330, 440, 18 }, "FOOTAGE REFERENCE", 11.0f, kLabel);
    const char* feet[4] = { "32'", "16'", "8'", "4'" };
    const char* cs[4] = { "C1", "C2", "C3", "C4" };
    for (int i = 0; i < 4; ++i)
    {
        const float x = 130.0f + static_cast<float> (i) * 100.0f;
        lamp (g, { x, 372.0f }, osc.activeScaleIndex() == i, juce::Colour (0xff6fe36f));
        text (g, { x - 40.0f, 386.0f, 80.0f, 16.0f }, feet[i], 12.0f, kLabel);
        text (g, { x - 40.0f, 402.0f, 80.0f, 14.0f }, cs[i], 10.0f, kDim);
    }
    text (g, { 60, 440, 440, 30 }, "which C plays at HZ/V LIN 1 V and at V/OCT 0 V", 9.5f, kDim);

    // QUALITY: HQ 2x, latency, CPU.
    text (g, { 580, 90, 440, 18 }, utf8 ("HQ 2\xc3\x97 (VCO + VCF)  \xc2\xb7  default OFF"), 11.0f, kLabel);
    auto* hq = processor.hqParameter();
    toggle (g, { 720, 114, 160, 26 }, { "OFF", "ON" }, hq != nullptr && hq->get() ? 1 : 0,
            [hq] (int i) { if (hq != nullptr) hq->setValueNotifyingHost (static_cast<float> (i)); });
    lcd (g, { 680, 170, 240, 30 }, "latency " + juce::String (processor.getLatencySamples()) + " smp", 14.0f);
    text (g, { 580, 206, 440, 30 }, "ON runs the engine at 2x and reports 23 samples of latency to the host", 9.5f, kDim);
    lcd (g, { 680, 260, 240, 30 }, "CPU " + juce::String (processor.cpuPercent(), 1) + " %", 14.0f);

    // VCF: drive pull (fixed) and live effective cutoff.
    const auto& filter = processor.vcfModule();
    text (g, { 1100, 90, 440, 18 }, "DRIVE PULL", 11.0f, kLabel);
    lcd (g, { 1200, 114, 240, 30 }, juce::String (Vcf::kInputPull, 3) + " (fixed)", 14.0f);
    const double newOct = std::log2 (Vcf::effectiveHzFor (1000.0, 2.5, Vcf::kInputPull) / 1000.0);
    text (g, { 1100, 150, 440, 30 }, "a 2.5 V input pulls the cutoff down " + juce::String (-newOct, 2) + " octave",
          9.5f, kDim);
    lcd (g, { 1200, 220, 240, 30 }, "CUTOFF " + str (knobunits::hz (static_cast<double> (filter.effectiveHz()))), 14.0f);
    text (g, { 1100, 256, 440, 30 }, "knob + CV + drive pull, live; sample-rate independent", 9.5f, kDim);
}

// ---- ENV -------------------------------------------------------------------------------------

namespace {

// Attack x += (6-x)a until 5 V; decay/release toward the target with c = ln 100 (EgLaw). Normalised curve.
float attackShape (float t)   // t in 0..1 of the attack time
{
    return static_cast<float> (6.0 * (1.0 - std::pow (1.0 / 6.0, t)) / 5.0);
}

float fallShape (float t)
{
    return static_cast<float> (std::exp (-std::log (100.0) * static_cast<double> (t)));
}

}

void EnvPage::paintPage (juce::Graphics& g)
{
    paintEg1 (g, { 40, 40, 740, 484 });
    paintEg2 (g, { 820, 40, 740, 484 });
}

void EnvPage::paintEg1 (juce::Graphics& g, juce::Rectangle<float> a)
{
    block (g, a, "EG 1");
    const auto& eg = processor.eg1Module();
    const bool ms = processor.egTimeInMs();
    lamp (g, { a.getCentreX() - 60.0f, a.getY() + 44.0f }, eg.triggerHeld(), juce::Colour (jcs::roleArgb (jcs::Role::STrig)));
    text (g, { a.getCentreX() - 46.0f, a.getY() + 36.0f, 140.0f, 16.0f }, "S-TRIG HELD", 10.5f, kLabel,
          juce::Justification::centredLeft);
    const char* labels[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
    const Eg1::Stage stages[4] = { Eg1::Stage::Attack, Eg1::Stage::Decay, Eg1::Stage::Sustain, Eg1::Stage::Release };
    for (int i = 0; i < 4; ++i)
    {
        const float x = a.getX() + 92.0f + static_cast<float> (i) * 185.0f;
        lamp (g, { x, a.getY() + 92.0f }, eg.stage() == stages[i], juce::Colour (0xff6fe36f));
        text (g, { x - 80.0f, a.getY() + 104.0f, 160.0f, 16.0f }, labels[i], 10.5f, kLabel);
        lcd (g, { x - 70.0f, a.getY() + 126.0f, 140.0f, 26.0f },
             str (knobunits::realUnits ("EG 1", labels[i], knob01 (processor, "EG 1", labels[i]), ms)), 13.0f);
    }

    // Live curve: segment widths follow the real times (log-compressed so short segments stay visible).
    const double at = EgLaw::secondsFor (static_cast<double> (knob01 (processor, "EG 1", "ATTACK")));
    const double dt = EgLaw::secondsFor (static_cast<double> (knob01 (processor, "EG 1", "DECAY")));
    const double rt = EgLaw::secondsFor (static_cast<double> (knob01 (processor, "EG 1", "RELEASE")));
    const float sus = knob01 (processor, "EG 1", "SUSTAIN");
    auto width = [] (double s) { return static_cast<float> (1.0 + std::log10 (1.0 + s * 1000.0)); };
    const float wa = width (at), wd = width (dt), ws = 3.0f, wr = width (rt);
    const float total = wa + wd + ws + wr;
    const juce::Rectangle<float> box (a.getX() + 50.0f, a.getY() + 190.0f, a.getWidth() - 100.0f, 230.0f);
    g.setColour (juce::Colour (0xff050506));
    g.fillRect (toLocal (box));
    juce::Path curve;
    const int steps = 240;
    for (int i = 0; i <= steps; ++i)
    {
        const float u = static_cast<float> (i) / steps * total;
        float y;
        if (u < wa)
            y = attackShape (u / wa);
        else if (u < wa + wd)
            y = sus + (1.0f - sus) * fallShape ((u - wa) / wd);
        else if (u < wa + wd + ws)
            y = sus;
        else
            y = sus * fallShape ((u - wa - wd - ws) / wr);
        const auto p = toLocal ({ box.getX() + box.getWidth() * u / total, box.getBottom() - 10.0f - (box.getHeight() - 20.0f) * y, 0, 0 });
        if (i == 0)
            curve.startNewSubPath (p.getX(), p.getY());
        else
            curve.lineTo (p.getX(), p.getY());
    }
    g.setColour (kGold);
    g.strokePath (curve, juce::PathStrokeType (1.6f * scale()));
    // Live level marker from EG 1 OUT A.
    const float level = processor.jackVolts (processor.eg1GraphIndex(), Eg1::kOutA) / 5.0f;
    const auto mark = toLocal ({ box.getX(), box.getBottom() - 10.0f - (box.getHeight() - 20.0f) * juce::jlimit (0.0f, 1.2f, level), box.getWidth(), 1.0f });
    g.setColour (juce::Colour (0x886fe36f));
    g.fillRect (mark);
    text (g, { a.getX(), a.getBottom() - 46.0f, a.getWidth(), 16.0f },
          utf8 ("real seconds (knob 1 ms \xe2\x80\xa6 60 s); labels match what you hear"), 9.5f, kDim);
}

void EnvPage::paintEg2 (juce::Graphics& g, juce::Rectangle<float> a)
{
    block (g, a, "EG 2");
    const auto& eg = processor.eg2Module();
    const bool ms = processor.egTimeInMs();
    lamp (g, { a.getCentreX() - 60.0f, a.getY() + 44.0f }, eg.triggerHeld(), juce::Colour (jcs::roleArgb (jcs::Role::STrig)));
    text (g, { a.getCentreX() - 46.0f, a.getY() + 36.0f, 140.0f, 16.0f }, "S-TRIG HELD", 10.5f, kLabel,
          juce::Justification::centredLeft);
    const char* labels[4] = { "HOLD", "DELAY", "ATTACK", "RELEASE" };
    const auto stage = eg.stage();
    const bool lit[4] = { stage == Eg2::Stage::Wait, stage == Eg2::Stage::Wait, stage == Eg2::Stage::Attack,
                          stage == Eg2::Stage::Release };
    for (int i = 0; i < 4; ++i)
    {
        const float x = a.getX() + 92.0f + static_cast<float> (i) * 185.0f;
        lamp (g, { x, a.getY() + 92.0f }, lit[i], juce::Colour (0xff6fe36f));
        text (g, { x - 80.0f, a.getY() + 104.0f, 160.0f, 16.0f }, labels[i], 10.5f, kLabel);
        lcd (g, { x - 70.0f, a.getY() + 126.0f, 140.0f, 26.0f },
             str (knobunits::realUnits ("EG 2", labels[i], knob01 (processor, "EG 2", labels[i]), ms)), 13.0f);
    }

    const double ht = EgLaw::holdDelaySeconds (static_cast<double> (knob01 (processor, "EG 2", "HOLD")));
    const double dl = EgLaw::holdDelaySeconds (static_cast<double> (knob01 (processor, "EG 2", "DELAY")));
    const double at = EgLaw::secondsFor (static_cast<double> (knob01 (processor, "EG 2", "ATTACK")));
    const double rt = EgLaw::secondsFor (static_cast<double> (knob01 (processor, "EG 2", "RELEASE")));
    auto width = [] (double s) { return static_cast<float> (1.0 + std::log10 (1.0 + s * 1000.0)); };
    const float ww = width (ht + dl), wa = width (at), wh = 3.0f, wr = width (rt);
    const float total = ww + wa + wh + wr;
    const juce::Rectangle<float> box (a.getX() + 50.0f, a.getY() + 190.0f, a.getWidth() - 100.0f, 230.0f);
    g.setColour (juce::Colour (0xff050506));
    g.fillRect (toLocal (box));
    juce::Path curve;
    const int steps = 240;
    for (int i = 0; i <= steps; ++i)
    {
        const float u = static_cast<float> (i) / steps * total;
        float y;
        if (u < ww)
            y = 0.0f;
        else if (u < ww + wa)
            y = attackShape ((u - ww) / wa);
        else if (u < ww + wa + wh)
            y = 1.0f;
        else
            y = fallShape ((u - ww - wa - wh) / wr);
        const auto p = toLocal ({ box.getX() + box.getWidth() * u / total, box.getBottom() - 10.0f - (box.getHeight() - 20.0f) * y, 0, 0 });
        if (i == 0)
            curve.startNewSubPath (p.getX(), p.getY());
        else
            curve.lineTo (p.getX(), p.getY());
    }
    g.setColour (kGold);
    g.strokePath (curve, juce::PathStrokeType (1.6f * scale()));
    const float level = processor.jackVolts (processor.eg2GraphIndex(), Eg2::kOutPos) / 5.0f;
    const auto mark = toLocal ({ box.getX(), box.getBottom() - 10.0f - (box.getHeight() - 20.0f) * juce::jlimit (0.0f, 1.2f, level), box.getWidth(), 1.0f });
    g.setColour (juce::Colour (0x886fe36f));
    g.fillRect (mark);
    text (g, { a.getX(), a.getBottom() - 46.0f, a.getWidth(), 16.0f },
          "HOLD then DELAY wait after TRIG (DELAY OUT fires a 5 V pulse at the end of HOLD)", 9.5f, kDim);
}

// ---- PATCH -----------------------------------------------------------------------------------

namespace {

juce::String flagFor (const RoninAudioProcessor& p, const Cable& c)
{
    const PortDesc from = p.portDesc (c.sourceModule, c.sourcePort);
    const PortDesc to = p.portDesc (c.destModule, c.destPort);
    juce::StringArray flags;
    if (c.legacyInvert)
        flags.add ("old inversion kept");
    // "gate converted to S-trig" appears exactly where the graph converts (PatchGraph::convertsToStrig).
    const auto badge = PatchGraph::cableBadge (from, to);
    if (badge != jcs::Badge::None)
        flags.add (utf8 (jcs::badgeText (badge)));
    if (c.colour != 0)
        flags.add ("colour override");
    return flags.joinIntoString ("  ");
}

}

int PatchPage::rowCount() const
{
    Cable cables[PatchGraph::kMaxCables];
    return processor.copyPublishedCables (cables, PatchGraph::kMaxCables);
}

juce::String PatchPage::rowText (int index) const
{
    Cable cables[PatchGraph::kMaxCables];
    const int n = processor.copyPublishedCables (cables, PatchGraph::kMaxCables);
    if (index < 0 || index >= n)
        return {};
    const Cable& c = cables[index];
    return jackName (processor, c.sourceModule, c.sourcePort) + " -> " + jackName (processor, c.destModule, c.destPort)
           + " | " + jcs::roleInfo (cableRole (processor, c)).name + " | " + flagFor (processor, c);
}

void PatchPage::paintPage (juce::Graphics& g)
{
    const juce::Rectangle<float> list (40, 40, 1060, 484), monitor (1140, 40, 420, 484);
    block (g, list, "CABLES");
    block (g, monitor, "JACK MONITOR");

    Cable cables[PatchGraph::kMaxCables];
    const int n = processor.copyPublishedCables (cables, PatchGraph::kMaxCables);
    if (selected_ >= n)
        selected_ = n - 1;
    const int visible = 11;
    scroll_ = juce::jlimit (0, juce::jmax (0, n - visible), scroll_);
    text (g, { 70, 60, 200, 16 }, "FROM", 9.5f, kDim, juce::Justification::centredLeft);
    text (g, { 400, 60, 200, 16 }, "TO", 9.5f, kDim, juce::Justification::centredLeft);
    text (g, { 690, 60, 200, 16 }, "TYPE", 9.5f, kDim, juce::Justification::centredLeft);
    text (g, { 830, 60, 200, 16 }, "FLAG", 9.5f, kDim, juce::Justification::centredLeft);
    for (int row = 0; row < visible && scroll_ + row < n; ++row)
    {
        const int i = scroll_ + row;
        const Cable& c = cables[i];
        const juce::Rectangle<float> r (60, 76.0f + static_cast<float> (row) * 36.0f, 1020, 30);
        g.setColour (i == selected_ ? juce::Colour (0xff3a3426) : juce::Colour (0xff0b0b0c));
        g.fillRect (toLocal (r));
        const auto role = cableRole (processor, c);
        const auto& info = jcs::roleInfo (role);
        const auto colour = cableColour (processor, c);
        const auto dot = toLocal ({ 72, r.getCentreY() - 6.0f, 12, 12 });
        g.setColour (colour);
        g.fillEllipse (dot);
        text (g, { 94, r.getY(), 300, 30 }, jackName (processor, c.sourceModule, c.sourcePort), 11.0f, kLabel,
              juce::Justification::centredLeft);
        text (g, { 380, r.getY(), 300, 30 }, utf8 ("\xe2\x86\x92 ") + jackName (processor, c.destModule, c.destPort),
              11.0f, kLabel, juce::Justification::centredLeft);
        text (g, { 690, r.getY(), 140, 30 }, utf8 (info.glyph) + " " + info.name, 11.0f,
              juce::Colour (jcs::argb (info.rgb)), juce::Justification::centredLeft);
        text (g, { 830, r.getY(), 245, 30 }, flagFor (processor, c), 9.5f, kDim, juce::Justification::centredLeft);
        addRegion (r, [this, i] (bool right) {
            selected_ = i;
            if (right)
                showMenu (i);
        });
    }
    if (n == 0)
        text (g, { 60, 200, 1020, 30 }, "No cables. Patch on the MAIN tab.", 12.0f, kDim);
    text (g, { 60, 486, 1020, 30 },
          utf8 ("click a row to monitor it \xc2\xb7 Del unplugs \xc2\xb7 right-click: colour override, convert, unplug "
                "\xc2\xb7 colour = source role"),
          9.5f, kDim);

    // Jack monitor: the selected cable's two jacks (the input shows its summed volts), and the host output.
    auto row = [&] (float y, const juce::String& name, jcs::Role role, const juce::String& value, bool over)
    {
        const auto ring = toLocal ({ 1170, y - 9.0f, 18, 18 });
        g.setColour (juce::Colour (jcs::roleArgb (role)));
        g.drawEllipse (ring, 2.5f * scale());
        text (g, { 1196, y - 12.0f, 180, 24 }, name, 10.5f, kLabel, juce::Justification::centredLeft);
        lcd (g, { 1380, y - 13.0f, 160, 26 }, value, 12.0f);
        lamp (g, { 1550, y }, over, juce::Colour (0xffff3b30));
    };
    if (selected_ >= 0 && selected_ < n)
    {
        const Cable& c = cables[selected_];
        const float vFrom = processor.jackVolts (c.sourceModule, c.sourcePort);
        const float vTo = processor.jackVolts (c.destModule, c.destPort);
        const PortDesc toDesc = processor.portDesc (c.destModule, c.destPort);
        row (110, jackName (processor, c.sourceModule, c.sourcePort), cableRole (processor, c),
             juce::String (vFrom, 2) + " V", processor.jackOverRange (c.sourceModule, c.sourcePort));
        juce::String toValue = juce::String (vTo, 2) + " V";
        if (toDesc.strigInput)
            toValue += vTo < 1.0f ? " held" : " rel";
        row (170, jackName (processor, c.destModule, c.destPort), portRole (toDesc), toValue, processor.jackOverRange (c.destModule, c.destPort));
    }
    else
    {
        text (g, { 1160, 120, 380, 30 }, "select a cable", 11.0f, kDim);
    }
    const float outL = processor.jackVolts (processor.outputGraphIndex(), 0) * OutputModule::kVoltsToHost;
    const juce::String db = std::fabs (outL) > 1.0e-6f ? juce::String (20.0 * static_cast<double> (std::log10 (std::fabs (outL))), 1) + " dBFS"
                                                       : juce::String ("-inf dBFS");
    row (230, "OUTPUT:L (dry in)", jcs::Role::Audio, db, std::fabs (outL) > 1.0f);
    text (g, { 1160, 486, 380, 30 }, "red lamp: over range, |V| > 5.5 V for over 10 ms", 9.5f, kDim);
}

void PatchPage::showMenu (int index)
{
    Cable cables[PatchGraph::kMaxCables];
    const int n = processor.copyPublishedCables (cables, PatchGraph::kMaxCables);
    if (index < 0 || index >= n)
        return;
    juce::PopupMenu menu;
    juce::PopupMenu colours;
    for (int i = 0; i < kOverrideCount; ++i)
        colours.addItem (100 + i, overrideName (i), true, overrideArgb (i) == cables[index].colour);
    menu.addSubMenu ("Cable colour", colours);
    if (cables[index].legacyInvert)
        menu.addItem (2, "Convert: drop the old S-trig inversion");
    menu.addItem (3, "Unplug");
    juce::Component::SafePointer<PatchPage> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options(), [safe, index] (int result) {
        if (safe == nullptr || result == 0)
            return;
        auto& p = safe->processor;
        Cable now[PatchGraph::kMaxCables];
        const int count = p.copyPublishedCables (now, PatchGraph::kMaxCables);
        if (index >= count)
            return;
        if (result >= 100)
            p.setCableColour (index, overrideArgb (result - 100));
        else if (result == 2)
            p.setCableLegacyInvert (index, false);
        else if (result == 3)
            p.disconnectJacks (now[index].sourceModule, now[index].sourcePort, now[index].destModule, now[index].destPort);
        if (safe->onCablesChanged)
            safe->onCablesChanged();
        safe->repaint();
    });
}

bool PatchPage::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        Cable cables[PatchGraph::kMaxCables];
        const int n = processor.copyPublishedCables (cables, PatchGraph::kMaxCables);
        if (selected_ >= 0 && selected_ < n)
        {
            const Cable& c = cables[selected_];
            processor.disconnectJacks (c.sourceModule, c.sourcePort, c.destModule, c.destPort);
            if (onCablesChanged)
                onCablesChanged();
            repaint();
            return true;
        }
    }
    return false;
}

void PatchPage::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scroll_ += wheel.deltaY < 0.0f ? 1 : -1;
    repaint();
}

// ---- SETUP -----------------------------------------------------------------------------------

void SetupPage::paintPage (juce::Graphics& g)
{
    const juce::Rectangle<float> ui (40, 40, 740, 484), report (820, 40, 740, 484);
    block (g, ui, "INTERFACE");
    block (g, report, "PATCH / MIGRATION");

    const int scales[5] = { 75, 100, 125, 150, 200 };
    text (g, { 60, 80, 700, 18 }, "UI SCALE", 11.0f, kLabel);
    juce::StringArray names;
    int selected = 1;
    for (int i = 0; i < 5; ++i)
    {
        names.add (juce::String (scales[i]) + " %");
        if (scales[i] == processor.uiScalePercent())
            selected = i;
    }
    toggle (g, { 110, 104, 600, 28 }, names, selected, [this, scales] (int i) {
        processor.setUiScalePercent (scales[i]);
        if (onScale)
            onScale (scales[i]);
    });
    if (auto* top = getParentComponent())
        lcd (g, { 260, 146, 300, 26 },
             juce::String (top->getWidth()) + utf8 (" \xc3\x97 ") + juce::String (top->getHeight()) + " (art + tabs)", 12.0f);

    text (g, { 60, 210, 300, 26 }, "CABLE COLOUR", 11.0f, kLabel, juce::Justification::centredLeft);
    toggle (g, { 420, 210, 260, 26 }, { "BY ROLE", "MANUAL" }, processor.cableColourByRole() ? 0 : 1, [this] (int i) {
        processor.setCableColourByRole (i == 0);
        if (onCableColourMode)
            onCableColourMode();
    });
    text (g, { 60, 240, 700, 30 },
          "BY ROLE: S-TRIG / AUDIO / V/OCT / GATE-CLK / HZ/V LIN / CV colours. MANUAL: the palette swatches.",
          9.5f, kDim, juce::Justification::centredLeft);
    text (g, { 60, 290, 300, 26 }, "EG TIME DISPLAY", 11.0f, kLabel, juce::Justification::centredLeft);
    toggle (g, { 420, 290, 260, 26 }, { "s", "ms" }, processor.egTimeInMs() ? 1 : 0,
            [this] (int i) { processor.setEgTimeInMs (i == 1); });

    // Migration report (M-R1 .. M-R5) of the last loaded patch.
    const auto& lines = processor.loadReport();
    juce::StringArray shown (lines);
    if (shown.isEmpty())
        shown.add (processor.loadedFormat() >= 2 ? "Format 2 patch: nothing to migrate."
                                                 : "New patch: nothing migrated.");
    for (int i = 0; i < shown.size() && i < 8; ++i)
        lcd (g, { 850, 80.0f + static_cast<float> (i) * 52.0f, 680, 42 }, shown[i], 10.5f);
    text (g, { 840, 500, 700, 18 }, "patch format " + juce::String (patchstate::kFormat) + ": knobs by id, cables by jack id",
          9.5f, kDim);
}

}
