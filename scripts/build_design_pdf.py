#!/usr/bin/env python3
"""Compile the design-pack PDF from the markdown sources.

The body is a research note: headings keep the wording of the sources, and
the type is 10 point. The reference list stays in APA and is the last part
of 01-research.md. It closes the pack.

The copyright notice and the pack line share one footer. There is no running
header. Body text flows. A heading stays with the line under it. A table or a
code listing moves to the next page only when that whole block fits on one
page and does not fit in the space left. A heading that follows a chart has
a break before it, and starts on the next page when less than two inches
remain.
"""

from __future__ import annotations

import html
import re
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import inch
from reportlab.platypus import (
    BaseDocTemplate,
    Flowable,
    Frame,
    PageTemplate,
    Paragraph,
    Preformatted,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.platypus.doctemplate import FrameBreak
from reportlab.platypus.flowables import KeepTogether, PageBreakIfNotEmpty

ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
OUT = DOCS / "MS50_Modular_Design_Pack.pdf"

CHAPTERS = [
    ROOT / "METHODOLOGY.md",
    DOCS / "01-research.md",
    DOCS / "SCHEMATICS.md",
    DOCS / "BUILD_GUIDE.md",
    DOCS / "TESTPLAN.md",
]

PAGE_W, PAGE_H = letter
LEFT = 1.0 * inch
RIGHT = 1.0 * inch
TOP = 1.0 * inch
# Footer, from the bottom: pack line, two Korg lines, copyright, then the rule.
FOOTER_BASE = 0.42 * inch
FOOTER_STEP = 11
FOOTER_RULE = FOOTER_BASE + 48
CONTENT_BOTTOM = FOOTER_RULE + 14
FRAME_WIDTH = PAGE_W - LEFT - RIGHT
FRAME_HEIGHT = PAGE_H - TOP - CONTENT_BOTTOM
# Courier 9 pt is 5.4 pt wide.
CODE_COLS = int((FRAME_WIDTH - 8) / 5.4)

INK = colors.HexColor("#1a1a1a")
RULE = colors.HexColor("#b9b2a4")
HEADING_AFTER_CHART = 2.0 * inch


class BreakAfterChart(Flowable):
    """Gap and rule after a chart. If the next heading would be cramped, start a new page."""

    def wrap(self, availWidth, availHeight):
        if availHeight < HEADING_AFTER_CHART:
            frame = self._doctemplateAttr("frame")
            if frame is not None:
                frame.add_generated_content(FrameBreak)
            return 0, 0
        self.width = availWidth
        self.height = 16
        return availWidth, 16

    def draw(self):
        self.canv.setStrokeColor(RULE)
        self.canv.setLineWidth(0.4)
        self.canv.line(0, 8, self.width, 8)


def styles():
    serif = "Times-Roman"
    serif_b = "Times-Bold"
    return {
        "h1": ParagraphStyle(
            "H1",
            fontName=serif_b,
            fontSize=16,
            leading=20,
            alignment=TA_LEFT,
            spaceBefore=12,
            spaceAfter=6,
            textColor=INK,
        ),
        "h2": ParagraphStyle(
            "H2",
            fontName=serif_b,
            fontSize=13,
            leading=16,
            alignment=TA_LEFT,
            spaceBefore=10,
            spaceAfter=4,
            textColor=INK,
        ),
        "h3": ParagraphStyle(
            "H3",
            fontName=serif_b,
            fontSize=11,
            leading=14,
            alignment=TA_LEFT,
            spaceBefore=8,
            spaceAfter=3,
            textColor=INK,
        ),
        "title": ParagraphStyle(
            "Title",
            fontName=serif_b,
            fontSize=22,
            leading=26,
            alignment=TA_LEFT,
            spaceBefore=0,
            spaceAfter=6,
            textColor=INK,
        ),
        "cover_sub": ParagraphStyle(
            "CoverSub",
            fontName=serif,
            fontSize=12,
            leading=16,
            alignment=TA_LEFT,
            spaceBefore=0,
            spaceAfter=8,
            textColor=colors.HexColor("#333333"),
        ),
        "body": ParagraphStyle(
            "Body",
            fontName=serif,
            fontSize=10,
            leading=13,
            alignment=TA_LEFT,
            spaceBefore=0,
            spaceAfter=6,
            textColor=INK,
        ),
        "bullet": ParagraphStyle(
            "BulletBody",
            fontName=serif,
            fontSize=10,
            leading=13,
            alignment=TA_LEFT,
            leftIndent=12,
            firstLineIndent=0,
            spaceBefore=0,
            spaceAfter=2,
            textColor=INK,
        ),
        "refhead": ParagraphStyle(
            "RefHead",
            fontName=serif_b,
            fontSize=12,
            leading=24,
            alignment=TA_CENTER,
            spaceBefore=0,
            spaceAfter=12,
            textColor=INK,
        ),
        "ref": ParagraphStyle(
            "Reference",
            fontName=serif,
            fontSize=12,
            leading=24,
            alignment=TA_LEFT,
            leftIndent=0.5 * inch,
            firstLineIndent=-0.5 * inch,
            spaceBefore=0,
            spaceAfter=0,
            textColor=INK,
        ),
        "refnote": ParagraphStyle(
            "ReferenceNote",
            fontName=serif,
            fontSize=12,
            leading=24,
            alignment=TA_LEFT,
            leftIndent=0.5 * inch,
            firstLineIndent=0,
            spaceBefore=0,
            spaceAfter=0,
            textColor=INK,
        ),
        "cell": ParagraphStyle(
            "Cell",
            fontName=serif,
            fontSize=8,
            leading=10,
            alignment=TA_LEFT,
            textColor=INK,
        ),
        "cellh": ParagraphStyle(
            "CellH",
            fontName=serif_b,
            fontSize=8,
            leading=10,
            alignment=TA_LEFT,
            textColor=INK,
        ),
        "code": ParagraphStyle(
            "Code",
            fontName="Courier",
            fontSize=9,
            leading=11,
            leftIndent=0,
            rightIndent=0,
            spaceBefore=4,
            spaceAfter=6,
            backColor=colors.HexColor("#f4f1ea"),
            textColor=INK,
        ),
    }


def inline(text: str) -> str:
    text = html.escape(text)
    text = re.sub(r"`([^`]+)`", r"<font face='Courier' size='9'>\1</font>", text)
    text = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", text)
    text = re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<i>\1</i>", text)
    return text


def heading(text: str, style) -> Paragraph:
    paragraph = Paragraph(inline(text), style)
    paragraph.keepWithNext = True
    return paragraph


def is_table_line(line: str) -> bool:
    stripped = line.strip()
    return stripped.startswith("|") and stripped.endswith("|") and stripped.count("|") >= 2


def split_row(line: str) -> list[str]:
    return [part.strip() for part in line.strip().strip("|").split("|")]


def is_separator(line: str) -> bool:
    return bool(re.match(r"^\s*\|?\s*:?-{3,}", line)) and set(
        line.replace("|", "").replace(":", "").replace("-", "").replace(" ", "")
    ) == set()


def column_widths(cols: int, width: float) -> list[float]:
    shares = {
        2: [1.6, 5.4],
        3: [1.5, 2.2, 2.8],
        4: [1.2, 1.7, 1.6, 2.0],
        5: [0.8, 1.4, 1.2, 1.4, 1.7],
    }
    parts = shares.get(cols)
    if parts is None:
        return [width / cols] * cols
    scale = width / sum(parts)
    return [part * scale for part in parts]


def measured_height(flowable) -> float:
    _width, height = flowable.wrap(FRAME_WIDTH, FRAME_HEIGHT)
    return height


def place_block(flow, block):
    """Move a chart or listing that fits on one page when the space left is short."""
    try:
        height = measured_height(block)
    except Exception:
        block.is_chart = True
        flow.append(block)
        return
    if height <= FRAME_HEIGHT - 12:
        block = KeepTogether([block])
    block.is_chart = True
    flow.append(block)


def add_table(flow, rows, st):
    if not rows:
        return
    cols = max(len(row) for row in rows)
    norm = [row + [""] * (cols - len(row)) for row in rows]
    data = []
    for index, row in enumerate(norm):
        style = st["cellh"] if index == 0 else st["cell"]
        data.append([Paragraph(inline(cell) if cell else "&nbsp;", style) for cell in row])
    table = Table(data, colWidths=column_widths(cols, FRAME_WIDTH), repeatRows=1)
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#e6e0d4")),
                ("GRID", (0, 0), (-1, -1), 0.3, RULE),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 3),
                ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                ("TOPPADDING", (0, 0), (-1, -1), 2),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
            ]
        )
    )
    if flow and not getattr(flow[-1], "keepWithNext", False):
        previous = flow[-1]
        if isinstance(previous, Paragraph) and len(previous.getPlainText()) < 80:
            previous.keepWithNext = True
    place_block(flow, table)


