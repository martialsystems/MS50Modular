# RONIN

**A semi-modular synthesizer you patch as an effect. Part of the Jidai Collection by Martial Systems.**

RONIN puts a whole analog-style rack inside one plugin. Run your track through it, or let it make its own sound: a VCO, a resonant diode-bridge filter, two VCAs, two envelopes, an LFO, noise, a ring modulator, sample and hold and more, all wired with patch cables you drag across the panel. Patch the filter into its own cutoff, ring-modulate a signal with itself, or gate the whole rack from your input, then blend it back in with one MIX knob.

## Features

### Sound sources
- **VCO** with saw, triangle and pulse outputs, pulse-width modulation, a footage switch (32', 16', 8', 4') and fine tune.
- **Two pitch inputs.** HZ/V is a linear Hz-per-volt input. V/OCT follows the Jidai standard, where 0 V plays C3 at 8', so BUSHIDO and the JIDAI rack play RONIN in tune with no calibration.
- **True triangle.** New patches use a band-limited triangle with only odd harmonics. A PARABOLA shape is kept for the original rounder tone.
- **Noise** in white and pink.

### Filter and amplifiers
- **VCF:** a resonant two-pole diode-bridge lowpass that self-oscillates, with an input-level pull that lets hot signals darken the filter. Its response is the same at every sample rate.
- **VCA 1**, an AC-coupled amplifier, and **VCA 2**, an opto-style amplifier with a smooth lag.

### Envelopes
- **EG 1 (ADSR)** and **EG 2 (hold, delay, attack, release)**, with S-trig inputs.
- **Times you can trust.** Each time knob shows the real duration from 1 ms to 60 s, and long attacks always reach full level.
- **Clean triggering.** Trigger inputs use hysteresis, so slow or noisy signals give one clean edge. EG 2's DELAY OUT fires a 5 V pulse.

### Modulation and utilities
- **MG:** an LFO from 0.01 Hz to 200 Hz with saw up, saw down, a morphing triangle and pulse, band-limited at audio rates.
- **Ring modulator**, **sample and hold** with an internal or external clock, a **÷2 and ÷4 divider**, an **inverter**, an **integrator** (slew), and a three-input **inverting mixer**.

### An effect first
- **Stereo in and out.** EXT IN turns your track into a signal, a mono sum, an envelope and a gate, with its own Threshold and Release. The gate opens and closes cleanly, even on bass.
- **HOLD** opens the EXT IN gate by hand. The **EFFECT** switch, **MIX** and **LEVEL** blend RONIN's patch with your dry signal.

### Patching
- **Drag cables between jacks.** Inputs sum, outputs fan out, and plugs stack on a jack. Cables hang and swing, and you can reorder a stack with a click.
- **Feedback is welcome.** Every feedback loop is delayed by exactly one sample, so self-patching stays stable and sounds the same every time.
- **Cable colours by role.** Pitch, audio, gate, trigger and CV each get their own colour, or you can pick colours by hand.
- **A VU meter on any jack.** Click a jack and the meter reads it (±5 V full scale).

### Panel and tabs
The front panel keeps its open, symmetric layout. Everything else lives on tabs above it:
- **MAIN:** the panel. Hover a knob or jack for its real value (Hz, note and cents, seconds, volts), or right-click a knob to type a value.
- **VOICE:** a tuner, the triangle or parabola choice, a footage reference, the filter's live effective cutoff, and the HQ quality switch with latency and CPU readouts.
- **ENV:** both envelopes with live stage lamps, real-time readouts and a curve view.
- **PATCH:** a list of every cable and a live jack monitor with an over-range lamp.
- **SETUP:** UI scale (75 to 200 %), cable colour mode, EG time display, and a report on what changed when an older patch loaded.

### Quality and automation
- **Smooth controls.** Every knob is a host parameter and moves without zipper noise. Double-click a knob to reset it.
- **HQ mode** runs the engine at 2× for less aliasing. It's off by default, so RONIN adds no latency; with HQ on it reports 23 samples to your host.

## Formats and compatibility

- VST3 effect for macOS (universal: Apple silicon and Intel) and Linux. Version 0.1 is a beta.
- Patches save with your DAW project. Older RONIN projects are updated when they load: envelope times keep their length, the oscillator keeps the parabola shape, and filter settings are adjusted for the new input pull. SETUP shows what changed. Once re-saved, a project needs this version of RONIN or later.
- 22 factory programs on the PRESET screen, voiced for classic EDM: effects for your input (filters, a trance gate, a side-chain pump, a random filter and a ring modulator), six self-playing acid basses ready for a BUSHIDO sequence, basses, a lead, drums and sound effects. Each one says what input it expects; see `docs/presets.md`.

## Quick start

1. Put RONIN on an audio track or bus. It opens on **01 INIT**: your track runs through the filter and VCA, and EG 1 opens them when the input crosses the EXT IN threshold.
2. Drag from any jack to another to patch. Drop a plug on empty space to unplug, right-click a cable to remove it, or click a jack to pick from its stack.
3. Hold the **HOLD** key to open the EXT IN gate by hand.
4. Use **MIX** to blend wet with dry, and the **EFFECT** switch to go back to dry.

## Build from source

You need CMake 3.22+, a C++20 compiler and git. JUCE 8 is fetched automatically.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Ronin_VST3
```

The plugin lands in `build/Ronin_artefacts/Release/VST3/RONIN.vst3`. On macOS with FL Studio, `scripts/install_fl_plugin.sh` builds the universal bundle and installs it for you. For tests and developer documents, see `docs/DEVELOPING.md`.

## The Jidai Collection

The Jidai Collection is Martial Systems' line of patchable instruments. They share one cable standard, so any output can be patched into any input across units.

- **BUSHIDO:** a 3 × 12 analog-style step sequencer.
- **RONIN:** the semi-modular synthesizer and effect.
- **SHOGUN:** a 16-voice analog-style drum machine with a full patch bay.
- **[ORIGAMI](https://github.com/martialsystems/origami):** the triple wave folder, available as a standalone effect and as a rack device.
- **JIDAI:** the rack that hosts the collection side by side.

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See `LICENSE`.

RONIN is an original Martial Systems design inspired by classic Korg gear. Korg is a trademark of its owner. Martial Systems LLC is not affiliated with or endorsed by Korg.
