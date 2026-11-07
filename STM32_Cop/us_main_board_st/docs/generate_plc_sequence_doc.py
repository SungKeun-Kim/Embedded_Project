"""미쓰비시 PLC 순서 설명서를 표준 라이브러리만으로 DOCX로 변환한다."""

from pathlib import Path
from xml.etree import ElementTree as ET
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import re


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "미쓰비시_PLC_RS485_출력_읽기쓰기_순서.md"
OUTPUT = SOURCE.with_suffix(".docx")
W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
ET.register_namespace("w", W)


def element(name, attrs=None):
    return ET.Element(f"{{{W}}}{name}", {
        f"{{{W}}}{key}": str(value) for key, value in (attrs or {}).items()
    })


def child(parent, name, attrs=None):
    node = element(name, attrs)
    parent.append(node)
    return node


def clean(text):
    text = re.sub(r"\[([^]]+)\]\(([^)]+)\)", r"\1 (\2)", text)
    return text.replace("**", "").replace("`", "")


def paragraph(parent, text, *, heading=0, small=False, bold=False):
    p = child(parent, "p")
    pp = child(p, "pPr")
    child(pp, "spacing", {"after": 90, "line": 280, "lineRule": "auto"})
    if heading:
        child(pp, "keepNext")
        child(pp, "outlineLvl", {"val": heading - 1})
    child(pp, "keepLines")
    run = child(p, "r")
    rp = child(run, "rPr")
    child(rp, "rFonts", {"ascii": "Malgun Gothic", "hAnsi": "Malgun Gothic",
                         "eastAsia": "맑은 고딕"})
    size = {1: 36, 2: 28, 3: 23}.get(heading, 18 if small else 20)
    child(rp, "sz", {"val": size})
    if heading or bold:
        child(rp, "b")
    if heading:
        child(rp, "color", {"val": "1F4E79"})
    t = child(run, "t")
    t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")
    t.text = clean(text)


def table(parent, lines):
    rows = [[v.strip() for v in line.strip().strip("|").split("|")]
            for line in lines]
    rows.pop(1)  # Markdown 표 구분선
    tbl = child(parent, "tbl")
    props = child(tbl, "tblPr")
    child(props, "tblW", {"w": 0, "type": "auto"})
    borders = child(props, "tblBorders")
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        child(borders, edge, {"val": "single", "sz": 4, "color": "B7C9DC"})
    width = 9900 // len(rows[0])
    grid = child(tbl, "tblGrid")
    for _ in rows[0]:
        child(grid, "gridCol", {"w": width})
    for index, values in enumerate(rows):
        row = child(tbl, "tr")
        rowprops = child(row, "trPr")
        child(rowprops, "cantSplit")
        if index == 0:
            child(rowprops, "tblHeader")
        for value in values:
            cell = child(row, "tc")
            cp = child(cell, "tcPr")
            child(cp, "tcW", {"w": width, "type": "dxa"})
            if index == 0:
                child(cp, "shd", {"fill": "D9EAF7"})
            paragraph(cell, value, small=True, bold=index == 0)
    paragraph(parent, "")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", nargs="?", type=Path, default=SOURCE,
                        help="변환할 Markdown 설명서. 생략하면 기존 순서 설명서")
    args = parser.parse_args()
    source = args.source.resolve()
    output = source.with_suffix(".docx")
    document = element("document")
    body = child(document, "body")
    lines = source.read_text(encoding="utf-8").splitlines()
    i = 0
    while i < len(lines):
        line = lines[i]
        if line.startswith("|"):
            block = []
            while i < len(lines) and lines[i].startswith("|"):
                block.append(lines[i])
                i += 1
            table(body, block)
            continue
        heading = re.match(r"^(#{1,3})\s+(.+)$", line)
        if heading:
            paragraph(body, heading[2], heading=len(heading[1]))
        elif line.strip():
            paragraph(body, "• " + line[2:] if line.startswith("- ") else line)
        i += 1

    section = child(body, "sectPr")
    child(section, "pgSz", {"w": 11906, "h": 16838})
    child(section, "pgMar", {"top": 1020, "bottom": 1020, "left": 1000,
                            "right": 1000, "header": 400, "footer": 400})
    xml = ET.tostring(document, encoding="utf-8", xml_declaration=True)
    content_types = '''<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>'''
    relationships = '''<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>'''
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", relationships)
        archive.writestr("word/document.xml", xml)

    # 패키지 무결성과 본문/표의 누락 여부를 생성 직후 확인한다.
    with ZipFile(output) as archive:
        assert archive.testzip() is None
        tree = ET.fromstring(archive.read("word/document.xml"))
        texts = list(tree.iter(f"{{{W}}}t"))
        joined = "\n".join(node.text or "" for node in texts)
        for term in ("0xFF00", "485 COMM", "0x0010", "통신 Timeout", "ADPRW"):
            assert term in joined, term
        assert len(list(tree.iter(f"{{{W}}}tbl"))) == sum(
            1 for line in lines if line.startswith("| ---")
        )
    print(f"생성 및 XML/표 검증 완료: {output}")


if __name__ == "__main__":
    main()
