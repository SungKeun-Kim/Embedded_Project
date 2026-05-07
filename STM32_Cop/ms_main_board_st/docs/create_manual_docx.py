"""
CSF JET Multi Megasonic LCD 패널 운전 매뉴얼 DOCX 생성기
실행 전: pip install python-docx Pillow
실행: python create_manual_docx.py
"""
import os, shutil, glob
from docx import Document
from docx.shared import Inches, Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.enum.section import WD_ORIENT

# --- 경로 설정 ---
DOCS = os.path.dirname(os.path.abspath(__file__))
BRAIN = r"C:\Users\USER\.gemini\antigravity\brain\66c19f02-b1d4-4525-ace0-faa023bac433"
OUT = os.path.join(DOCS, "MANUAL_CSF_JET_LCD_Panel_v1.docx")

# 이미지 파일 복사
IMG = {}
for name in ["panel_layout", "mode_flowchart", "lcd_screens", "rs485_wiring"]:
    matches = glob.glob(os.path.join(BRAIN, f"{name}_*.png"))
    if matches:
        dst = os.path.join(DOCS, f"{name}.png")
        shutil.copy2(matches[0], dst)
        IMG[name] = dst
        print(f"  복사: {name}.png")
# LCD_Display.png 원본도 사용
lcd_orig = os.path.join(DOCS, "LCD_Display.png")
if os.path.exists(lcd_orig):
    IMG["lcd_display"] = lcd_orig

doc = Document()

# --- 스타일 설정 ---
style = doc.styles['Normal']
style.font.name = '맑은 고딕'
style.font.size = Pt(10)
style.paragraph_format.space_after = Pt(4)

for level in range(1, 4):
    hs = doc.styles[f'Heading {level}']
    hs.font.name = '맑은 고딕'
    hs.font.color.rgb = RGBColor(0x1A, 0x3C, 0x6E)

def add_table(doc, headers, rows, col_widths=None):
    t = doc.add_table(rows=1+len(rows), cols=len(headers))
    t.style = 'Light Grid Accent 1'
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, h in enumerate(headers):
        c = t.rows[0].cells[i]
        c.text = h
        for p in c.paragraphs:
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            for r in p.runs:
                r.bold = True
                r.font.size = Pt(9)
    for ri, row in enumerate(rows):
        for ci, val in enumerate(row):
            cell = t.rows[ri+1].cells[ci]
            cell.text = str(val)
            for p in cell.paragraphs:
                for r in p.runs:
                    r.font.size = Pt(9)
    doc.add_paragraph()
    return t

def add_img(doc, key, width=5.5, caption=None):
    if key in IMG:
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run()
        r.add_picture(IMG[key], width=Inches(width))
        if caption:
            cp = doc.add_paragraph(caption)
            cp.alignment = WD_ALIGN_PARAGRAPH.CENTER
            cp.runs[0].italic = True
            cp.runs[0].font.size = Pt(9)
            cp.runs[0].font.color.rgb = RGBColor(0x66, 0x66, 0x66)

def add_lcd_box(doc, line1, line2, label=None):
    if label:
        doc.add_paragraph(label, style='List Bullet')
    t = doc.add_table(rows=2, cols=1)
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for ri, txt in enumerate([line1, line2]):
        cell = t.rows[ri].cells[0]
        cell.text = txt
        for p in cell.paragraphs:
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER
            for r in p.runs:
                r.font.name = 'Consolas'
                r.font.size = Pt(11)
    # 테두리/음영은 기본 표 스타일로 대체
    doc.add_paragraph()

# ========== 표지 ==========
doc.add_paragraph()
doc.add_paragraph()
title = doc.add_paragraph()
title.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = title.add_run("CSF JET Multi Megasonic Generator")
r.bold = True; r.font.size = Pt(24); r.font.color.rgb = RGBColor(0x1A, 0x3C, 0x6E)

sub = doc.add_paragraph()
sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
r = sub.add_run("LCD 패널 운전 매뉴얼")
r.bold = True; r.font.size = Pt(18); r.font.color.rgb = RGBColor(0x33, 0x33, 0x33)

doc.add_paragraph()
add_img(doc, "lcd_display", 4.5)
doc.add_paragraph()

info = doc.add_paragraph()
info.alignment = WD_ALIGN_PARAGRAPH.CENTER
info.add_run("문서 버전: v1.0\n적용 보드: STM32G474 기반 메가소닉 메인보드\n작성일: 2026-05-07").font.size = Pt(11)

