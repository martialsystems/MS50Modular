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

    // Copies the published snapshot. The caller supplies storage. No allocation.
    int copyPublishedCables (Cable* dest, int capacity) const;

    // The newest cable that closes a cycle. Other cables stay zero-delay.
    int delayedCableCount() const;
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
    void contributeCables (int moduleIndex, const Snapshot& snapshot, bool includeZeroDelayFeedback) const;

    Module* modules_[kMaxModules] {};
    int moduleCount_ = 0;

    Cable editCables_[kMaxCables] {};
    int editCableCount_ = 0;

    Snapshot snapshots_[2] {};
    std::atomic<int> published_ { 0 };
    double preparedRate_ = 0.0;
    const char* stateError_ = "";
};
