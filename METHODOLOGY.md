# Methodology

Date of this packet: 2026-09-21.

The plugin is a white-box modular instrument. Each module has a stated signal path, a stated set of jacks, and a list of numbers that are either taken from the 1978 drawings or marked as stand-ins. A later revision replaces a stand-in by editing one constant and its note. It does not retrain a network.

## Why white-box

The hardware was not available for measurement. What exists is a schematic set, one peer-reviewed analysis of the filter (Rest, Parker, Werner, DAFx 2017), and a handful of public recordings. That evidence describes topology, a few labeled voltages, and one filter circuit. It does not describe a corpus of paired input and output audio.

A neural model would fit those recordings, including the room, the other synths in the video, and the effects some of them print. It would also be free to invent a Korg-35 filter, one pitch law, and two ADSRs, which are the usual mistakes listed in the research. A modular FX plugin with animated cables needs separate modules anyway. The structure has to exist as objects, not as a single black box.

White-box here means: the phase 1 VCO is a digital oscillator with the confirmed control topology, not a SPICE netlist of the 2SC1583. The phase 1 filter starts as a 2-pole lowpass (step 8) and moves toward the diode bridge (step 20). Both stages are written down. Neither stage claims to be a transistor-level copy.

## Source hierarchy

When two sources disagree, the higher one wins.

| Rank | Source | Use |
|---|---|---|
| 1 | Korg drawings, 78-11-8 and 78-11-9, KOD-A40038 through A40049 | Jack names, knob names, topology, parts that are printed (CA3019, RC4200, 4013, 555, 7815/7915, +18 V on the VCO, MG voltage cartoons, EG2 voltage cartoons, footage switch) |
| 2 | Rest, Parker, Werner, DAFx-17 paper 88 | Filter small-signal structure, which parts of their model were simplified, the warning that 1N4148 parameters are substitutes |
| 3 | Public recordings and Alex Ball's panel survey | Listening checks and the VCA 1 / VCA 2 panel mapping |
| 4 | Inference in `docs/01-research.md` | Trigger polarity, MG modulation law, EG2 time order. Usable only where the research marks INFERRED |
| 5 | Stand-ins in this file | Plugin numbers that the drawings do not print |

MS-20 owner's manual voltages are rank 4 at best and only as a warning. They are not copied onto the MS-50.

Panel mapping that is inferred, not printed: the sheet says `VCA` for the CA3019 audio VCA with `LO CUT` and `INTENSITY`, and `MVCA` for the optocoupler. This packet calls them VCA 1 and VCA 2 because that is the only published panel description that matches those knobs (Alex Ball). Code comments must say "schematic name VCA" and "schematic name MVCA".

## Accuracy target

The target is a patched modular FX that a reader of the drawings would recognize:

*   Confirmed topology is present: two VCO pitch jacks, scale switch on Hz/V only, simultaneous VCO waves, diode-bridge filter goal, two different VCAs, MG ranges and voltage cartoons, EG1 ADSR with three outs, EG2 without decay or sustain, divider by 2 and by 4 only, inverter gain -1, integrator as lag, ring mod as a four-quadrant multiply.
*   Confirmed labels that have numbers are reproduced on the internal volt scale below.
*   Unknowns are stand-ins, each with an id, a single constant, and a replacement test.
*   Listening against public video is qualitative. There will be no null against a Gladén recording. Those files include an MS-20, a room, and unknown knob positions.

Out of target for steps 0 to 20: transistor VCO, measured Hz/V tracking error, CA3019 Shockley parameters from a datasheet (the paper says the datasheet does not give them), optocoupler lag in milliseconds, EG time in seconds per knob degree, noise spectrum in dB, buffered versus passive multiples.

## Volt scale

Internal audio-thread values are volts, as `float`. The drawings that print a voltage use 5 V and 2.5 V. The plugin converts at the output stage only.

| Hardware | Float volts in the graph | Float at the plugin output, after Output level |
|---|---|---|
| 0 V | 0 | 0 |
| +5 V | 5 | +1.0 at default output level (S-01) |
| -5 V | -5 | -1.0 |
| MG triangle, confirmed ±2.5 V | ±2.5 | ±0.5 |
| MG pulse, confirmed 0 to +5 V | 0 to +5 | 0 to +1 |