def split_references(text: str) -> tuple[str, str]:
    """The reference list closes the pack."""
    lines = text.splitlines(keepends=True)
    for index, line in enumerate(lines):
        if line.strip().lower() in {"## references", "## bibliography"}:
            return "".join(lines[:index]), "".join(lines[index:])
    return text, ""


def strip_banner(text: str) -> str:
    """The footer carries the copyright notice. Do not repeat it before each chapter."""
    lines = text.splitlines()
    index = 0
    while index < len(lines) and lines[index].strip() == "":
        index += 1
    if index < len(lines) and lines[index].startswith("Copyright (c) 2026"):
        while index < len(lines) and lines[index].strip() != "":
            index += 1
        while index < len(lines) and lines[index].strip() == "":
            index += 1
        if index < len(lines) and lines[index].startswith("The Korg MS-50"):
            while index < len(lines) and lines[index].strip() != "":
                index += 1
            while index < len(lines) and lines[index].strip() == "":
                index += 1
    return "\n".join(lines[index:])


def markdown_to_flow(text: str, st) -> list:
    flow = []
    lines = text.splitlines()
    index = 0
    in_references = False
    while index < len(lines):
        line = lines[index]
        if line.strip() == "":
            index += 1
            continue
        if line.startswith("```"):
            index += 1
            buf = []
            while index < len(lines) and not lines[index].startswith("```"):
                buf.append(lines[index].replace("\t", "    "))
                index += 1
            index += 1
            wrapped = []
            for raw in buf or [""]:
                while len(raw) > CODE_COLS:
                    wrapped.append(raw[:CODE_COLS])
                    raw = raw[CODE_COLS:]
                wrapped.append(raw)
            place_block(flow, Preformatted("\n".join(wrapped), st["code"]))
            continue
        if is_table_line(line):
            rows = []
            while index < len(lines) and is_table_line(lines[index]):
                if not is_separator(lines[index]):
                    rows.append(split_row(lines[index]))
                index += 1
            add_table(flow, rows, st)
            continue
        if line.startswith("#"):
            level = len(line) - len(line.lstrip("#"))
            title = line[level:].strip()
            in_references = title.lower() in {"references", "bibliography"}
            if in_references:
                flow.append(PageBreakIfNotEmpty())
                paragraph = Paragraph("References", st["refhead"])
                paragraph.keepWithNext = True
                flow.append(paragraph)
            else:
                if flow and getattr(flow[-1], "is_chart", False):
                    flow.append(BreakAfterChart())
                key = {1: "h1", 2: "h2"}.get(min(level, 3), "h3")
                flow.append(heading(title, st[key]))
            index += 1
            continue
        if line.lstrip().startswith(("- ", "* ")):
            while index < len(lines) and lines[index].lstrip().startswith(("- ", "* ")):
                item = lines[index].lstrip()[2:].strip()
                if in_references:
                    flow.append(Paragraph(inline(item), st["ref"]))
                else:
                    flow.append(Paragraph("- " + inline(item), st["bullet"]))
                index += 1
            if not in_references:
                flow.append(Spacer(1, 4))
            continue
        buf = [line.strip()]
        index += 1
        while index < len(lines) and lines[index].strip() and not lines[index].startswith(("#", "```", "|", "- ", "* ")):
            buf.append(lines[index].strip())
            index += 1
        text_line = " ".join(buf)
        if in_references and text_line.startswith("Note:"):
            flow.append(Paragraph(inline(text_line[5:].strip()), st["refnote"]))
        elif in_references:
            flow.append(Paragraph(inline(text_line), st["ref"]))
        else:
            paragraph = Paragraph(inline(text_line), st["body"])
            if text_line.endswith(":") and len(text_line) < 80:
                paragraph.keepWithNext = True
            flow.append(paragraph)
    return flow


