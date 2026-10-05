#!/usr/bin/env python3
"""Write panel/assets/panel.svg and Source/UI/PanelGeometry.inc from layout.json.

Positions stay in layout.json. This script does not move a jack. The SVG is the
resizable panel (no baked PNG). Re-run from the repo root:

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
}

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
    # Bodies are drawn live. The divider switch has 2 / 4 / 16 labels and no tick ring.
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
    # Top mix track is drawn empty. The editor paints the thumb from Output mix.
    parts.append(
        f'<text x="{M + 4}" y="{CH_Y + 21}" font-family="Helvetica" font-size="14" '
        f'font-weight="bold" fill="#9a9684">SYNTHESIZER</text>'
        f'<text x="{W - M - 228}" y="{CH_Y + 19}" font-family="Helvetica" font-size="11" '
        f'font-weight="bold" fill="{INK}">MIX</text>'
        f'<rect x="{W - M - 190}" y="{CH_Y + 13}" width="170" height="4" rx="2" fill="#050505" stroke="#3a3a3c"/>'
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
        "    int module; // 0 none, 1 Ext In, 2 Output, 3 Noise",
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
        "inline constexpr float kMixTrackX = 1396.0f;",
        "inline constexpr float kMixTrackY = 25.0f;",
        "inline constexpr float kMixTrackW = 170.0f;",
        "inline constexpr float kMixTrackH = 4.0f;",
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
        lines.append(
            f'    {{ "{c_escape(knob["section"])}", "{c_escape(knob["label"])}", '
            f'{knob["cx"]:.1f}f, {knob["cy"]:.1f}f, {knob["radius"]:.1f}f, {knob["default"]:.2f}f, '
            f'{hit[0]:.1f}f, {hit[1]:.1f}f, {hit[2]:.1f}f, {hit[3]:.1f}f, {kind} }},'
        )
    lines.append("};")
    lines.append("")
    return "\n".join(lines) + "\n"


def main():
    lay = json.load(open(LAY))
    assert lay["canvas"] == [W, H]
    assert len(lay["jacks"]) == 58, len(lay["jacks"])
    assert len(lay["knobs"]) == 31, len(lay["knobs"])
    titles = [column["title"] for column in lay["columns"]]
    assert titles[-2:] == ["EXT IN", "OUTPUT"], titles
    svg = build_svg(lay)
    assert "kbody" not in svg
    os.makedirs(os.path.dirname(SVG), exist_ok=True)
    os.makedirs(os.path.dirname(INC), exist_ok=True)
    with open(SVG, "w") as handle:
        handle.write(svg)
    with open(INC, "w") as handle:
        handle.write(build_inc(lay))
    print(f"wrote {SVG} ({len(svg)} bytes) and {INC}")


if __name__ == "__main__":
    main()
