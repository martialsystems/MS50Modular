#!/usr/bin/env python3
"""Compile the design-pack PDF from the markdown sources.

Reading order is fixed. The bibliography is the last part of 01-research.md.

Page geometry is one set of constants. The header rule, the body column, and
the footer rule share the same left and right edges. Each markdown heading
starts a page. A heading that is already at the top of a page does not insert
a blank page.
"""

from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    HRFlowable,
    PageTemplate,
    Paragraph,
    Preformatted,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.platypus.flowables import PageBreakIfNotEmpty

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
OUT = DOCS / "MS50_Modular_Design_Pack.pdf"

CHAPTERS = [
    ("Methodology", ROOT / "METHODOLOGY.md"),
    ("Research summary", DOCS / "01-research.md"),
    ("Software schematics", DOCS / "SCHEMATICS.md"),
    ("Build guide", DOCS / "BUILD_GUIDE.md"),
    ("Test plan", DOCS / "TESTPLAN.md"),
]

PAGE_W, PAGE_H = letter
LEFT = 0.75 * inch
RIGHT = 0.75 * inch
# Distances from the top of the page.
HEADER_LINE_1 = 0.40 * inch
HEADER_LINE_2 = 0.54 * inch
HEADER_LINE_3 = 0.68 * inch
HEADER_RULE = 0.84 * inch
CONTENT_TOP = 1.04 * inch
# Distances from the bottom of the page.
FOOTER_RULE = 0.58 * inch
FOOTER_BASELINE = 0.36 * inch
CONTENT_BOTTOM = FOOTER_RULE + 0.20 * inch
FRAME_WIDTH = PAGE_W - LEFT - RIGHT
FRAME_HEIGHT = PAGE_H - CONTENT_TOP - CONTENT_BOTTOM
# Courier at 7.5 pt is 4.5 pt wide. Leave the code indent inside the column.
CODE_COLS = int((FRAME_WIDTH - 12) / 4.5)

INK = colors.HexColor("#1a1a1a")
RULE = colors.HexColor("#1a1a1a")
FOOTER_INK = colors.HexColor("#333333")


def styles():
    serif = "Times-Roman"
    serif_b = "Times-Bold"
    mono = "Courier"
    return {
        "h1": ParagraphStyle(
            "H1",
            fontName=serif_b,
            fontSize=16,
            leading=20,
            spaceBefore=0,
            spaceAfter=8,
            textColor=INK,
        ),
        "h2": ParagraphStyle(
            "H2",
            fontName=serif_b,
            fontSize=13,
            leading=16,
            spaceBefore=0,
            spaceAfter=6,
            textColor=INK,
        ),
        "h3": ParagraphStyle(
            "H3",
            fontName=serif_b,
            fontSize=11,
            leading=14,
            spaceBefore=0,
            spaceAfter=4,
            textColor=INK,
        ),
        "body": ParagraphStyle(
            "Body",
            fontName=serif,
            fontSize=10,
            leading=13,
            alignment=TA_JUSTIFY,
            spaceAfter=6,
        ),
        "bullet": ParagraphStyle(
            "BulletBody",
            fontName=serif,
            fontSize=10,
            leading=13,
            leftIndent=0,
            spaceAfter=2,
        ),
        "cell": ParagraphStyle(
            "Cell",
            fontName=serif,
            fontSize=8,
            leading=10,
        ),
        "cellh": ParagraphStyle(
            "CellH",
            fontName=serif_b,
            fontSize=8,
            leading=10,
        ),
        "cover_title": ParagraphStyle(
            "CoverTitle",
            fontName=serif_b,
            fontSize=22,
            leading=26,
            alignment=TA_LEFT,
            spaceAfter=8,
            textColor=INK,
        ),
        "cover_sub": ParagraphStyle(
            "CoverSub",
            fontName=serif,
            fontSize=12,
            leading=16,
            textColor=colors.HexColor("#333333"),
            spaceAfter=6,
        ),
        "toc": ParagraphStyle(
            "TOC",
            fontName=serif,
            fontSize=12,
            leading=18,
        ),
        "code": ParagraphStyle(
            "Code",
            fontName=mono,
            fontSize=7.5,
            leading=9.5,
            leftIndent=6,
            rightIndent=4,
            spaceBefore=4,
            spaceAfter=8,
            backColor=colors.HexColor("#f4f1ea"),
        ),
    }


