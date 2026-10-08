# Factory presets

The factory bank holds 22 programs on the PRESET screen: 01 INIT, six effects that treat your input, six acid basses, two more basses, a lead, a siren, three drums, a laser zap and a noise riser. The programs are voiced for classic EDM at 125 BPM. Every program has Effect on and starts the VCO on the true TRIANGLE. Programs with no input play on their own: the MG is their clock. Each description says what the program expects at EXT IN.

A fresh instance is INIT, loaded exactly as choosing it from the list: the eight INIT cables, Effect on, and every knob on the default table below. Choosing a program replaces the cables and resets every host knob to that table, then applies the program's own knobs. Double-click on a knob resets it to the same table. Programs are stored in state format 2 terms (knobs by host parameter id, cables by jack id) in `Source/Modular/FactoryPresets.h`, which is compiled into the plugin.

The top rocker is EFFECT, not power. Off is dry: the dry L and R cables pass and Output Mix is unused. On is wet. The graph keeps running either way, Output Level still applies, and the preset screen stays lit.

## Default table

One table, `Source/Modular/PanelDefaults.h`, feeds the host parameters, `panel/assets/layout.json`, double-click reset and program load.

*   VCO: Range 0.5, Fine 0.5, PW 0.5, FM 1 0, FM 2 0.
*   VCF: Cutoff 0.45, Peak 0.2, Mod 0.4.
*   VCA 1: Initial 0, Mod 0.85, Low Cut 0. Initial 0 means EG 1 (or whatever is on Env) still controls the VCA. Initial above 0 passes audio with no gate.
*   VCA 2: Initial 0, Mod 1. Gain is `clamp01(CV / 5 V + Initial) * Mod`, so these defaults are the plain CV law.
*   MG: Rate 0.5, PW 0.5.
*   EG 1: Attack 0.2079 (9.85 ms), Decay 0.39 (73 ms), Sustain 0.6, Release 0.39 (73 ms).
*   EG 2: Hold 0.3, Delay 0, Attack 0.2079 (9.85 ms), Release 0.39 (73 ms).

EG time knobs read in real time, 1 ms to 60 s (RONIN_Redesign §3.1). The EG values are the earlier 0.05 / 0.3 defaults passed through migration M-R1, so INIT keeps its envelope timing.
*   S&H Rate 0.5. Integrator Time 0.5. Mixer Level 1, 2 and 3 at 0.8. Output Mix 1. Output Level 0.7 (unity).
*   Divider switch /2. It has two positions, /2 and /4, and is saved with the session. The /2 and /4 jacks both always run. There is no /16.

## Ext In gate

EXT IN has two host knobs, Threshold and Release, drawn under the MONO and GATE jacks. They set when EXT IN GATE opens from the host input, so INIT uses them (the HOLD key opens the gate regardless).

*   Threshold: 0.05 V times 40 to the knob travel. Default travel 0.3758 is 0.2 V.
*   Release: 10 ms times 50 to the knob travel. Default travel 0.5316 is 80 ms.

Every program load returns both to these defaults.

## Programs

Knob values are host parameter travel (0 to 1). Knobs not listed stay on the default table. At 125 BPM, MG Rate 0.6791 is 16th notes, 0.6091 8ths, 0.5391 quarters and 0.4691 half notes; S&H Rate 0.6403 is 16ths. EG knobs read 0.063 = 2 ms, 0.2093 = 10 ms, 0.2723 = 20 ms, 0.3556 = 50 ms, 0.4351 = 120 ms, 0.4816 = 200 ms, 0.5184 = 300 ms, 0.5649 = 500 ms and 0.6279 = 1 s.

### INIT

1. INIT: Effect on. Ext In Mono to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, Ext In L to Output L, Ext In R to Output R, Ext In Gate to EG 1 Trig, EG 1 OutA to VCA 1 Env, EG 1 OutA to VCF Cutoff. The Ext In button or a gate opens EG 1, which opens VCA 1 and moves the filter. VCA 1 Initial is 0.

INIT starts the VCO on the true TRIANGLE (PolyBLAMP, odd harmonics only). PARABOLA (legacy), the integrated saw, stays selectable on the VOICE tab (TRI SHAPE) and is what user-saved format-1 states load on (RONIN_Redesign M-R2).

### Effects on the input

