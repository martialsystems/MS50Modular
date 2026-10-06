Copyright (c) 2026 Martial Systems LLC. All rights reserved. RONIN is part of the Jidai Collection.

# Developing RONIN

Read this before changing code. The DSP behaviour is specified in `docs/SCHEMATICS.md`, the stand-in rules in `docs/METHODOLOGY.md`, the build order in `docs/BUILD_GUIDE.md` and the tests in `docs/TESTPLAN.md`. Factory programs and the default knob table are in `docs/presets.md`.

## Repo map

```text
README.md                 product page
LICENSE                   copyright and trademark notice
CMakeLists.txt            plugin (target MS50Modular, product name RONIN), tests, panel probe
Source/
  PluginProcessor.h/.cpp
  PluginEditor.h/.cpp
  Modular/                graph and modules, no JUCE types in process()
  UI/                     panel patch bay
  Tests/                  MS50ModularTests
panel/                    layout, SVG, geometry emitter
tools/PanelProbe.cpp      standalone window check
tools/vst3_load_check.cpp loads a built bundle and processes a dry block
scripts/install_fl_plugin.sh   macOS: build the universal Release VST3 and point FL Studio at it
scripts/sine_through_fx.py     plays a sine through a built bundle at mix 0
docs/                     the developer documents above
```

Internal names (the CMake targets, `MS50ModularAudioProcessor`, the `MS-50/` jack-id prefix in the web rack) predate the RONIN name and stay, so saved sessions and patches keep loading.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target MS50ModularTests MS50Modular_VST3 MS50PanelProbe
./build/MS50ModularTests              # exit 0; look for SINE_DRY PASS
xvfb-run -a build/MS50PanelProbe_artefacts/Release/MS50PanelProbe   # Linux; prints PROBE PASS
```

Requirements: CMake 3.22 or newer, a C++20 compiler and git. JUCE 8.0.4 is fetched by CMake, not vendored. Format: VST3. `IS_SYNTH` false. MIDI input off.

FL Studio on macOS does not open the Debug bundle. After a VST3 change, quit FL Studio and Plugin Manager, then run `scripts/install_fl_plugin.sh`, then Find plugins in FL Studio. The script publishes one universal Release bundle through `~/Library/Audio/Plug-Ins/VST3/RONIN.vst3` and removes the old `MS-50 Modular.vst3` link and scan records. The procedure is in `docs/BUILD_GUIDE.md` under FL Studio install (2026-10-05).

## Work rules for coding agents

* Do not contradict a CONFIRMED finding. If the code would be simpler by merging Hz/V and OCT/V, the code is wrong.
* If a number is UNKNOWN, use the stand-in id already assigned. Do not invent a second constant for the same quantity. New unknowns get a new id in `docs/METHODOLOGY.md` in the same change.
* Keep the integrator on the rack. Do not fold lag into the VCO.
* DSP modules do not include JUCE headers. The processor copies buffers in and out.
* `process()` and `processSample()` do not allocate, lock, or log.
* Inputs sum. A second cable into an input stays in the graph. Stack order, cable color, and cable shape do not change the sound. The cable rule is in `docs/METHODOLOGY.md`.
* Knob defaults live in one table, `Source/Modular/PanelDefaults.h`. A test checks that FaceKnobs, `panel/assets/layout.json` and the Voice program agree.
* Do not commit DAW projects, samples, `.env` files, or schematic scans.
* UI work that changes a control or a cable must be clicked through in a real plugin host or a standalone window (the panel probe counts) before it is called done. Say which host.
* After a VST3 change that should load in FL Studio, run `scripts/install_fl_plugin.sh`. Do not copy a second bundle into `/Library/Audio/Plug-Ins/VST3`.