S-01: `kOutputVoltsToUnit = 0.2` so that 5 V becomes 1.0 at the DAC when Output level is 1. This scale is a stand-in for jacks whose peak voltage was not printed. It is fixed by the MG and EG2 cartoons for those jacks. Replacement: if a scope shot shows the MG triangle is not ±2.5 V at the jack, change the MG output gains, not this constant, unless the whole cartoon set was misread.

## How stand-ins are marked

In documents: the id `S-xx` and the word `STAND-IN`.

In code, on the constant itself:

```cpp
// STAND-IN S-03: 1 V on CV IN Hz/V produces the footage frequency.
// Replace after measuring Hz versus DC at each SCALE position.
inline constexpr float kHzPerVoltRef = 1.0f;
```

Rules:

*   One id, one constant. Modules call the constant. They do not restate the number.
*   A CONFIRMED quantity does not get a stand-in id. MG frequency endpoints 0.01 Hz and 200 Hz are confirmed. The pot taper between them is S-16.
*   PAPER-SUBSTITUTE is a stand-in that came from the DAFx paper's 1N4148 fit (S-10). It is not a Korg number.
*   Do not "improve" a stand-in by ear during a step whose acceptance test is functional. Ear changes get a new id and a note of what was listened to.

### Stand-in register

| ID | Quantity | Value used in the plugin | Replace when |
|---|---|---|---|
| S-01 | DAC scale | 5 V graph = 1.0 output unit at level 1 | A confirmed jack voltage disagrees with the cartoons |
| S-02 | VCO wave peaks | Saw, triangle, and pulse are bipolar ±5 V | A scope shot of the three VCO jacks |
| S-03 | Hz/V law | `f = footageHz * max(volts, 0.05) / 1.0` | Measured Hz versus DC on `CV IN Hz/V` at each scale |
| S-04 | OCT/V law and fine tune | 0 V = footage Hz. +1 V = +1 octave. Fine tune ±2 semitones | Measured OCT/V slope and fine-tune span |
| S-05 | VCO FREQ A/B depth | Full attenuator, ±5 V in, adds ±5 octaves through the expo summer | Measured FM depth |
| S-06 | VCO pulse width | Duty 0.05 to 0.95. Knob and `PWM` jack sum | Measured duty versus knob |
| S-07 | VCF cutoff knob | Exponential 20 Hz to 18 kHz at 0 V CV, small input | Measured cutoff versus knob at low peak |
| S-08 | VCF cutoff CV | Attenuator 0 to 1 scales a ±5 V jack. Positive volts raise cutoff. Not V/octave and not Hz/V | Measured cutoff versus DC |
| S-09 | VCF peak, step 8 | Resonant 2-pole, Q from 0.5 to 8, stable at full peak | Step 20 replaces this row's Q law |
| S-10 | Diode bridge, step 20 | `rd = n*VT/Ib` with paper 1N4148 `Is = 2.52e-9`, `n = 1.752`. Feedback clip is `tanh`. Input level may pull cutoff down. Do not hard-code 250 Hz | A measured CA3019 fit, or a decision to drop the paper's Is and n |
| S-11 | VCA 1 gain | `gain = clamp(envVolts/5, 0, 1) * intensity`. Low-cut is a one-pole highpass, 10 Hz to 2 kHz | A diode-bridge gain law measured or derived from KOD-A40045 without new guesses |
| S-12 | VCA 2 / MVCA | `gain = clamp(cvVolts/5, 0, 1)` then a one-pole smoother, 20 ms | Measured optocoupler step response |
| S-13 | EG time knobs | Exponential 1 ms to 10 s | A scope shot or a manual sentence |
| S-14 | EG1 levels | OutA 0 to +5 V. OutB 0 to -5 V. OutC bipolar: peak +5 V, sustain 0.5 maps to 0 V | OutC is the weak one: the EG1 sheet did not print volts. OutA/OutB volts are also unprinted and follow EG2's 5 V cartoons only as a stand-in |
| S-15 | Trigger threshold | A CV/audio trig jack is "held" when volts < 1.5. Unpatched Trig rests at +5 V (released). A Gate held promotes to 0 V. A Gate released promotes to +5 V | Measured EG trig threshold |
| S-16 | MG pot curves | Frequency knob exponential from 0.01 Hz to 200 Hz. One PW knob sets triangle symmetry and pulse width together | The endpoints are confirmed. The curve and the symmetry map are not |
| S-17 | Noise level | White and pink bipolar, peak about ±2.5 V, fixed internal trim | Measured jack level and spectrum |
| S-18 | Divider thresholds | Schmitt: high at +0.5 V, low at +0.3 V. Outputs are 0 V or +5 V, square | Measured comparator threshold and pulse height |
| S-19 | Inverter offset | Offset forced to 0. The sheet's trim exists and is not a panel control | A unit with a large residual offset |
| S-20 | Integrator time | One-pole lag, tau 1 ms to 2 s, same sign as the input. A held input settles at that voltage | Measured glide time, and a check that a held DC input does not ramp away |
| S-21 | Ring scale | `outVolts = (aVolts * bVolts) / 5`. Bleed constant 0 | Measured full-scale product and residual feedthrough |
| S-22 | Ext In gate | Envelope of `abs(mono)` with an attack/release follower. Fires above a threshold knob. Manual button ORs in a held gate | Phase 2 ESP trigger and TRIG SW replace this. The button is not a separate module |
| S-23 | Output mix law | `y = dry * (1 - mix) + wet * mix`, wet mono copied to L and R, then `* level * 0.2` | Only if the headphone amp is ever modeled, which is phase 2 |
| S-24 | Feedback edge | In a cycle, the newest cable is the back-edge and delays one sample | A different deterministic rule, if documented in the same change as the tests |
| S-25 | Control rate | Knobs and CV are read every audio sample. There is no slower CV block | A profiled need for a coarser rate. Do not add one for style |
| S-26 | EG2 timeline | See SCHEMATICS, EG2. No sustain plateau | An owner's sentence or a scope shot of hold versus delay |
| S-27 | Output fan-out | One output may feed many inputs. One input accepts one cable | If a later pass removes fan-out because the hardware jacks do not stack, the multiples module (phase 2) becomes required first |

