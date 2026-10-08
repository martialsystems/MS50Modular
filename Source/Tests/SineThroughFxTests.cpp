// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {

constexpr int kSamples = 512;
constexpr double kSampleRate = 48000.0;
constexpr double kHz = 220.0;
constexpr double kPi = 3.14159265358979323846;
constexpr float kAmplitude = 0.25f;

struct SineRack {
    PatchGraph graph;
    ExtIn extIn;
    OutputModule output;

    SineRack()
    {
        const int extIndex = graph.addModule (extIn);
        const int outIndex = graph.addModule (output);
        graph.connect (extIndex, 0, outIndex, 0);
        graph.connect (extIndex, 1, outIndex, 1);
        output.setMix (0.0f);
        output.setLevel (1.0f);
        graph.prepare (kSampleRate);
    }

    // Same sample loop as RoninAudioProcessor::processBlock.
    void render (const float* inLeft, const float* inRight, float* outLeft, float* outRight, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            extIn.setHostSample (inLeft[i], inRight[i]);
            graph.process();
            outLeft[i] = output.hostLeft();
            outRight[i] = output.hostRight();
        }
    }
};

float leftSample (int n)
{
    return static_cast<float> (kAmplitude * std::sin (2.0 * kPi * kHz * static_cast<double> (n) / kSampleRate));
}

float rightSample (int n)
{
    return static_cast<float> (kAmplitude * std::sin (2.0 * kPi * kHz * static_cast<double> (n) / kSampleRate + kPi / 2.0));
}

void fillStereo (float* left, float* right)
{
    for (int n = 0; n < kSamples; ++n)
    {
        left[n] = leftSample (n);
        right[n] = rightSample (n);
    }
}

bool allFinite (const float* left, const float* right, int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (left[i]) || ! std::isfinite (right[i]))
            return false;
    }
    return true;
}

float maxAbsError (const float* actual, const float* expected, int n)
{
    float worst = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (actual[i]) || ! std::isfinite (expected[i]))
            return 1.0e30f;
        worst = std::max (worst, std::abs (actual[i] - expected[i]));
    }
    return worst;
}

float maxAbs (const float* samples, int n)
{
    float worst = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        if (! std::isfinite (samples[i]))
            return 1.0e30f;
        worst = std::max (worst, std::abs (samples[i]));
    }
    return worst;
}

double rms (const float* samples, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += static_cast<double> (samples[i]) * static_cast<double> (samples[i]);
    return std::sqrt (sum / static_cast<double> (n));
}