2. AUTO FILTER: Resonant low-pass swept by the MG, one sweep per bar. Input: any audio (pads, loops, chords).
    *   Cables: EXT IN MONO to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG TRI to VCF CUTOFF.
    *   Knobs: VCF Cutoff 0.56, VCF Peak 0.62, VCF Mod 0.75, VCA 1 Initial 1, MG Rate 0.3991, MG PW 0.5.

3. ENV FILTER: Envelope filter: each hit at EXT IN snaps the resonant cutoff open. Input: drums, bass or plucked parts.
    *   Cables: EXT IN MONO to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, EXT IN GATE to EG 1 TRIG, EG 1 OUT A to VCF CUTOFF.
    *   Knobs: VCF Cutoff 0.4, VCF Peak 0.72, VCF Mod 0.8, VCA 1 Initial 1, EG 1 Attack 0.2093, EG 1 Decay 0.5184, EG 1 Sustain 0.25, EG 1 Release 0.4816.

4. TRANCE GATE: 16th-note gate chopping the input, edges softened by INT. Input: sustained audio (pads, chords, vocals).
    *   Cables: EXT IN MONO to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to INT IN, INT OUT to VCA 1 ENV.
    *   Knobs: VCA 1 Mod 1, MG Rate 0.6791, MG PW 0.6111, Int Time 0.2117.

5. PUMP: Quarter-note ducking from EG 2 into VCA 1, the side-chain pump. Input: sustained audio or a full mix.
    *   Cables: EXT IN MONO to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 2 TRIG, EG 2 OUT − to VCA 1 ENV.
    *   Knobs: VCA 1 Initial 1, VCA 1 Mod 1, MG Rate 0.5391, MG PW 0.5, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.5184.

6. S&H FILTER: Random stepped filter: S&H samples noise in 16ths and moves the cutoff. Input: any audio, best with pads or loops.
    *   Cables: EXT IN MONO to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, NOISE WHITE to S&H IN, S&H OUT to VCF CUTOFF.
    *   Knobs: VCF Cutoff 0.58, VCF Peak 0.7, VCF Mod 0.55, VCA 1 Initial 1, S&H Rate 0.6403.

7. RING MOD: The input times the VCO triangle, with slow MG drift on the carrier. Input: any audio; voices and drums go metallic.
    *   Cables: EXT IN MONO to RING A, VCO TRI to RING B, RING OUT to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG TRI to VCO FM 1.
    *   Knobs: VCO Range 0.6667, VCO FM 1 0.12, VCF Cutoff 0.8, VCF Peak 0.1, VCA 1 Initial 1, VCA 1 Mod 1, MG Rate 0.3.

### Acid basses

The acid basses play themselves: MG PULSE clocks 16th notes into EG 1 (the VCA gate), EG 2 (the filter snap on VCF Cutoff) and DIV. DIV /2 and /4 run through MIX Level 1 and 2 and the inverter into INT, which slides the pattern into VCO V/OCT. Mix Level 1 at 0.2 is an octave (1 V) and each 0.0167 of Mix Level 2 is a semitone.

To drive one from BUSHIDO or another sequencer in the JIDAI rack: patch its pitch into INT IN (it sums with the built-in pattern; turn Mix Level 1 and 2 to 0 to hear its notes alone), patch its gate into EG 1 TRIG and EG 2 TRIG in place of the MG PULSE cables, and patch an accent CV into VCF CUTOFF. Slide comes from INT Time; turn it down for steps without slide.

8. ACID LINE: Self-playing acid bass: 16ths at 125 BPM, octave and fifth pattern, short slide. No input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCF CUTOFF, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT.
    *   Knobs: VCO Range 0.3333, VCF Cutoff 0.34, VCF Peak 0.85, VCF Mod 0.72, VCA 1 Mod 0.64, EG 1 Attack 0.063, EG 1 Decay 0.5184, EG 1 Sustain 0.8, EG 1 Release 0.2723, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.4351, MG Rate 0.6791, MG PW 0.6667, Mix Level 1 0.2, Mix Level 2 0.1167, Mix Level 3 0, Int Time 0.3029.

9. ACID SQUELCH: Acid bass at full peak with a longer filter snap, a minor-third pattern and a slower slide. No input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCF CUTOFF, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT.
    *   Knobs: VCO Range 0.3333, VCF Cutoff 0.28, VCF Peak 1, VCF Mod 0.88, VCA 1 Mod 0.62, EG 1 Attack 0.063, EG 1 Decay 0.5649, EG 1 Sustain 0.85, EG 1 Release 0.3556, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.5184, MG Rate 0.6791, MG PW 0.5, Mix Level 1 0.2, Mix Level 2 0.05, Mix Level 3 0, Int Time 0.3563.

