// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "RackView.h"
#include "JackView.h"
#include "PluginProcessor.h"

#include <cstring>
#include <iterator>

namespace {

struct JackSpec {
    const char* name;
    PortType type;
    PortDir dir;
};

struct PlateSpec {
    const char* name;
    const JackSpec* jacks;
    int count;
};

constexpr JackSpec kExtIn[] = {
    { "L", PortType::Audio, PortDir::Out },
    { "R", PortType::Audio, PortDir::Out },
    { "Mono", PortType::Audio, PortDir::Out },
    { "Gate", PortType::Gate, PortDir::Out },
};

constexpr JackSpec kVco[] = {
    { "Hz/V", PortType::CV, PortDir::In },
    { "Oct/V", PortType::CV, PortDir::In },
    { "FreqA", PortType::CV, PortDir::In },
    { "FreqB", PortType::CV, PortDir::In },
    { "PWM", PortType::CV, PortDir::In },
    { "Saw", PortType::Audio, PortDir::Out },
    { "Tri", PortType::Audio, PortDir::Out },
    { "Pulse", PortType::Audio, PortDir::Out },
};

constexpr JackSpec kVcf[] = {
    { "SigIn", PortType::Audio, PortDir::In },
    { "Cutoff", PortType::CV, PortDir::In },
    { "SigOut", PortType::Audio, PortDir::Out },
};

constexpr JackSpec kVca1[] = {
    { "SigIn", PortType::Audio, PortDir::In },
    { "Env", PortType::CV, PortDir::In },
    { "Out", PortType::Audio, PortDir::Out },
};

constexpr JackSpec kVca2[] = {
    { "In", PortType::CV, PortDir::In },
    { "Control", PortType::CV, PortDir::In },
    { "Out", PortType::CV, PortDir::Out },
};

constexpr JackSpec kOutput[] = {
    { "L", PortType::Audio, PortDir::In },
    { "R", PortType::Audio, PortDir::In },
    { "Wet", PortType::Audio, PortDir::In },
};

constexpr JackSpec kMg[] = {
    { "FreqMod", PortType::CV, PortDir::In },
    { "PWM", PortType::CV, PortDir::In },
    { "Tri", PortType::Audio, PortDir::Out },
    { "SawUp", PortType::Audio, PortDir::Out },
    { "SawDown", PortType::Audio, PortDir::Out },
    { "Pulse", PortType::Audio, PortDir::Out },
};

constexpr JackSpec kEg1[] = {
    { "Trig", PortType::CV, PortDir::In },
    { "OutA", PortType::CV, PortDir::Out },
    { "OutB", PortType::CV, PortDir::Out },
    { "OutC", PortType::CV, PortDir::Out },
};

constexpr JackSpec kEg2[] = {
    { "Trig", PortType::CV, PortDir::In },
    { "OutPos", PortType::CV, PortDir::Out },
    { "OutNeg", PortType::CV, PortDir::Out },
    { "DelayTrig", PortType::Gate, PortDir::Out },
};

constexpr JackSpec kNoise[] = {
    { "White", PortType::Audio, PortDir::Out },
    { "Pink", PortType::Audio, PortDir::Out },
};

constexpr JackSpec kRing[] = {
    { "A", PortType::CV, PortDir::In },
    { "B", PortType::CV, PortDir::In },
    { "Out", PortType::CV, PortDir::Out },
};

constexpr JackSpec kDivider[] = {
    { "In", PortType::CV, PortDir::In },
    { "Div2", PortType::CV, PortDir::Out },
    { "Div4", PortType::CV, PortDir::Out },
};

constexpr JackSpec kInverter[] = {
    { "In", PortType::CV, PortDir::In },
    { "Out", PortType::CV, PortDir::Out },
};

constexpr JackSpec kIntegrator[] = {
    { "In", PortType::CV, PortDir::In },
    { "Out", PortType::CV, PortDir::Out },
};

// Faceplate order. These slots are not PatchGraph indices.
constexpr int kExtInSlot = 0;
constexpr int kOutputSlot = 5;
constexpr int kNoiseSlot = 9;

constexpr PlateSpec kPlates[] = {
    { "Ext In", kExtIn, 4 },
    { "VCO", kVco, 8 },
    { "VCF", kVcf, 3 },
    { "VCA 1", kVca1, 3 },
    { "VCA 2", kVca2, 3 },
    { "Output", kOutput, 3 },
    { "MG", kMg, 6 },
    { "EG 1", kEg1, 4 },
    { "EG 2", kEg2, 4 },
    { "Noise", kNoise, 2 },
    { "Ring", kRing, 3 },
    { "Divider", kDivider, 3 },
    { "Inverter", kInverter, 2 },
    { "Integrator", kIntegrator, 2 },
};

}