def inline(text: str) -> str:
    text = html.escape(text)
    text = re.sub(r"`([^`]+)`", r"<font face='Courier' size='8'>\1</font>", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", text)
    return text


def is_table_line(line: str) -> bool:
    s = line.strip()
    return s.startswith("|") and s.endswith("|") and s.count("|") >= 2


def split_row(line: str) -> list[str]:
    return [p.strip() for p in line.strip().strip("|").split("|")]


def is_separator(line: str) -> bool:
    return bool(re.match(r"^\s*\|?\s*:?-{3,}", line)) and set(
        line.replace("|", "").replace(":", "").replace("-", "").replace(" ", "")
    ) == set()


def column_widths(cols: int, width: float) -> list[float]:
    # Shares of the body column, not of the paper. A 7 inch table on a
    # narrower frame is what pushed rows past the footer rule.
    shares = {
        2: [1.6, 5.4],
        3: [1.3, 2.2, 3.5],
        4: [1.15, 1.7, 1.7, 2.45],
        5: [0.7, 1.5, 1.3, 1.5, 2.0],
    }
    parts = shares.get(cols)
    if parts is None:
        return [width / cols] * cols
    scale = width / sum(parts)
    return [part * scale for part in parts]


def add_table(flow, rows, st):
    if not rows:
        return
    cols = max(len(r) for r in rows)
    norm = [r + [""] * (cols - len(r)) for r in rows]
    data = []
    for i, row in enumerate(norm):
        style = st["cellh"] if i == 0 else st["cell"]
        data.append([Paragraph(inline(c) if c else "&nbsp;", style) for c in row])
    table = Table(data, colWidths=column_widths(cols, FRAME_WIDTH), repeatRows=1)
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e6e0d4")),
                ("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#b9b2a4")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 3),
                ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                ("TOPPADDING", (0, 0), (-1, -1), 2),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
            ]
        )
    )
    flow.append(Spacer(1, 4))
    flow.append(table)
    flow.append(Spacer(1, 8))


def markdown_to_flow(text: str, st, skip_first_h1: bool = False) -> list:
    flow = []
    lines = text.splitlines()
    i = 0
    first_h1 = True
    while i < len(lines):
        line = lines[i]
        if line.strip() == "":
            i += 1
            continue
        if line.startswith("```"):
            i += 1
            buf = []
            while i < len(lines) and not lines[i].startswith("```"):
                buf.append(lines[i].replace("\t", "    "))
                i += 1
            i += 1
            block = "\n".join(buf)
            wrapped = []
            for raw in block.splitlines() or [""]:
                while len(raw) > CODE_COLS:
                    wrapped.append(raw[:CODE_COLS])
                    raw = raw[CODE_COLS:]
                wrapped.append(raw)
            flow.append(Preformatted("\n".join(wrapped), st["code"]))
            continue
        if is_table_line(line):
            rows = []
            while i < len(lines) and is_table_line(lines[i]):
                if not is_separator(lines[i]):
                    rows.append(split_row(lines[i]))
                i += 1
            add_table(flow, rows, st)
            continue
        if line.startswith("#"):
            level = len(line) - len(line.lstrip("#"))
            title = line[level:].strip()
            if level == 1 and first_h1 and skip_first_h1:
                first_h1 = False
                i += 1
                continue
            first_h1 = False
            level = min(level, 3)
            # Already-empty pages drop this break, so two headings in a row
            # do not produce a blank sheet.
            flow.append(PageBreakIfNotEmpty())
            key = {1: "h1", 2: "h2"}.get(level, "h3")
            flow.append(Paragraph(inline(title), st[key]))
            i += 1
            continue
        if line.lstrip().startswith(("- ", "* ")):
            while i < len(lines) and lines[i].lstrip().startswith(("- ", "* ")):
                item = lines[i].lstrip()[2:].strip()
                flow.append(Paragraph("- " + inline(item), st["bullet"]))
                i += 1
            flow.append(Spacer(1, 4))
            continue
        buf = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(("#", "```", "|", "- ", "* ")):
            buf.append(lines[i].strip())
            i += 1
        flow.append(Paragraph(inline(" ".join(buf)), st["body"]))
    return flow


