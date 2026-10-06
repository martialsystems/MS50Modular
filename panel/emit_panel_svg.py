#!/usr/bin/env python3
"""Refresh Source/UI/PanelGeometry.inc from panel/assets/layout.json.

The faceplate art is panel/assets/panel.svg and panel/assets/panel_bg.svg
(the Ronin export). This script does not rewrite those files. Re-run from
the repo root after a layout change:

  python3 panel/emit_panel_svg.py
"""

import json
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LAY = os.path.join(ROOT, "panel", "assets", "layout.json")
SVG = os.path.join(ROOT, "panel", "assets", "panel.svg")
INC = os.path.join(ROOT, "Source", "UI", "PanelGeometry.inc")

# section, label -> (module id, port, dir). dir: 0 in, 1 out.
# Only modules that exist on the graph today. Other jacks stay unmapped.
LIVE = {
    ("EXT IN", "L"): (1, 0, 1),
    ("EXT IN", "R"): (1, 1, 1),
    ("EXT IN", "MONO"): (1, 2, 1),
    ("EXT IN", "GATE"): (1, 3, 1),
    ("OUTPUT", "L"): (2, 0, 0),
    ("OUTPUT", "R"): (2, 1, 0),
    ("OUTPUT", "WET"): (2, 2, 0),
    ("NOISE", "WHITE"): (3, 0, 1),
    ("NOISE", "PINK"): (3, 1, 1),
    ("VCF", "IN"): (4, 0, 0),
    ("VCF", "CUTOFF"): (4, 1, 0),
    ("VCF", "OUT"): (4, 2, 1),
    ("VCA 1", "IN"): (5, 0, 0),
    ("VCA 1", "ENV"): (5, 1, 0),
    ("VCA 1", "OUT"): (5, 2, 1),
    ("VCA 2", "IN"): (6, 0, 0),
    ("VCA 2", "CV"): (6, 1, 0),
    ("VCA 2", "OUT"): (6, 2, 1),
    ("EG 1", "TRIG"): (7, 0, 0),
    ("EG 1", "OUT A"): (7, 1, 1),
    ("EG 1", "OUT B"): (7, 2, 1),
    ("EG 1", "OUT C"): (7, 3, 1),
    ("MG", "FM"): (8, 0, 0),
    ("MG", "PWM"): (8, 1, 0),
    ("MG", "TRI"): (8, 2, 1),
    ("MG", "SAW"): (8, 3, 1),
    ("MG", "INV SAW"): (8, 4, 1),
    ("MG", "PULSE"): (8, 5, 1),
    ("VCO", "HZ/V"): (9, 0, 0),
    ("VCO", "V/OCT"): (9, 1, 0),
    ("VCO", "FM 1"): (9, 2, 0),
    ("VCO", "FM 2"): (9, 3, 0),
    ("VCO", "PWM"): (9, 4, 0),
    ("VCO", "TRI"): (9, 6, 1),
    ("VCO", "SAW"): (9, 5, 1),
    ("VCO", "PULSE"): (9, 7, 1),
    ("EG 2", "TRIG"): (10, 0, 0),
    ("EG 2", "OUT +"): (10, 1, 1),
    ("EG 2", "OUT −"): (10, 2, 1),
    ("EG 2", "DELAY"): (10, 3, 1),
    ("RING", "A"): (11, 0, 0),
    ("RING", "B"): (11, 1, 0),
    ("RING", "OUT"): (11, 2, 1),
    ("DIV", "IN"): (12, 0, 0),
    ("DIV", "/2"): (12, 1, 1),
    ("DIV", "/4"): (12, 2, 1),
    ("INV", "IN"): (13, 0, 0),
    ("INV", "OUT"): (13, 1, 1),
    ("INT", "IN"): (14, 0, 0),
    ("INT", "OUT"): (14, 1, 1),
    ("MIX", "IN 1"): (15, 0, 0),
    ("MIX", "IN 2"): (15, 1, 0),
    ("MIX", "IN 3"): (15, 2, 0),
    ("MIX", "OUT"): (15, 3, 1),
    ("S&H", "IN"): (16, 0, 0),
    ("S&H", "OUT"): (16, 1, 1),
    ("S&H", "CLOCK"): (16, 2, 0),
}

