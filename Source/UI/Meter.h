// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

#include <cmath>

// Display. Not a graph module. setSource does not add a cable and does not run the graph.
class Meter {
public:
    static constexpr float kFullScaleVolts = 5.0f;

    void setSource (int module, int port) noexcept
    {
        if (module < 0 || port < 0)
            return;
        module_ = module;
        port_ = port;
    }

    bool hasSelection() const noexcept { return module_ >= 0 && port_ >= 0; }
    int sourceModule() const noexcept { return module_; }
    int sourcePort() const noexcept { return port_; }

    int readingModule (int defaultModule) const noexcept
    {
        return hasSelection() ? module_ : defaultModule;
    }

    int readingPort (int defaultPort) const noexcept
    {
        return hasSelection() ? port_ : defaultPort;
    }

    // -1 at -5 V, 0 at 0 V, +1 at +5 V. Past full scale the needle stays at the end.
    static float needle (float volts) noexcept
    {
        if (! std::isfinite (volts))
            return 0.0f;
        const float unit = volts / kFullScaleVolts;
        if (unit < -1.0f)
            return -1.0f;
        if (unit > 1.0f)
            return 1.0f;
        return unit;
    }

private:
    int module_ = -1;
    int port_ = -1;
};