struct RackView::Faceplate : public juce::Component
{
    juce::String title;
    juce::OwnedArray<JackView> jacks;

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (juce::Colour (0xff5a5a5a));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawFittedText (title, getLocalBounds().removeFromTop (22), juce::Justification::centred, 1);
    }

    void resized() override
    {
        int inputIndex[8] {};
        int outputIndex[8] {};
        int inputCount = 0;
        int outputCount = 0;

        for (int i = 0; i < jacks.size(); ++i)
        {
            if (jacks[i]->getPortDir() == PortDir::In)
                inputIndex[inputCount++] = i;
            else
                outputIndex[outputCount++] = i;
        }

        auto band = getLocalBounds().reduced (4).withTrimmedTop (22);
        const int rows = (inputCount > 0 ? 1 : 0) + (outputCount > 0 ? 1 : 0);
        if (rows == 0 || band.isEmpty())
            return;

        const int rowHeight = band.getHeight() / rows;

        auto place = [this] (const int* indices, int count, juce::Rectangle<int> row)
        {
            if (count <= 0)
                return;
            const int width = row.getWidth() / count;
            for (int i = 0; i < count; ++i)
                jacks[indices[i]]->setBounds (row.getX() + i * width, row.getY(), width, row.getHeight());
        };

        if (inputCount > 0)
            place (inputIndex, inputCount, band.removeFromTop (rowHeight));
        if (outputCount > 0)
            place (outputIndex, outputCount, band);
    }
};

RackView::RackView (MS50ModularAudioProcessor& audioProcessor)
    : processor (audioProcessor),
      cables (*this, audioProcessor)
{
    jassert (std::strcmp (kPlates[kExtInSlot].name, "Ext In") == 0);
    jassert (std::strcmp (kPlates[kOutputSlot].name, "Output") == 0);
    jassert (std::strcmp (kPlates[kNoiseSlot].name, "Noise") == 0);

    for (int slot = 0; slot < static_cast<int> (std::size (kPlates)); ++slot)
    {
        const PlateSpec& spec = kPlates[slot];
        auto* plate = plates.add (new Faceplate());
        plate->title = spec.name;
        for (int port = 0; port < spec.count; ++port)
        {
            const JackSpec& jack = spec.jacks[port];
            auto* view = plate->jacks.add (new JackView (slot, port, jack.type, jack.dir, jack.name));
            plate->addAndMakeVisible (view);
        }
        plate->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (plate);
    }

    addAndMakeVisible (cables);
    setName ("rack");
}

RackView::~RackView() = default;

void RackView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161616));
}

void RackView::resized()
{
    cables.setBounds (getLocalBounds());

    auto area = getLocalBounds().reduced (8);
    const int count = plates.size();
    if (count == 0 || area.isEmpty())
        return;

    const int minCellWidth = 230;
    int columns = juce::jmax (1, area.getWidth() / minCellWidth);
    columns = juce::jmin (columns, count);
    const int rows = (count + columns - 1) / columns;
    const int cellWidth = area.getWidth() / columns;
    const int cellHeight = area.getHeight() / rows;

    for (int i = 0; i < count; ++i)
    {
        const int column = i % columns;
        const int row = i / columns;
        plates[i]->setBounds (area.getX() + column * cellWidth,
                              area.getY() + row * cellHeight,
                              cellWidth,
                              cellHeight);
    }
}

