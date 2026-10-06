// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Eg1.h"
#include "PatchGraph.h"
#include "Vca1.h"
#include "Vcf.h"

// Factory cables in SCHEMATICS.md creation order.
// Pass eg1 < 0 to stop after the five voice-and-dry cables.
inline bool connectFactoryCables (PatchGraph& graph, int ext, int output, int vcf, int vca1, int eg1 = -1)
{
    if (ext < 0 || output < 0 || vcf < 0 || vca1 < 0)
        return false;

    bool ok = true;
    auto add = [&] (int sourceModule, int sourcePort, int destModule, int destPort)
    {
        ok = graph.connect (sourceModule, sourcePort, destModule, destPort) && ok;
    };

    add (ext, 2, vcf, Vcf::kSigIn);
    add (vcf, Vcf::kSigOut, vca1, Vca1::kSigIn);
    add (vca1, Vca1::kOut, output, 2);
    add (ext, 0, output, 0);
    add (ext, 1, output, 1);
    if (eg1 >= 0)
    {
        add (ext, 3, eg1, Eg1::kTrig);
        add (eg1, Eg1::kOutA, vca1, Vca1::kEnv);
        add (eg1, Eg1::kOutA, vcf, Vcf::kCutoff);
    }
    return ok;
}