# layout.json "default" is the one default table (Source/Modular/PanelDefaults.h).
# A test checks that FaceKnobs, this file and the Voice program agree.


def knob_default(value):
    # Two decimals, or four when the table needs them (Ext In 0.3758 and 0.5316).
    text = f"{float(value):.4f}".rstrip("0")
    whole, frac = text.split(".")
    return f"{whole}.{frac.ljust(2, '0')}f"


def cf(value):
    return f"{float(value):.2f}f"

INK = "#dcd6c2"
GOLD = "#c29f4c"
W, H, M = 1600, 640, 14
CH_Y = 12


def c_escape(text):
    return text.replace("\\", "\\\\").replace('"', '\\"')


def font_size(rect_h):
    # build_panel stores height as font_size * 0.94 (12 pt titles, 11 pt labels).
    return 12 if rect_h >= 11.0 else 11


def baseline(rect):
    z = font_size(rect[3])
    return rect[1] + z * 0.74


def knob_ticks(knob):
    # Bodies are drawn live. The divider switch has 2 / 4 labels and no tick ring.
    if knob["label"] == "RATIO SWITCH":
        return ""
    cx, cy, r = knob["cx"], knob["cy"], knob["radius"]
    ticks = []
    for i in range(11):
        ang = math.radians(-135 + 27 * i - 90)
        length = 4 if i % 5 == 0 else 2.5
        ticks.append(
            f'<line x1="{cx + (r + 4) * math.cos(ang):.1f}" y1="{cy + (r + 4) * math.sin(ang):.1f}" '
            f'x2="{cx + (r + 4 + length) * math.cos(ang):.1f}" y2="{cy + (r + 4 + length) * math.sin(ang):.1f}" '
            f'stroke="{INK}" stroke-width="1.1"/>'
        )
    return "".join(ticks)


def jack_svg(hx, cy, radius):
    return (
        f'<circle cx="{hx:.1f}" cy="{cy:.1f}" r="{radius + 1.5}" fill="#000" opacity="0.55"/>'
        f'<circle cx="{hx:.1f}" cy="{cy:.1f}" r="{radius}" fill="url(#js)" stroke="#2a2a2c" stroke-width="0.9"/>'
        f'<circle cx="{hx:.1f}" cy="{cy:.1f}" r="{radius - 2.6}" fill="url(#jn)"/>'
        f'<circle cx="{hx:.1f}" cy="{cy:.1f}" r="{radius - 5}" fill="#030303"/>'
    )


def text_svg(lab):
    x, y, w, h = lab["rect"]
    z = font_size(h)
    by = baseline(lab["rect"])
    content = lab["text"].replace("&", "&amp;").replace("<", "&lt;")
    return (
        f'<text x="{x:.1f}" y="{by:.1f}" font-family="Helvetica" font-size="{z}" '
        f'font-weight="bold" fill="{INK}">{content}</text>'
    )