doc.add_page_break()

# ========== 1. 제품 사양 ==========
doc.add_heading('1. 제품 사양', level=1)
add_table(doc,
    ['항목', '사양'],
    [
        ['작동 주파수', '10CH (CH0~CH9), 500 kHz ~ 2.2 MHz'],
        ['최대 출력', '10 W'],
        ['출력 설정 범위', '0.10 W ~ 10.00 W (0.01 W 단위)'],
        ['표시 장치', 'LCD1602 (2행 × 16문자)'],
        ['상태 LED', '6개 (NORMAL / H·L SET / 8 POWER / REMOTE / EXT / TX RX)'],
        ['버튼', '5개 (START/STOP, MODE, UP, DOWN, SET)'],
        ['통신', 'RS-485 Half-Duplex, Modbus RTU'],
        ['기본 통신 속도', '9600 bps'],
        ['운전 모드', 'NORMAL / REMOTE / EXT'],
    ])

# ========== 2. 전면 패널 구성 ==========
doc.add_heading('2. 전면 조작 패널 구성', level=1)
add_img(doc, "panel_layout", 5.5, "[그림 2-1] 전면 조작 패널 배치도")

doc.add_heading('2.1 LED 표시등 (6개)', level=2)
add_table(doc,
    ['위치', '명칭', '의미'],
    [
        ['좌측 D1', 'NORMAL', 'NORMAL 모드 활성 시 점등'],
        ['좌측 D2', 'H/L SET', 'HIGH/LOW 알람 설정 모드 시 점등'],
        ['좌측 D3', '8 POWER', '8단계 출력 설정 모드 시 점등'],
        ['우측 D4', 'REMOTE', 'REMOTE 모드 활성 시 점등 (운전 중 점멸)'],
        ['우측 D5', 'EXT', 'EXT(통신) 모드 활성 시 점등'],
        ['우측 D6', 'TX RX', 'RS-485 통신 송수신 시 점멸'],
    ])

doc.add_heading('2.2 버튼 (5개)', level=2)
add_table(doc,
    ['버튼', '실크 인쇄', '기본 기능', 'EXT 부가기능'],
    [
        ['TS1', 'START/STOP', '발진 시작/정지', 'ADDR 설정 진입'],
        ['TS2', 'MODE', '모드/화면 전환', 'BPS 설정 전환'],
        ['TS3', 'UP', '값 증가', 'ADDR/BPS 값 증가'],
        ['TS4', 'DOWN', '값 감소', 'ADDR/BPS 값 감소'],
        ['TS5', 'SET', '선택/확정/저장', '설정 확정 저장'],
    ])

doc.add_heading('2.3 LCD 기본 화면 레이아웃', level=2)
add_table(doc,
    ['위치', '표시 내용', '예시'],
    [
        ['1행 좌측', '출력 전력 (설정/실측)', 'OUT 0.50W'],
        ['1행 우측', '주파수 (채널+kHz)', 'F1000k'],
        ['2행 좌측', '운전 모드', 'NOR / REM / EXT'],
        ['2행 중앙', 'RUN/STOP 상태', 'RUN / STOP'],
        ['2행 우측', '통신 주소 / 상태', 'ADR01 / RX'],
    ])

# ========== 3. 전원 투입 전 점검 ==========
doc.add_heading('3. 전원 투입 전 점검', level=1)
checks = [
    'AC/DC 전원 규격이 장비 사양과 일치하는가?',
    '보호접지(PE)가 정확히 연결되었는가?',
    '트랜스듀서/출력 라인이 올바르게 결선되었는가?',
    'SENSOR 입력이 정상(SHORT) 상태인가?',
    'REMOTE 단자 사용 여부가 운전 계획과 일치하는가?',
    'RS-485 A/B 극성이 마스터 장치와 일치하는가?',
    '슬레이브 주소 충돌 없는가?',
]
for i, c in enumerate(checks, 1):
    doc.add_paragraph(f'{i}. {c}')

warn = doc.add_paragraph()
r = warn.add_run('⚠ 경고: SENSOR 단자가 OPEN 상태이면 전원 투입 즉시 Err7이 발생한다.')
r.bold = True; r.font.color.rgb = RGBColor(0xCC, 0x00, 0x00)

# ========== 4. 전원 ON 및 모드 진입 ==========
doc.add_heading('4. 전원 ON 및 모드 진입', level=1)
doc.add_paragraph('전원 ON → LCD 초기화면 표시 → NORMAL 모드 자동 진입')

