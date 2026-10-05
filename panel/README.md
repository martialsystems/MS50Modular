Copyright (c) 2026 Martial Systems LLC. All rights reserved.

The Korg MS-50, the MS-50 name, and the circuit designs of that instrument are the property of Korg Inc. Martial Systems LLC claims copyright only in the original text of this repository and in any code later written here. The work is an independent study of published schematics and of the literature cited in the research summary. Korg has not produced, sponsored, or endorsed it. No license is granted to the MS-50 design, to the Korg drawings, or to the Korg trademarks. The instrument's name is used only to identify the subject of the study.

# Panel

`assets/layout.json` is the jack and knob geometry: 58 jacks on a 1600 by 640 canvas.

`emit_panel_svg.py` reads that file and writes `assets/panel.svg` and `Source/UI/PanelGeometry.inc`. It does not move a jack. Run it from the repo root:

```bash
python3 panel/emit_panel_svg.py
```

`build_panel.py` is the generator that first wrote `layout.json`. Re-run it only when the module list should change. A different font moves the jacks. It also writes PNGs. Those PNGs are not the editor asset and are not committed.

The cable rule is in `METHODOLOGY.md` under Cable rule (2026-10-05).

Live jacks in the geometry table: Ext In L, R, Mono, Gate, Output L, R, Wet, Noise White, Pink. Every other jack is drawn and unmapped.