def build_svg(lay):
    cols = lay["columns"]
    frame_top = lay["bands"]["titles"][0] - 6
    frame_bottom = lay["bands"]["lane"][0]
    rules = [c["x"] for c in cols[1:]]
    parts = []
    parts.append(
        f'<rect width="{W}" height="{H}" rx="8" fill="url(#pf)"/>'
        f'<rect x="3" y="3" width="{W - 6}" height="{H - 6}" rx="6" fill="none" stroke="#050506" stroke-width="2"/>'
        f'<rect x="{M}" y="{frame_top}" width="{W - 2 * M}" height="{frame_bottom - frame_top}" '
        f'fill="none" stroke="{GOLD}" stroke-width="1.6"/>'
    )
    for rule in rules:
        parts.append(
            f'<line x1="{rule}" y1="{frame_top}" x2="{rule}" y2="{frame_bottom}" stroke="{GOLD}" stroke-width="1.6"/>'
        )
    # Top control is an on/off. FL Studio's FX slot owns the wet percent.
    # The editor paints the thumb snapped to one end.
    parts.append(
        f'<text x="{M + 4}" y="{CH_Y + 21}" font-family="Helvetica" font-size="14" '
        f'font-weight="bold" fill="#9a9684">SYNTHESIZER</text>'
        f'<text x="1484" y="31" font-family="Helvetica" font-size="11" '
        f'font-weight="bold" fill="{INK}">ON</text>'
        f'<rect x="1524" y="16" width="48" height="16" rx="8" fill="#050505" stroke="#3a3a3c"/>'
    )
    mtr = next(c for c in cols if c["title"] == "MTR")
    cx = mtr["x"] + mtr["w"] / 2
    parts.append(
        f'<rect x="{cx - 22:.1f}" y="82" width="44" height="32" rx="3" fill="#050505"/>'
        f'<rect x="{cx - 19:.1f}" y="85" width="38" height="26" rx="2" fill="url(#mg)"/>'
        f'<line x1="{cx:.1f}" y1="109" x2="{cx - 8:.1f}" y2="89" stroke="#111" stroke-width="1.5"/>'
        f'<circle cx="{cx:.1f}" cy="109" r="2" fill="#111"/>'
    )
    for knob in lay["knobs"]:
        parts.append(knob_ticks(knob))
    for jack in lay["jacks"]:
        parts.append(jack_svg(jack["x"], jack["y"], jack["radius"]))
    for lab in lay["labels"]:
        parts.append(text_svg(lab))
    for sx, sy in ((9, 9), (W - 9, 9), (9, H - 9), (W - 9, H - 9)):
        parts.append(
            f'<g transform="translate({sx} {sy})">'
            f'<circle r="5" fill="url(#js)" stroke="#000" stroke-width="0.9"/>'
            f'<line x1="-3.5" x2="3.5" stroke="#1a1a1a" stroke-width="1.5"/>'
            f'<line y1="-3.5" y2="3.5" stroke="#1a1a1a" stroke-width="1.5"/>'
            f"</g>"
        )
    defs = (
        '<defs>'
        '<linearGradient id="pf" x1="0" y1="0" x2="0" y2="1">'
        '<stop offset="0" stop-color="#242426"/><stop offset="1" stop-color="#161618"/>'
        "</linearGradient>"
        '<radialGradient id="js" cx="0.4" cy="0.35" r="0.8">'
        '<stop offset="0" stop-color="#e6e6e1"/><stop offset="1" stop-color="#7d7d79"/>'
        "</radialGradient>"
        '<radialGradient id="jn" cx="0.4" cy="0.35" r="0.8">'
        '<stop offset="0" stop-color="#9a9a96"/><stop offset="1" stop-color="#3c3c3c"/>'
        "</radialGradient>"
        '<linearGradient id="kb" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#4b4b4e"/><stop offset="0.5" stop-color="#1a1a1b"/>'
        '<stop offset="1" stop-color="#060607"/></linearGradient>'
        '<linearGradient id="kt" x1="0" y1="0" x2="1" y2="1">'
        '<stop offset="0" stop-color="#2a2a2c"/><stop offset="1" stop-color="#131314"/>'
        "</linearGradient>"
        '<radialGradient id="mg" cx="0.5" cy="0.9" r="1">'
        '<stop offset="0" stop-color="#f3dc92"/><stop offset="1" stop-color="#c9983a"/>'
        "</radialGradient></defs>"
    )
    body = "".join(parts)
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">'
        f"{defs}{body}</svg>\n"
    )


