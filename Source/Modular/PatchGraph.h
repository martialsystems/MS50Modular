// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include "Cable.h"
#include "Module.h"

#include <atomic>

// Fixed rack. The caller owns each Module. The graph only stores the address.
class PatchGraph {
public:
    static constexpr int kMaxModules = 16;
    static constexpr int kMaxCables = 64;
    static constexpr int kMaxPorts = 8;

    int addModule (Module& module);

    // False leaves the cable list unchanged.
    bool connect (int sourceModule, int sourcePort, int destModule, int destPort);
    void disconnect (int sourceModule, int sourcePort, int destModule, int destPort);

    // Shared by the editor mouse-up path and by GraphTests. Only Ok calls connect().
    enum class ConnectResult {
        Ok,
        BadType,
        Cycle,
        Rejected
    };

    ConnectResult attemptConnect (int sourceModule, int sourcePort, int destModule, int destPort);
    static const char* connectResultText (ConnectResult result) noexcept;

    void prepare (double sampleRate);
    void process();

    int moduleCount() const { return moduleCount_; }
    Module* moduleAt (int index) noexcept;
    int cableCount() const { return editCableCount_; }

    // Live jack voltage for the meter. Not used by process().
    float portVolts (int module, int port) const noexcept;
    // JCS R15: true once |V| > 5.5 V has lasted more than 10 ms on that port, held for 100 ms after it ends so a
    // 20 Hz UI cannot miss it. Written by process() (relaxed), read by the UI.
    bool portOverRange (int module, int port) const noexcept;

    // Copies the published snapshot. The caller supplies storage. No allocation.
    int copyPublishedCables (Cable* dest, int capacity) const;

    // JCS R9: every cable that closes a cycle (walking oldest to newest) is delayed one sample.
    int delayedCableCount() const;

    // The volts one cable adds to its destination (JCS R3s, M3). Shared with the UI's jack monitor.
    static float cableVolts (float raw, const PortDesc& sourceDesc, const PortDesc& destDesc, bool legacyInvert) noexcept;
    // True exactly where cableVolts converts a gate to S-trig on a non-migrated cable: a Gate-type output that is not
    // already S-trig, landing on an S-trig input (EG 1/EG 2 TRIG). A GATE/CLK-role output of CV type (DIV /2 and /4)
    // passes its volts as written.
    static bool convertsToStrig (const PortDesc& sourceDesc, const PortDesc& destDesc) noexcept;
    // The patch bay's cable badge: jcs::cableBadge on the two roles, except that "gate converted to S-trig" shows
    // exactly where convertsToStrig is true (the same rule as the rack's badge).
    static jcs::Badge cableBadge (const PortDesc& sourceDesc, const PortDesc& destDesc) noexcept;
    bool cableIsDelayed (int index) const;

    static constexpr int kStateVersion = 1;
    static constexpr int kMaxPresetKnobs = 8;

    // Little-endian RONIN blob (magic "RNIN"): magic, version, module count, cable count,
    // then each module's knob floats and scale index, then each cable's four ids.
    // No color, no stack order, no filter memory, no noise seed.
    // Returns the bytes written, or 0 when the buffer is too small.
    int getState (void* dest, int capacity) const;

    // False leaves knobs, cables, and module memory as they were.
    // An accepted load calls prepare so filter and envelope memory restart.
    bool setState (const void* data, int size);

    // Replaces the cable list. Does not append. Cycles stay, and publish picks the newest feedback edge.
    // False leaves the previous list in place.
    bool setCables (const Cable* cables, int count);

    bool writePresetKnob (int module, int knob, float value);
    // Edit one cable's saved fields (message thread). They never change the sum's routing; legacyInvert changes the
    // conversion of that one cable (PATCH tab "Convert"). False for a bad index.
    bool setCableColour (int index, std::uint32_t argb);
    bool setCableLegacyInvert (int index, bool legacy);
    // True when connect() would accept this cable (indices, direction, type, not one jack to itself).
    bool cableIsLegal (const Cable& cable) const;
    const char* stateError() const noexcept { return stateError_; }

private:
    struct Snapshot {
        Cable cables[kMaxCables] {};
        int cableCount = 0;
        bool feedback[kMaxCables] {};
        bool delayed[kMaxCables] {};
        float held[kMaxCables] {};
        int order[kMaxModules] {};
        int orderCount = 0;
    };

    bool indicesLegal (int sourceModule, int sourcePort, int destModule, int destPort) const;
    // True when `from` can walk to `target` along cables already kept out of the feedback set.
    bool keptReaches (int from, int target, const Snapshot& snapshot, const bool* kept) const;
    void publish();
    void fillOrder (Snapshot& snapshot) const;
    void clearModuleInputs (int moduleIndex, const bool patched[kMaxModules][kMaxPorts]) const;
    void contributeCables (int moduleIndex, const Snapshot& snapshot) const;

    Module* modules_[kMaxModules] {};
    int moduleCount_ = 0;

    Cable editCables_[kMaxCables] {};
    int editCableCount_ = 0;

    Snapshot snapshots_[2] {};
    std::atomic<int> published_ { 0 };
    double preparedRate_ = 0.0;
    const char* stateError_ = "";
    // JCS R15 tracking: the shared jcs::OverRangeLed per port (audio thread), a 100 ms UI hold, and a relaxed
    // atomic flag the UI reads.
    jcs::OverRangeLed overLed_[kMaxModules][kMaxPorts] {};
    int overHold_[kMaxModules][kMaxPorts] {};
    std::atomic<bool> overFlag_[kMaxModules][kMaxPorts] {};
    int overHoldSamples_ = 4800;
};
