Copyright (c) 2026 Martial Systems LLC. All rights reserved. RONIN is part of the Jidai Collection.

# Developing RONIN

Read this before changing code. The DSP behaviour is specified in `docs/SCHEMATICS.md`, the stand-in rules in `docs/METHODOLOGY.md`, the build order in `docs/BUILD_GUIDE.md` and the tests in `docs/TESTPLAN.md`. The factory program (INIT only for now) and the default knob table are in `docs/presets.md`.

## Repo map

```text
README.md                 product page
LICENSE                   copyright and trademark notice
CMakeLists.txt            plugin (target Ronin, product name RONIN), tests, panel probe
Source/
  PluginProcessor.h/.cpp
  PluginEditor.h/.cpp
  Modular/                graph and modules, no JUCE types in process()
    Jcs.h                 adapter: `#include <jidai/CableStandard.h>` + `namespace jcs = jidai::jcs` (JCS v1.1 rules)
    PatchState.h/.cpp     format-2 jack ids (SECTION:LABEL, shared parser) and the format-1 migration (M-R1..M-R5)
    Smoothing.h, EgLaw.h  knob smoothing, EG real-time law
    HqPair.h              HQ 2x: one 2fs sub-sample per statement, earlier first, into the shared halfband
  UI/                     panel patch bay (MAIN), tab strip and VOICE / ENV / PATCH / SETUP pages, real-unit read-outs
  Tests/                  RoninTests (JUCE-free) and ProcessorTests.cpp (RoninProcessorTests, needs JUCE)
third_party/jidai-common/ vendored shared JCS + DSP headers (commit in VENDOR.md; no local edits)
panel/                    layout, SVG, geometry emitter
tools/PanelProbe.cpp      standalone window check
tools/vst3_load_check.cpp loads a built bundle as a host: vendor, Fx class, INIT wet first block, dry after Effect off
scripts/install_fl_plugin.sh   macOS: build the universal Release VST3 and point FL Studio at it
scripts/sine_through_fx.py     plays a sine through a built bundle at mix 0
docs/                     the developer documents above
```

Internal names follow the product name: the CMake targets (`Ronin`, `RoninTests`, `RoninProcessorTests`, `RoninPanelProbe`), `RoninAudioProcessor`, and the `RONIN/` jack-id prefix in the web rack. The plugin code stays `Rnin`. Saved state is format 2 XML (`<RONIN format="2">`, JCS R7); the old `RNIN` format-1 blob still loads and is migrated.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target RoninTests RoninProcessorTests Ronin_VST3 RoninPanelProbe
./build/RoninTests              # exit 0; look for SINE_DRY PASS
xvfb-run -a ./build/RoninProcessorTests_artefacts/Release/RoninProcessorTests   # processor-level tests (HQ, TRI, state)
xvfb-run -a build/RoninPanelProbe_artefacts/Release/RoninPanelProbe   # Linux; prints PROBE PASS
```

Requirements: CMake 3.22 or newer, a C++20 compiler and git. JUCE 8.0.4 is fetched by CMake, not vendored. Format: VST3. `IS_SYNTH` false. MIDI input off.

FL Studio on macOS does not open the Debug bundle. After a VST3 change, quit FL Studio and Plugin Manager, then run `scripts/install_fl_plugin.sh`, then Find plugins in FL Studio. The script publishes one universal Release bundle through `~/Library/Audio/Plug-Ins/VST3/RONIN.vst3`. The procedure is in `docs/BUILD_GUIDE.md` under FL Studio install (2026-10-05).

## Work rules for coding agents

* Do not contradict a CONFIRMED finding. If the code would be simpler by merging Hz/V and OCT/V, the code is wrong.
* If a number is UNKNOWN, use the stand-in id already assigned. Do not invent a second constant for the same quantity. New unknowns get a new id in `docs/METHODOLOGY.md` in the same change.
* Keep the integrator on the rack. Do not fold lag into the VCO.
* DSP modules do not include JUCE headers. The processor copies buffers in and out.
* `process()` and `processSample()` do not allocate, lock, or log.
* Inputs sum. A second cable into an input stays in the graph. Stack order, cable color, and cable shape do not change the sound. The cable rule is in `docs/METHODOLOGY.md`.
* The MAIN tab is `panel/assets/panel.svg`, unchanged (RONIN_Redesign §4.0). Do not add, move or resize anything on the face; new controls go on a tab. Only transient overlays (hover read-outs, right-click boxes and menus) and cable colours may draw over it. Run the pixel check in `docs/TESTPLAN.md` after touching `PatchBayView`.
* Cross-unit signal rules come from the vendored jidai-common through `Source/Modular/Jcs.h` (`jcs::`). Do not hard-code a threshold, gate level or role colour elsewhere, and do not edit `third_party/jidai-common/`: update it from upstream (see its VENDOR.md).
* Never pass two stateful calls as arguments of one call (`f (a.process(), a.process())`): C++ leaves the order unspecified and g++ evaluates right to left. One sub-sample per statement (`testHqSubSampleOrder`).
* Knob defaults live in one table, `Source/Modular/PanelDefaults.h`. A test checks that FaceKnobs, `panel/assets/layout.json` and the INIT program agree.
* Do not commit DAW projects, samples, `.env` files, or schematic scans.
* UI work that changes a control or a cable must be clicked through in a real plugin host or a standalone window (the panel probe counts) before it is called done. Say which host.
* After a VST3 change that should load in FL Studio, run `scripts/install_fl_plugin.sh`. Do not copy a second bundle into `/Library/Audio/Plug-Ins/VST3`.