def draw_page(canvas, _doc):
    canvas.saveState()
    canvas.setFillColor(INK)
    canvas.setFont("Times-Bold", 8)
    canvas.drawString(LEFT, FOOTER_BASE + 3 * FOOTER_STEP, "Copyright (c) 2026 Martial Systems LLC. All rights reserved.")
    canvas.setFont("Times-Roman", 8)
    canvas.drawString(
        LEFT,
        FOOTER_BASE + 2 * FOOTER_STEP,
        "The Korg MS-50, its name, and its circuit designs are the property of Korg Inc.",
    )
    canvas.drawString(
        LEFT,
        FOOTER_BASE + FOOTER_STEP,
        "This independent study is not produced or endorsed by Korg, and it grants no license to those designs.",
    )
    canvas.setFont("Times-Roman", 9)
    canvas.drawString(LEFT, FOOTER_BASE, "MS-50 Modular design pack")
    canvas.drawCentredString(PAGE_W / 2.0, FOOTER_BASE, "2026-09-21")
    canvas.drawRightString(PAGE_W - RIGHT, FOOTER_BASE, str(canvas.getPageNumber()))
    canvas.setStrokeColor(INK)
    canvas.setLineWidth(0.6)
    canvas.line(LEFT, FOOTER_RULE, PAGE_W - RIGHT, FOOTER_RULE)
    canvas.restoreState()


