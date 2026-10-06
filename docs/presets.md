# Factory presets

The host list has thirteen programs. A fresh instance is 03 Voice, loaded exactly as choosing it from the list: the eight Voice cables, Effect on, and every knob on the default table below. Choosing a program replaces the cables and resets every host knob to that table, then applies the program's own knobs. Double-click on a knob resets it to the same table.

The top rocker is EFFECT, not power. Off is dry: the dry L and R cables pass and Output Mix is unused. On is wet. The graph keeps running either way, Output Level still applies, and the preset screen stays lit.

## Default table

One table, `Source/Modular/PanelDefaults.h`, feeds the host parameters, `panel/assets/layout.json`, double-click reset and program load.

*   VCO: Range 0.5, Fine 0.5, PW 0.5, FM 1 0, FM 2 0.
*   VCF: Cutoff 0.45, Peak 0.2, Mod 0.4.
*   VCA 1: Initial 0, Mod 0.85, Low Cut 0. Initial 0 means EG 1 (or whatever is on Env) still controls the VCA. Initial above 0 passes audio with no gate.
*   VCA 2: Initial 0, Mod 1. Gain is `clamp01(CV / 5 V + Initial) * Mod`, so these defaults are the plain CV law.
*   MG: Rate 0.5, PW 0.5.
*   EG 1: Attack 0.05, Decay 0.3, Sustain 0.6, Release 0.3.
*   EG 2: Hold 0.3, Delay 0, Attack 0.05, Release 0.3.
*   S&H Rate 0.5. Integrator Time 0.5. Mixer Level 1, 2 and 3 at 0.8. Output Mix 1. Output Level 0.7 (unity).
*   Divider switch /2. It has two positions, /2 and /4, and is saved with the session. The /2 and /4 jacks both always run. There is no /16.

## Ext In gate

EXT IN has two host knobs, Threshold and Release, drawn under the MONO and GATE jacks. They set when EXT IN GATE opens from the host input, so Voice and Hold use them (the HOLD key opens the gate regardless).

*   Threshold: 0.05 V times 40 to the knob travel. Default travel 0.3758 is 0.2 V.
*   Release: 10 ms times 50 to the knob travel. Default travel 0.5316 is 80 ms.

Every program load returns both to these defaults.

## Programs

1. Dry: Effect off. Ext In L to Output L, Ext In R to Output R. Stereo host input passes. No wet cable.
2. Noise to mixer: Effect on. Noise White to Mixer In 1, Mixer Out to Output Wet, Level 1 at 0.8. Inverted white noise.
3. Voice: Effect on. Ext In Mono to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, Ext In L to Output L, Ext In R to Output R, Ext In Gate to EG 1 Trig, EG 1 OutA to VCA 1 Env, EG 1 OutA to VCF Cutoff. The Ext In button or a gate opens EG 1, which opens VCA 1 and moves the filter.
4. Ring: Effect on. Noise White to Ring A, MG Tri to Ring B, Ring Out to Output Wet. White noise multiplied by the MG triangle.
5. S&H: Effect on. Noise White to S&H In, S&H Out to VCF Cutoff, Ext In Mono to VCF SigIn, VCF SigOut to Output Wet. The host input is filtered, and the cutoff steps through held noise.
6. Feedback: Effect on. VCO Saw to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, VCF SigOut to VCF Cutoff. The saw passes the filter, and the filter output moves its own cutoff. The cutoff cable is the newest one, and it is the delayed cable. VCA 1 Initial is 0.7, so the saw is audible with no gate.
7. Hold: Effect on. VCO Saw to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, Ext In Gate to EG 1 Trig, EG 1 OutA to VCA 1 Env, EG 1 OutA to VCF Cutoff. Button up is silence. Button down fades in the saw and opens the filter, and button up releases it.
8. Filter loop: Effect on. VCO Saw to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, VCF SigOut to VCF Cutoff. Cutoff is 0.4 and Peak is 0.7. VCA 1 Initial is 0.7. With no Hold press the saw is heard, and the filter output moves its own cutoff. The cutoff cable is the newest one, and it is the delayed cable.
9. MG into filter: Effect on. VCO Saw to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, MG Tri to VCF Cutoff. MG Rate is 0.3. VCA 1 Initial is 0.7. With no Hold press the saw is heard, and the triangle slowly sweeps the cutoff.
10. Stepped cutoff: Effect on. VCO Saw to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, Noise White to S&H In, S&H Out to VCF Cutoff. S&H Rate is 0.4. VCA 1 Initial is 0.7. With no Hold press the saw is heard, and the cutoff jumps between held noise samples.
11. Ring drone: Effect on. VCO Saw to Ring A, MG Tri to Ring B, Ring Out to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet. MG Rate is 0.25. VCA 1 Initial is 0.7. With no Hold press the tone is metallic, and the triangle keeps it moving.
12. Delayed bounce: Effect on. Noise White to Integrator In, Integrator Out to VCF Cutoff, VCO Saw to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet. Integrator Time is 0.6. VCA 1 Initial is 0.7. With no Hold press the saw is heard, and the cutoff lags the noise.
13. Self ring: Effect on. VCO Saw to Ring A, Ring Out to Ring B, Ring Out to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet. Ring Out to Ring B is the delayed cable, and the cycle stays patched. VCA 1 Initial is 0.7. With no Hold press the wet path stays quiet: the ring output is the saw times its own previous sample, and that product starts at 0.