RackView::JackHit RackView::jackAt (juce::Point<float> rackPoint) const
{
    JackHit hit;
    for (int i = 0; i < plates.size(); ++i)
    {
        Faceplate* plate = plates[i];
        if (plate == nullptr || ! plate->isVisible())
            continue;
        const auto inPlate = plate->getLocalPoint (this, rackPoint);
        if (! plate->getLocalBounds().toFloat().contains (inPlate))
            continue;
        for (int j = 0; j < plate->jacks.size(); ++j)
        {
            JackView* jack = plate->jacks[j];
            if (jack == nullptr || jack->getWidth() <= 0 || jack->getHeight() <= 0)
                continue;
            if (jack->getHitBounds().toFloat().contains (inPlate))
            {
                hit.jack = jack;
                hit.plate = plate;
                return hit;
            }
        }
    }
    return hit;
}

bool RackView::graphEndpoint (const JackView& jack, int& module, int& port) const
{
    const int slot = jack.getModuleIndex();
    int graph = -1;
    if (slot == kExtInSlot)
        graph = processor.extInGraphIndex();
    else if (slot == kOutputSlot)
        graph = processor.outputGraphIndex();
    else if (slot == kNoiseSlot)
        graph = processor.noiseGraphIndex();
    if (graph < 0)
        return false;
    module = graph;
    port = jack.getPortIndex();
    return true;
}

juce::Point<float> RackView::jackCentreInRack (const Faceplate& plate, const JackView& jack) const
{
    return getLocalPoint (&plate, jack.getHitBounds().toFloat().getCentre());
}

void RackView::showStatus (const char* text)
{
    if (statusTarget != nullptr)
        statusTarget->showPatchStatus (text != nullptr ? text : "");
}

void RackView::clearDrag()
{
    dragging = false;
    dragMapped = false;
    dragKind = DragKind::None;
    dragModule = -1;
    dragPort = -1;
    grabDestModule = -1;
    grabDestPort = -1;
    cables.clearLiftedCable();
    cables.clearRubberBand();
}

void RackView::finishGrab (const juce::MouseEvent& event, int sourceModule, int sourcePort,
                           int oldDestModule, int oldDestPort)
{
    const JackHit hit = jackAt (event.position);
    int destModule = -1;
    int destPort = -1;
    const bool destIsInput = hit.jack != nullptr && hit.jack->getPortDir() == PortDir::In;
    const bool destMapped = destIsInput && graphEndpoint (*hit.jack, destModule, destPort);
    if (destMapped && destModule == oldDestModule && destPort == oldDestPort)
    {
        showStatus ("");
        cables.repaint();
        return;
    }

    processor.disconnectJacks (sourceModule, sourcePort, oldDestModule, oldDestPort);
    if (! destMapped)
    {
        showStatus ("");
        cables.repaint();
        return;
    }

    const auto result = processor.connectJacks (sourceModule, sourcePort, destModule, destPort);
    showStatus (PatchGraph::connectResultText (result));
    cables.repaint();
}

