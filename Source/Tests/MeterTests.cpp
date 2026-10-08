// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "UI/Meter.h"
#include "UI/PatchBayLogic.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace {

int gChecks = 0;

void check (bool ok, const char* message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message);
        ++gChecks;
    }
}

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

std::string functionBody (const std::string& text, const std::string& marker)
{
    const auto start = text.find (marker);
    if (start == std::string::npos)
        return {};
    const auto body = text.find ('{', start);
    if (body == std::string::npos)
        return {};

    int depth = 0;
    std::size_t end = body;
    for (; end < text.size(); ++end)
    {
        if (text[end] == '{')
            ++depth;
        else if (text[end] == '}')
        {
            --depth;
            if (depth == 0)
                break;
        }
    }
    if (end >= text.size())
        return {};
    return text.substr (start, end - start + 1);
}

std::string readFile (const char* path)
{
    std::ifstream input (path);
    if (! input)
        return {};
    return std::string ((std::istreambuf_iterator<char> (input)), std::istreambuf_iterator<char>());
}

}

int testMeterFollowsSelectedJack()
{
    OutputModule output;
    NoiseModule noise;
    output.portValue[2] = 2.5f;
    noise.portValue[NoiseModule::kWhite] = -4.0f;

    PatchGraph graph;
    const int noiseIndex = graph.addModule (noise);
    const int outputIndex = graph.addModule (output);
    check (graph.connect (noiseIndex, NoiseModule::kWhite, outputIndex, 0), "a cable stays beside the meter");
    const int cables = graph.cableCount();

    Meter meter;
    check (! meter.hasSelection(), "nothing clicked yet");
    check (meter.readingModule (outputIndex) == outputIndex, "default module is output");
    check (meter.readingPort (2) == 2, "default port is wet");
    const float wet = graph.portVolts (meter.readingModule (outputIndex), meter.readingPort (2));
    check (std::fabs (wet - 2.5f) < 1.0e-5f, "default read is output wet");
    check (std::fabs (Meter::needle (wet) - 0.5f) < 1.0e-5f, "2.5 V is half scale");

    meter.setSource (noiseIndex, NoiseModule::kWhite);
    check (meter.hasSelection(), "a jack was selected");
    const float selected = graph.portVolts (meter.readingModule (outputIndex), meter.readingPort (2));
    check (std::fabs (selected - (-4.0f)) < 1.0e-5f, "the needle follows the selected jack");
    check (std::fabs (Meter::needle (selected) - (-0.8f)) < 1.0e-5f, "-4 V is -0.8 of full scale");
    check (Meter::needle (9.0f) == 1.0f, "+full scale stops at +1");
    check (Meter::needle (-9.0f) == -1.0f, "-full scale stops at -1");
    check (graph.cableCount() == cables, "selecting a source adds no cable");

    graph.prepare (48000.0);
    graph.process();
    check (graph.cableCount() == cables, "process leaves the cable list alone");
    check (meter.sourceModule() == noiseIndex && meter.sourcePort() == NoiseModule::kWhite,
           "process leaves the meter source alone");

    int module = -1;
    int port = -1;
    const int wetJack = panelJackIndex ("OUTPUT", "WET");
    check (panelJackAddress (wetJack, 0, outputIndex, noiseIndex, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                             module, port),
           "wet has a graph address");
    check (module == outputIndex && port == 2, "wet address is output port 2");
    module = 0;
    port = 0;
    check (! panelJackAddress (panelJackIndex ("DIV", "/16"), 0, outputIndex, noiseIndex, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                               module, port),
           "/16 is not a meter source");
    check (module == -1 && port == -1, "an unmapped click does not invent a port");

    const std::string process = functionBody (readFile (RONIN_PATCH_GRAPH_SOURCE), "void PatchGraph::process()");
    check (! process.empty(), "process() is readable");
    check (process.find ("Meter") == std::string::npos, "process() does not mention the meter");
    check (process.find ("portVolts") == std::string::npos, "process() does not read the meter");

    const std::string view = readFile (RONIN_PATCH_BAY_VIEW_SOURCE);
    const std::string paint = functionBody (view, "void PatchBayView::paint");
    const auto holdPaint = paint.find ("paintExtInHold");
    const auto cablePaint = paint.find ("for (int cable");
    check (holdPaint != std::string::npos && cablePaint != std::string::npos && holdPaint < cablePaint,
           "the hold button is painted before the cables");
    const std::string down = functionBody (view, "void PatchBayView::mouseDown");
    check (down.find ("selectMeter") != std::string::npos, "mouse down on a jack selects the meter");
    const std::string select = functionBody (view, "void PatchBayView::selectMeter");
    check (select.find ("setSource") != std::string::npos, "a jack click selects the meter");
    check (select.find ("connect") == std::string::npos, "selecting the meter does not connect a cable");
    return finish ("testMeterFollowsSelectedJack");
}
