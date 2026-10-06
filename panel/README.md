Copyright (c) 2026 Martial Systems LLC. All rights reserved. RONIN is part of the Jidai Collection.

# Panel

`assets/layout.json` is the jack and knob geometry: 57 jacks and 33 knobs on a 1600 by 564 canvas. EXT IN is the column immediately left of OUTPUT. The editor paints knob bodies on top of the tick marks. Turning a knob does not write the graph.

`emit_panel_svg.py` reads that file and writes `Source/UI/PanelGeometry.inc`. It leaves `assets/panel.svg` and `assets/panel_bg.svg` alone, and it does not move a jack. Run it from the repo root:

```bash
python3 panel/emit_panel_svg.py
```

`build_panel.py` is the generator that first wrote `layout.json`. Re-run it only when the module list should change. A different font moves the jacks. It also writes PNGs. Those PNGs are not the editor asset and are not committed.

The cable rule is in `docs/METHODOLOGY.md` under Cable rule (2026-10-05).

Live jacks in the geometry table: Ext In L, R, Mono, Gate, Output L, R, Wet, Noise White, Pink. Every other jack is drawn and unmapped.

## Later: loadable panel art

The panel used to end in an empty 88-unit band under the module frame that was meant for a picture. It was cut on 2026-10-06; the panel now stops 26 units under the frame, like BUSHIDO's. When we want art back, make it a picture the user can load into a strip of the panel, the way the character art works in FL Studio's Harmless: a default image ships with RONIN, the user can pick their own PNG, and the choice is saved with the session. It must never cover a jack or a knob.