doc.add_heading('4.1 부팅 시 키 조합 (모드 선택)', level=2)
add_img(doc, "mode_flowchart", 5.0, "[그림 4-1] 모드 전환 흐름도")

add_table(doc,
    ['키 조합', '진입 모드', '설명'],
    [
        ['전원 ON + SET + DOWN 유지', 'NORMAL', '강제 NORMAL 복귀'],
        ['전원 ON + SET + MODE 유지', 'REMOTE', '외부 접점 제어'],
        ['전원 ON + UP + MODE 유지', 'EXT', '통신 제어'],
    ])

# ========== 5. 모드별 운전 방법 ==========
doc.add_heading('5. 모드별 운전 방법', level=1)
doc.add_paragraph('아래 그림은 각 모드별 LCD 화면 예시이다.')
add_img(doc, "lcd_screens", 5.5, "[그림 5-1] 모드별 LCD 화면 예시")

doc.add_heading('5.1 NORMAL 모드 (D1 점등)', level=2)
doc.add_paragraph('로컬 패널에서 직접 운전하는 기본 모드.')
add_table(doc,
    ['키', '동작'],
    [
        ['START/STOP', '발진 시작 ↔ 정지 토글'],
        ['UP / DOWN', '출력 설정값 ±0.01 W 조정'],
        ['SET', '현재 설정값 Flash 저장'],
        ['MODE', 'H/L SET → 8 POWER → NORMAL 순환'],
    ])
doc.add_paragraph('권장 운전 절차:')
for s in ['UP/DOWN으로 출력값을 목표값으로 조정',
           'START/STOP으로 운전 시작',
           'LCD에서 실측 출력 안정화 확인',
           'SET으로 설정 저장']:
    doc.add_paragraph(s, style='List Number')

doc.add_heading('5.2 H/L SET 모드 (D2 점등)', level=2)
doc.add_paragraph('HIGH/LOW 알람 임계값을 설정하는 모드.')
add_table(doc,
    ['키', '동작'],
    [
        ['SET', 'HIGH ↔ LOW 항목 전환, 저장'],
        ['UP / DOWN', '알람 임계값 조정'],
        ['MODE', '8 POWER 모드로 이동'],
        ['START/STOP', '동작 없음 (실수 방지)'],
    ])

doc.add_heading('5.3 8 POWER 모드 (D3 점등)', level=2)
doc.add_paragraph('Step별 출력 레시피를 사전 저장하여 운전하는 모드.')
add_table(doc,
    ['키', '동작'],
    [
        ['SET', 'Step 선택/확정'],
        ['UP / DOWN', 'Step별 출력값 조정'],
        ['MODE', 'NORMAL 모드로 복귀'],
        ['START/STOP', '동작 없음 (실수 방지)'],
    ])

doc.add_heading('5.4 REMOTE 모드 (D4 점등)', level=2)
doc.add_paragraph('외부 접점 신호로 ON/OFF를 제어하는 모드.')
doc.add_paragraph('특징: REMOTE 입력 신호가 운전 제어 우선권. 로컬 키 입력 제한.')
doc.add_paragraph('진입: 전원 ON + SET + MODE 동시 유지')
doc.add_paragraph('복귀: 전원 ON + SET + DOWN 동시 유지')

doc.add_heading('5.5 EXT 모드 (D5 점등)', level=2)
doc.add_paragraph('RS-485 Modbus RTU 통신 중심 운전 및 주소/BPS 설정 모드.')
add_table(doc,
    ['키', '동작'],
    [
        ['START/STOP', 'ADDR 설정 화면 진입'],
        ['MODE', 'ADDR ↔ BPS 항목 전환'],
        ['UP / DOWN', '값 변경'],
        ['SET', '확정 저장'],
    ])
doc.add_paragraph('운전 순서:')
for s in ['UP + MODE 부팅으로 EXT 진입',
           'START/STOP으로 ADDR 설정 화면 진입',
           'MODE로 ADDR/BPS 항목 선택',
           'UP/DOWN으로 값 조정 → SET 저장',
           '마스터에서 Modbus 제어 시작']:
    doc.add_paragraph(s, style='List Number')

# ========== 6. 키 조합 빠른 참조표 ==========
doc.add_heading('6. 키 조합 빠른 참조표', level=1)
add_table(doc,
    ['구분', '조합', '동작'],
    [
        ['에러 해제', 'MODE + DOWN (2초)', 'Err1/Err2/Err5 해제'],
        ['NORMAL 진입', '전원 ON + SET + DOWN', 'NORMAL 모드'],
        ['REMOTE 진입', '전원 ON + SET + MODE', 'REMOTE 모드'],
        ['EXT 진입', '전원 ON + UP + MODE', 'EXT 모드'],
        ['EXT ADDR/BPS', 'START/STOP → MODE → UP/DOWN → SET', '통신 설정'],
    ])
