Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Korg owns the MS-50, its name, and its circuit designs. This repository is an independent study of published drawings and papers. It is not a Korg product, it is not endorsed by Korg, and it does not license those designs.

# Research summary (functional evidence)

Date: 2026-09-21.

This file is the evidence the software schematic is allowed to use. It condenses the study of the Korg MS-50 drawings and the DAFx 2017 filter paper. It is not a second schematic of the PCB.

Status words:

*   CONFIRMED: printed or drawn on the November 1978 sheets, or stated by the DAFx paper as a property of those sheets.
*   INFERRED: a reading of a drawing, or a later panel description.
*   UNKNOWN: not printed, and not settled by the paper.

Plugin numbers for UNKNOWN items live in `METHODOLOGY.md` as stand-ins. They are not repeated here as if they were measured.

## What was opened

| Document | What it actually is |
|---|---|
| z3sound `Korg MS-50 Schematics.pdf` | 12 image pages. Drawing dates 78-11-8 and 78-11-9. Boards KLM-164 / KLM-164B and KLM-165B. Title blocks KOD-A40038 through A40049 |
| Archive.org `Korg_MS-50_Service_Manual.pdf` inside `korg_service_manuals` | Another 12-page image set with the same page sizes. Not a written calibration procedure |
| Rest, Parker, Werner, DAFx-17 paper 88 | Analysis and a simplified WDF of the VCF. Their reference [15] is "KORG, Model MS-50 Circuit Diagram, Nov. 1978" |

No MS-50 owner's manual was found. MS-20 owner's manual (Archive.org `JL11275`) is family context for S-trig and for what not to copy.

No cable on these sheets runs from one module's output to another module's input. A voice exists only if it is patched. CONFIRMED.

Power, KOD-A40038: regulators 7815 and 7915, so +15 V and -15 V, plus a +18 V node ahead of the 7815. The VCO integrator is drawn from +18 V. Transformer printed as KA-331 or KB-331. Fuse 0.5 A. Which transformer is which mains voltage is UNKNOWN.

## Pitch and trigger

| Path | Evidence | Status |
|---|---|---|
| `CV IN Hz/V` | Linear chain of eight 100 kΩ 0.1% resistors. `SCALE` 4', 8', 16', 32' sits on this chain only | CONFIRMED |
| `CV IN OCT/V` | Separate jack into an exponential pair. R1 marked 1 kΩ, +3000 ppm/°C | CONFIRMED |
| `FREQ A`, `FREQ B` | Attenuated inputs drawn into the same summer as OCT/V | CONFIRMED topology. Volts per octave of these jacks: UNKNOWN |
| VCF `CUTOFF FREQ` | Attenuator into the diode bias summer. No Hz/V or OCT/V label | CONFIRMED unlabeled |
| EG `TRIG IN` | Diode and transistor network. No threshold printed | Voltage UNKNOWN. S-trig (active short to ground) is INFERRED |
| `TRIG SW` | Rests pulled up. Button pulls the output toward ground through 2.2 kΩ | CONFIRMED active-low. Phase 2 module |

MG pulse cartoon: 0 to +5 V. MG triangle cartoon: ±2.5 V. EG2 cartoons: 0 to +5 V on one jack and 0 to -5 V on the other. CONFIRMED annotations. VCO, VCF, VCA, noise, divider, and EG1 peak voltages are UNKNOWN. Do not paste MS-20 "5 V peak-to-peak" or "2OCT/V" onto them.

## Phase 1 modules

### VCO (KOD-A40044)

Knobs: `SCALE` (4', 8', 16', 32'), `FREQ A`, `FREQ B`, `FINE TUNING`, `PW/PWM`. `ADJ` is an internal trim.

Jacks: `CV IN Hz/V`, `CV IN OCT/V`, two FM inputs, `PWM`, and three waveform outputs drawn with no series output capacitor. Cartoons: pulse, saw, triangle, simultaneous. No sync jack.

Quirks that the plugin must keep: footage switch does not sit on the OCT/V jack. FM jacks are not a second Hz/V input. Core supply is +18 V, which the digital model does not simulate. Wave amplitudes UNKNOWN (stand-in S-02).

### VCF (KOD-A40045)

CA3019 (six matched diodes) biased as controlled resistances inside a Sallen-Key lowpass. `PEAK` is VR10, 10 kΩ, in the feedback. Eight discrete diodes D5 to D12 clip that feedback. `SIG IN` meets R59 = 100 kΩ, a unity buffer, then capacitive coupling into the bridge (paper, describing the sheet). `SIG OUT` is AC-coupled. Manual cutoff, external cutoff, and a temperature term become opposite bias voltages.

