// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#include "PatchState.h"

#include "EgLaw.h"
#include "UI/PatchBayLogic.h"
#include "Vcf.h"
#include "Vco.h"

#include <cstring>

namespace patchstate {

namespace {

bool address (const RackIndices& r, int jack, int& module, int& port)
{
    return panelJackAddress (jack, r.ext, r.output, r.noise, r.vcf, r.vca1, r.vca2, r.eg1, r.mg, r.vco, r.eg2, r.ring,
                             r.divider, r.inverter, r.integrator, r.mixer, r.sampleHold, module, port);
}

}

std::string jackId (const RackIndices& rack, int module, int port)
{
    for (int jack = 0; jack < kPanelJackCount; ++jack)
    {
        int m = -1;
        int p = -1;
        if (address (rack, jack, m, p) && m == module && p == port)
            return std::string (kPanelJacks[jack].section) + ":" + kPanelJacks[jack].label;
    }
    return {};
}

const jcs::AliasTable& roninAliases()
{
    static const jcs::AliasTable table;   // JCS R6: {old id -> canonical id}; empty, no RONIN jack was renamed
    return table;
}

bool jackAddress (const RackIndices& rack, const std::string& rawId, int& module, int& port)
{
    // JCS R6 (shared parser): SECTION:LABEL, RONIN/SECTION:LABEL and RONIN#N/SECTION:LABEL bind to this device.
    // Another device's prefix is not ours. The per-device alias table is applied first (RONIN renames no jacks,
    // so it is empty). No legacy model-name prefix aliases, as in jidai-common.
    // resolve() also returns the alias's input law; RONIN declares no aliases, so it is always the identity.
    const jcs::AliasTable::Resolved resolved = roninAliases().resolve (rawId);
    if (! resolved.conversion.identity())
        return false;   // a voltage-converting alias would need the cable to carry the law; none exists in RONIN
    const auto parsed = jcs::parseJackId (resolved.id);
    if (! parsed || (! parsed->prefix.empty() && parsed->prefix != "RONIN"))
        return false;
    const std::string& section = parsed->section;
    const std::string& label = parsed->label;
    for (int jack = 0; jack < kPanelJackCount; ++jack)
    {
        if (section == kPanelJacks[jack].section && label == kPanelJacks[jack].label)
            return address (rack, jack, module, port);
    }
    return false;
}

int markLegacyInvert (PatchGraph& graph, Cable* cables, int count)
{
    int marked = 0;
    for (int i = 0; i < count; ++i)
    {
        Module* source = graph.moduleAt (cables[i].sourceModule);
        Module* dest = graph.moduleAt (cables[i].destModule);
        if (source == nullptr || dest == nullptr)
            continue;
        const PortDesc from = source->port (cables[i].sourcePort);
        const PortDesc to = dest->port (cables[i].destPort);
        const bool gateSource = from.type == PortType::Gate && ! from.strigVolts;
        if (gateSource && ! to.strigInput && to.type != PortType::Gate)
        {
            cables[i].legacyInvert = true;
            ++marked;
        }
    }
    return marked;
}

VcfFeed directVcfFeed (const Cable* cables, int count, const RackIndices& rack)
{
    VcfFeed feed = VcfFeed::None;
    int feeds = 0;
    for (int i = 0; i < count; ++i)
    {
        if (cables[i].destModule != rack.vcf || cables[i].destPort != Vcf::kSigIn)
            continue;
        ++feeds;
        VcfFeed thisFeed = VcfFeed::Other;
        if (cables[i].sourceModule == rack.vco && cables[i].sourcePort == Vco::kSaw)
            thisFeed = VcfFeed::VcoSaw;
        else if (cables[i].sourceModule == rack.vco && cables[i].sourcePort == Vco::kPulse)
            thisFeed = VcfFeed::VcoPulse;
        feed = thisFeed;
    }
    // Conservative: only a single direct VCO SAW or PULSE cable has a known reference level.
    if (feeds > 1)
        return VcfFeed::Mixed;
    return feed;
}

double referenceLevel (VcfFeed feed) noexcept
{
    if (feed == VcfFeed::VcoSaw)
        return 2.5;
    if (feed == VcfFeed::VcoPulse)
        return 5.0;
    return 0.0;
}

double compensateCutoff (double knob01, double envVolts)
{
    const double target = Vcf::effectiveHzFor (Vcf::knobHzFor (knob01), envVolts, Vcf::kLegacyInputPull);
    double lo = 0.0;
    double hi = 1.0;
    for (int i = 0; i < 60; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        if (Vcf::effectiveHzFor (Vcf::knobHzFor (mid), envVolts, Vcf::kInputPull) < target)
            lo = mid;
        else
            hi = mid;
    }
    return 0.5 * (lo + hi);
}

double migrateEgAttack (double oldKnob01) { return EgLaw::migrateAttackKnob (oldKnob01); }
double migrateEgDecayRelease (double oldKnob01) { return EgLaw::migrateDecayReleaseKnob (oldKnob01); }
bool attackStalledInV1 (double oldKnob01) noexcept { return oldKnob01 >= 0.735; }

}
