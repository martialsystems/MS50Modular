// Copyright (c) 2026 Martial Systems LLC. All rights reserved.

#pragma once

// Intentional exact float comparison (a value that must land exactly, a switch position, a table default).
// Same result as `a == b`, including NaN != NaN; it only names the intent so -Wfloat-equal stays on everywhere else
// (RONIN_STRICT_WARNINGS). Both sides must be the same type, so no hidden promotion. JUCE-free, constexpr.

namespace ronin {

#if defined (__GNUC__) || defined (__clang__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

template <typename T>
constexpr bool exactlyEqual (T a, T b) noexcept
{
    return a == b;
}

#if defined (__GNUC__) || defined (__clang__)
 #pragma GCC diagnostic pop
#endif

}
