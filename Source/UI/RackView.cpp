// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "RackView.h"
#include "JackView.h"

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

RackView::RackView()
{
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
        addAndMakeVisible (plate);
    }
}

RackView::~RackView() = default;

void RackView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff161616));
}

void RackView::resized()
{
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
