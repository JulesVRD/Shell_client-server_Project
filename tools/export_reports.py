#!/usr/bin/env python3
"""Exporter les sources Markdown des rapports avec une mise en page A4."""
import html
import json
from pathlib import Path
import re
import textwrap

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    HRFlowable, PageBreak, Paragraph, Preformatted, SimpleDocTemplate, Spacer,
    Table, TableStyle,
)

ROOT = Path(__file__).resolve().parents[1]
TOKENS = json.loads((ROOT / 'tools/report-style.json').read_text())
PALETTE = {key: colors.HexColor(value) for key, value in TOKENS['colors'].items()}
TYPE = TOKENS['type']
MARGIN = TOKENS['spacing']['margin_mm'] * mm


def register_fonts():
    choices = [
        (Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'),
         Path('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf')),
        (Path('/System/Library/Fonts/Supplemental/Arial.ttf'),
         Path('/System/Library/Fonts/Supplemental/Arial Bold.ttf')),
    ]
    for regular, bold in choices:
        if regular.exists() and bold.exists():
            pdfmetrics.registerFont(TTFont('ReportBody', str(regular)))
            pdfmetrics.registerFont(TTFont('ReportBold', str(bold)))
            pdfmetrics.registerFontFamily('ReportBody', normal='ReportBody', bold='ReportBold')
            return
    raise SystemExit('Installer localement DejaVu Sans ou adapter les chemins de polices dans le script.')


def inline(text):
    text = html.escape(text)
    text = re.sub(r'\[([^\]]+)\]\([^)]+\)', r'\1', text)
    text = re.sub(r'\*\*(.*?)\*\*', r'<b>\1</b>', text)
    return re.sub(r'`([^`]+)`', r'<font name="Courier">\1</font>', text)


def create_styles():
    body = ParagraphStyle('body', fontName='ReportBody', fontSize=TYPE['body'],
                          leading=TYPE['leading'], textColor=PALETTE['ink'],
                          spaceAfter=TOKENS['spacing']['paragraph_after'], alignment=TA_LEFT)
    return {
        'body': body,
        'title': ParagraphStyle('title', parent=body, fontName='ReportBold',
                                fontSize=TYPE['title'], leading=33, spaceAfter=15),
        'heading': ParagraphStyle('heading', parent=body, fontName='ReportBold',
                                  fontSize=TYPE['heading'], leading=19,
                                  spaceBefore=12, spaceAfter=9, keepWithNext=True),
        'cell': ParagraphStyle('cell', parent=body, fontSize=9.0, leading=12.4, spaceAfter=0),
        'bullet': ParagraphStyle('bullet', parent=body, leftIndent=10, firstLineIndent=-9,
                                 spaceAfter=5),
        'code': ParagraphStyle('code', fontName='Courier', fontSize=TYPE['code'],
                               leading=11.5, textColor=PALETTE['ink'], leftIndent=8,
                               spaceBefore=6, spaceAfter=10),
    }


def parse_document(source, styles):
    lines = source.splitlines()
    items, index = [], 0
    while index < len(lines):
        line = lines[index]
        if not line.strip():
            index += 1
            continue
        if line == '<!-- pagebreak -->':
            items.append(PageBreak())
        elif line.startswith('# '):
            items.extend([Paragraph(inline(line[2:]), styles['title']),
                          HRFlowable(width='100%', thickness=2, color=PALETTE['accent']),
                          Spacer(1, 10)])
        elif line.startswith('## '):
            items.append(Paragraph(inline(line[3:]), styles['heading']))
        elif line.startswith('```'):
            code = []
            index += 1
            while index < len(lines) and not lines[index].startswith('```'):
                code.extend(textwrap.wrap(lines[index], width=87, replace_whitespace=False,
                                          drop_whitespace=False) or [''])
                index += 1
            items.append(Preformatted('\n'.join(code), styles['code']))
        elif line.startswith('|'):
            rows = []
            while index < len(lines) and lines[index].startswith('|'):
                cells = [cell.strip() for cell in lines[index].strip('|').split('|')]
                if not all(re.fullmatch(r'[-: ]+', cell) for cell in cells):
                    rows.append([Paragraph(inline(cell), styles['cell']) for cell in cells])
                index += 1
            width = A4[0] - 2 * MARGIN
            weights = [0.42, 0.58] if len(rows[0]) == 2 else [0.16, 0.32, 0.52]
            table = Table(rows, colWidths=[width * weight for weight in weights], repeatRows=1,
                          hAlign='LEFT')
            table.setStyle(TableStyle([
                ('BACKGROUND', (0, 0), (-1, 0), PALETTE['surface']),
                ('LINEBELOW', (0, 0), (-1, 0), 0.8, PALETTE['accent']),
                ('LINEBELOW', (0, 1), (-1, -1), 0.35, PALETTE['line']),
                ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                ('LEFTPADDING', (0, 0), (-1, -1), 7),
                ('RIGHTPADDING', (0, 0), (-1, -1), 7),
                ('TOPPADDING', (0, 0), (-1, -1), 7),
                ('BOTTOMPADDING', (0, 0), (-1, -1), 7),
            ]))
            items.extend([table, Spacer(1, 10)])
            continue
        elif line.startswith('- '):
            items.append(Paragraph('• ' + inline(line[2:]), styles['bullet']))
        else:
            paragraph = [line]
            while index + 1 < len(lines) and lines[index + 1].strip() and not lines[index + 1].startswith(('#', '-', '|', '```', '<!--')):
                index += 1
                paragraph.append(lines[index])
            items.append(Paragraph(inline(' '.join(paragraph)), styles['body']))
        index += 1
    return items


def footer(title):
    def draw(canvas, document):
        canvas.saveState()
        canvas.setStrokeColor(PALETTE['line'])
        canvas.line(MARGIN, 15 * mm, A4[0] - MARGIN, 15 * mm)
        canvas.setFont('ReportBody', 8)
        canvas.setFillColor(PALETTE['muted'])
        canvas.drawString(MARGIN, 10.5 * mm, f'rvsh / LO14 2026-2027 / {title}')
        canvas.drawRightString(A4[0] - MARGIN, 10.5 * mm, str(document.page))
        canvas.restoreState()
    return draw


def main():
    register_fonts()
    styles = create_styles()
    output = ROOT / 'livrables'
    output.mkdir(exist_ok=True)
    for name in ('rapport-intermediaire', 'rapport-final', 'guide-soutenance'):
        source = (ROOT / 'docs/rapports' / f'{name}.md').read_text()
        title = source.splitlines()[0][2:]
        path = output / f'{name}.pdf'
        document = SimpleDocTemplate(str(path), pagesize=A4, rightMargin=MARGIN,
                                     leftMargin=MARGIN, topMargin=17 * mm,
                                     bottomMargin=21 * mm, title=title,
                                     author='Jules et Selyan', pageCompression=1)
        document.build(parse_document(source, styles), onFirstPage=footer(title),
                       onLaterPages=footer(title))
        print(f'PDF créé : {path.name}')


if __name__ == '__main__':
    main()
