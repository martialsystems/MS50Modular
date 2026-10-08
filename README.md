# RONIN

**A semi-modular synthesizer you patch as an effect. Part of the Jidai Collection by Martial Systems.**

RONIN is a full rack of analog-style modules inside one plugin. Run your track through it, or let it make its own sound: a VCO, a resonant diode-bridge filter, two VCAs, two envelopes, an LFO, noise, a ring modulator, sample and hold and more, all wired with patch cables you drag across the panel. Patch the filter into its own cutoff, ring-modulate a signal with itself, or gate the whole rack from your input. Then blend it back in with one MIX knob.

## Highlights

- **A whole rack.** VCO (saw, triangle and pulse, with Hz/V and OCT/V inputs), VCF (a resonant two-pole diode-bridge lowpass that can self-oscillate), VCA 1 and VCA 2, EG 1 (ADSR), EG 2 (hold, delay, attack, release), MG (0.01 Hz to 200 Hz), white and pink noise, a ring modulator, sample and hold, a /2 and /4 divider, an inverter, an integrator (slew), a three-input mixer, and stereo EXT IN and OUTPUT.
- **Real patching.** Drag cables between jacks. Inputs sum, outputs fan out, and plugs stack on a jack. Cables hang and swing, and you can reorder a stack with a click.
- **Feedback is allowed.** Close a loop and RONIN delays only the newest cable by one sample, the way a hardware rack behaves, so self-modulation stays stable and repeatable.
- **An effect first.** Stereo in, stereo out. EXT IN turns your track into a signal, a mono sum and a gate with its own Threshold and Release. The EFFECT switch and MIX knob blend RONIN's patch with your dry signal, and LEVEL sets the output.
- **INIT program.** The factory bank is cleared for now and holds only INIT, a filtered voice on your input. New factory programs will be written later.
- **Every knob is automatable.** Each panel control is a host parameter, and double-click returns it to its default.
- **A VU meter on any jack.** Click a jack and the meter reads it (±5 V full scale).
- **Built to be sequenced.** Patch BUSHIDO, RONIN's Jidai Collection partner, into the Hz/V and trigger inputs and RONIN plays sequences.

## Formats

VST3 effect, macOS (universal: Apple silicon and Intel) and Linux. Version 0.1 is a beta.

## Quick start

1. Put RONIN on an audio track or bus. It opens on **01 INIT**: your track runs through the filter and VCA, and EG 1 opens them when the input crosses the EXT IN threshold.
2. Click the **PRESET** screen to pick a program. For now the list holds only INIT, and your own patches are saved with the DAW project.
3. Drag from any jack to another to patch. Drop a plug on empty space to unplug, right-click a cable to remove it, or click a jack to pick from its stack.
4. Hold the **HOLD** key to open the EXT IN gate by hand.
5. Use **MIX** to blend wet with dry, and the **EFFECT** switch for a quick bypass to dry.

## Build from source

You need CMake 3.22+, a C++20 compiler and git. JUCE 8 is fetched automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Ronin_VST3
```

The plugin lands in `build/Ronin_artefacts/Release/VST3/RONIN.vst3`. On macOS with FL Studio, `scripts/install_fl_plugin.sh` builds the universal bundle and installs it for you. For tests, the panel probe and the developer documents, see `docs/DEVELOPING.md`.

## The Jidai Collection

The Jidai Collection is Martial Systems' line of patchable instruments. Its pieces share one patch format and one cable feel.

- **BUSHIDO**: a 3 x 12 analog step sequencer.
- **RONIN**: the semi-modular synthesizer and effect.

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See `LICENSE`.

RONIN is inspired by classic Korg gear. Korg is a trademark of its owner. Martial Systems is not affiliated with or endorsed by Korg.
