# Build guide

Date: 2026-09-21.

Implement one step, run its acceptance test, then stop. A later step that seems small is still a later step. If the test fails, fix it inside the same step. If the fix needs a change to `METHODOLOGY.md` or `SCHEMATICS.md`, make that change in the same branch and say why. Do not silently retune a stand-in.

Branch name: `feat/step-XX` with XX from 00 to 20. Open it from `main` after the previous step is merged.

Global ban, every step: no MIDI note path, no neural net, no phase 2 module, no second copy of a phase 1 module, no heap use in `processBlock` or `processSample`, no Korg drawing scans.

JUCE is fetched at tag `8.0.4` unless that tag will not configure, in which case stop and record the tag that did. Do not vendor the JUCE tree.

## Step 0: repo, docs, license

Files: `README.md`, `METHODOLOGY.md`, `LICENSE`, `.gitignore`, `docs/01-research.md`, `docs/SCHEMATICS.md`, `docs/BUILD_GUIDE.md`, `docs/TESTPLAN.md`, `docs/REPO_SETUP.md`, `docs/MS50_Modular_Design_Pack.pdf`, `scripts/init-repo.sh`, `scripts/build_design_pdf.py`.

Acceptance: `python3 scripts/build_design_pdf.py` exits 0 and the PDF opens. `git status` shows no audio files and no schematic scans. A search of the markdown finds no second stand-in number for a quantity that already has an id.

Do not touch: nothing exists yet besides these files. Do not add `CMakeLists.txt`.

Rollback: delete the commit on the design branch before it is pushed. After it is pushed, revert the commit. Do not rewrite `main` if anyone else has it.

## Step 1: empty JUCE FX plugin builds

Files to create:

*   `CMakeLists.txt` as below
*   `Source/PluginProcessor.h`
*   `Source/PluginProcessor.cpp`
*   `Source/PluginEditor.h`
*   `Source/PluginEditor.cpp`

`CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)
project(MS50Modular VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MS50Modular
  COMPANY_NAME "Personal"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  PLUGIN_MANUFACTURER_CODE Psnl
  PLUGIN_CODE Ms50
  FORMATS VST3
  PRODUCT_NAME "MS-50 Modular")

target_sources(MS50Modular PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(MS50Modular PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MS50Modular PRIVATE
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags)
```

Processor: stereo in, stereo out, `isBusesLayoutSupported` accepts only stereo. `processBlock` copies input to output. Editor: a label "MS-50 Modular" and the words "no patch yet".

Acceptance:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

The VST3 bundle exists under `build/`. Load it in a host. Stereo audio passes, bit-identical to the input aside from host float handling. MIDI notes do nothing because MIDI input is off.

Do not touch: docs, except a one-line "step 1 builds" note if the JUCE tag had to change.

Rollback: remove `CMakeLists.txt` and `Source/` and the `build/` directory. `build/` is gitignored.

## Step 2: Port, Cable, Module, PatchGraph

Files: `Source/Modular/Port.h`, `Module.h`, `Module.cpp`, `Cable.h`, `PatchGraph.h`, `PatchGraph.cpp`, `Source/Tests/GraphTests.cpp`, and a CMake target `MS50ModularTests` that is a console app, not a plugin. It links the modular sources and does not link the plugin target.

Implement the rules in `SCHEMATICS.md` up to, but not including, delayed feedback. A cycle is rejected. One cable per input. Fan-out allowed. Type matrix enforced. Two snapshots, atomic index, no alloc in a method named `process`.

`Module` is the base from the schematic sketch. No concrete synth modules yet. Tests use a `GainModule` in the test file only, gain fixed at 1, not on the rack.

