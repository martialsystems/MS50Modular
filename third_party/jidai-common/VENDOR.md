# Vendored copy

- Source: `jidai-collection` repository, folder `jidai-common/`
- Branch: `redesign/jidai`
- Commit: `8a4b5aecf3d6c2ac8900cc2e88db8feecf8440a8` ("jidai-common 1.1.2: AMT 0 transparent, group delay API, lin55 floor
  (re-vendor this commit)"), version 1.1.2
- Copied verbatim with `git archive 8a4b5ae jidai-common`; no local edits. Only this VENDOR.md was added.

RONIN uses:
- `jidai/CableStandard.h` through `Source/Modular/Jcs.h`, a two-line adapter (`#include <jidai/CableStandard.h>`
  and `namespace jcs = jidai::jcs;`): detectors, volts, over-range LED, pitch, roles, jack ids.
- `jidai/dsp/Halfband.h` (`jidai::dsp::Downsampler2x`, 93 taps, 23 base samples) as the HQ 2x decimator.
- Not used: `jidai/dsp/TripleShaper.h`.

To update: replace this folder with a newer `git archive` of `jidai-common/`, update the commit above, rebuild,
and run RoninTests, RoninProcessorTests and RoninPanelProbe. The library's own tests are off in the plugin build
(`JIDAI_COMMON_TESTS OFF`); build them standalone to check a new copy.