def build():
    st = styles()
    story = []
    story.append(Spacer(1, 8))
    story.append(Paragraph("MS-50 Modular", st["title"]))
    story.append(Paragraph("Design pack for a white-box modular FX instrument", st["cover_sub"]))
    story.append(Paragraph(
        "The Korg MS-50, the MS-50 name, and the circuit designs of that instrument are the property of Korg Inc. "
        "Martial Systems LLC claims copyright only in the original text of this repository and in any code later written here. "
        "The work is an independent study of published schematics and of the literature cited in the reference list. "
        "Korg has not produced, sponsored, or endorsed it. "
        "No license is granted to the MS-50 design, to the Korg drawings, or to the Korg trademarks. "
        "The instrument's name is used only to identify the subject of the study.",
        st["body"],
    ))
    story.append(Paragraph("Document date: 2026-09-21.", st["body"]))
    story.append(heading("Revisions", st["h1"]))
    for line in (
        "2026-09-21: First compiled pack. Methodology, research summary, software schematic, build guide, test plan, and reference list.",
        "2026-09-21: Copyright and the Korg notice placed on every page. The repository is public.",
        "2026-09-21: Rights statement set out in full. Korg Inc. is named as owner of the MS-50, its name, and its circuit designs. Martial Systems LLC claims copyright only in the original text and code of this repository.",
        "2026-09-21: Research page form. Headings use the wording of the notes. Body text is 10 point. The footer is one block: the copyright notice, the Korg notice, the pack title, the date, and the page number. A heading stays with the line under it. A table or a code listing moves when it does not fit. A heading after a chart has a break before it, and starts on the next page when less than two inches remain. The reference list stays in APA and closes the pack.",
    ):
        story.append(Paragraph(line, st["body"]))
    story.append(Paragraph(
        "A generated timestamp is not a revision. Later edits add a line here and a date on the changed section heading in the markdown.",
        st["body"],
    ))

    reference_blocks = []
    for path in CHAPTERS:
        main, references = split_references(strip_banner(path.read_text(encoding="utf-8")))
        story.extend(markdown_to_flow(main, st))
        if references.strip():
            reference_blocks.append(references)
    for references in reference_blocks:
        story.extend(markdown_to_flow(references, st))

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
