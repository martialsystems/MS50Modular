Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The Korg MS-50, the MS-50 name, and the circuit designs of that instrument are the property of Korg Inc. Martial Systems LLC claims copyright only in the original text of this repository and in any code later written here. The work is an independent study of published schematics and of the literature cited in the research summary. Korg has not produced, sponsored, or endorsed it. No license is granted to the MS-50 design, to the Korg drawings, or to the Korg trademarks. The instrument's name is used only to identify the subject of the study.

# MS-50 Modular

Personal white-box FX plugin modeled on the module set of the Korg MS-50 (1978). Stereo audio enters, a fixed rack of mono modules processes a patched signal, and stereo audio leaves. Cables are visible and animated. The instrument is the patch.

This packet is the build specification. The research it may not contradict is `docs/01-research.md`. DSP behavior is `docs/SCHEMATICS.md`. The order of implementation is `docs/BUILD_GUIDE.md`.

Drawing dates used as evidence: 78-11-8 and 78-11-9, boards KLM-164 / KLM-164B and KLM-165B.

## What this is

A VST3 FX plugin (JUCE 8, C++20, CMake) with one instance of each phase 1 module, free patching between their jacks, and a mono internal graph. The filter target is the MS-50 diode-bridge 2-pole lowpass (CA3019 in a Sallen-Key), not the MS-20 Korg-35. The modulation generator keeps its panel name, MG. The integrator stays a rack module with jacks.

Phase 1 rack, fixed, not a spawner:

| UI name | Schematic name on the 1978 sheets | Role |
|---|---|---|
| Ext In | Plugin I/O. Not an MS-50 module. | Stereo in, mono sum, gate stand-in |
| VCO | VCO, KOD-A40044 | Saw, triangle, pulse together. Hz/V and OCT/V are different jacks |
| VCF | VCF, KOD-A40045 | Diode-bridge lowpass target. Early steps use a labeled 2-pole stand-in |
| VCA 1 | VCA, KOD-A40045 | Audio VCA. Manual low-cut. Intensity is an output attenuator |
| VCA 2 | MVCA, KOD-A40043 | Optocoupler modulation VCA. DC-coupled. No panel knobs |
| MG | MG, KOD-A40048 | 0.01 Hz to 200 Hz. Triangle, two saws, unipolar pulse |
| EG 1 | EG1 ADSR, KOD-A40046 | Attack, decay, sustain, release. Three outputs |
| EG 2 | EG-2 HDAR, KOD-A40047 | Hold, delay, attack, release. No decay. No sustain |
| Noise | NOISE GENERATOR, KOD-A40042 | White and pink |
| Divider | DIVIDER, KOD-A40041 | Divide by 2 and divide by 4 only |
| Inverter | INVERTOR, KOD-A40043 | Gain of -1, DC-coupled |
| Integrator | INTEGRATOR, KOD-A40039 | Lag / slew. One TIME knob |
| Ring | RM, KOD-A40040 | RC4200 four-quadrant multiplier |
| Output | Plugin I/O. Not the headphone amp | Dry stereo plus mono wet, mix and level |

## What it is not

*   Not a Korg product, not affiliated with Korg, and not a copy of the Korg drawings. Those drawings stay outside the repo. Names are used to identify the instrument that was studied.
*   Not a keyboard synth. No MIDI note input, no piano roll, no hidden keyboard-to-Hz path.
*   Not a neural, sample, or black-box model of a recording.
*   Not the MS-20. Envelope numbering, filter circuit, and jack laws differ. MS-20 panel voltages are not copied onto unlabeled MS-50 jacks.
*   Not a module browser. One of each phase 1 module, as on the panel.
*   Not phase 2. Adding amplifier, sample and hold, external signal processor, multiples, manual trigger switch, volt source, meter, and headphone amp are specified in `docs/SCHEMATICS.md` and are not built in steps 0 to 20.

## Features in the finished phase 1 plugin

*   Stereo in and stereo out. Processing modules are mono.
*   Click-drag patch cables, one cable per input. An output may feed several inputs (stand-in S-27).
*   Animated cables drawn on the message thread.
*   Port colors: audio, CV, gate. Illegal cables are refused.
*   No heap allocation on the audio thread.
*   Feedback is legal only with a one-sample delay on a chosen back-edge (step 19).
*   State save and load of knobs and cables (step 18).
*   Every unknown hardware quantity sits behind a `STAND-IN` id from `docs/METHODOLOGY.md`.

## Default patch

Audible FX patch. Mix defaults to fully wet, so a silent input is silent: VCA 1 has no initial gain.