warn2 = doc.add_paragraph()
r = warn2.add_run('⛔ Err7/Err8은 키 조합으로 해제 불가. 전원 OFF → 센서 확인 → 전원 ON.')
r.bold = True; r.font.color.rgb = RGBColor(0xCC, 0x00, 0x00)

# ========== 7. 주파수 채널 ==========
doc.add_heading('7. 주파수 채널 설정', level=1)
add_table(doc,
    ['채널', '주파수(kHz)', '비고'],
    [
        ['CH0', '500', '저주파 시작'],
        ['CH1', '700', ''],
        ['CH2', '900', ''],
        ['CH3', '1100', ''],
        ['CH4', '1300', ''],
        ['CH5', '1500', ''],
        ['CH6', '1700', ''],
        ['CH7', '1900', ''],
        ['CH8', '2100', ''],
        ['CH9', '2200', '상한 채널'],
    ])
doc.add_paragraph('※ 채널 변경 시 1초간 출력 정지 후 새 주파수로 전환된다.')

# ========== 8. 에러 코드 ==========
doc.add_heading('8. 에러 코드 및 복구', level=1)
add_table(doc,
    ['코드', '명칭', '발생 조건', '해제 방법'],
    [
        ['Err1', 'LOW ALARM', '출력 < LOW 임계, 5초 지속', 'MODE+DOWN(2초)'],
        ['Err2', 'HIGH ALARM', '출력 > HIGH 임계, 5초 지속', 'MODE+DOWN(2초)'],
        ['Err5', 'TRANSDUCER', '진동자/부하 이상, 5초 지속', 'MODE+DOWN(2초)'],
        ['Err6', 'SETTING ERROR', '허용 범위 밖 설정 시도', '값 재입력'],
        ['Err7', 'SENSOR OFF', '정지 중 SENSOR 오픈', '전원 사이클'],
        ['Err8', 'SENSOR RUN', '운전 중 SENSOR 오픈', '전원 사이클'],
    ])

doc.add_heading('8.1 에러별 조치 절차', level=2)
for code, steps in [
    ('Err1 (LOW ALARM)', ['출력선 결선/부하 상태 확인', 'LOW 임계값 과도 설정 여부 확인', 'MODE+DOWN(2초) 해제', '재기동 후 실측 출력 추적']),
    ('Err2 (HIGH ALARM)', ['출력 설정값과 실제 부하 비교', 'HIGH 임계값 재검토', 'MODE+DOWN(2초) 해제', '단계 출력으로 재시험']),
    ('Err5 (TRANSDUCER)', ['트랜스듀서/케이블/커넥터 점검', '과출력/과주파수 여부 점검', 'MODE+DOWN(2초) 해제 후 저출력 재시험']),
    ('Err7/Err8 (SENSOR)', ['즉시 출력 정지 확인', 'SENSOR 회로 물리 점검', '전원 OFF', 'SENSOR 정상(SHORT) 확인', '전원 ON 후 재기동']),
]:
    doc.add_paragraph(code, style='List Bullet')
    for s in steps:
        doc.add_paragraph(s, style='List Number')

# ========== 9. RS-485 통신 ==========
doc.add_heading('9. RS-485 Modbus RTU 통신', level=1)

doc.add_heading('9.1 물리 사양', level=2)
add_img(doc, "rs485_wiring", 5.0, "[그림 9-1] RS-485 배선도")
add_table(doc,
    ['항목', '사양'],
    [
        ['규격', 'RS-485 Half-Duplex'],
        ['배선', 'A/B 2선 + GND'],
        ['종단 저항', '라인 양단 120 Ω'],
        ['보레이트', '9600 / 19200 / 38400 / 115200 bps'],
        ['데이터', '8-N-1 또는 8-E-1'],
        ['CRC', 'CRC-16 (0xA001)'],
    ])

doc.add_heading('9.2 기능 코드', level=2)
add_table(doc,
    ['FC', '기능'],
    [['0x03', 'Holding Register 읽기'], ['0x06', '단일 Register 쓰기'], ['0x10', '복수 Register 쓰기']])