Acceptance: `MS50ModularTests` runs `testOneCablePerInput`, `testFanOutAllowed`, `testRejectSignalIntoGate`, `testRejectCycle`, `testSnapshotSwapDoesNotAllocate` (the process function's source does not call `new` or `push_back`; a debug counter of allocations stays 0 across 1000 samples).

Do not touch: `PluginProcessor` audio behavior, the editor layout.

Rollback: delete `Source/Modular` and `Source/Tests` and the test target. The plugin from step 1 still passes audio.

## Step 3: Ext In, Output, cable copy

Files: `Source/Modular/ExtIn.h/.cpp`, `OutputModule.h/.cpp`. Processor owns one `PatchGraph`, one Ext In, one Output. It converts host floats using S-01 and runs the graph.

Default cables for this step only: Ext In L to Output L, Ext In R to Output R. No wet cable yet. Mix 0.

Acceptance: `testDryMixPassesStereo` and `testExtInMonoAveragesStereo`. In a host, a stereo file played through the plugin at mix 0 matches the dry file within -90 dBFS residual. Mono material on the left only stays on the left.

Do not touch: cable drawing, VCF, any other module.

Rollback: processor returns to the step 1 copy. Keep the graph types.

## Step 4: GUI rack and jacks, no DSP change

Files: `Source/UI/RackView.h/.cpp`, `JackView.h/.cpp`. Editor shows the 14 faceplates, all of them, including Integrator, even though only Ext In and Output process audio. Other faceplates are inert and their jacks do not connect yet. Each jack is a circle with a type color: audio amber, CV blue, gate white.

Acceptance: the plugin still passes the step 3 dry test. The window lists all 14 names from the README table. Resizing the editor does not move audio. A search shows `RackView` does not call `processSample`.

Do not touch: graph rules, Ext In math.

Rollback: editor back to the step 1 label. Keep the view files only if they compile out via the editor.

## Step 5: animated cables

Files: `Source/UI/CableView.h/.cpp`. Draw one cubic from jack center to jack center for each cable in the published snapshot. Animation is a dash phase advanced by a `Timer` on the message thread, 30 Hz is enough. No timer on the audio thread. No allocation per frame: a `juce::Path` member is rebuilt only when endpoints move.

Acceptance: with the two dry cables, both cables are visible and the dashes move. CPU of the audio thread does not include `CableView` (it is not called from `processBlock`). Dry audio test still passes.

Do not touch: DSP modules.

Rollback: draw nothing. Cables still exist in the graph.

## Step 6: click-drag patching

Files: `RackView` mouse handlers, `PatchGraph::connect` / `disconnect` called only from the message thread.

Drag from an output to an input. On illegal pairs, refuse and show a one-line status string: "that jack does not take this cable" or "input already has a cable". Right-click a cable to remove it. Creating a cycle shows "feedback is not available until step 19" and does not connect. That string changes in step 19.

Acceptance: `testOneCablePerInput` still passes when the UI path is used (call `connect` the way the UI calls it). A manual check: drag Ext In Mono onto Output Wet, then set mix to 1, and a mono sum is heard on both speakers. Drag a second cable onto Output Wet and the first remains.

Do not touch: module DSP that does not exist yet.

Rollback: ignore mouse-up. Keyboard of cables is not required.

## Step 7: Noise

Files: `Source/Modular/Noise.h/.cpp`. Add it to the rack's process list. Faceplate already exists; wire the two jacks.

Acceptance: `testNoiseBothJacksMove`, `testNoiseSeedRepeats`, `testNoisePinkIsDarkerThanWhite`, `testNoiseHasNoKnobs`. Patch White to Output Wet, mix 1, and confirm the host meters move. No level knob on the faceplate.

Do not touch: VCF, VCO, graph rules.

Rollback: remove Noise from the process list. Leave the faceplate.

## Step 8: VCF stand-in

Files: `Source/Modular/Vcf.h/.cpp`. Implement step 8 formulas only (S-07, S-08, S-09). Do not include S-10 diode code in this step. A comment at the top of `Vcf.cpp` says "STAND-IN step 8, replaced in step 20".

Wire knobs on the faceplate: Cutoff, Peak, Cutoff amount.

Cables that may exist at the end of this step: Ext In L to Output L, Ext In R to Output R, Ext In Mono to VCF SigIn, VCF SigOut to Output Wet. Cables that mention VCA 1 or EG 1 wait for steps 9 and 10. Do not claim the eight-cable default patch yet.

Acceptance: the three VCF tests in `SCHEMATICS.md`. A host tone through Mono, the VCF, and Output Wet is quieter when cutoff is 0 than when cutoff is 1, and it does not explode at peak 1. Residual after 2 seconds of a loud sine is finite. Mix 1 for that listen. Mix 0 still passes dry stereo on the L/R cables.

Do not touch: VCA law, MG, the graph cycle rule.

Rollback: bypass VCF (`SigOut = SigIn`) and say so in the editor status line. Do not leave a half-updated biquad.

## Step 9: VCA 1 and VCA 2

Files: `Source/Modular/Vca1.h/.cpp`, `Vca2.h/.cpp`. Different classes. No shared "Vca" with a mode flag.

Rewire the wet path: VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet. Remove the direct VCF-to-Output cable from step 8.

Acceptance: all VCA tests in the schematic. The test harness sets VCA 1 Env to +5 V when it needs a tone. In the plugin, until step 10, Wet is silent because Env is unpatched at 0 V. That silence is correct. Do not add a secret initial gain to make it louder. Dry mix 0 still passes.

Do not touch: VCF formulas, Ext In gate math beyond what step 3 already did.

Rollback: VCA 1 output 0. VCA 2 output 0. Do not route around them with a hidden gain.

## Step 10: EG 1

Files: `Source/Modular/Eg1.h/.cpp`. Wire the default cables that use OutA. Ext In Gate promotes into Trig.

Add the remaining default cables: Ext In Gate to EG 1 Trig, EG 1 OutA to VCA 1 Env, EG 1 OutA to VCF Cutoff. The patch now matches the eight-cable list in `SCHEMATICS.md`.

Acceptance: EG1 tests in the schematic. Manual: hold the Ext In button, a tone at the input, mix 1, and the sound follows attack and release. Release the button and the sound dies. OutB patched to a meter or to VCA 2 control in a throwaway test cable moves the opposite way. Remove that throwaway cable before committing. Unpatched Trig rests at +5 V and does not hold the envelope open.

Do not touch: EG 2, VCF diode work.

Rollback: leave VCA 1 Env unpatched (0 V) so the default patch goes silent rather than stuck open.

## Step 11: MG

Files: `Source/Modular/Mg.h/.cpp`. Faceplate title `MG`.

Acceptance: MG tests in the schematic, including unipolar pulse and 0.01 Hz to 200 Hz. Listen: patch Tri to VCF Cutoff with the default OutA cable removed for the listen, then restore the default cables before commit. The listen is not the acceptance test. The test is.

Do not touch: VCO, EG 2.

Rollback: MG outputs 0. Frequency knob stays on the panel.

## Step 12: VCO

Files: `Source/Modular/Vco.h/.cpp`. PolyBLEP stand-in. `inputConnected` flags from the graph, as the schematic requires, so an unpatched Hz/V jack does not fall to the 0.05 floor.

Acceptance: VCO tests in the schematic. Listen: saw into Output Wet, scale switch changes pitch, Oct/V from the volt-free test (a test-only +1 V) doubles frequency. Do not add a keyboard to perform that test.

Do not touch: filter code, MG waveform code (do not "share an oscillator class" if that merges their levels; MG is ±2.5 V and the VCO stand-in is ±5 V).

Rollback: VCO outputs 0. Keep the jacks.

## Step 13: EG 2

Files: `Source/Modular/Eg2.h/.cpp`. No decay parameter. No sustain parameter. If a knob widget is added for either, the step fails.

Acceptance: EG2 tests, including no plateau and delay-trig timing. Patch DelayTrig to EG 1 Trig in place of Ext In Gate for a manual check, then restore the default patch.

Do not touch: EG 1 state machine. Do not generalize both EGs into one class with a mode enum. Two classes may share a private one-pole segment helper if it contains no knob policy.

Rollback: EG 2 outputs 0 and DelayTrig low.

## Step 14: Ring

Files: `Source/Modular/Ring.h/.cpp`.

Acceptance: ring tests. Manual: Noise White into A, MG Tri into B, Out into Output Wet. The result is not the sum of the two inputs (sum would still show the MG alone if one input is silent; product goes quiet if either side is 0). Restore the default patch after listening.

Do not touch: VCF. Do not reuse ring code as the filter nonlinearity.

Rollback: ring output 0.

## Step 15: Divider

Files: `Source/Modular/Divider.h/.cpp`.

Acceptance: divider tests. No ÷8 jack on the faceplate. Manual: VCO pulse into divider, Div2 into Output Wet, pitch drops one octave relative to the pulse. Pulse stand-in is bipolar ±5 V (S-02), and the Schmitt still triggers because ±5 V crosses ±0.5 V. If it does not, fix the Schmitt window in S-18's constant, in this step, and record the edit.

Do not touch: VCO pitch laws.

Rollback: both divider outputs 0.

## Step 16: Inverter

Files: `Source/Modular/Inverter.h/.cpp`.

Acceptance: inverter tests. Manual: EG 1 OutA into the inverter into VCF Cutoff, with cable 8 removed. The filter moves the opposite way from OutA into the cutoff. Restore cable 8.

Do not touch: adding-amp code. Do not add an offset knob.

Rollback: output 0, which is wrong musically but safe. Prefer rollback to `Out = In` only if a test name says so. This step's rollback is `Out = 0` so a bad negate cannot ship as "wire".

## Step 17: Integrator

Files: `Source/Modular/Integrator.h/.cpp`. The rack already shows the faceplate from step 4. This step makes it compute.

Acceptance: integrator tests, including settle-to-input and "module is on the rack." Manual: a fast MG saw through a slow integrator into Output Wet is duller than the dry saw. A + constant (use VCA 2 with a held gate-promoted control and a DC input from Ext In of a quiet DC, or a unit test) settles. The unit test is the acceptance. The listen is extra.

Do not touch: VCO fine tune. Do not move glide inside the VCO.

Rollback: `Out = In` would hide a broken lag. Rollback is `Out = 0` plus a status line "integrator bypassed to zero".

## Step 18: state save and load

Files: `PatchGraph::getState` / `setState`, processor `getStateInformation` / `setStateInformation`.

Store version integer 1, every knob, the scale index, and cables as integer ids. Do not store filter memory or the noise seed. On load, clear module state via the existing `prepare` reset path.

Acceptance: `testPresetRoundTrip` builds the default patch, moves cutoff and adds one extra legal cable, saves, loads into a fresh graph, and sees the same knobs and cables. `testPresetRejectsBadVersion` keeps the previous patch. Host check: save a DAW project, reopen, cables match. The DAW project file is not committed.

Do not touch: DSP formulas.

Rollback: `getStateInformation` writes an empty block and `setState` no-ops. Document that presets will not survive.

## Step 19: topological order and feedback delay

Files: `PatchGraph` sort and delay. Update the UI string from step 6.

Acceptance: `testFeedbackIsOneSample` builds a one-module cycle through a test gain of 1 (input + output cable loop). The delayed edge is the newest cable. An impulse returns on the next sample, not the same sample. `testNoAllocInProcess` still passes. Two legal feedback cables in one cycle: only the newest is delayed. An older cycle-free patch (the default eight cables) has zero delayed cables.

Do not touch: module formulas. Do not delay every cable "to be safe."

Rollback: reject cycles again, as in step 2. The UI string returns to the step 6 wording.

## Step 20: filter closer to the diode bridge

Files: `Vcf.cpp` only, plus the tests named for step 20. Update the file header to "step 20, S-10". Keep the jacks and knob ids stable so presets from step 18 still point at Cutoff, Peak, and Amount.

Implement the S-10 notes in the schematic: paper Is and n marked PAPER-SUBSTITUTE, tanh stand-in for D5 to D12, a small input-level pull, 5 Hz output highpass stand-in, no hard-coded 250 Hz.

Acceptance: step 8 tests that are still meaningful (`testVcfPositiveCvRaisesCutoff`, finite output) plus `testVcfStaysFiniteWhenDrivenHard` and `testVcfHotInputMovesSpectrum`. Listen to Gladén's video using the procedure in `METHODOLOGY.md`. Write `docs/listening/step-20.md` with the three lines the methodology asks for (matched, not matched, which id). That listening note is committed. Recordings are not.

Do not touch: VCO, VCA classes, EG classes, graph rules. Do not replace the VCF with a copied WDF framework.

Rollback: restore the step 8 biquad in `Vcf.cpp` and say "S-10 reverted" in the listening note. Keep the step 20 tests compiling against the old behavior only if you mark them skipped. Do not delete the listening note.

## After step 20

Stop. Phase 2 is a new packet. Do not start the adding amplifier because fan-out made it look easy.
