// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "Modular/DefaultPatch.h"
#include "Modular/Eg1.h"
#include "Modular/ExtIn.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/Vca1.h"
#include "Modular/Vcf.h"

#include <cstdio>

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

class GainModule : public Module {
public:
    int numPorts() const override { return 2; }

    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", PortType::Audio, PortDir::In };
        return { "Out", PortType::Audio, PortDir::Out };
    }

    void setKnob (int, float) override {}
    void prepare (double rate) override { sampleRate = rate; }
    void processSample() override { portValue[1] = portValue[0]; }
};

class ImpulseModule : public Module {
public:
    int numPorts() const override { return 2; }

    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", PortType::Audio, PortDir::In };
        return { "Out", PortType::Audio, PortDir::Out };
    }

    void setKnob (int, float) override {}

    void prepare (double rate) override
    {
        sampleRate = rate;
        remaining_ = 1;
    }

    void processSample() override
    {
        portValue[1] = remaining_ > 0 ? 1.0f : 0.0f;
        if (remaining_ > 0)
            --remaining_;
    }

private:
    int remaining_ = 1;
};

class SilentModule : public Module {
public:
    int numPorts() const override { return 2; }

    PortDesc port (int index) const override
    {
        if (index == 0)
            return { "In", PortType::Audio, PortDir::In };
        return { "Out", PortType::Audio, PortDir::Out };
    }

    void setKnob (int, float) override {}
    void prepare (double rate) override { sampleRate = rate; }
    void processSample() override { portValue[1] = 0.0f; }
};

int finish (const char* name)
{
    const int failed = gChecks;
    gChecks = 0;
    std::printf ("%s %s\n", name, failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}

}

int testFeedbackIsOneSample()
{
    PatchGraph loop;
    ImpulseModule impulse;
    GainModule gain;
    const int iImpulse = loop.addModule (impulse);
    const int iGain = loop.addModule (gain);
    check (loop.connect (iImpulse, 1, iGain, 0), "impulse into the gain");
    check (loop.connect (iGain, 1, iGain, 0), "gain output into its input");
    check (! loop.cableIsDelayed (0), "impulse cable is zero-delay");
    check (loop.cableIsDelayed (1), "self cable is the delayed edge");
    check (loop.delayedCableCount() == 1, "one delayed cable in the self loop");

    loop.prepare (48000.0);
    loop.process();
    check (gain.portValue[1] == 1.0f, "first sample is the impulse alone");
    gain.portValue[1] = 0.0f;
    loop.process();
    check (gain.portValue[1] == 1.0f, "impulse returns on the next sample");

    gain.portValue[1] = 0.0f;
    SilentModule idle;
    loop.addModule (idle);
    loop.process();
    check (gain.portValue[1] == 1.0f, "republish keeps the delayed sample");

    PatchGraph pair;
    ImpulseModule pairImpulse;
    GainModule a;
    GainModule b;
    const int iPairImpulse = pair.addModule (pairImpulse);
    const int iA = pair.addModule (a);
    const int iB = pair.addModule (b);
    check (pair.connect (iPairImpulse, 1, iA, 0), "impulse into A");
    check (pair.connect (iA, 1, iB, 0), "A into B");
    check (pair.connect (iB, 1, iA, 0), "B into A");
    check (! pair.cableIsDelayed (0), "pair impulse cable is zero-delay");
    check (! pair.cableIsDelayed (1), "A to B is zero-delay");
    check (pair.cableIsDelayed (2), "B to A is the delayed edge");
    check (pair.delayedCableCount() == 1, "pair has one delayed cable");
    pair.prepare (48000.0);
    pair.process();
    check (a.portValue[1] == 1.0f, "A is the impulse on the first sample");
    check (b.portValue[1] == 1.0f, "B hears A's output with no delay");
    b.portValue[1] = 0.0f;
    pair.process();
    check (a.portValue[1] == 1.0f, "A hears the delayed B, not the wiped jack");

    PatchGraph stacked;
    ImpulseModule stackedImpulse;
    GainModule left;
    GainModule right;
    const int iStackedImpulse = stacked.addModule (stackedImpulse);
    const int iLeft = stacked.addModule (left);
    const int iRight = stacked.addModule (right);
    check (stacked.connect (iStackedImpulse, 1, iLeft, 0), "stacked impulse");
    check (stacked.connect (iLeft, 1, iRight, 0), "left into right");
    check (stacked.connect (iRight, 1, iLeft, 0), "older right into left");
    check (stacked.connect (iRight, 1, iLeft, 0), "newer right into left");
    // JCS R9: every feedback cable is delayed one sample, the older one too.
    check (stacked.cableIsDelayed (2), "older feedback cable is delayed");
    check (stacked.cableIsDelayed (3), "newest feedback cable is delayed");
    check (stacked.delayedCableCount() == 2, "stacked cycle delays both cables");
    stacked.prepare (48000.0);
    stacked.process();
    check (left.portValue[1] == 1.0f, "first sample is the impulse alone");
    stacked.process();
    check (left.portValue[1] == 2.0f, "both feedback cables arrive one sample later");

    PatchGraph factory;
    ExtIn ext;
    OutputModule output;
    Vcf vcf;
    Vca1 vca1;
    Eg1 eg1;
    const int iExt = factory.addModule (ext);
    const int iOut = factory.addModule (output);
    const int iVcf = factory.addModule (vcf);
    const int iVca = factory.addModule (vca1);
    const int iEg = factory.addModule (eg1);
    check (connectFactoryCables (factory, iExt, iOut, iVcf, iVca, iEg), "factory cables");
    check (factory.cableCount() == 8, "eight factory cables");
    check (factory.delayedCableCount() == 0, "factory patch delays nothing");
    for (int i = 0; i < factory.cableCount(); ++i)
        check (! factory.cableIsDelayed (i), "factory cable is zero-delay");

    return finish ("testFeedbackIsOneSample");
}