int report (const char* tag, const char* name, bool ok, double maxError)
{
    if (ok)
        std::printf ("%s PASS\n", tag);
    else
        std::printf ("%s FAIL max error %.3e\n", tag, maxError);
    std::printf ("%s %s\n", name, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

void writeLe16 (std::ostream& out, unsigned value)
{
    const unsigned char bytes[2] = {
        static_cast<unsigned char> (value & 0xffu),
        static_cast<unsigned char> ((value >> 8) & 0xffu),
    };
    out.write (reinterpret_cast<const char*> (bytes), 2);
}

void writeLe32 (std::ostream& out, unsigned value)
{
    const unsigned char bytes[4] = {
        static_cast<unsigned char> (value & 0xffu),
        static_cast<unsigned char> ((value >> 8) & 0xffu),
        static_cast<unsigned char> ((value >> 16) & 0xffu),
        static_cast<unsigned char> ((value >> 24) & 0xffu),
    };
    out.write (reinterpret_cast<const char*> (bytes), 4);
}

int16_t pcm16 (float sample)
{
    float clamped = sample;
    if (clamped > 1.0f)
        clamped = 1.0f;
    if (clamped < -1.0f)
        clamped = -1.0f;
    const float scaled = clamped * 32767.0f;
    return static_cast<int16_t> (scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

bool writeStereoWav (const char* path, const float* left, const float* right, int n)
{
    std::error_code ec;
    std::filesystem::create_directories ("artifacts", ec);
    if (ec)
        return false;

    std::ofstream out (path, std::ios::binary);
    if (! out)
        return false;

    const unsigned dataBytes = static_cast<unsigned> (n) * 4u;
    out.write ("RIFF", 4);
    writeLe32 (out, 36u + dataBytes);
    out.write ("WAVE", 4);
    out.write ("fmt ", 4);
    writeLe32 (out, 16u);
    writeLe16 (out, 1u);
    writeLe16 (out, 2u);
    writeLe32 (out, static_cast<unsigned> (kSampleRate));
    writeLe32 (out, static_cast<unsigned> (kSampleRate) * 4u);
    writeLe16 (out, 4u);
    writeLe16 (out, 16u);
    out.write ("data", 4);
    writeLe32 (out, dataBytes);
    for (int i = 0; i < n; ++i)
    {
        const int16_t l = pcm16 (left[i]);
        const int16_t r = pcm16 (right[i]);
        writeLe16 (out, static_cast<unsigned> (static_cast<uint16_t> (l)));
        writeLe16 (out, static_cast<unsigned> (static_cast<uint16_t> (r)));
    }
    return static_cast<bool> (out);
}

void tryWriteDryPair (const float* inLeft, const float* inRight, const float* outLeft, const float* outRight)
{
    try
    {
        writeStereoWav ("artifacts/sine_dry.wav", inLeft, inRight, kSamples);
        writeStereoWav ("artifacts/sine_out.wav", outLeft, outRight, kSamples);
    }
    catch (...)
    {
    }
}

}

int testSineDryStereoPasses()
{
    float inLeft[kSamples];
    float inRight[kSamples];
    float outLeft[kSamples];
    float outRight[kSamples];
    fillStereo (inLeft, inRight);

    SineRack rack;
    rack.render (inLeft, inRight, outLeft, outRight, kSamples);
    tryWriteDryPair (inLeft, inRight, outLeft, outRight);

    const bool finite = allFinite (outLeft, outRight, kSamples);
    const float err = std::max (maxAbsError (outLeft, inLeft, kSamples), maxAbsError (outRight, inRight, kSamples));
    const bool aligned = err <= 1.0e-5f;
    const double shown = finite ? static_cast<double> (err) : 1.0e30;
    return report ("SINE_DRY", "testSineDryStereoPasses", finite && aligned, shown);
}

int testSineLeftOnlyStaysLeft()
{
    float inLeft[kSamples];
    float inRight[kSamples];
    float outLeft[kSamples];
    float outRight[kSamples];
    for (int n = 0; n < kSamples; ++n)
    {
        inLeft[n] = leftSample (n);
        inRight[n] = 0.0f;
    }

    SineRack rack;
    rack.render (inLeft, inRight, outLeft, outRight, kSamples);

    const float leftErr = maxAbsError (outLeft, inLeft, kSamples);
    const float rightLeak = maxAbs (outRight, kSamples);
    const bool ok = leftErr <= 1.0e-5f && rightLeak < 1.0e-6f;
    return report ("SINE_LEFT", "testSineLeftOnlyStaysLeft", ok, static_cast<double> (std::max (leftErr, rightLeak)));
}

int testSineWetUnpatchedIsSilence()
{
    float inLeft[kSamples];
    float inRight[kSamples];
    float outLeft[kSamples];
    float outRight[kSamples];
    fillStereo (inLeft, inRight);

    SineRack rack;
    rack.output.setMix (1.0f);
    rack.render (inLeft, inRight, outLeft, outRight, kSamples);

    const float worst = std::max (maxAbs (outLeft, kSamples), maxAbs (outRight, kSamples));
    return report ("SINE_WET_SILENCE", "testSineWetUnpatchedIsSilence", worst < 1.0e-6f, static_cast<double> (worst));
}

int testSineRmsInRange()
{
    float inLeft[kSamples];
    float inRight[kSamples];
    float outLeft[kSamples];
    float outRight[kSamples];
    fillStereo (inLeft, inRight);

    SineRack rack;
    rack.render (inLeft, inRight, outLeft, outRight, kSamples);

    if (! allFinite (outLeft, outRight, kSamples))
        return report ("SINE_RMS", "testSineRmsInRange", false, 1.0e30);

    const double inL = rms (inLeft, kSamples);
    const double inR = rms (inRight, kSamples);
    const double outL = rms (outLeft, kSamples);
    const double outR = rms (outRight, kSamples);
    const double relL = (inL > 1.0e-8) ? std::abs (outL - inL) / inL : 1.0;
    const double relR = (inR > 1.0e-8) ? std::abs (outR - inR) / inR : 1.0;
    const double worst = std::max (relL, relR);
    return report ("SINE_RMS", "testSineRmsInRange", worst <= 0.10, worst);
}
