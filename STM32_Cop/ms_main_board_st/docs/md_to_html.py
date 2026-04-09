"""circuit_ref.md → 인쇄용 HTML 변환 스크립트"""
import markdown
import pathlib
import sys

SRC = pathlib.Path(__file__).parent / "circuit_ref.md"
DST = pathlib.Path(__file__).parent / "circuit_ref_print.html"

md_text = SRC.read_text(encoding="utf-8")

html_body = markdown.markdown(
    md_text,
    extensions=["tables", "fenced_code", "toc", "nl2br"],
    output_format="html5",
)

HTML_TEMPLATE = """\
<!DOCTYPE html>
<html lang="ko">
<head>
<meta charset="UTF-8">
<title>메가소닉 게이트 드라이브 회로 참조 문서</title>
<style>
  @page {
    size: A4;
    margin: 15mm 12mm 15mm 12mm;
  }
  @media print {
    body { font-size: 9pt; }
    h1 { font-size: 16pt; }
    h2 { font-size: 13pt; page-break-before: always; }
    h2:first-of-type { page-break-before: avoid; }
    h3 { font-size: 11pt; }
    h4 { font-size: 10pt; }
    pre, table { page-break-inside: avoid; }
    .no-print { display: none; }
  }
  body {
    font-family: "Malgun Gothic", "맑은 고딕", "Noto Sans KR", sans-serif;
    line-height: 1.55;
    color: #1a1a1a;
    max-width: 210mm;
    margin: 0 auto;
    padding: 10mm;
  }
  h1 { font-size: 22pt; border-bottom: 3px solid #333; padding-bottom: 6px; margin-top: 0; }
  h2 { font-size: 17pt; border-bottom: 2px solid #666; padding-bottom: 4px; margin-top: 28px; color: #1a3a6a; }
  h3 { font-size: 13pt; margin-top: 20px; color: #2a5a2a; }
  h4 { font-size: 11pt; margin-top: 16px; }
  table {
    border-collapse: collapse;
    width: 100%;
    margin: 10px 0;
    font-size: 9pt;
  }
  th, td {
    border: 1px solid #888;
    padding: 4px 8px;
    text-align: left;
  }
  th { background: #e8e8e8; font-weight: bold; }
  tr:nth-child(even) td { background: #f6f6f6; }
  pre {
    background: #f4f4f4;
    border: 1px solid #ccc;
    border-radius: 4px;
    padding: 8px 10px;
    overflow-x: auto;
    font-size: 8.5pt;
    line-height: 1.4;
    font-family: "Consolas", "D2Coding", monospace;
  }
  code {
    font-family: "Consolas", "D2Coding", monospace;
    font-size: 9pt;
    background: #eee;
    padding: 1px 4px;
    border-radius: 3px;
  }
  pre code { background: none; padding: 0; }
  blockquote {
    border-left: 3px solid #c0392b;
    margin: 10px 0;
    padding: 6px 12px;
    background: #fdf2f2;
    font-size: 9pt;
  }
  hr { border: none; border-top: 1px solid #aaa; margin: 20px 0; }
  a { color: #2a66d9; }
  .footer {
    text-align: center;
    font-size: 8pt;
    color: #999;
    margin-top: 30px;
    border-top: 1px solid #ccc;
    padding-top: 8px;
  }
</style>
</head>
<body>
BODY_PLACEHOLDER
<div class="footer">
  메가소닉 게이트 드라이브 회로 참조 문서 &mdash; 인쇄용
</div>
</body>
</html>
"""

final_html = HTML_TEMPLATE.replace("BODY_PLACEHOLDER", html_body)
DST.write_text(final_html, encoding="utf-8")

print(f"OK → {DST}  ({DST.stat().st_size:,} bytes)")