def build_inc(lay):
    lines = [
        "// Generated by panel/emit_panel_svg.py from panel/assets/layout.json.",
        "// Jack coordinates are design units on a 1600x640 canvas.",
        "",
        "struct PanelLabelRec {",
        "    const char* text;",
        "    float x;",
        "    float y;",
        "    float w;",
        "    float h;",
        "};",
        "",
        "struct PanelJackRec {",
        "    const char* section;",
        "    const char* label;",
        "    float x;",
        "    float y;",
        "    int module; // 0 none, 1 Ext In, 2 Output, 3 Noise, 4 VCF, 5 VCA 1, 6 VCA 2, 7 EG 1, 8 MG, 9 VCO, 10 EG 2, 11 Ring, 12 Divider, 13 Inverter, 14 Integrator, 15 Mixer, 16 Sample and hold",
        "    int port;",
        "    int dir; // 0 in, 1 out, -1 when the jack is not on the graph",
        "};",
        "",
        "struct PanelKnobRec {",
        "    const char* section;",
        "    const char* label;",
        "    float cx;",
        "    float cy;",
        "    float radius;",
        "    float valueDefault;",
        "    float hitX;",
        "    float hitY;",
        "    float hitW;",
        "    float hitH;",
        "    int kind; // 0 rotary, 1 divider switch",
        "};",
        "",
        "inline constexpr float kPanelW = 1600.0f;",
        "inline constexpr float kPanelH = 640.0f;",
        "",
    ]
    rocker = lay["power"]["rocker"]
    lines += [
        "// EFFECT rocker (layout key \"power\"). Left half is off (dry), right half is on (wet). The raised end points at that word.",
        f"inline constexpr float kPowerX = {cf(rocker[0])};",
        f"inline constexpr float kPowerY = {cf(rocker[1])};",
        f"inline constexpr float kPowerW = {cf(rocker[2])};",
        f"inline constexpr float kPowerH = {cf(rocker[3])};",
        "",
    ]
    hold = lay["buttons"][0]
    hit = hold["hit"]
    lines += [
        "// Momentary HOLD key under the EXT IN jacks. Mouse down holds the gate. Mouse up releases it.",
        "// The view paints the cap before the cables.",
        f"inline constexpr float kHoldCx = {cf(hold['cx'])};",
        f"inline constexpr float kHoldCy = {cf(hold['cy'])};",
        f"inline constexpr float kHoldHitX = {cf(hit[0])};",
        f"inline constexpr float kHoldHitY = {cf(hit[1])};",
        f"inline constexpr float kHoldHitW = {cf(hit[2])};",
        f"inline constexpr float kHoldHitH = {cf(hit[3])};",
        "// Red lamp above the HOLD key, lit while the key is held (like the BUSHIDO START lamp). The dark bezel is in the SVG.",
        f"inline constexpr float kHoldLampCx = {cf(hold['lamp']['cx'])};",
        f"inline constexpr float kHoldLampCy = {cf(hold['lamp']['cy'])};",
        f"inline constexpr float kHoldLampR = {cf(hold['lamp']['r'])};",
        "",
    ]
    screen = lay["screen"]
    bezel, lcd, key = screen["bezel"], screen["lcd"], screen["button"]
    lines += [
        "// Preset LCD in the top bar. The glass is in the SVG. The view draws the dots.",
        f"inline constexpr float kPresetBezelX = {cf(bezel[0])};",
        f"inline constexpr float kPresetBezelY = {cf(bezel[1])};",
        f"inline constexpr float kPresetBezelW = {cf(bezel[2])};",
        f"inline constexpr float kPresetBezelH = {cf(bezel[3])};",
        f"inline constexpr float kPresetLcdX = {cf(lcd[0])};",
        f"inline constexpr float kPresetLcdY = {cf(lcd[1])};",
        f"inline constexpr float kPresetLcdW = {cf(lcd[2])};",
        f"inline constexpr float kPresetLcdH = {cf(lcd[3])};",
        f"inline constexpr float kPresetKeyX = {cf(key[0])};",
        f"inline constexpr float kPresetKeyY = {cf(key[1])};",
        f"inline constexpr float kPresetKeyW = {cf(key[2])};",
        f"inline constexpr float kPresetKeyH = {cf(key[3])};",
        f"inline constexpr int kPresetChars = {int(screen['chars'])};",
        "",
    ]
    meter = lay["meter"]
    face, pivot = meter["face"], meter["pivot"]
    lines += [
        "// VU face is in the SVG. The view draws the needle from the selected jack.",
        f"inline constexpr float kMeterFaceX = {cf(face[0])};",
        f"inline constexpr float kMeterFaceY = {cf(face[1])};",
        f"inline constexpr float kMeterFaceW = {cf(face[2])};",
        f"inline constexpr float kMeterFaceH = {cf(face[3])};",
        f"inline constexpr float kMeterPivotX = {cf(pivot[0])};",
        f"inline constexpr float kMeterPivotY = {cf(pivot[1])};",
        f"inline constexpr float kMeterRadius = {cf(meter['radius'])};",
        f"inline constexpr float kMeterScale = {cf(meter['scale'])};",
        f"inline constexpr float kMeterMinAngle = {cf(meter['minAngle'])};",
        f"inline constexpr float kMeterMaxAngle = {cf(meter['maxAngle'])};",
        "",
    ]
    labels = lay["labels"]
    lines.append(f"inline constexpr int kPanelLabelCount = {len(labels)};")
    lines.append("inline constexpr PanelLabelRec kPanelLabels[] = {")
    for lab in labels:
        x, y, w, h = lab["rect"]
        lines.append(
            f'    {{ "{c_escape(lab["text"])}", {x:.1f}f, {y:.1f}f, {w:.1f}f, {h:.1f}f }},'
        )
    lines.append("};")
    lines.append("")
    jacks = lay["jacks"]
    lines.append(f"inline constexpr int kPanelJackCount = {len(jacks)};")
    lines.append("inline constexpr PanelJackRec kPanelJacks[] = {")
    for jack in jacks:
        module, port, direction = LIVE.get((jack["section"], jack["label"]), (0, -1, -1))
        lines.append(
            f'    {{ "{c_escape(jack["section"])}", "{c_escape(jack["label"])}", '
            f'{jack["x"]:.1f}f, {jack["y"]:.1f}f, {module}, {port}, {direction} }},'
        )
    lines.append("};")
    lines.append("")
    knobs = lay["knobs"]
    lines.append(f"inline constexpr int kPanelKnobCount = {len(knobs)};")
    lines.append("inline constexpr PanelKnobRec kPanelKnobs[] = {")
    for knob in knobs:
        kind = 1 if knob["label"] == "RATIO SWITCH" else 0
        hit = knob["hit"]
        default = knob_default(knob["default"])
        lines.append(
            f'    {{ "{c_escape(knob["section"])}", "{c_escape(knob["label"])}", '
            f'{knob["cx"]:.1f}f, {knob["cy"]:.1f}f, {knob["radius"]:.1f}f, {default}, '
            f'{hit[0]:.1f}f, {hit[1]:.1f}f, {hit[2]:.1f}f, {hit[3]:.1f}f, {kind} }},'
        )
    lines.append("};")
    lines.append("")
    return "\n".join(lines) + "\n"


def main():
    lay = json.load(open(LAY))
    assert lay["canvas"] == [W, H]
    assert len(lay["jacks"]) == 57, len(lay["jacks"])
    assert len(lay["knobs"]) == 33, len(lay["knobs"])
    titles = [column["title"] for column in lay["columns"]]
    assert titles[-2:] == ["EXT IN", "OUTPUT"], titles
    bg = os.path.join(os.path.dirname(SVG), "panel_bg.svg")
    assert os.path.isfile(SVG), SVG
    assert os.path.isfile(bg), bg
    os.makedirs(os.path.dirname(INC), exist_ok=True)
    with open(INC, "w") as handle:
        handle.write(build_inc(lay))
    print(f"wrote {INC}")


if __name__ == "__main__":
    main()
