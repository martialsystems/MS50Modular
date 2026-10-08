# Factory presets

The factory bank is cleared for now and holds one program, 01 INIT. New factory programs will be written later. A fresh instance is INIT, loaded exactly as choosing it from the list: the eight INIT cables, Effect on, and every knob on the default table below. Choosing a program replaces the cables and resets every host knob to that table, then applies the program's own knobs. Double-click on a knob resets it to the same table.

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

1. INIT: Effect on. Ext In Mono to VCF SigIn, VCF SigOut to VCA 1 SigIn, VCA 1 Out to Output Wet, Ext In L to Output L, Ext In R to Output R, Ext In Gate to EG 1 Trig, EG 1 OutA to VCA 1 Env, EG 1 OutA to VCF Cutoff. The Ext In button or a gate opens EG 1, which opens VCA 1 and moves the filter. VCA 1 Initial is 0.

INIT starts the VCO on the true TRIANGLE (PolyBLAMP, odd harmonics only). PARABOLA (legacy), the integrated saw, stays selectable on the VOICE tab (TRI SHAPE) and is what user-saved format-1 states load on (RONIN_Redesign M-R2).