doc.add_heading('9.3 모드별 접근 정책', level=2)
add_table(doc,
    ['모드', '읽기(FC03)', '쓰기(FC06/10)'],
    [['NORMAL', '○', '△ (제한)'], ['REMOTE', '○', '△ (제한)'], ['EXT', '○', '○ (허용)']])

doc.add_heading('9.4 레지스터 맵', level=2)
doc.add_paragraph('제어 그룹 (0x0000~):')
add_table(doc,
    ['주소', 'R/W', '항목', '단위/범위'],
    [
        ['0x0000', 'R/W', 'RUN 제어', '0=OFF, 1=ON'],
        ['0x0001', 'R/W', '출력 설정값', '×0.01W (10~1000)'],
        ['0x0002', 'R', '실측 출력', '×0.01W'],
        ['0x0003', 'R/W', '주파수 채널', '0~9'],
        ['0x0004', 'R', '현재 주파수', '×0.1kHz'],
        ['0x0005', 'R', '상태 플래그', 'bit field'],
        ['0x0006', 'R/W', '운전 모드', '0=NOR, 1=REM, 2=EXT'],
        ['0x0007', 'R/W', '슬레이브 주소', '1~247'],
    ])

doc.add_paragraph('알람/진단 그룹:')
add_table(doc,
    ['주소', 'R/W', '항목', '범위'],
    [
        ['0x0010', 'R/W', 'HIGH 알람', '×0.01W'],
        ['0x0011', 'R/W', 'LOW 알람', '×0.01W'],
        ['0x0012', 'R', '에러 상태', 'bit field'],
        ['0x0013', 'W', '에러 리셋', '1=리셋'],
        ['0x0017', 'R', '펌웨어 버전', 'major/minor'],
    ])

# ========== 10. 트러블슈팅 ==========
doc.add_heading('10. 트러블슈팅', level=1)
add_table(doc,
    ['증상', '원인 후보', '조치'],
    [
        ['통신 응답 없음', 'A/B 반전, 주소/BPS 불일치', '배선·주소·속도 재확인'],
        ['출력 직후 정지', 'SENSOR 오픈, 알람 과민', 'SENSOR·알람값 점검'],
        ['출력 ≠ 목표', '부하 변동, 튜닝 필요', '저출력 재튜닝 후 단계 상승'],
        ['Err7 반복', 'SENSOR 배선 불안정', '센서 회로 물리 점검/교체'],
        ['REMOTE 불안정', '접점 바운스', '접점 품질 개선, 3초 간격'],
    ])

# ========== 11. 유지보수 ==========
doc.add_heading('11. 유지보수 점검', level=1)
add_table(doc,
    ['주기', '점검 항목'],
    [
        ['일일', '케이블/커넥터 이탈, LCD/LED 정상, 알람 이력'],
        ['주간', '출력 재현성, 통신 응답, 주소 충돌'],
        ['월간', '트랜스듀서 절연, 알람 임계값, 펌웨어/설정 백업'],
    ])

# ========== 12. 출하 체크리스트 ==========
doc.add_heading('12. 출하·설치 체크리스트', level=1)
doc.add_paragraph('출하 전:')
for s in ['기본 부팅, LCD 표시 정상', 'NORMAL/REMOTE/EXT 진입 조합 확인',
           'START/STOP 동작 확인', '알람 해제 조합 확인 (Err7 제외)',
           'RS-485 기본 통신 확인 (FC03)']:
    doc.add_paragraph(f'☐ {s}')
doc.add_paragraph('설치 후:')
for s in ['현장 주소 할당 완료', 'BPS/패리티 일치 확인',
           '저전력 시험 완료', '보호 동작(Err7/Err8) 확인']:
    doc.add_paragraph(f'☐ {s}')

# ========== 부록 ==========
doc.add_heading('부록 — 판넬 라벨 권장안', level=1)
doc.add_paragraph('전면 실크: START/STOP · MODE · UP · DOWN · SET')
doc.add_paragraph('후면 라벨:')
add_table(doc,
    ['항목', '키 조합'],
    [
        ['NORMAL', 'PWR ON + SET+DOWN'],
        ['REMOTE', 'PWR ON + SET+MODE'],
        ['EXT', 'PWR ON + UP+MODE'],
        ['ADDR/BPS', 'START/STOP → MODE → UP/DOWN → SET'],
        ['ERROR RESET', 'MODE+DOWN (2s)'],
    ])

# --- 저장 ---
doc.save(OUT)
print(f"\n✅ 매뉴얼 생성 완료: {OUT}")