This is the circuit used in the Korg 700, 700S, 800DV, and 900PS, patent US 4,039,980 (paper). It is not the MS-20 Korg-35 and not the later MS-20 OTA.

The paper's WDF omitted D5 to D12 on purpose, replaced the bias op-amps with 50 Ω sources, and used 1N4148 parameters (`Is = 2.52e-9`, `n = 1.752`) because the CA3019 datasheet did not supply them. Those diode numbers are PAPER-SUBSTITUTE (S-10), not Korg values. In that simplified model, a normalized resonance of 0.85 self-oscillated near 250 Hz and fell toward 200 Hz as a slow input ramp reached 2 V, and the oscillation nearly died. Those hertz figures are one experiment. Do not hard-code them.

Parts that appear on the sheet and in the paper's Table 1: R44 220 kΩ, R45 1.8 kΩ, R46 100 Ω, R48 2.7 kΩ, R50 2.2 kΩ, R52 47 kΩ, R54 47 kΩ, R56 1 kΩ, C11 and C12 22 nF, C15 2.7 nF, C17 10 µF, C52 220 pF, PEAK pot 10 kΩ. `Rs = 50 Ω` is the paper's substitute.

Cutoff knob hertz span: UNKNOWN (S-07). Cutoff CV law: diode bias, not a keyboard standard (S-08).

### VCA 1, sheet name VCA (KOD-A40045)

Second CA3019 as the gain cell. `ENV IN` feeds the bias summer. `LO CUT` (VR14) is a manual highpass in front of the cell, with no CV jack. Alex Ball's video states the same thing: non-resonant, manual only. `INTENSITY` (VR15, 100 kΩ A) is drawn after the bridge as an output attenuator, not as a CV-depth knob. `OUT` is AC-coupled. No initial-gain jack. Closed-state feedthrough is an internal trim (VR13).

Panel number "VCA 1" is INFERRED. Gain law in volts is UNKNOWN (S-11).

### VCA 2, sheet name MVCA (KOD-A40043)

Optocoupler marked HTV between two 4558 stages. `CONTROL IN` drives the lamp transistor. No panel knob. The signal path as drawn has no series coupling capacitor, so it can pass CV. Lamp lag is real and UNKNOWN (S-12). Panel number "VCA 2" is INFERRED.

### MG (KOD-A40048)

The sheet says MG, not LFO. 555 core. Printed frequency text: 0.01 Hz to 200 Hz. Outputs: triangle ±2.5 V, two saws of opposite slope annotated ±2.5 V, pulse 0 to +5 V. One `PW/PWM` pot is wired into both the triangle symmetry network and the pulse comparator. `FREQ MOD` enters the timing current. A linear-Hz reading of that jack is INFERRED. The pot curve between 0.01 Hz and 200 Hz is a stand-in (S-16). The top of the range is audio, so MG is a second oscillator as well as an LFO.

### EG 1 ADSR (KOD-A40046)

Knobs: attack, decay, sustain, release. One `TRIG IN`. Three output jacks with different cartoons. Voltages are not printed (S-14). This is not the MS-20 EG1, which is delay-attack-release.

### EG 2 HDAR (KOD-A40047)

Knobs printed: HOLD, DELAY, ATTACK, RELEASE. No decay knob. No sustain knob. Jacks: `TRIG IN`, two envelope outs annotated 0 to +5 V and 0 to -5 V, and `DELAY TRIG OUT`. Left-to-right drawing order is hold, then delay, then the attack/release slope generator. The musical definition of hold versus delay is INFERRED (S-26). Cartoons are bumps, not a sustain shelf.

### Noise (KOD-A40042)

Selected transistor marked CG44R. `W.NOISE` before a passive filter. `P.NOISE` after it (0.022 µF and 220 kΩ are readable in that branch). No panel knob. Internal `LEVEL ADJ`. Both outputs AC-coupled. Peak voltage UNKNOWN (S-17).

### Divider (KOD-A40041)

`IN` to an LM339, then a 4013 as two toggle stages. Outputs labeled ÷2 and ÷4. No ÷8. Output amplitude UNKNOWN (S-18). Threshold UNKNOWN.

### Inverter (KOD-A40043)

4558, input and feedback both drawn as 100 kΩ metal film, so the drawn gain is -1. DC-coupled. Offset trim VR2 is internal, not a panel knob (S-19).

### Integrator (KOD-A40039)

`IN`, `TIME`, `OUT`. Op-amp lag: a held input must settle at the input voltage, same sign. It is not a resonant filter and not a ramp that runs away. Tau versus knob is UNKNOWN (S-20). The module stays on the rack.