def draw_page(canvas, doc):
    canvas.saveState()
    canvas.setFillColor(INK)
    canvas.setFont("Times-Bold", 8)
    canvas.drawString(LEFT, PAGE_H - HEADER_LINE_1, "Copyright (c) 2026 Martial Systems LLC. All rights reserved.")
    canvas.setFont("Times-Roman", 7.5)
    canvas.drawString(
        LEFT,
        PAGE_H - HEADER_LINE_2,
        "The Korg MS-50, its name, and its circuit designs are the property of Korg Inc.",
    )
    canvas.drawString(
        LEFT,
        PAGE_H - HEADER_LINE_3,
        "This independent study is not produced or endorsed by Korg, and it grants no license to those designs.",
    )
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.6)
    canvas.line(LEFT, PAGE_H - HEADER_RULE, PAGE_W - RIGHT, PAGE_H - HEADER_RULE)

    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.6)
    canvas.line(LEFT, FOOTER_RULE, PAGE_W - RIGHT, FOOTER_RULE)
    canvas.setFillColor(FOOTER_INK)
    canvas.setFont("Times-Roman", 9)
    canvas.drawString(LEFT, FOOTER_BASELINE, "MS-50 Modular design pack")
    canvas.drawCentredString(PAGE_W / 2.0, FOOTER_BASELINE, "2026-09-21")
    canvas.drawRightString(PAGE_W - RIGHT, FOOTER_BASELINE, str(canvas.getPageNumber()))
    canvas.restoreState()


def build():
    st = styles()
    story = []
    story.append(Spacer(1, 0.12 * inch))
    story.append(Paragraph("MS-50 Modular", st["cover_title"]))
    story.append(Paragraph("Design pack for a white-box modular FX VST", st["cover_sub"]))
    story.append(Paragraph(
        "The Korg MS-50, the MS-50 name, and the circuit designs of that instrument are the property of Korg Inc. "
        "Martial Systems LLC claims copyright only in the original text of this repository and in any code later written here. "
        "The work is an independent study of published schematics and of the literature cited in the research summary. "
        "Korg has not produced, sponsored, or endorsed it. "
        "No license is granted to the MS-50 design, to the Korg drawings, or to the Korg trademarks. "
        "The instrument's name is used only to identify the subject of the study.",
        st["body"],
    ))
    story.append(Spacer(1, 0.15 * inch))
    story.append(HRFlowable(width="100%", thickness=1, color=RULE, spaceBefore=0, spaceAfter=0))
    story.append(Spacer(1, 0.12 * inch))
    story.append(Paragraph("Document date: 2026-09-21", st["body"]))
    story.append(Paragraph(
        "Stack specified here: C++20, JUCE 8, CMake, VST3 effect, stereo in and stereo out, "
        "mono module graph, public git repository.",
        st["body"],
    ))
    story.append(Spacer(1, 0.12 * inch))
    story.append(Paragraph("Revisions", st["h2"]))
    story.append(Paragraph(
        "2026-09-21: first compiled pack. Cover, methodology, research summary, software "
        "schematic, build guide, test plan, and bibliography.",
        st["body"],
    ))
    story.append(Paragraph(
        "2026-09-21: copyright and the Korg notice placed at the top of every page. "
        "The repository is public.",
        st["body"],
    ))
    story.append(Paragraph(
        "2026-09-21: rights statement set out in full. Korg Inc. is named as owner of the MS-50, "
        "its name, and its circuit designs. Martial Systems LLC claims copyright only in the original text and code of this repository.",
        st["body"],
    ))
    story.append(Paragraph(
        "2026-09-21: page layout. Each heading starts on a new page. The footer is one rule, "
        "the pack title, the date, and the page number, on the same baselines on every page. "
        "The body column stops above that rule.",
        st["body"],
    ))
    story.append(Paragraph(
        "A generated timestamp is not a revision. Later edits add a line here and a date "
        "on the changed section heading in the markdown.",
        st["body"],
    ))
    story.append(Spacer(1, 0.12 * inch))
    story.append(Paragraph("Contents", st["h2"]))
    for name, _path in CHAPTERS:
        story.append(Paragraph(name, st["toc"]))
    story.append(Paragraph("Bibliography (end of the research summary)", st["toc"]))

    for title, path in CHAPTERS:
        story.append(PageBreakIfNotEmpty())
        story.append(Paragraph(title, st["h1"]))
        story.append(HRFlowable(width="100%", thickness=0.6, color=RULE, spaceBefore=2, spaceAfter=8))
        text = path.read_text(encoding="utf-8")
        story.extend(markdown_to_flow(text, st, skip_first_h1=True))

    frame = Frame(
        LEFT,
        CONTENT_BOTTOM,
        FRAME_WIDTH,
        FRAME_HEIGHT,
        leftPadding=0,
        rightPadding=0,
        topPadding=0,
        bottomPadding=0,
        id="body",
        showBoundary=0,
    )
    doc = BaseDocTemplate(
        str(OUT),
        pagesize=letter,
        title="MS-50 Modular design pack",
        author="Martial Systems LLC",
        pageTemplates=[PageTemplate(id="main", frames=[frame], onPage=draw_page)],
    )
    doc.build(story)
    print(OUT)


if __name__ == "__main__":
    build()
