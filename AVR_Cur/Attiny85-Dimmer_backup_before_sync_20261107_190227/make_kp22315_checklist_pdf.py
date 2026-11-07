from pathlib import Path

import fitz


ROOT = Path(__file__).resolve().parent
HTML_PATH = ROOT / "KP22315_간헐스위칭_점검체크리스트.html"
PDF_PATH = ROOT / "KP22315_간헐스위칭_점검체크리스트.pdf"
BASE_PDF_PATH = ROOT / "KP22315_간헐스위칭_점검체크리스트.base.pdf"
FONT_DIR = Path(r"C:\Windows\Fonts")


def make_pdf() -> None:
    html = HTML_PATH.read_text(encoding="utf-8")
    font_css = """
    @font-face {
      font-family: MalgunLocal;
      src: url(malgun.ttf);
    }
    @font-face {
      font-family: MalgunLocal;
      src: url(malgunbd.ttf);
      font-weight: bold;
    }
    body {
      font-family: MalgunLocal, sans-serif !important;
    }
    """

    story = fitz.Story(
        html=html,
        user_css=font_css,
        archive=fitz.Archive(str(FONT_DIR)),
    )

    a4 = fitz.paper_rect("a4")
    body_rect = fitz.Rect(34, 32, a4.width - 34, a4.height - 34)
    if BASE_PDF_PATH.exists():
        BASE_PDF_PATH.unlink()
    writer = fitz.DocumentWriter(str(BASE_PDF_PATH))

    more = True
    page_number = 0
    while more:
        page_number += 1
        device = writer.begin_page(a4)
        more, _ = story.place(body_rect)
        story.draw(device)
        writer.end_page()

    writer.close()

    doc = fitz.open(BASE_PDF_PATH)
    for index in range(doc.page_count):
        page = doc.load_page(index)
        page.insert_text(
            (a4.width - 58, a4.height - 15),
            f"{index + 1} / {doc.page_count}",
            fontsize=7,
            color=(0.36, 0.40, 0.44),
        )
        del page
    if PDF_PATH.exists():
        PDF_PATH.unlink()
    doc.save(PDF_PATH, garbage=4, deflate=True)
    doc.close()


if __name__ == "__main__":
    make_pdf()
