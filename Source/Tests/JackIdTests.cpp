// Copyright (c) 2026 Martial Systems LLC. All rights reserved.
// JCS R6 jack ids through the shared jidai-common parser: every RONIN id whose SECTION or LABEL contains '/'
// (VCO:HZ/V, VCO:V/OCT, DIV:/2, DIV:/4) round-trips in the bare, RONIN/ and RONIN#N/ forms.

#include "Modular/Divider.h"
#include "Modular/Eg1.h"
#include "Modular/Eg2.h"
#include "Modular/ExtIn.h"
#include "Modular/Integrator.h"
#include "Modular/Inverter.h"
#include "Modular/Mg.h"
#include "Modular/Mixer.h"
#include "Modular/Noise.h"
#include "Modular/OutputModule.h"
#include "Modular/PatchGraph.h"
#include "Modular/PatchState.h"
#include "Modular/Ring.h"
#include "Modular/SampleHold.h"
#include "Modular/Vca1.h"
#include "Modular/Vca2.h"
#include "Modular/Vcf.h"
#include "Modular/Vco.h"
#include "UI/PatchBayLogic.h"

#include <cstdio>
#include <string>

namespace {

int gChecks = 0;

void check (bool ok, const std::string& message)
{
    if (! ok)
    {
        std::printf ("  FAIL %s\n", message.c_str());
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

struct FullRack {
    ExtIn ext;
    OutputModule out;
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
    Mixer mixer;
    SampleHold sampleHold;
    PatchGraph graph;
    RackIndices r;

    FullRack()
    {
        r.ext = graph.addModule (ext);
        r.output = graph.addModule (out);
        r.noise = graph.addModule (noise);
        r.vcf = graph.addModule (vcf);
        r.vca1 = graph.addModule (vca1);
        r.vca2 = graph.addModule (vca2);
        r.eg1 = graph.addModule (eg1);
        r.mg = graph.addModule (mg);
        r.vco = graph.addModule (vco);
        r.eg2 = graph.addModule (eg2);
        r.ring = graph.addModule (ring);
        r.divider = graph.addModule (divider);
        r.inverter = graph.addModule (inverter);
        r.integrator = graph.addModule (integrator);
        r.mixer = graph.addModule (mixer);
        r.sampleHold = graph.addModule (sampleHold);
    }
};

}

int testJackIdSlashRoundTrip()
{
    FullRack rack;
    int slashIds = 0;
    bool sawHzV = false;
    bool sawVOct = false;
    for (int jack = 0; jack < kPanelJackCount; ++jack)
    {
        const std::string section = kPanelJacks[jack].section;
        const std::string label = kPanelJacks[jack].label;
        const std::string bare = section + ":" + label;
        if (bare.find ('/') == std::string::npos)
            continue;
        ++slashIds;
        sawHzV = sawHzV || bare == "VCO:HZ/V";
        sawVOct = sawVOct || bare == "VCO:V/OCT";

        // Find the graph port this panel jack drives, then the id the saver writes for it.
        int module = -1;
        int port = -1;
        for (int m = 0; m < rack.graph.moduleCount() && module < 0; ++m)
            for (int p = 0; p < rack.graph.moduleAt (m)->numPorts(); ++p)
                if (patchstate::jackId (rack.r, m, p) == bare)
                {
                    module = m;
                    port = p;
                    break;
                }
        check (module >= 0, bare + ": the saver writes this id for a graph port");

        const std::string forms[] = { bare, "RONIN/" + bare, "RONIN#2/" + bare, "RONIN#12/" + bare };
        for (const std::string& text : forms)
        {
            const auto parsed = jcs::parseJackId (text);
            check (parsed.has_value(), text + ": parses");
            if (parsed)
            {
                check (parsed->section == section && parsed->label == label, text + ": SECTION and LABEL intact");
                check (parsed->text() == text, text + ": formats back to the same text");
                check (parsed->local() == bare, text + ": local form is the bare id");
            }
            int m = -1;
            int p = -1;
            check (patchstate::jackAddress (rack.r, text, m, p) && m == module && p == port,
                   text + ": binds to the same graph port");
        }
        int m = -1;
        int p = -1;
        check (! patchstate::jackAddress (rack.r, "SHOGUN/" + bare, m, p), bare + ": another device's prefix is not ours");
    }
    check (slashIds == 4, "RONIN has four ids with '/' (VCO:HZ/V, VCO:V/OCT, DIV:/2, DIV:/4), found " + std::to_string (slashIds));
    check (sawHzV && sawVOct, "VCO:HZ/V and VCO:V/OCT are covered");
    return finish ("testJackIdSlashRoundTrip");
}