Footage frequencies used with S-03 and S-04, equal-tempered C, A440. These pitches are a stand-in for what "32'" meant at the jack with no cable inserted. The switch positions 32', 16', 8', 4' are confirmed. The hertz numbers are not.

| SCALE | Stand-in Hz at the reference |
|---|---|
| 32' | 32.703 |
| 16' | 65.406 |
| 8' | 130.813 |
| 4' | 261.626 |

## How to A/B against public recordings

Do this after step 8 (filter stand-in) and again after step 20 (diode-bridge pass). Do not treat adjectives as data. One listener called the MS-50 harsher than his MS-20. Another called it softer.

Procedure:

1. Play the reference video from the start time in `docs/TESTPLAN.md`. Note only what the uploader claimed about the patch (saw, peak down, peak up, filter alone, Hz/V, S-trig, effects on or off).
2. Set the plugin to the same claim. Unknown knobs stay at the packet defaults. Write the knob values into the test note.
3. Listen for the structural checks in the test plan: peak changes the tone, full peak can ring, a hot input in step 20 moves more than the level, Hz/V and OCT/V do not track the same keyboard, EG2 has no sustain flat-top, MG pulse is unipolar.
4. Record the plugin dry from the host, no extra plug-ins. Store that recording outside the repo (see `docs/REPO_SETUP.md`).
5. Write three lines: what matched, what did not, which stand-in id would have to move. Do not retune S-numbers in the same session as the listening. A second change, with the id edited and the test note linked, is the retune.

Files that are not dry enough for timbre: Dr. Kunz (Small Stone, SDD-3000, dbx) and the Perfect Circuit jam (MS-20 in the same mix). They are patch ideas only.

## Definition of done for a module

A module is done when all of the following are true:

1. Its jacks, knobs, and confirmed topology match `docs/SCHEMATICS.md`.
2. Every numeric choice is either confirmed or an id in the register above.
3. The automated test named in the build-guide step passes.
4. Unpatched inputs read 0 V, except Gate promotion tests.
5. `processSample()` does not allocate. A test or a code search shows no `new`, `vector::push_back`, `string`, lock, or `printf` on that path.
6. The module is on the rack as its own faceplate. The integrator is not hidden inside another module.
7. A wrong cable (second cable into an input, or a signal into a Gate-only jack) is rejected by the graph, not only hidden by the UI.

A module is not done because it "sounds analog."

## Definition of done for the phase 1 packet

Steps 0 through 20 in `docs/BUILD_GUIDE.md` have each been accepted. Default patch loads. Dry mix 0 passes stereo. Step 20 filter is the one a listener A/Bs, and the note from that listen exists, including anything that still disagrees.

Phase 2 remains unbuilt.
