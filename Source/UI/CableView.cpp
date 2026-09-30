// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "CableView.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDashOn = 12.0f;
constexpr float kDashOff = 10.0f;
constexpr float kDashPeriod = kDashOn + kDashOff;
constexpr float kDashAdvance = 6.0f;
constexpr float kBodyThickness = 5.2f;
constexpr float kCoreThickness = 1.8f;
constexpr int kCurveSteps = 28;
constexpr float kHitRadius = 8.0f;

int typeSlot (PortType type)
{
    if (type == PortType::CV)
        return 1;
    if (type == PortType::Gate)
        return 2;
    return 0;
}

juce::Colour bodyColour (PortType type)
{
    if (type == PortType::CV)
        return juce::Colour (0xff1d4eaa);
    if (type == PortType::Gate)
        return juce::Colour (0xffbdbdbd);
    return juce::Colour (0xffa87418);
}

juce::Colour coreColour (PortType type)
{
    if (type == PortType::CV)
        return juce::Colour (0xff9ec2ff);
    if (type == PortType::Gate)
        return juce::Colours::white;
    return juce::Colour (0xffffe08a);
}

}

bool CableView::sameCubic (const Cubic& a, const Cubic& b)
{
    const float tol = 0.01f;
    return std::abs (a.x0 - b.x0) < tol
        && std::abs (a.y0 - b.y0) < tol
        && std::abs (a.c1x - b.c1x) < tol
        && std::abs (a.c1y - b.c1y) < tol
        && std::abs (a.c2x - b.c2x) < tol
        && std::abs (a.c2y - b.c2y) < tol
        && std::abs (a.x1 - b.x1) < tol
        && std::abs (a.y1 - b.y1) < tol
        && a.type == b.type
        && a.sourceModule == b.sourceModule
        && a.sourcePort == b.sourcePort
        && a.destModule == b.destModule
        && a.destPort == b.destPort;
}

juce::Point<float> CableView::pointOnCubic (const Cubic& cubic, float t)
{
    const float u = 1.0f - t;
    const float uu = u * u;
    const float tt = t * t;
    const float uuu = uu * u;
    const float ttt = tt * t;
    const float x = (uuu * cubic.x0)
                  + (3.0f * uu * t * cubic.c1x)
                  + (3.0f * u * tt * cubic.c2x)
                  + (ttt * cubic.x1);
    const float y = (uuu * cubic.y0)
                  + (3.0f * uu * t * cubic.c1y)
                  + (3.0f * u * tt * cubic.c2y)
                  + (ttt * cubic.y1);
    return { x, y };
}

float CableView::distanceToCubic (const Cubic& cubic, juce::Point<float> point)
{
    float best = 1.0e9f;
    for (int step = 0; step <= kCurveSteps; ++step)
    {
        const auto onCurve = pointOnCubic (cubic, static_cast<float> (step) / static_cast<float> (kCurveSteps));
        best = std::min (best, point.getDistanceFrom (onCurve));
    }
    return best;
}

CableView::CableView (CableJackLookup& lookup, MS50ModularAudioProcessor& audioProcessor)
    : jacks (lookup),
      processor (audioProcessor)
{
    setInterceptsMouseClicks (false, false);
    setOpaque (false);
    for (auto& curve : curves_)
        curve.preallocateSpace (PatchGraph::kMaxCables * 12);
    rubber_.preallocateSpace (48);
    startTimerHz (30);
}

CableView::~CableView()
{
    stopTimer();
}

bool CableView::drawnCable (int index, float& x0, float& y0, float& x1, float& y1, PortType& type) const noexcept
{
    if (index < 0 || index >= cubicCount_)
        return false;
    const Cubic& cubic = cubics_[index];
    x0 = cubic.x0;
    y0 = cubic.y0;
    x1 = cubic.x1;
    y1 = cubic.y1;
    type = cubic.type;
    return true;
}

void CableView::setRubberBand (juce::Point<float> from, juce::Point<float> to)
{
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float dist = std::sqrt (dx * dx + dy * dy);
    const float sag = std::max (14.0f, dist * 0.22f);
    rubber_.clear();
    rubber_.startNewSubPath (from.x, from.y);
    rubber_.cubicTo (from.x + dx * 0.33f, from.y + dy * 0.33f + sag,
                     from.x + dx * 0.66f, from.y + dy * 0.66f + sag,
                     to.x, to.y);
    rubberVisible_ = true;
    repaint();
}

void CableView::clearRubberBand()
{
    rubber_.clear();
    rubberVisible_ = false;
    repaint();
}

bool CableView::cableAt (juce::Point<float> point, int& sourceModule, int& sourcePort, int& destModule, int& destPort)
{
    refreshGeometry();
    int best = -1;
    float bestDist = kHitRadius;
    for (int i = 0; i < cubicCount_; ++i)
    {
        const float dist = distanceToCubic (cubics_[i], point);
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }
    if (best < 0)
        return false;

    sourceModule = cubics_[best].sourceModule;
    sourcePort = cubics_[best].sourcePort;
    destModule = cubics_[best].destModule;
    destPort = cubics_[best].destPort;
    return true;
}

