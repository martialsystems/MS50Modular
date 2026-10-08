# Vendored copy

- Source: `jidai-collection` repository, folder `jidai-common/`
- Branch: `redesign/jidai`
- Commit: `24ee621` ("jidai-common 1.1.1: SHOGUN + RONIN fix batch (re-vendor this commit)"), version 1.1.1
- Copied verbatim with `git archive 24ee621 jidai-common`; no local edits. Only this VENDOR.md was added.

RONIN uses:
- `jidai/CableStandard.h` through `Source/Modular/Jcs.h`, a two-line adapter (`#include <jidai/CableStandard.h>`
  and `namespace jcs = jidai::jcs;`): detectors, volts, over-range LED, pitch, roles, jack ids.
- `jidai/dsp/Halfband.h` (`jidai::dsp::Downsampler2x`, 93 taps, 23 base samples) as the HQ 2x decimator.
- Not used: `jidai/dsp/TripleShaper.h`.

To update: replace this folder with a newer `git archive` of `jidai-common/`, update the commit above, rebuild,
and run RoninTests, RoninProcessorTests and RoninPanelProbe. The library's own tests are off in the plugin build
(`JIDAI_COMMON_TESTS OFF`); build them standalone to check a new copy.
