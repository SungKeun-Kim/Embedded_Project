#!/usr/bin/env python3
"""2.5초 감지 업데이트 HEX 검증 및 현장 배포 ZIP/DOCX 생성 (외부 모듈 불필요)."""

from hashlib import sha256
from pathlib import Path
import re
import shutil
from xml.etree import ElementTree as ET
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[1]
PRODUCTION = ROOT / "production"
STAGING = PRODUCTION / "FIELD_UPDATE_V1.2.0_20261014_2500ms"
GUIDE = ROOT / "docs" / "ST_프로그래머_2.5초감지_업데이트_20261014.md"
W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
ET.register_namespace("w", W)


def node(parent, name, text=None, **attributes):
    element = ET.SubElement(parent, f"{{{W}}}{name}",
                            {f"{{{W}}}{key}": str(value) for key, value in attributes.items()})
    if text is not None:
        element.text = text
    return element


def paragraph(parent, text, style=None):
    p = node(parent, "p")
    if style:
        node(node(p, "pPr"), "pStyle", val=style)
    run = node(p, "r")
    t = node(run, "t", text.replace("**", "").replace("`", ""))
    t.set("{http://www.w3.org/XML/1998/namespace}space", "preserve")


def make_docx(markdown, destination):
    # Markdown의 제목, 표, 코드, 순서 문장을 Word에서도 읽기 쉽게 유지한다.
    document = ET.Element(f"{{{W}}}document")
    body = node(document, "body")
    lines = markdown.splitlines()
    index = 0
    code = False
    while index < len(lines):
        line = lines[index]
        if line.startswith("```"):
            code = not code
        elif code:
            paragraph(body, line, "Code")
        elif line.startswith("|"):
            table = node(body, "tbl")
            props = node(table, "tblPr")
            node(props, "tblW", w=0, type="auto")
            borders = node(props, "tblBorders")
            for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
                node(borders, edge, val="single", sz=4, color="999999")
            while index < len(lines) and lines[index].startswith("|"):
                cells = [cell.strip() for cell in lines[index].strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?", cell) for cell in cells):
                    row = node(table, "tr")
                    for cell in cells:
                        paragraph(node(row, "tc"), cell)
                index += 1
            continue
        elif line.startswith("# "):
            paragraph(body, line[2:], "Title")
        elif line.startswith("## "):
            paragraph(body, line[3:], "Heading1")
        elif line.strip():
            paragraph(body, line)
        index += 1
    section = node(body, "sectPr")
    node(section, "pgSz", w=11906, h=16838)
    node(section, "pgMar", top=1000, right=1000, bottom=1000, left=1000)

    styles = ET.Element(f"{{{W}}}styles")
    defaults = node(styles, "docDefaults")
    run_defaults = node(node(defaults, "rPrDefault"), "rPr")
    node(run_defaults, "rFonts", ascii="Malgun Gothic", hAnsi="Malgun Gothic", eastAsia="맑은 고딕")
    node(run_defaults, "sz", val=20)
    node(run_defaults, "lang", val="ko-KR", eastAsia="ko-KR")
    for name, size in (("Title", 32), ("Heading1", 26), ("Code", 18)):
        style = node(styles, "style", type="paragraph", styleId=name)
        node(style, "name", val=name)
        pprops = node(style, "pPr")
        node(pprops, "spacing", before=180, after=100)
        if name != "Code":
            node(pprops, "keepNext")
        rprops = node(style, "rPr")
        node(rprops, "sz", val=size)
        if name != "Code":
            node(rprops, "b")
        else:
            node(rprops, "rFonts", ascii="Consolas", hAnsi="Consolas")

    content_types = '<?xml version="1.0" encoding="UTF-8"?>' + (
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
        '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
        '<Default Extension="xml" ContentType="application/xml"/>'
        '<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>'
        '<Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>'
        '</Types>')
    relationships = '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
    with ZipFile(destination, "w", ZIP_DEFLATED) as archive:
        archive.writestr("[Content_Types].xml", content_types)
        archive.writestr("_rels/.rels", relationships +
                         '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>')
        archive.writestr("word/_rels/document.xml.rels", relationships +
                         '<Relationship Id="rIdStyles" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>')
        archive.writestr("word/document.xml", ET.tostring(document, encoding="utf-8", xml_declaration=True))
        archive.writestr("word/styles.xml", ET.tostring(styles, encoding="utf-8", xml_declaration=True))


def validate_hex(path):
    base = 0
    memory = {}
    eof = False
    for line in path.read_text(encoding="ascii").splitlines():
        assert line.startswith(":"), "HEX record 형식 오류"
        data = bytes.fromhex(line[1:])
        assert len(data) == data[0] + 5 and sum(data) % 256 == 0, "HEX checksum 오류"
        address = int.from_bytes(data[1:3], "big")
        kind = data[3]
        payload = data[4:-1]
        assert not eof, "EOF 뒤 record 존재"
        if kind == 0:
            for offset, value in enumerate(payload):
                absolute = base + address + offset
                assert 0x08000000 <= absolute < 0x0800F800, "UPDATE HEX에 설정 또는 잘못된 주소 포함"
                assert absolute not in memory, "중복 HEX 주소"
                memory[absolute] = value
        elif kind == 4:
            base = int.from_bytes(payload, "big") << 16
        elif kind == 2:
            base = int.from_bytes(payload, "big") << 4
        elif kind == 1:
            eof = True
        else:
            assert kind in (3, 5), "미지원 HEX record"
    assert eof and memory, "HEX EOF 또는 데이터 누락"
    binary = (ROOT / "build-release" / "us_main_board.bin").read_bytes()
    # objcopy -O binary는 ELF section 사이의 빈 공간을 0으로 채운다.
    image = bytes(memory.get(0x08000000 + offset, 0) for offset in range(len(binary)))
    assert image == binary and max(memory) < 0x08000000 + len(binary), "Release BIN/HEX 불일치"
    print(f"[PASS] UPDATE HEX: {len(memory)} bytes, 설정 Page 미포함, Release BIN 일치")


def main():
    firmware = PRODUCTION / "US_MAIN_BOARD_UPDATE_V1.2.0.hex"
    validate_hex(firmware)
    STAGING.mkdir(exist_ok=True)
    docx = GUIDE.with_suffix(".docx")
    make_docx(GUIDE.read_text(encoding="utf-8"), docx)
    files = [firmware, PRODUCTION / "program_update.bat", GUIDE, docx,
             PRODUCTION / "변경_검증_기록_20261014.md"]
    for source in files:
        shutil.copy2(source, STAGING / source.name)
    sums = "".join(f"{sha256(source.read_bytes()).hexdigest()}  {source.name}\n" for source in files)
    (STAGING / "SHA256SUMS.txt").write_text(sums, encoding="utf-8")
    files = [STAGING / source.name for source in files] + [STAGING / "SHA256SUMS.txt"]
    output = PRODUCTION / "US_MAIN_BOARD_FIELD_UPDATE_V1.2.0.zip"
    with ZipFile(output, "w", ZIP_DEFLATED) as archive:
        for source in files:
            archive.write(source, arcname=source.name)
    with ZipFile(output) as archive:
        assert archive.testzip() is None
        assert not any("FACTORY" in name.upper() for name in archive.namelist())
        assert archive.read(firmware.name) == firmware.read_bytes()
    # 기존 배포 폴더의 HEX/BAT도 같은 최신 파일로 갱신한다.
    legacy = PRODUCTION / "FIELD_UPDATE_V1.2.0"
    if legacy.is_dir():
        for source in files:
            shutil.copy2(source, legacy / source.name)
    print(f"[PASS] 배포 ZIP 무결성 및 최신 HEX 일치: {output}")
    print(f"[PASS] Word 안내서: {docx}")
    print(f"HEX SHA-256: {sha256(firmware.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
