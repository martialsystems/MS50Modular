#!/usr/bin/env python3
"""Compile the design-pack PDF from the markdown sources.

Reading order is fixed. The bibliography is the last part of 01-research.md.
"""

from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    CondPageBreak,
    HRFlowable,
    ListFlowable,
    ListItem,
    PageBreak,
    Paragraph,
    Preformatted,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

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


def styles():
    base = getSampleStyleSheet()
    serif = "Times-Roman"
    serif_b = "Times-Bold"
    mono = "Courier"
    return {
        "h1": ParagraphStyle(
            "H1",
            fontName=serif_b,
            fontSize=16,
            leading=20,
            spaceBefore=14,
            spaceAfter=8,
            textColor=colors.HexColor("#1a1a1a"),
        ),
        "h2": ParagraphStyle(
            "H2",
            fontName=serif_b,
            fontSize=13,
            leading=16,
            spaceBefore=12,
            spaceAfter=6,
        ),
        "h3": ParagraphStyle(
            "H3",
            fontName=serif_b,
            fontSize=11,
            leading=14,
            spaceBefore=10,
            spaceAfter=4,
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
        "footer": ParagraphStyle(
            "Footer",
            fontName=serif,
            fontSize=8,
            textColor=colors.HexColor("#444444"),
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
    parts = [p.strip() for p in line.strip().strip("|").split("|")]
    return parts


def is_separator(line: str) -> bool:
    return bool(re.match(r"^\s*\|?\s*:?-{3,}", line)) and set(line.replace("|", "").replace(":", "").replace("-", "").replace(" ", "")) == set()


def add_table(flow, rows, st):
    if not rows:
        return
    width = 7.0 * inch
    cols = max(len(r) for r in rows)
    # Pad short rows.
    norm = [r + [""] * (cols - len(r)) for r in rows]
    # Weight the last column when there are many columns.
    if cols == 2:
        widths = [1.6 * inch, 5.4 * inch]
    elif cols == 3:
        widths = [1.3 * inch, 2.2 * inch, 3.5 * inch]
    elif cols == 4:
        widths = [1.15 * inch, 1.7 * inch, 1.7 * inch, 2.45 * inch]
    elif cols == 5:
        widths = [0.7 * inch, 1.5 * inch, 1.3 * inch, 1.5 * inch, 2.0 * inch]
    else:
        widths = [width / cols] * cols
    data = []
    for i, row in enumerate(norm):
        style = st["cellh"] if i == 0 else st["cell"]
        data.append([Paragraph(inline(c) if c else "&nbsp;", style) for c in row])
    table = Table(data, colWidths=widths, repeatRows=1)
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
            # Preformatted does not wrap. Break long lines.
            wrapped = []
            for raw in block.splitlines() or [""]:
                while len(raw) > 96:
                    wrapped.append(raw[:96])
                    raw = raw[96:]
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
        # Paragraph. Join following non-special lines.
        buf = [line.strip()]
        i += 1
        while i < len(lines) and lines[i].strip() and not lines[i].startswith(("#", "```", "|", "- ", "* ")):
            buf.append(lines[i].strip())
            i += 1
        flow.append(Paragraph(inline(" ".join(buf)), st["body"]))
    return flow


def draw_page(canvas, doc):
    canvas.saveState()
    width, height = letter
    canvas.setFillColor(colors.HexColor("#1a1a1a"))
    canvas.setFont("Times-Bold", 8)
    canvas.drawString(
        0.85 * inch,
        height - 0.42 * inch,
        "Copyright (c) 2026 Martial Systems LLC. All rights reserved.",
    )
    canvas.setFont("Times-Roman", 7.5)
    canvas.drawString(
        0.85 * inch,
        height - 0.56 * inch,
        "Korg owns the MS-50 name and its circuit designs. Independent study. Not a Korg product. Not a license of those designs.",
    )
    canvas.setStrokeColor(colors.HexColor("#1a1a1a"))
    canvas.line(0.85 * inch, height - 0.68 * inch, width - 0.85 * inch, height - 0.68 * inch)
    canvas.setFillColor(colors.HexColor("#444444"))
    canvas.setFont("Times-Roman", 8)
    canvas.drawString(0.85 * inch, 0.48 * inch, "MS-50 Modular design pack  |  2026-09-21")
    canvas.drawRightString(width - 0.85 * inch, 0.48 * inch, f"{doc.page}")
    canvas.setStrokeColor(colors.HexColor("#c8c2b4"))
    canvas.line(0.85 * inch, 0.64 * inch, width - 0.85 * inch, 0.64 * inch)
    canvas.restoreState()


def build():
    st = styles()
    story = []
    story.append(Spacer(1, 0.15 * inch))
    story.append(Paragraph("MS-50 Modular", st["cover_title"]))
    story.append(Paragraph("Design pack for a white-box modular FX VST", st["cover_sub"]))
    story.append(Paragraph("Public study notes. Not a Korg product.", st["cover_sub"]))
    story.append(Spacer(1, 0.2 * inch))
    story.append(HRFlowable(width="100%", thickness=1, color=colors.HexColor("#1a1a1a")))
    story.append(Spacer(1, 0.15 * inch))
    story.append(Paragraph("Document date: 2026-09-21", st["body"]))
    story.append(Paragraph(
        "Stack specified here: C++20, JUCE 8, CMake, VST3 effect, stereo in and stereo out, "
        "mono module graph, public git repository.",
        st["body"],
    ))
    story.append(Spacer(1, 0.15 * inch))
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
        "A generated timestamp is not a revision. Later edits add a line here and a date "
        "on the changed section heading in the markdown.",
        st["body"],
    ))
    story.append(Spacer(1, 0.2 * inch))
    story.append(Paragraph("Contents", st["h2"]))
    for name, _path in CHAPTERS:
        story.append(Paragraph(name, st["toc"]))
    story.append(Paragraph("Bibliography (end of the research summary)", st["toc"]))
    story.append(PageBreak())

    for title, path in CHAPTERS:
        story.append(Paragraph(title, st["h1"]))
        story.append(HRFlowable(width="100%", thickness=0.4, color=colors.HexColor("#1a1a1a")))
        story.append(Spacer(1, 6))
        text = path.read_text(encoding="utf-8")
        story.extend(markdown_to_flow(text, st, skip_first_h1=True))
        story.append(PageBreak())

    doc = SimpleDocTemplate(
        str(OUT),
        pagesize=letter,
        leftMargin=0.85 * inch,
        rightMargin=0.85 * inch,
        topMargin=0.92 * inch,
        bottomMargin=0.8 * inch,
        title="MS-50 Modular design pack",
        author="Martial Systems LLC",
    )
    doc.build(story, onFirstPage=draw_page, onLaterPages=draw_page)
    print(OUT)


if __name__ == "__main__":
    build()
