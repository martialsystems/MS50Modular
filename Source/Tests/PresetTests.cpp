// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Tests/TestSuite.h"
#include "Modular/DefaultPatch.h"
#include "Modular/Divider.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/ExtIn.h"
#include "Modular/Integrator.h"
#include "Modular/Inverter.h"
#include "Modular/Mg.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/Ring.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "Modular/Vco.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

struct Rack {
    ExtIn ext;
    OutputModule output;
    NoiseModule noise;
    Vcf vcf;
    Vca1 vca1;
    Vca2 vca2;
    Eg1 eg1;
    MgModule mg;
    Vco vco;
    Eg2 eg2;
    Ring ring;
    Divider divider;
    Inverter inverter;
    Integrator integrator;
    PatchGraph graph;

    void addAll()
    {
        graph.addModule (ext);
        graph.addModule (output);
        graph.addModule (noise);
        graph.addModule (vcf);
        graph.addModule (vca1);
        graph.addModule (vca2);
        graph.addModule (eg1);
        graph.addModule (mg);
        graph.addModule (vco);
        graph.addModule (eg2);
        graph.addModule (ring);
        graph.addModule (divider);
        graph.addModule (inverter);
        graph.addModule (integrator);
    }
};

bool sameKnob (float a, float b)
{
    return std::fabs (a - b) < 1.0e-6f;
}

bool knobsMatch (const Rack& a, const Rack& b)
{
    const Module* left[] = { &a.ext, &a.output, &a.noise, &a.vcf, &a.vca1, &a.vca2, &a.eg1,
                             &a.mg, &a.vco, &a.eg2, &a.ring, &a.divider, &a.inverter, &a.integrator };
    const Module* right[] = { &b.ext, &b.output, &b.noise, &b.vcf, &b.vca1, &b.vca2, &b.eg1,
                              &b.mg, &b.vco, &b.eg2, &b.ring, &b.divider, &b.inverter, &b.integrator };
    for (int module = 0; module < 14; ++module)
    {
        if (left[module]->presetKnobCount() != right[module]->presetKnobCount())
            return false;
        if (left[module]->presetScaleIndex() != right[module]->presetScaleIndex())
            return false;
        for (int knob = 0; knob < left[module]->presetKnobCount(); ++knob)
        {
            if (! sameKnob (left[module]->presetKnob (knob), right[module]->presetKnob (knob)))
                return false;
        }
    }
    return true;
}

bool cablesMatch (const PatchGraph& a, const PatchGraph& b)
{
    Cable left[PatchGraph::kMaxCables] {};
    Cable right[PatchGraph::kMaxCables] {};
    const int leftCount = a.copyPublishedCables (left, PatchGraph::kMaxCables);
    const int rightCount = b.copyPublishedCables (right, PatchGraph::kMaxCables);
    if (leftCount != rightCount)
        return false;
    for (int i = 0; i < leftCount; ++i)
    {
        if (left[i].sourceModule != right[i].sourceModule || left[i].sourcePort != right[i].sourcePort)
            return false;
        if (left[i].destModule != right[i].destModule || left[i].destPort != right[i].destPort)
            return false;
    }
    return true;
}

int scaleIndexInBlob (const unsigned char* blob, int size, int moduleIndex)
{
    if (blob == nullptr || size < 16 || std::memcmp (blob, "RNIN", 4) != 0)
        return -2;
    const unsigned char* cursor = blob + 16;
    const unsigned char* end = blob + size;
    for (int module = 0; module < 14; ++module)
    {
        if (end - cursor < 4)
            return -2;
        std::int32_t knobs = 0;
        std::memcpy (&knobs, cursor, 4);
        cursor += 4;
        if (knobs < 0 || end - cursor < knobs * 4 + 4)
            return -2;
        cursor += knobs * 4;
        std::int32_t scale = 0;
        std::memcpy (&scale, cursor, 4);
        cursor += 4;
        if (module == moduleIndex)
            return scale;
    }
    return -2;
}

}