10. ACID ACCENT: Acid bass with random accents: S&H noise on the cutoff per step. No input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCF CUTOFF, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT, NOISE WHITE to S&H IN, MG PULSE to S&H CLOCK, S&H OUT to VCF CUTOFF.
    *   Knobs: VCO Range 0.3333, VCF Cutoff 0.32, VCF Peak 0.9, VCF Mod 0.7, VCA 1 Mod 0.55, EG 1 Attack 0.063, EG 1 Decay 0.5184, EG 1 Sustain 0.8, EG 1 Release 0.2723, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.4554, MG Rate 0.6791, MG PW 0.6667, Mix Level 1 0.2, Mix Level 2 0.1167, Mix Level 3 0, Int Time 0.3029.

11. ACID PULSE: Acid bass on the narrow pulse, EG 2 also sweeping the pulse width. No input needed.
    *   Cables: VCO PULSE to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCF CUTOFF, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT, EG 2 OUT + to VCO PWM.
    *   Knobs: VCO Range 0.3333, VCO PW 0.2, VCF Cutoff 0.36, VCF Peak 0.82, VCF Mod 0.7, VCA 1 Mod 0.49, EG 1 Attack 0.063, EG 1 Decay 0.5184, EG 1 Sustain 0.8, EG 1 Release 0.2723, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.4351, MG Rate 0.6791, MG PW 0.6667, Mix Level 1 0.2, Mix Level 2 0.1167, Mix Level 3 0, Int Time 0.3029.

12. ACID DRIVE: Saw and pulse summed into the VCF for a hotter, driven acid line. No input needed.
    *   Cables: VCO SAW to VCF IN, VCO PULSE to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCF CUTOFF, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT.
    *   Knobs: VCO Range 0.3333, VCO PW 0.35, VCF Cutoff 0.4, VCF Peak 0.92, VCF Mod 0.75, VCA 1 Mod 0.6, EG 1 Attack 0.063, EG 1 Decay 0.5184, EG 1 Sustain 0.8, EG 1 Release 0.2723, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.4351, MG Rate 0.6791, MG PW 0.6667, Mix Level 1 0.2, Mix Level 2 0.1167, Mix Level 3 0, Int Time 0.3029.

13. ACID SLIDE: Long legato gates, a fourth and octave pattern and a slow slide: the rubbery acid line. No input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCF CUTOFF, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT.
    *   Knobs: VCO Range 0.3333, VCF Cutoff 0.33, VCF Peak 0.88, VCF Mod 0.65, VCA 1 Mod 0.7, EG 1 Attack 0.063, EG 1 Decay 0.6279, EG 1 Sustain 0.9, EG 1 Release 0.3556, EG 2 Hold 0, EG 2 Attack 0.063, EG 2 Release 0.4816, MG Rate 0.6791, MG PW 0.2778, Mix Level 1 0.2, Mix Level 2 0.0833, Mix Level 3 0, Int Time 0.3941.

### Basses and leads

14. SUB BASS: Round triangle sub on quarter notes, straight into VCA 1 with no filter. No input needed.
    *   Cables: VCO TRI to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, EG 1 OUT A to VCA 1 ENV.
    *   Knobs: VCO Range 0.3333, VCA 1 Mod 0.6, EG 1 Attack 0.1463, EG 1 Decay 0.5649, EG 1 Sustain 0.7, EG 1 Release 0.3983, MG Rate 0.5391, MG PW 0.5.

15. OFFBEAT BASS: Plucky saw bass on the second half of each beat, the off-beat bass. No input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, EG 1 OUT A to VCA 1 ENV, EG 1 OUT A to VCF CUTOFF.
    *   Knobs: VCO Range 0.3333, VCF Cutoff 0.42, VCF Peak 0.3, VCF Mod 0.45, VCA 1 Mod 0.72, EG 1 Attack 0.063, EG 1 Decay 0.4554, EG 1 Sustain 0.35, EG 1 Release 0.3556, MG Rate 0.5391, MG PW 0.5.