void RackView::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        clearDrag();
        int sourceModule = -1;
        int sourcePort = -1;
        int destModule = -1;
        int destPort = -1;
        if (cables.cableAt (event.position, sourceModule, sourcePort, destModule, destPort))
        {
            processor.disconnectJacks (sourceModule, sourcePort, destModule, destPort);
            showStatus ("");
            cables.repaint();
        }
        return;
    }

    if (! event.mods.isLeftButtonDown())
        return;

    // The wire is tested before output jacks. A jack circle still starts a new cable,
    // so fan-out from an output that already has a cable keeps working.
    const JackHit hit = jackAt (event.position);
    const bool onJack = hit.jack != nullptr;
    int sourceModule = -1;
    int sourcePort = -1;
    int destModule = -1;
    int destPort = -1;
    if (! onJack && cables.cableAt (event.position, sourceModule, sourcePort, destModule, destPort))
    {
        const CableEnd from = jackCentre (sourceModule, sourcePort);
        dragging = true;
        dragKind = DragKind::Grab;
        dragMapped = from.found;
        dragModule = sourceModule;
        dragPort = sourcePort;
        grabDestModule = destModule;
        grabDestPort = destPort;
        dragX = from.found ? from.x : event.position.x;
        dragY = from.found ? from.y : event.position.y;
        cables.setLiftedCable (sourceModule, sourcePort, destModule, destPort);
        cables.setRubberBand ({ dragX, dragY }, event.position);
        return;
    }

    if (hit.jack == nullptr || hit.plate == nullptr || hit.jack->getPortDir() != PortDir::Out)
        return;

    const auto from = jackCentreInRack (*hit.plate, *hit.jack);
    dragging = true;
    dragKind = DragKind::Create;
    dragMapped = graphEndpoint (*hit.jack, dragModule, dragPort);
    if (! dragMapped)
    {
        dragModule = -1;
        dragPort = -1;
    }
    grabDestModule = -1;
    grabDestPort = -1;
    dragX = from.x;
    dragY = from.y;
    cables.setRubberBand (from, event.position);
}

void RackView::mouseDrag (const juce::MouseEvent& event)
{
    if (! dragging)
        return;
    cables.setRubberBand ({ dragX, dragY }, event.position);
}

void RackView::mouseUp (const juce::MouseEvent& event)
{
    if (! dragging || event.mods.isPopupMenu())
    {
        if (dragging)
            clearDrag();
        return;
    }

    const DragKind kind = dragKind;
    const bool mapped = dragMapped;
    const int sourceModule = dragModule;
    const int sourcePort = dragPort;
    const int oldDestModule = grabDestModule;
    const int oldDestPort = grabDestPort;
    clearDrag();

    if (kind == DragKind::Grab)
    {
        finishGrab (event, sourceModule, sourcePort, oldDestModule, oldDestPort);
        return;
    }

    const JackHit hit = jackAt (event.position);
    int destModule = -1;
    int destPort = -1;
    const bool destIsInput = hit.jack != nullptr && hit.jack->getPortDir() == PortDir::In;
    const bool destMapped = destIsInput && graphEndpoint (*hit.jack, destModule, destPort);
    if (! mapped || ! destMapped)
    {
        showStatus ("");
        return;
    }

    const auto result = processor.connectJacks (sourceModule, sourcePort, destModule, destPort);
    showStatus (PatchGraph::connectResultText (result));
    if (result == PatchGraph::ConnectResult::Ok)
        cables.repaint();
}

CableEnd RackView::jackCentre (int graphModule, int port) const
{
    CableEnd end;
    if (graphModule < 0 || port < 0)
        return end;

    int slot = -1;
    if (graphModule == processor.extInGraphIndex())
        slot = kExtInSlot;
    else if (graphModule == processor.outputGraphIndex())
        slot = kOutputSlot;
    else if (graphModule == processor.noiseGraphIndex())
        slot = kNoiseSlot;
    if (slot < 0 || slot >= plates.size())
        return end;

    const Faceplate* plate = plates[slot];
    if (plate == nullptr || port >= plate->jacks.size())
        return end;

    const JackView* jack = plate->jacks[port];
    if (jack == nullptr || jack->getWidth() <= 0 || jack->getHeight() <= 0)
        return end;
    if (jack->getPortIndex() != port)
        return end;

    const auto inPlate = jack->getHitBounds().toFloat().getCentre();
    const auto inRack = getLocalPoint (plate, inPlate);
    end.x = inRack.x;
    end.y = inRack.y;
    end.type = jack->getPortType();
    end.found = true;
    return end;
}
