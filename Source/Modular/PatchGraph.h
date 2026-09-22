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

    void prepare (double sampleRate);
    void process();

    int moduleCount() const { return moduleCount_; }
    int cableCount() const { return editCableCount_; }

private:
    struct Snapshot {
        Cable cables[kMaxCables] {};
        int cableCount = 0;
        int order[kMaxModules] {};
        int orderCount = 0;
    };

    bool indicesLegal (int sourceModule, int sourcePort, int destModule, int destPort) const;
    bool closesCycle (int sourceModule, int destModule) const;
    void publish();
    void fillOrder (Snapshot& snapshot) const;

    Module* modules_[kMaxModules] {};
    int moduleCount_ = 0;

    Cable editCables_[kMaxCables] {};
    int editCableCount_ = 0;

    Snapshot snapshots_[2] {};
    std::atomic<int> published_ { 0 };
};