16. PWM LEAD: Pulse-width lead: EG 2 sweeps the width on every note, an 8th-note fifth and octave pattern with glide. No input needed.
    *   Cables: VCO PULSE to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, MG PULSE to DIV IN, EG 1 OUT A to VCA 1 ENV, EG 1 OUT A to VCF CUTOFF, EG 2 OUT + to VCO PWM, DIV /2 to MIX IN 1, DIV /4 to MIX IN 2, MIX OUT to INV IN, INV OUT to INT IN, INT OUT to VCO V/OCT.
    *   Knobs: VCO Range 0.6667, VCO PW 0.3, VCF Cutoff 0.56, VCF Peak 0.25, VCF Mod 0.35, VCA 1 Mod 0.55, EG 1 Attack 0.1463, EG 1 Decay 0.5649, EG 1 Sustain 0.7, EG 1 Release 0.4816, EG 2 Hold 0, EG 2 Attack 0.4816, EG 2 Release 0.5649, MG Rate 0.6091, MG PW 0.3889, Mix Level 1 0.1167, Mix Level 2 0.2, Mix Level 3 0, Int Time 0.4475.

17. SIREN: Rising and falling siren: MG triangle on the VCO pitch. Plays continuously, no input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG TRI to VCO FM 1.
    *   Knobs: VCO Range 0.6667, VCO FM 1 0.4, VCF Cutoff 0.62, VCF Peak 0.35, VCA 1 Initial 1, VCA 1 Mod 0.55, MG Rate 0.35, MG PW 0.5.

### Percussion and effects

18. KICK DRUM: Four-on-the-floor kick: triangle with an EG 2 pitch drop. No input needed.
    *   Cables: VCO TRI to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCO FM 1.
    *   Knobs: VCO Range 0.3333, VCO Fine 0, VCO FM 1 0.5, VCA 1 Mod 0.55, EG 1 Attack 0, EG 1 Decay 0.5184, EG 1 Sustain 0, EG 1 Release 0.4816, EG 2 Hold 0, EG 2 Attack 0, EG 2 Release 0.3556, MG Rate 0.5391, MG PW 0.8.

19. OFFBEAT HATS: High-passed noise hats on the off-beats. No input needed.
    *   Cables: NOISE WHITE to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, EG 1 OUT A to VCA 1 ENV.
    *   Knobs: VCF Cutoff 0.95, VCF Peak 0.3, VCA 1 Mod 1, VCA 1 Low Cut 1, EG 1 Attack 0, EG 1 Decay 0.4351, EG 1 Sustain 0, EG 1 Release 0.4351, MG Rate 0.5391, MG PW 0.5.

20. NOISE SNARE: Noise and triangle snare on beats 2 and 4. No input needed.
    *   Cables: NOISE WHITE to MIX IN 1, VCO TRI to MIX IN 2, MIX OUT to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, EG 1 OUT A to VCA 1 ENV.
    *   Knobs: VCO Range 0.6667, VCO Fine 0.75, VCF Cutoff 0.78, VCF Peak 0.35, VCA 1 Mod 0.85, VCA 1 Low Cut 0.45, EG 1 Attack 0, EG 1 Decay 0.4554, EG 1 Sustain 0, EG 1 Release 0.4554, MG Rate 0.4691, MG PW 0.5, Mix Level 1 0.8, Mix Level 2 0.45, Mix Level 3 0.

21. LASER ZAP: 8th-note laser zaps: EG 2 drops the VCO pitch fast. No input needed.
    *   Cables: VCO SAW to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG PULSE to EG 1 TRIG, MG PULSE to EG 2 TRIG, EG 1 OUT A to VCA 1 ENV, EG 2 OUT + to VCO FM 1.
    *   Knobs: VCO Range 0.6667, VCO FM 1 0.8, VCF Cutoff 0.7, VCF Peak 0.55, VCA 1 Mod 0.58, EG 1 Attack 0, EG 1 Decay 0.4816, EG 1 Sustain 0, EG 1 Release 0.4351, EG 2 Hold 0, EG 2 Attack 0, EG 2 Release 0.4351, MG Rate 0.6091, MG PW 0.7.

22. NOISE RISER: Four-bar noise riser: the MG saw opens the filter and the VCA, then resets. No input needed.
    *   Cables: NOISE WHITE to VCF IN, VCF OUT to VCA 1 IN, VCA 1 OUT to OUTPUT WET, EXT IN L to OUTPUT L, EXT IN R to OUTPUT R, MG SAW to VCF CUTOFF, MG SAW to VCA 1 ENV.
    *   Knobs: VCF Cutoff 0.62, VCF Peak 0.65, VCF Mod 1, VCA 1 Initial 0.5, VCA 1 Mod 1, MG Rate 0.2591, MG PW 0.5.