### Ring (KOD-A40040)

RC4200 four-quadrant multiplier. `A IN`, `B IN`, `OUT`. DC-coupled. Null trims VR7, VR8, VR9 are internal. No panel knob. This is not the filter's CA3019. Full-scale product is UNKNOWN (S-21). Using one input as a unipolar CV makes it a VCA. That patch is documented by a player (Dr. Kunz) and is allowed by a four-quadrant multiply.

## Phase 2, on the same sheets, not in steps 0 to 20

| Block | Sheet | Keep for later |
|---|---|---|
| Adding amplifier | KOD-A40049 | Three level pots, inverting summer, DC-coupled. 220 kΩ into 330 kΩ feedback as drawn. No published gain spec |
| Sample and hold | KOD-A40040 | IN, OUT, EXT CLOCK IN, CLOCK OUT, CLOCK pot, 555, K30(GR) FET, 3140 buffer, 0.1 µF hold cap drawn |
| Audio amplifier (ESP) | KOD-A40042 | SG IN with level, SG OUT, envelope follower, peak LED, trig LED, TRIG OUT. No bandpass. No frequency-to-voltage converter |
| TRIG SW | KOD-A40041 | Active-low momentary. Phase 1 uses Ext In's button as stand-in S-22, not this module |
| Volt supply | KOD-A40039 | Pot between the rails, 3.3 kΩ in each leg. Soft bipolar CV. Do not label it ±5 V or ±10 V |
| Meter | KOD-A40039 | AC.IN, DC.IN, movement YL-80S |
| Headphone amp | KOD-A40038 | SIG IN, level, PHONES. No separate line-out jack is drawn on this sheet |
| Multiples | absent | Alex Ball lists buffered multiples. Buffering is UNKNOWN. No circuit is on the 12 pages |

## Recordings worth using

| Item | Use |
|---|---|
| Magnus Gladén, "Korg MS-20 vs. Korg MS-50 low pass filter test" (YouTube `MwJk36KnB04`) | Best filter compare. Saw, peak down, peak up, filter alone |
| Synth Party, MS-50 sequenced by a Yamaha CS-30 (YouTube `C6oqCY0ArvM`) | After 0:51 the uploader says the line is direct, driven by Hz/V and S-trig |
| Alex Ball, "The Rare KORG MS-50 from 1978" (YouTube `3YFDDDXw6F8`), filter talk near 7:41 and compare near 8:43 | Diode ring versus type 35, and the spoken low-cut claim. Musical, not a lab sweep |
| vitalance, SQ-10 / MS-50 / KR-55 (YouTube `TZKxrRzoDU0`) | Trigger and cutoff motion. Other gear is present |
| Dr. Kunz, MS-50 Textures (YouTube `_iJJYsWUYd0`) | Ring used as a VCA. Effects are on the file. Not a timbre reference |
| Perfect Circuit MS-20 and MS-50 jam (YouTube `UXW39LO-bxY`) | Not isolated |

## Bibliography

*   Korg MS-50 schematics, drawings 78-11-8 and 78-11-9. https://z3sound.com/manuals/Korg/Korg%20MS-50%20Schematics.pdf
*   Archive.org image set: https://archive.org/details/synthmanual-korg-ms-50-schematics
*   Archive.org file titled service manual, same kind of image set: https://archive.org/download/korg_service_manuals/KORG_ALL_SM/Korg_MS-50_Service_Manual.pdf
*   Synthfool index of other MS-series files: https://synthfool.com/docs/Korg/MS_series
*   Maximilian Rest, Julian D. Parker, Kurt James Werner, "WDF Modeling of a Korg MS-50 Based Non-Linear Diode Bridge VCF," DAFx-17, Edinburgh, 5 to 9 September 2017. https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_88.pdf
*   Yasuji Nagahama, US 4,039,980, "Voltage-controlled filter," issued 2 August 1977. https://patents.google.com/patent/US4039980A/en
*   Tim Stinchcombe, "A study of the Korg MS10 & MS20 filters" (2006). Contrast only. http://www.timstinchcombe.co.uk/synth/MS20_study.pdf
*   Alex Ball, KORG MS Series panel notes. https://www.alexballmusic.com/korg-ms-series
*   Korg MS-20 owner's manual, Archive.org JL11275. https://archive.org/details/JL11275
*   Sequencer.de MS50 page. Useful as an example of the "two ADSRs" error. https://www.sequencer.de/syns/korg/MS50.html
*   AMSynths AM8319. A later Eurorack reading that adds a highpass the MS-50 VCF sheet does not show. https://modulargrid.net/e/amsynths-am8319
