#!/usr/bin/env python3
"""Render the editable CAN signal matrix RTF/Markdown source as a PDF."""

from __future__ import annotations

import html
import re
import subprocess
import tempfile
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_LEFT
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    KeepTogether,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "docs/firmware/can-signal-matrix.rtf"
OUTPUT = ROOT / "docs/firmware/can-signal-matrix.pdf"
PAGE_W, PAGE_H = landscape(A4)


def plain_source() -> str:
    with tempfile.TemporaryDirectory() as temp_dir:
        target = Path(temp_dir) / "matrix.txt"
        subprocess.run(
            ["textutil", "-convert", "txt", "-output", str(target), str(SOURCE)],
            check=True,
        )
        return target.read_text(encoding="utf-8")


def inline_markup(value: str) -> str:
    value = html.escape(value.strip())
    value = re.sub(r"`([^`]+)`", r'<font name="Courier">\1</font>', value)
    value = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", value)
    value = re.sub(r"\[([^]]+)\]\([^)]+\)", r"\1", value)
    return value


def column_widths(rows: list[list[str]], available: float) -> list[float]:
    count = len(rows[0])
    maxima = [max(len(row[i]) for row in rows) for i in range(count)]
    weights = [max(8.0, min(34.0, value ** 0.62)) for value in maxima]
    if count >= 5:
        weights[0] *= 0.7
    total = sum(weights)
    return [available * weight / total for weight in weights]


def render_story(text: str):
    styles = getSampleStyleSheet()
    title = ParagraphStyle(
        "MatrixTitle", parent=styles["Title"], fontName="Helvetica-Bold",
        fontSize=18, leading=21, textColor=colors.HexColor("#17324D"),
        spaceAfter=5 * mm,
    )
    h1 = ParagraphStyle(
        "MatrixH1", parent=styles["Heading1"], fontSize=13, leading=15,
        textColor=colors.HexColor("#17324D"), spaceBefore=3 * mm, spaceAfter=2 * mm,
    )
    h2 = ParagraphStyle(
        "MatrixH2", parent=styles["Heading2"], fontSize=10.5, leading=12.5,
        textColor=colors.HexColor("#2B5D7D"), spaceBefore=2.5 * mm, spaceAfter=1.5 * mm,
    )
    body = ParagraphStyle(
        "MatrixBody", parent=styles["BodyText"], fontSize=7.0, leading=8.3,
        textColor=colors.HexColor("#263238"), spaceAfter=1.5 * mm,
    )
    meta = ParagraphStyle(
        "MatrixMeta", parent=body, fontSize=7.8, leading=9.5,
        textColor=colors.HexColor("#52616B"), leftIndent=4 * mm,
    )
    bullet = ParagraphStyle(
        "MatrixBullet", parent=body, leftIndent=5 * mm, firstLineIndent=-3 * mm,
        bulletIndent=1 * mm,
    )
    cell = ParagraphStyle("MatrixCell", parent=body, fontSize=5.9, leading=6.9, spaceAfter=0)
    cell_header = ParagraphStyle(
        "MatrixCellHeader", parent=cell, fontName="Helvetica-Bold",
        textColor=colors.white, alignment=TA_LEFT,
    )

    lines = text.splitlines()
    story = []
    paragraph: list[str] = []

    def flush_paragraph() -> None:
        if paragraph:
            story.append(Paragraph(inline_markup(" ".join(paragraph)), body))
            paragraph.clear()

    i = 0
    while i < len(lines):
        raw = lines[i].rstrip()
        line = raw.strip()
        if not line:
            flush_paragraph()
            i += 1
            continue
        if line.startswith("| "):
            flush_paragraph()
            table_lines = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                table_lines.append(lines[i].strip())
                i += 1
            parsed = [[part.strip() for part in row.strip("|").split("|")] for row in table_lines]
            if len(parsed) > 1 and all(set(part) <= {"-", ":"} for part in parsed[1]):
                parsed.pop(1)
            table_rows = []
            for row_index, row in enumerate(parsed):
                style = cell_header if row_index == 0 else cell
                table_rows.append([Paragraph(inline_markup(value), style) for value in row])
            available = PAGE_W - 24 * mm
            table = Table(
                table_rows,
                colWidths=column_widths(parsed, available),
                repeatRows=1,
                hAlign="LEFT",
            )
            table.setStyle(TableStyle([
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#2B5D7D")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#B6C2C9")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F2F6F8")]),
                ("LEFTPADDING", (0, 0), (-1, -1), 3),
                ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
            ]))
            story.extend([table, Spacer(1, 2 * mm)])
            continue
        if line.startswith("# "):
            flush_paragraph()
            story.append(Paragraph(inline_markup(line[2:]), title))
        elif line.startswith("## "):
            flush_paragraph()
            story.append(Paragraph(inline_markup(line[3:]), h1))
        elif line.startswith("### "):
            flush_paragraph()
            story.append(Paragraph(inline_markup(line[4:]), h2))
        elif line.startswith(">"):
            flush_paragraph()
            content = line.lstrip("> ")
            if content:
                story.append(Paragraph(inline_markup(content), meta))
        elif line.startswith("- "):
            flush_paragraph()
            bullet_text = line[2:]
            while i + 1 < len(lines) and lines[i + 1].startswith("  "):
                i += 1
                bullet_text += " " + lines[i].strip()
            story.append(Paragraph(inline_markup(bullet_text), bullet, bulletText="•"))
        else:
            paragraph.append(line)
        i += 1
    flush_paragraph()
    return story


def decorate(canvas, doc):
    canvas.saveState()
    canvas.setFillColor(colors.HexColor("#17324D"))
    canvas.setFont("Helvetica-Bold", 7.2)
    canvas.drawString(12 * mm, PAGE_H - 8 * mm, "MICROLINO OPEN TELEMETRY")
    canvas.setFillColor(colors.HexColor("#6B7780"))
    canvas.setFont("Helvetica", 7)
    canvas.drawRightString(PAGE_W - 12 * mm, 8 * mm, f"Working evidence matrix · Page {doc.page}")
    canvas.setStrokeColor(colors.HexColor("#D7E0E5"))
    canvas.line(12 * mm, 11 * mm, PAGE_W - 12 * mm, 11 * mm)
    canvas.restoreState()


def main() -> None:
    doc = BaseDocTemplate(
        str(OUTPUT), pagesize=landscape(A4),
        leftMargin=12 * mm, rightMargin=12 * mm,
        topMargin=12 * mm, bottomMargin=14 * mm,
        title="MOT CAN Signal Matrix", author="Microlino Open Telemetry",
    )
    frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id="matrix")
    doc.addPageTemplates([PageTemplate(id="matrix", frames=[frame], onPage=decorate)])
    doc.build(render_story(plain_source()))


if __name__ == "__main__":
    main()