void CableView::timerCallback()
{
    dashPhase_ += kDashAdvance;
    if (dashPhase_ >= 100000.0f)
        dashPhase_ = std::fmod (dashPhase_, kDashPeriod);
    repaint();
}

void CableView::refreshGeometry()
{
    Cable published[PatchGraph::kMaxCables];
    const int publishedCount = processor.copyPublishedCables (published, PatchGraph::kMaxCables);

    Cubic next[PatchGraph::kMaxCables];
    int count = 0;
    for (int i = 0; i < publishedCount; ++i)
    {
        const CableEnd source = jacks.jackCentre (published[i].sourceModule, published[i].sourcePort);
        const CableEnd dest = jacks.jackCentre (published[i].destModule, published[i].destPort);
        if (! source.found || ! dest.found)
            continue;

        Cubic& cubic = next[count];
        cubic.x0 = source.x;
        cubic.y0 = source.y;
        cubic.x1 = dest.x;
        cubic.y1 = dest.y;
        const float dx = dest.x - source.x;
        const float dy = dest.y - source.y;
        const float dist = std::sqrt (dx * dx + dy * dy);
        const float sag = std::max (14.0f, dist * 0.22f);
        cubic.c1x = source.x + dx * 0.33f;
        cubic.c1y = source.y + dy * 0.33f + sag;
        cubic.c2x = source.x + dx * 0.66f;
        cubic.c2y = source.y + dy * 0.66f + sag;
        cubic.type = source.type;
        cubic.sourceModule = published[i].sourceModule;
        cubic.sourcePort = published[i].sourcePort;
        cubic.destModule = published[i].destModule;
        cubic.destPort = published[i].destPort;
        ++count;
    }

    bool unchanged = count == cubicCount_;
    for (int i = 0; unchanged && i < count; ++i)
        unchanged = sameCubic (cubics_[i], next[i]);
    if (unchanged)
        return;

    cubicCount_ = count;
    for (int i = 0; i < count; ++i)
        cubics_[i] = next[i];
    rebuildPaths();
}

void CableView::rebuildPaths()
{
    for (auto& curve : curves_)
        curve.clear();

    for (int i = 0; i < cubicCount_; ++i)
    {
        juce::Path& curve = curves_[typeSlot (cubics_[i].type)];
        curve.startNewSubPath (cubics_[i].x0, cubics_[i].y0);
        curve.cubicTo (cubics_[i].c1x, cubics_[i].c1y,
                       cubics_[i].c2x, cubics_[i].c2y,
                       cubics_[i].x1, cubics_[i].y1);
    }
    ++rebuilds_;
}

void CableView::strokeBody (juce::Graphics& g) const
{
    const juce::PathStrokeType stroke (kBodyThickness,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);
    const PortType types[3] = { PortType::Audio, PortType::CV, PortType::Gate };
    for (int i = 0; i < 3; ++i)
    {
        if (curves_[i].isEmpty())
            continue;
        g.setColour (bodyColour (types[i]));
        g.strokePath (curves_[i], stroke);
    }
}

void CableView::strokeDashes (juce::Graphics& g) const
{
    const float phase = std::fmod (dashPhase_, kDashPeriod);

    for (int cable = 0; cable < cubicCount_; ++cable)
    {
        juce::Point<float> points[kCurveSteps + 1];
        for (int step = 0; step <= kCurveSteps; ++step)
            points[step] = pointOnCubic (cubics_[cable], static_cast<float> (step) / static_cast<float> (kCurveSteps));

        float travelled = 0.0f;
        g.setColour (coreColour (cubics_[cable].type));
        for (int step = 0; step < kCurveSteps; ++step)
        {
            const float len = points[step].getDistanceFrom (points[step + 1]);
            if (len <= 0.001f)
            {
                travelled += len;
                continue;
            }

            float local = 0.0f;
            int guard = 0;
            while (local < len && guard < 64)
            {
                ++guard;
                const float along = travelled + local + phase;
                const float into = std::fmod (along, kDashPeriod);
                const bool solid = into < kDashOn;
                const float remain = solid ? (kDashOn - into) : (kDashPeriod - into);
                const float piece = std::min (remain, len - local);
                if (piece <= 0.0001f)
                    break;
                if (solid)
                {
                    const float t0 = local / len;
                    const float t1 = (local + piece) / len;
                    const float x0 = points[step].x + (points[step + 1].x - points[step].x) * t0;
                    const float y0 = points[step].y + (points[step + 1].y - points[step].y) * t0;
                    const float x1 = points[step].x + (points[step + 1].x - points[step].x) * t1;
                    const float y1 = points[step].y + (points[step + 1].y - points[step].y) * t1;
                    g.drawLine (x0, y0, x1, y1, kCoreThickness);
                }
                local += piece;
            }
            travelled += len;
        }
    }
}

void CableView::paint (juce::Graphics& g)
{
    refreshGeometry();
    strokeBody (g);
    strokeDashes (g);
    if (rubberVisible_ && ! rubber_.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (rubber_, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