int testPresetRoundTrip()
{
    Rack source;
    source.addAll();
    check (connectFactoryCables (source.graph, 0, 1, 3, 4, 6), "factory cables");
    check (source.graph.connect (2, 0, 5, Vca2::kIn), "extra cable");
    source.vcf.setKnob (Vcf::kKnobCutoff, 0.20f);
    source.vco.setKnob (Vco::kKnobScale, 0.0f);
    source.graph.prepare (48000.0);

    for (int i = 0; i < 1000; ++i)
    {
        source.noise.processSample();
        source.vcf.portValue[Vcf::kSigIn] = 0.3f;
        source.vcf.processSample();
    }

    unsigned char first[4096];
    unsigned char second[4096];
    const int firstBytes = source.graph.getState (first, static_cast<int> (sizeof first));
    for (int i = 0; i < 1000; ++i)
    {
        source.noise.processSample();
        source.vcf.processSample();
    }
    const int secondBytes = source.graph.getState (second, static_cast<int> (sizeof second));
    check (firstBytes > 16 && firstBytes == secondBytes, "state size stays fixed");
    check (std::memcmp (first, second, static_cast<std::size_t> (firstBytes)) == 0,
           "filter memory and the noise seed are not in the blob");
    check (scaleIndexInBlob (first, firstBytes, 8) == 0, "saved scale index is 32 foot");
    check (source.graph.cableCount() == 9, "eight factory cables plus one");

    Rack dest;
    dest.addAll();
    check (dest.graph.connect (0, 0, 1, 0), "dest starts on a different cable");
    dest.vcf.setKnob (Vcf::kKnobCutoff, 0.90f);
    dest.vco.setKnob (Vco::kKnobScale, 1.0f);
    dest.graph.prepare (48000.0);
    dest.eg1.setKnob (Eg1::kKnobAttack, 0.0f);
    for (int i = 0; i < 2400; ++i)
    {
        dest.eg1.portValue[Eg1::kTrig] = 0.0f;
        dest.eg1.processSample();
    }
    check (dest.eg1.portValue[Eg1::kOutA] > 1.0f, "destination envelope was open");

    check (dest.graph.setState (first, firstBytes), "load accepts version 1");
    check (dest.graph.stateError()[0] == '\0', "a good load clears the error");
    check (knobsMatch (source, dest), "knobs and scale index match");
    check (cablesMatch (source.graph, dest.graph), "cables match");
    check (std::fabs (dest.vcf.presetKnob (Vcf::kKnobCutoff) - 0.20f) < 1.0e-6f, "cutoff restored");
    dest.graph.process();
    check (dest.eg1.portValue[Eg1::kOutA] < 0.05f, "load cleared envelope memory");
    return finish ("testPresetRoundTrip");
}

int testPresetRejectsBadVersion()
{
    Rack rack;
    rack.addAll();
    check (connectFactoryCables (rack.graph, 0, 1, 3, 4, 6), "factory cables");
    rack.vcf.setKnob (Vcf::kKnobCutoff, 0.30f);
    rack.graph.prepare (48000.0);

    unsigned char blob[4096];
    const int bytes = rack.graph.getState (blob, static_cast<int> (sizeof blob));
    check (bytes > 16, "saved");
    blob[4] = 2;
    blob[5] = 0;
    blob[6] = 0;
    blob[7] = 0;

    rack.vcf.setKnob (Vcf::kKnobCutoff, 0.80f);
    check (rack.graph.connect (2, 0, 5, Vca2::kIn), "later cable");
    const int cablesBefore = rack.graph.cableCount();
    const float cutoffBefore = rack.vcf.presetKnob (Vcf::kKnobCutoff);

    check (! rack.graph.setState (blob, bytes), "version 2 is rejected");
    check (std::strcmp (rack.graph.stateError(), "preset version is not supported") == 0, "version error");
    check (rack.graph.cableCount() == cablesBefore, "cables stay");
    check (std::fabs (rack.vcf.presetKnob (Vcf::kKnobCutoff) - cutoffBefore) < 1.0e-6f, "cutoff stays");

    check (! rack.graph.setState (nullptr, bytes), "null state is rejected");
    check (! rack.graph.setState (blob, 0), "empty state is rejected");
    unsigned char garbage[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    check (! rack.graph.setState (garbage, 8), "garbage is rejected");
    check (! rack.graph.setState (blob, 12), "a short header is rejected");

    blob[4] = 1;
    check (! rack.graph.setState (blob, bytes / 2), "a truncated blob is rejected");
    check (std::strcmp (rack.graph.stateError(), "preset state is corrupt") == 0, "corrupt error");
    check (rack.graph.cableCount() == cablesBefore, "truncated load keeps the cables");
    check (std::fabs (rack.vcf.presetKnob (Vcf::kKnobCutoff) - cutoffBefore) < 1.0e-6f, "truncated load keeps the cutoff");
    return finish ("testPresetRejectsBadVersion");
}