Cables:

1. Ext In `Mono` to VCF `SigIn`
2. VCF `SigOut` to VCA 1 `SigIn`
3. VCA 1 `Out` to Output `Wet`
4. Ext In `L` to Output `L`
5. Ext In `R` to Output `R`
6. Ext In `Gate` to EG 1 `Trig`
7. EG 1 `OutA` to VCA 1 `Env`
8. EG 1 `OutA` to VCF `Cutoff` (fan-out from OutA)

Knobs at first load: see the default column in `docs/SCHEMATICS.md`. Short version: VCF cutoff mid, peak low, cutoff attenuator partly up, VCA 1 intensity high, low-cut at minimum, EG 1 a medium ADSR, Ext In gate threshold mid, Output mix fully wet.

Dry check: set Output mix to 0. Left and right pass through cables 4 and 5. The wet chain is ignored.

Synth-like check, not the default: unplug Ext In `Mono` from the VCF, patch VCO saw to VCF `SigIn`, and fire EG 1 from the Ext In manual button (stand-in for the phase 2 TRIG SW). Use `CV In OCT/V` only for a 1 V/octave source. Use `CV In Hz/V` only for a Hz/V source.

## Repo map

```text
MS50Modular/
  README.md                 front door
  METHODOLOGY.md            evidence rules, stand-ins, done definition
  LICENSE                   copyright, and the Korg notice
  .gitignore
  scripts/init-repo.sh      first commit helper
  scripts/build_design_pdf.py
  docs/01-research.md       functional evidence
  docs/SCHEMATICS.md        software schematic
  docs/BUILD_GUIDE.md       steps 0 to 20
  docs/TESTPLAN.md
  docs/REPO_SETUP.md
  docs/MS50_Modular_Design_Pack.pdf
  Source/                   created in step 1, not in the design-pack commit
    PluginProcessor.h/.cpp
    PluginEditor.h/.cpp
    Modular/                graph and modules, no JUCE types in process()
    UI/                     rack, jacks, cables
```

The design-pack commit is documents, license, gitignore, and scripts. `CMakeLists.txt` and `Source/` arrive in step 1 so this commit does not contain a plugin that does not build.

## How to open, build, and test

Design pack only (this commit):

```bash
git clone https://github.com/martialsystems/MS50Modular.git
cd MS50Modular
python3 scripts/build_design_pdf.py
```

The PDF writer needs `reportlab` (`python3 -m pip install reportlab`).

Plugin build starts at step 1 of `docs/BUILD_GUIDE.md`. Expected shape, after that step exists:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug
```

Requirements: CMake 3.22 or newer, a C++20 compiler, git (JUCE 8 is fetched by CMake, not vendored). Format: VST3 only. `IS_SYNTH` false. MIDI input off.

Tests: `docs/TESTPLAN.md`. Each build-guide step names one acceptance command or one listening check. Do not skip to a later step because a later feature seems small.

## Work rules for coding agents

Read these three files before editing code: `docs/01-research.md`, `docs/METHODOLOGY.md`, `docs/SCHEMATICS.md`. Then implement exactly one step from `docs/BUILD_GUIDE.md`.

*   Do not contradict a CONFIRMED finding. If the code would be simpler by merging Hz/V and OCT/V, the code is wrong.
*   If a number is UNKNOWN in the research, use the stand-in id already assigned. Do not invent a second constant for the same quantity. New unknowns get a new id in `METHODOLOGY.md` in the same change.
*   Do not add modules, a keyboard, MIDI notes, a neural net, a preset browser of factory MS-50 sounds, or phase 2 blocks during steps 0 to 20.
*   Keep the integrator on the rack. Do not fold lag into the VCO.
*   DSP modules do not include JUCE headers. The processor copies buffers in and out.
*   `process()` and `processSample()` do not allocate, lock, or log.
*   One cable per input. Reject the second cable in the graph, not only in the UI.
*   Branch `feat/step-XX` off `main`. One step per branch. Do not start step N+1 on a branch whose acceptance test fails.
*   When that step's acceptance checks pass, merge the branch into `main` and push `main` before the next step. Leave it unmerged only when the user says to keep it on the branch.
*   Do not commit DAW projects, samples, `.env` files, or scans of the Korg schematics.
*   UI work that changes a control or a cable must be clicked through in a real plugin host or a standalone window before the step is called done. Say which host.

## License note

The statement at the head of this file is the rights notice for the repository. `LICENSE` carries the same statement, together with the limits on publication and warranty.
