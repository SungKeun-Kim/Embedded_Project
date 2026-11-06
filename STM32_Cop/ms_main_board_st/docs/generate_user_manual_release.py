"""사용자 매뉴얼 생성: python docs/generate_user_manual_release.py --word-pdf

python-docx, PyMuPDF, pywin32 및 Microsoft Word 사용.
원본 Word의 패널 이미지는 그대로 사용하고 본문은 현재 펌웨어와 대조해 작성한다.
"""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
from pathlib import Path
from zipfile import ZipFile
from xml.etree import ElementTree

import fitz
from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_TAB_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs/distribution'
BASE = 'CSF_JET_LCD_User_Manual_KR_v5.0'
DATE = '2026-10-16'
REFERENCE = ROOT / 'docs/MANUAL_CSF_JET_LCD_Panel_v1.docx'
SOURCES = ['include/params.h', 'include/board_runtime_config.h', 'include/config.h',
           'include/menu.h', 'src/menu.c', 'src/menu_screen.c', 'src/button.c',
           'src/megasonic_ctrl.c', 'src/adc_control.c', 'src/gpio_init.c',
           'src/settings_store.c', 'src/modbus_regs.c', 'src/modbus_rtu.c']
HASHES = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in SOURCES}


def p(text): return ('p', text)
def h(text): return ('h', text)
def note(text): return ('note', text)
def screen(title, line1, line2):
    assert len(line1) <= 16 and len(line2) <= 16, (title, line1, line2)
    return ('screen', title, line1, line2)
def table(headers, rows, widths=None): return ('table', headers, rows, widths)


FREQUENCIES = [400, 600, 800, 1000, 1200, 1400, 1600, 1800, 2000, 2300]
FREQ_ROWS = [[f'CH{i+1:02}', f'{f:,} kHz', '1111' if i == 3 else ('0001' if i == 8 else '0000')]
             for i, f in enumerate(FREQUENCIES)]
POWER_ROWS = [[f'PWR{i+1}', '0.30 W', f'{0.50 + i * 0.25:.2f} W', '2.50 W'] for i in range(8)]
LC_ROWS = [[f'{v:04b}', *['ON' if v & (1 << bit) else 'OFF' for bit in [3, 2, 1, 0]],
            f'{(194 + sum(w for bit, w in enumerate([69, 220, 470, 680]) if v & (1 << bit))) / 100:.2f} µH']
           for v in range(16)]

PAGES = [
('사용자 매뉴얼', [
    ('subtitle', 'CSF JET Multi Megasonic Generator'),
    p('LCD 패널형 · 설치 / 패널 조작 / 프로파일 설정 / 원격 운전 / 알람 대응'),
    ('panel', '참고 매뉴얼의 LCD 패널 배치 이미지. 실제 외관은 장비 개정에 따라 다를 수 있습니다.'),
    table(['문서', '적용 정보'], [
        ['버전 / 발행일', f'v5.0-KR / {DATE}'],
        ['적용 범위', '현재 저장된 메가소닉 메인보드 펌웨어의 LCD 조작 방식'],
        ['참고 문서', 'MANUAL_CSF_JET_LCD_Panel_v1.docx / v4.1-KR'],
        ['함께 보는 문서', 'Megasonic_RS485_Protocol_KR_v1.0 (통신 담당자용)'],
    ], [27, 73]),
    note('사용 전 확인: 현재 적용본은 SENSOR 자동 차단과 LOW/HIGH 알람 정지를 사용하지 않습니다. Auto Tuning도 비활성 상태입니다. 2쪽의 기능 상태를 먼저 확인하세요.'),
    h('찾아보기'),
    table(['내용', '쪽', '내용', '쪽'], [
        ['적용 범위·기능 상태', '2', 'LC BIT 조합', '8'],
        ['버튼·LED', '3', 'POWER SET', '9'],
        ['화면·기본 운전', '4', '운전 중 조정', '10'],
        ['배선·25pin D-SUB', '5', 'EXT / RS-485', '11'],
        ['REMOTE / BCD', '6', '알람 표시·조치', '12'],
        ['FREQ SET', '7', '저장·문제 해결 / 점검·기록', '13 / 14'],
    ], [36, 8, 46, 10]),
]),
('1. 적용 범위와 기능 상태', [
    p('이 문서는 현재 펌웨어의 조작 방법을 설명합니다. 장비의 입력 전원, 출력 정격, 허용 부하, 환경 조건은 장비 명판과 제조사가 승인한 사양을 따릅니다. 화면에서 설정할 수 있는 수치가 모든 노즐·공정에서 허용되는 운전 범위를 뜻하지는 않습니다.'),
    h('현재 적용본에서 확인해야 할 사항'),
    table(['항목', '현재 상태', '사용 시 의미'], [
        ['SENSOR 인터록', '비활성', 'SENSOR가 OPEN이어도 자동 시작 금지·정지를 기대할 수 없음.'],
        ['LOW / HIGH 알람', '설정 저장 가능\n알람 정지는 비활성', 'LOW/HIGH 값은 출력 프로파일 범위에도 사용됨. 보호 정지 기능으로 사용하지 않음.'],
        ['Auto Tuning', '비활성', 'START(3SEC):AUTO 문구가 보여도 자동 튜닝은 시작되지 않음.'],
        ['자동 주파수 재튜닝', '비활성', '승인된 주파수·프로파일을 사용하고 임의 최적화를 기대하지 않음.'],
        ['알람 / GO / OPERATE 접점', '현재 코드에 운전·알람 연동 구동 없음', '외부 PLC의 운전 확인·안전 차단 신호로 사용 전 제조사 검증 필요.'],
        ['TERM / TX RX LED', '종단 하드웨어 전환·통신 LED 연동 없음', '실제 종단 저항과 통신 응답으로 접속을 확인.'],
    ], [24, 28, 48]),
    note('SENSOR·알람 접점에 의한 자동 차단이 필요한 설비는 해당 보호 동작을 제조사와 확인한 뒤 사용하세요. 본 문서는 보호 기능을 활성화하거나 장비의 출고 검증을 대신하지 않습니다.'),
    h('패널 기능 요약'),
    table(['항목', '현재 구성'], [
        ['표시 / 조작', 'LCD 16문자 × 2행 / START/STOP, MODE, UP, DOWN, SET'],
        ['운전 방식', 'NORMAL(패널), REMOTE(외부 접점), EXT(RS-485)'],
        ['주파수', '10개 프로파일 / 기본 400~2300 kHz / 편집 범위 300~2300 kHz'],
        ['LC / 출력 프로파일', 'LC BIT 0000~1111 / PWR1~PWR8'],
        ['설정 표시 범위', 'POWER SET의 LOW/HIGH는 통상 0.30~2.50 W, DEF는 LOW~HIGH 사이'],
        ['저장', '내부 Flash. 기존 저장값이 있으면 기본값보다 우선.'],
    ], [30, 70]),
    p('현재 코드에는 과전류·전류 센서 이상·시작 전압·Buck 과전압에 대한 정지 처리가 있습니다. 동작 여부와 실제 보호 응답은 해당 장비에서 확인해야 합니다.'),
]),
('2. 버튼과 상태 표시', [
    h('버튼은 현재 화면에 따라 기능이 달라집니다'),
    table(['버튼', '정지 / 선택 화면', 'RUN 화면', '설정 화면'], [
        ['START/STOP', '짧게 눌렀다 놓으면 시작', '짧게 눌렀다 놓으면 정지', '현재판에서 Auto Tuning 시작 안 함'],
        ['MODE 짧게', '초기 화면 → 선택 모드\nMODE → FREQ CH → 8 POWER', '출력 조정 ↔ 주파수 조정', '편집 항목 이동'],
        ['MODE 길게', '약 1.5초: 설정 메뉴 진입', '설정 메뉴 진입 안 함', '하위 설정에서 저장 후 상위 메뉴 복귀'],
        ['UP / DOWN', '선택 중인 모드·채널 변경', '선택된 조정값 증가 / 감소', '편집값 증가 / 감소'],
        ['SET 짧게', '확정 및 저장', '현재 조정 지점 저장\n저장 중 출력 일시 정지 가능', '확정 및 저장'],
        ['SET 길게', '선택 모드에서 초기 화면 복귀', '복귀 조작으로 사용하지 않음', '약 3초 유지: 저장 후 초기 화면 복귀'],
    ], [18, 28, 28, 26]),
    p('UP/DOWN은 길게 누르면 반복 입력됩니다. START/STOP은 짧게 눌렀다 놓아야 일반 시작·정지 이벤트가 발생합니다. 정지하려고 계속 누르고 있지 마세요.'),
    h('LED 읽는 방법'),
    table(['표시', '현재 동작'], [
        ['NORMAL / REMOTE / EXT', '선택된 모드의 LED가 정지 중 켜지고 RUN 중 점멸합니다.'],
        ['H/L SET', 'POWER SET에서 LOW 또는 HIGH를 편집할 때 켜집니다. 알람 발생 표시가 아닙니다.'],
        ['8 POWER', '8 POWER 선택 또는 POWER SET의 CH/DEF 편집 중 켜집니다.'],
        ['TX RX', '현재 소프트웨어에서 송수신 점멸 표시를 제공하지 않습니다.'],
    ], [30, 70]),
    h('비프음'),
    table(['소리', '의미'], [
        ['짧게 1회', '일반 조작 인식'],
        ['길게 1회', '길게 누름 인식 또는 알람. 화면 표시와 함께 구분.'],
        ['2회', 'SET 저장 성공'],
        ['3회', '저장 실패. 재시도 후에도 반복되면 점검 요청.'],
    ], [30, 70]),
    note('기판에 인쇄된 ADDR/BPS 또는 버튼 연결선은 현재 펌웨어의 동시 누름 조작을 뜻하지 않습니다. 통신 조건은 EXT:RS485 메뉴에서 설정하세요.'),
]),
('3. LCD 화면과 기본 운전', [
    p('아래는 표시 형식을 설명하는 예시입니다. 숫자는 장비 상태에 따라 달라집니다. RUN 화면의 회전 표시는 예시에서 *로 나타냈습니다.'),
    screen('정지 기본 화면', '0.50W    0000kHz', 'V12.5 STOP C01'),
    screen('RUN 기본 화면', '0.48W*   0400kHz', 'I0040mA V12.5'),
    table(['표시', '읽는 방법'], [
        ['W', '정지 중: 선택한 프로파일의 설정값. RUN 중: 전류·전압 기반 추정 전력.'],
        ['kHz', '정지 기본 화면은 0000kHz. 채널 설정값은 FREQ CH 선택 화면에서 확인.'],
        ['RUN 주파수', '하드웨어 설정으로 계산된 출력 주파수. 설정 분해능 때문에 입력값과 조금 다를 수 있음.'],
        ['I / V / C', 'I=전류 mA, V=정지 시 목표 전압·RUN 시 계측 전압, C=주파수 채널.'],
    ], [25, 75]),
    h('NORMAL 운전 순서'),
    table(['단계', '조작'], [
        ['1', '배선·부하·공급 조건을 확인하고 전원을 켭니다. 외부 REMOTE 명령은 OFF로 둡니다.'],
        ['2', '정지 화면에서 MODE를 짧게 눌러 선택 모드로 들어갑니다. UP/DOWN으로 NORMAL 선택 → SET.'],
        ['3', 'MODE로 FREQ CH 선택 → UP/DOWN으로 승인된 채널 선택 → SET.'],
        ['4', 'MODE로 8 POWER 선택 → UP/DOWN으로 승인된 PWR 선택 → SET.'],
        ['5', 'START/STOP을 짧게 눌렀다 놓습니다. 시작 전압 준비 후 모드 LED 점멸·계측 표시를 확인합니다.'],
        ['6', '정지는 START/STOP을 짧게 눌렀다 놓습니다. STOP 표시와 모드 LED의 연속 점등을 확인합니다.'],
    ], [10, 90]),
    note('RUN의 W 표시는 교정된 초음파 출력 보증값이 아닙니다. 시작 준비 중에는 연속으로 버튼을 누르지 말고 화면과 알람을 확인하세요.'),
    p('설정 흐름: STOP → MODE 길게 → SETTING MODE → UP/DOWN으로 메뉴 선택 → SET 진입 → MODE로 항목 이동 → UP/DOWN 변경 → SET 저장 → SET 약 3초로 초기 화면 복귀.'),
]),
('4. 설치와 외부 연결', [
    p('장비 전원을 끈 상태에서 지정된 전원·보호접지·노즐/트랜스듀서·출력 케이블을 연결합니다. 케이블 종류·길이와 부하는 제조사 승인 조건을 따르고, 통풍을 확보하세요. 출력 케이블을 분리한 상태에서 시작하지 마세요.'),
    h('25pin D-SUB 신호표'),
    p('핀 번호는 참고 매뉴얼 기준입니다. 실제 장비의 커넥터 각인·출고 배선도와 대조하세요. 커넥터 정면과 배선면은 좌우가 바뀌므로 그림의 방향만 보고 연결하지 마세요.'),
    table(['핀 번호', '신호', '연결 의미 / 현재 적용 상태'], [
        ['1 – 14', 'LOW ALARM', '저출력 알람용 릴레이 접점 명칭. 현재 자동 구동 미구현.'],
        ['2 – 15', 'GO', '출력 정상 범위 표시용 접점 명칭. 현재 자동 구동 미구현.'],
        ['3 – 16', 'HIGH ALARM', '고출력 알람용 접점 명칭. 현재 자동 구동 미구현.'],
        ['6 – 19', 'TRANSDUCER', '진동자 알람용 접점 명칭. 현재 자동 구동 미구현.'],
        ['7 – 20', 'OPERATE', '통합 상태용 접점 명칭. 현재 정상/알람 상태 연동 미구현.'],
        ['9', 'COM', 'REMOTE 및 BCD 입력 공통'],
        ['22', 'REMOTE', '9번 COM과 접점 연결. REMOTE 모드의 시작/정지 입력.'],
        ['11', 'BCD1', '8 POWER 선택의 최하위 비트'],
        ['23', 'BCD2', '8 POWER 선택의 중간 비트'],
        ['10', 'BCD3', '8 POWER 선택의 최상위 비트'],
        ['13 – 25', 'SENSOR', '정상 신호는 SHORT. 현재 펌웨어에서는 시작/정지 인터록 비활성.'],
        ['4, 5, 8, 12,\n17, 18, 21, 24', 'N.C.', '연결하지 않음'],
        ['커넥터 셸', 'SHIELD', '장비 배선도에 따른 쉴드 연결'],
    ], [20, 24, 56]),
    note('현재 적용본의 알람·OPERATE 접점을 장비 안전 차단의 근거로 사용하지 마세요. 접점 정격과 실제 외부 회로 동작은 출고 배선도 및 제조사 확인이 필요합니다.'),
    h('RS-485 연결'),
    p('RS-485 A/B는 전용 통신 단자에 연결합니다. 위 25pin 표에 RS-485 신호를 임의 배정하지 마세요. 마스터와 보드의 극성·주소·속도·패리티를 맞춥니다. 종단 저항은 실제 버스 끝단 하드웨어를 확인하며 패널 TERM 값만으로 연결 여부를 판단하지 않습니다.'),
]),
('5. REMOTE 접점 운전', [
    h('외부 시작·정지'),
    table(['순서', '조작'], [
        ['1', '출력 정지 및 전원 OFF 후 COM(9)과 REMOTE(22)를 승인된 외부 접점 회로에 연결합니다.'],
        ['2', 'REMOTE 접점을 OPEN(OFF)으로 둔 상태에서 전원을 켭니다.'],
        ['3', '패널에서 REMOTE를 선택하고 SET으로 저장합니다. 주파수와 출력 프로파일을 확인합니다.'],
        ['4', '접점이 OPEN 상태임을 한 번 확인한 뒤 SHORT(ON)하면 시작합니다.'],
        ['5', '접점을 OPEN으로 바꾸면 정지합니다. LCD의 STOP을 확인합니다.'],
    ], [12, 88]),
    note('전원 투입 또는 REMOTE 모드 진입 시 이미 ON인 접점만으로 시작되지 않을 수 있습니다. 먼저 OFF로 만든 뒤 ON으로 전환하세요. 외부 ON이 유지되면 패널·통신으로 정지한 뒤 다시 시작될 수 있으므로, REMOTE 운전의 정지는 외부 명령도 OFF로 만드세요.'),
    h('외부 8 POWER 선택'),
    p('접점 1=COM과 SHORT(활성), 0=OPEN(비활성)입니다. BCD라는 명칭을 사용하지만 아래 3비트 조합으로 PWR1~PWR8을 선택합니다.'),
    table(['BCD3 (10)', 'BCD2 (23)', 'BCD1 (11)', '선택'], [
        ['0','0','0','PWR1'], ['0','0','1','PWR2'], ['0','1','0','PWR3'], ['0','1','1','PWR4'],
        ['1','0','0','PWR5'], ['1','0','1','PWR6'], ['1','1','0','PWR7'], ['1','1','1','PWR8'],
    ], [25,25,25,25]),
    p('외부 PWR 변경은 운전 중에도 반영될 수 있습니다. 일반 운용은 REMOTE OFF → BCD 변경 → 선택 확인 → REMOTE ON 순서로 진행하세요. 외부 접점과 패널에서 동시에 다른 PWR를 선택하지 마세요.'),
    h('확인할 사항'),
    p('REMOTE LED와 선택 PWR가 의도한 값인지 확인합니다. 시작하지 않으면 접점을 OFF→ON 했는지, 알람이 있는지, 출력 프로파일과 전원 상태가 유효한지 확인하세요. SENSOR가 OPEN인데도 시작되는 현상은 현재 인터록 비활성 상태와 관련됩니다.'),
]),
('6. FREQ SET: 주파수 설정', [
    h('기본 주파수 프로파일'),
    table(['채널', '기본 주파수', '기본 LC BIT'], FREQ_ROWS, [23,40,37]),
    p('저장값이 있으면 위 기본값보다 우선합니다. CH10 기본값은 현재 2300 kHz입니다. 이전 매뉴얼의 2200 kHz 표와 혼용하지 마세요.'),
    h('채널 선택과 프로파일 편집은 서로 다릅니다'),
    p('평소에는 정지 선택 화면의 FREQ CH에서 승인된 채널만 선택합니다. 프로파일 자체를 수정할 때 아래 FREQ SET 절차를 사용합니다.'),
    table(['순서', '설정 절차'], [
        ['1', '정지 화면 → MODE 약 1.5초 → 1.FREQ SET → SET'],
        ['2', 'MODE로 CH → FREQ → BIT 순서로 편집 항목 이동'],
        ['3', 'CH: UP/DOWN으로 편집할 CH01~CH10 선택'],
        ['4', 'FREQ: UP/DOWN으로 주파수 조정. 기본 변화 단위 1 kHz, 범위 300~2300 kHz'],
        ['5', 'BIT: 승인된 LC 릴레이 조합 선택. 0000~1111의 16단계'],
        ['6', 'SET 저장. MODE 길게는 저장 후 상위 메뉴, SET 약 3초는 저장 후 초기 화면 복귀'],
    ], [12,88]),
    note('주파수·BIT를 바꾸면 편집 중에도 내부 값과 릴레이 미리보기에 반영됩니다. 뒤로 가기는 취소 기능이 아닙니다. 작업 전 값을 기록하고, 승인된 부하·조건에서만 변경하세요.'),
    p('E는 프로파일의 기대 BIT, A는 제어부에 적용된 BIT 코드입니다. A 표시는 릴레이 접점이 실제로 움직였다는 물리적 피드백이 아닙니다.'),
]),
('7. LC BIT와 Auto Tuning 표시', [
    p('LC BIT는 4개 릴레이의 조합입니다. 왼쪽부터 RELAY4(bit3), RELAY3(bit2), RELAY2(bit1), RELAY1(bit0)이며 1은 ON, 0은 OFF 명령입니다.'),
    table(['BIT', 'RELAY4', 'RELAY3', 'RELAY2', 'RELAY1', '계산 L'], LC_ROWS, [15,16,16,16,16,21]),
    p('계산값은 고정 1.94 µH에 각 ON 릴레이의 0.69 / 2.20 / 4.70 / 6.80 µH를 더한 참고값입니다. 실제 부품 공차·배선·부하를 포함한 측정값과 다를 수 있습니다.'),
    screen('BIT 편집 표시 예', 'START(3SEC):AUTO', 'BIT:1111 16/16'),
    note('현재 적용본에서는 Auto Tuning이 비활성화되어 있습니다. 화면에 START(3SEC):AUTO가 남아 있어도 버튼을 길게 눌러 자동 튜닝할 수 없습니다.'),
    p('기존 설명서의 AUTO TUNE DONE, NO DETECT, TIMEOUT 운용 절차는 이 적용본의 일반 사용자 절차에 포함하지 않습니다. LC와 주파수 조합은 제조사가 확인한 설정을 사용하세요.'),
]),
('8. POWER SET: 출력 프로파일', [
    p('PWR1~PWR8에는 LOW, HIGH, DEF를 저장합니다. 현재 LOW/HIGH는 출력 조정 범위와 전압 환산에도 사용됩니다. 알람 정지는 비활성 상태이므로, 이 값을 설정했다고 LOW/HIGH 보호가 동작하는 것은 아닙니다.'),
    table(['항목', '의미와 제약'], [
        ['CH', '편집할 PWR1~PWR8'],
        ['LOW', '프로파일 하한. 통상 0.30~2.50 W 안에서 HIGH보다 작게 설정.'],
        ['HIGH', '프로파일 상한. 통상 0.30~2.50 W 안에서 LOW보다 크게 설정.'],
        ['DEF', '프로파일 선택·START 시 사용하는 기본 설정. LOW~HIGH 사이로 제한.'],
    ], [20,80]),
    h('기본값'),
    table(['POWER', 'LOW', 'DEF', 'HIGH'], POWER_ROWS, [25,25,25,25]),
    h('설정 절차'),
    p('STOP → MODE 길게 → UP/DOWN으로 2.POWER SET 선택 → SET → MODE로 CH / LOW / HIGH / DEF 이동 → UP/DOWN 변경 → SET 저장.'),
    p('일반 변화 단위는 0.05 W입니다. LOW < HIGH와 LOW ≤ DEF ≤ HIGH가 유지되도록 다른 값이 함께 보정될 수 있으므로 저장 전에 세 값을 모두 확인하세요. 경계에서는 표시가 일반 변화 단위와 다를 수 있습니다.'),
    screen('DEF 편집 표시 예', 'P1 CH01 0400k', 'D:0.50W V12.5'),
    note('DEF 옆의 V는 설정값에 대응하는 목표 전압입니다. W 설정은 전압으로 환산되어 제어되므로 실제 RUN 추정 전력이 설정 W와 일치한다고 보장하지 않습니다. LOW/HIGH를 바꾸면 같은 DEF에서도 목표 전압이 달라질 수 있습니다.'),
    p('설정 범위는 실제 장비의 승인 출력 정격을 대신하지 않습니다. DEF를 변경하면 해당 POWER의 기존 조정 지점이 초기화될 수 있으므로 변경 전 기록하세요.'),
]),
('9. RUN 중 조정과 저장', [
    h('일반 운전은 승인된 프로파일로 시작합니다'),
    p('아래 조작은 운전 중 변경이 허용된 작업에서만 사용하세요. RUN 중에는 일반 선택 모드와 설정 메뉴로 들어가지 않으며, 모드·주파수 채널·출력 프로파일 변경은 정지 후 진행합니다.'),
    table(['RUN 화면', 'MODE 짧게', 'UP / DOWN'], [
        ['기본 계측 화면\nIxxxxmA Vxx.x', '주파수 조정 화면으로 이동', '출력 설정 환산값을 0.05 W 단위로 조정하여 목표 전압 변경'],
        ['주파수 조정 화면\nFxxx.xk CHxx', '기본 계측 화면으로 복귀', '주파수 명령을 1 kHz 단위로 조정'],
    ], [34,29,37]),
    screen('RUN 주파수 조정 예', '0.48W*   0400kHz', 'F400.0k CH01'),
    p('표시되는 주파수는 하드웨어 설정으로 계산하므로 버튼을 한 번 눌러도 표시 변화가 일정하지 않을 수 있습니다. RUN의 W는 계측 추정값이며 UP/DOWN의 설정 증가량과 동일하게 변하지 않을 수 있습니다.'),
    h('RUN 중 SET의 의미'),
    p('현재 선택된 주파수 채널과 PWR 조합에 대해 조정된 주파수·게이트 듀티 지점을 저장합니다. 일반적인 POWER SET의 DEF 또는 현재 목표 전압 전체를 저장하는 조작과는 다릅니다. 정지·재시작하면 프로파일이 다시 적용되므로 운전 조건이 그대로 유지된다고 가정하지 마세요.'),
    note('RUN 중 SET 저장 과정에는 출력이 일시 정지된 뒤 다시 시작되는 동작이 포함됩니다. 연속 출력이 필요한 공정 중에는 사용하지 마세요.'),
    h('전압 상한 안내와 알람 구분'),
    screen('조정 상한에 도달한 안내', ' BUCK OVER VOLT', 'LIMIT V38.0'),
    p('위 LIMIT V38.0 화면은 더 이상 목표 전압을 올릴 수 없다는 일시 안내이며, 반드시 보호 정지를 뜻하지는 않습니다. ALARM STOP과 함께 BUCK OVER VOLT가 표시되면 보호 정지이므로 12쪽의 조치를 따르세요.'),
    h('변경 후 확인'),
    p('전류·전압·추정 전력의 변화를 관찰합니다. 승인 조건을 벗어나거나 이상이 있으면 정지하고 원인을 확인하세요. 정지 후 다음 시작에 사용할 FREQ CH와 PWR를 다시 확인합니다.'),
]),
('10. EXT / RS-485 운전', [
    h('패널 통신 설정'),
    p('출력을 정지하고 패널에서 EXT를 선택 → SET 저장 → MODE 길게 → 3.EXT:RS485 → SET으로 진입합니다. MODE로 BAUD / ADDR / TERM / PARITY를 이동하고 UP/DOWN으로 값을 바꿉니다.'),
    table(['필드', '설정값', '설명'], [
        ['BAUD', '9600 / 19200 / 38400 / 115200', '통신 속도 bps'],
        ['ADDR', '1~247', '같은 버스의 장치마다 다른 주소 사용'],
        ['TERM', 'OFF / ON', '설정 기록만 수행. 실제 종단 저항 전환 안 함.'],
        ['PARITY', 'NONE-8-1 / EVEN-8-1', '데이터 8비트, 정지 1비트'],
    ], [18,43,39]),
    screen('통신 설정 예', ' EXT:RS485 SET', 'BAUD:9600'),
    p('설정 변경은 통신에 즉시 반영됩니다. SET으로 저장한 후 설정 화면에서 나오고 PC/PLC의 통신 조건을 동일하게 맞추세요. 기본값은 주소 1 / 9600 bps / NONE-8-1이며 기존 저장값이 있으면 다를 수 있습니다.'),
    h('마스터에서 운전하기'),
    table(['순서', '기능'], [
        ['1. 확인', '출력 정지, 패널 EXT, 사용할 주파수·출력 프로파일 저장 상태를 확인합니다.'],
        ['2. 읽기 시험', 'FC03 / 실제 주소 0x0000 / 수량 6으로 상태를 읽습니다.'],
        ['3. 시작', 'FC06 / 0x0000에 1. 시작 처리가 끝난 뒤 RUN 상태를 읽습니다.'],
        ['4. 감시', 'RUN과 계측값을 읽습니다. 운전 중 설정 쓰기는 정지를 제외하고 거부됩니다.'],
        ['5. 정지', 'FC06 / 0x0000에 0. RUN=0과 패널 STOP을 확인합니다.'],
    ], [23,77]),
    note('통신이 끊겼다고 자동으로 출력이 정지하지 않습니다. 외부 설비에서 필요한 정지 수단을 확인하세요. RS-485 시작은 패널에 선택된 프로파일을 적용하며 임의 W·주파수 지정 운전 기능과 동일하지 않습니다.'),
    p('기능 코드·주소·CRC 예제·설정 저장 조건·현재 통신 타이머 제한은 별도 배포한 「RS-485 통신 프로토콜 v1.0」을 참조하세요. 본 매뉴얼의 통신 설명은 기본 조작 요약입니다.'),
]),
('11. 알람 표시와 대응', [
    h('현재 사용하는 보호 표시'),
    table(['LCD 표시', '의미', '사용자 조치'], [
        ['OVER CURRENT', '과전류 감지', '출력 정지 확인. 부하·케이블·설정 변경 이력을 기록하고 점검 요청.'],
        ['CUR SENSOR FLT', '전류 센서 이상 또는 포화 감지', '반복 재시작하지 말고 센서·배선 점검 요청.'],
        ['ALARM STOP\nLOW START VOLT', '시작에 필요한 전압 준비 실패', '입력 전원·부하·프로파일 확인. 반복 시 전원부 점검 요청.'],
        ['ALARM STOP\nBUCK OVER VOLT', 'Buck 전압 이상 감지', '전원부·피드백 점검 요청. LIMIT V38.0 안내와 구분.'],
    ], [31,28,41]),
    screen('과전류 표시 예', 'OVER CURRENT', 'I0500mA V28.0'),
    p('과전류·전류 센서 오류 화면의 I/V는 오류 발생 시 기록한 값입니다. 표시값을 현재 정상 운전값으로 해석하지 마세요.'),
    h('원인 확인과 해제'),
    p('① 외부 REMOTE 명령을 OFF로 하고 출력 정지를 확인합니다. ② 화면과 당시 설정을 기록합니다. ③ 배선 점검이 필요하면 전원을 끄고 제조사 절차에 따라 점검합니다. ④ 원인을 제거한 뒤 알람 화면에서 SET을 짧게 눌러 표시를 해제합니다. ⑤ 재시작 전에 승인된 설정과 외부 명령을 다시 확인합니다.'),
    note('SET은 오류 표시를 지우는 조작이며 고장 원인을 고치는 조작이 아닙니다. 같은 알람이 반복되면 재기동을 중단하고 점검을 요청하세요. REMOTE가 ON인 채 알람을 해제하면 다시 시작될 수 있습니다.'),
    h('이 적용본에서 기대하면 안 되는 과거 알람'),
    table(['기존 표시', '현재 상태'], [
        ['ERR1 LOW ALARM / ERR2 HIGH ALRM', 'LOW/HIGH 알람 정지 비활성. 낮거나 높은 출력에 대한 이 알람을 기대하지 않음.'],
        ['ERR7 SNS OFF / ERR8 SNS RUN', 'SENSOR 인터록 비활성. SENSOR OPEN에 따른 이 알람을 기대하지 않음.'],
        ['ERR5 TRANS ALM / ERR6 SET ERROR', '표시 정의는 있으나 현재 정상 운전 경로의 자동 발생 기능은 확인되지 않음.'],
    ], [44,56]),
    p('위 과거 표시가 실제 장비에 나타나면 펌웨어 적용본이 이 문서와 같은지 확인하고 제조사에 문의하세요.'),
]),
('12. 저장과 문제 해결', [
    h('저장되는 항목'),
    p('SET으로 저장한 패널 운전 모드, 선택 주파수 채널·PWR, 채널별 주파수·BIT, PWR별 LOW/HIGH/DEF, RS-485 설정은 전원을 껐다 켜도 복원됩니다. 유효한 저장값이 없을 때만 기본값을 사용합니다.'),
    table(['항목', '저장 방법 / 주의'], [
        ['패널 선택·설정', 'SET 저장 후 비프 2회 확인. 편집 중 값 변경이 즉시 반영될 수 있고 화면을 나갈 때도 저장될 수 있음.'],
        ['취소', '별도 취소 기능 없음. 되돌리려면 기록한 이전 값을 다시 입력하고 저장.'],
        ['RUN 조정 지점', 'RUN 중 SET은 주파수·게이트 듀티 지점 저장. 출력이 일시 정지될 수 있음.'],
        ['통신으로 변경한 값', '통신 쓰기만으로 영구 저장이 보장되지 않음. 통신 설명서의 저장 동작 확인.'],
        ['공장 초기화', '이 적용본에 일반 사용자용 전체 초기화 메뉴 없음. 임의 리셋 조합을 사용하지 않음.'],
    ], [27,73]),
    h('증상별 확인'),
    table(['증상', '확인·조치'], [
        ['START를 눌러도 시작 안 됨', '짧게 눌렀다 놓았는지, 설정 화면이 아닌지, 알람·저장 실패·시작 전압 오류가 있는지 확인.'],
        ['STOP인데 주파수가 0000kHz', '정지 기본 화면의 정상 표시. FREQ CH 선택 화면에서 설정 주파수 확인.'],
        ['START(3SEC):AUTO가 동작 안 함', '현재 Auto Tuning 비활성. 수동 프로파일 사용.'],
        ['SENSOR를 열어도 정지 안 됨', '현재 인터록 비활성. 이 입력을 안전 정지 수단으로 사용하지 않음.'],
        ['LOW/HIGH를 넘겨도 알람 없음', '현재 LOW/HIGH 알람 정지 비활성. 외부 확인 및 제조사 문의.'],
        ['REMOTE에서 시작 안 됨', 'REMOTE 모드·9/22 접점·OFF→ON 전환·현재 알람 확인.'],
        ['정지 후 다시 시작됨', 'REMOTE ON 유지 여부 확인. 외부 RUN 명령부터 OFF.'],
        ['RS-485 응답 없음', 'A/B, 주소, 속도, 패리티, 정지 1비트, 실제 종단 구성 확인.'],
        ['저장값이 복원되지 않음', 'SET 성공음·현재 편집 채널 확인. 3회 비프가 반복되면 저장부 점검 요청.'],
        ['RUN W와 설정 W가 다름', '설정값의 전압 환산과 계측 추정값 차이. 승인 운전 조건·측정 상태 확인.'],
    ], [33,67]),
]),
('13. 점검·설정 기록·서비스', [
    h('일상 점검'),
    table(['시점', '점검 내용'], [
        ['사용 전', '출력 케이블·커넥터·접지·부하·공정 공급 조건, 선택 채널·PWR 확인.'],
        ['운전 중', '전류·전압·추정 전력 변화, 이상음·냄새·과열·알람 확인. 이상 시 정지.'],
        ['사용 후', 'STOP 및 외부 RUN OFF 확인. 이상 상황과 변경값 기록.'],
        ['정기 점검', '제조사 절차에 따라 케이블·절연·접점·보호 동작·통신·저장 복원 확인.'],
    ], [25,75]),
    p('정격·교정·부하 허용 범위와 점검 주기는 장비 공급자의 승인 문서를 따릅니다. 사용자가 임의로 내부 회로를 개조하거나 보호 설정을 우회하지 마세요.'),
    h('장비별 설정 기록'),
    table(['기록 항목', '기록란'], [
        ['모델 / 제조번호', '________________________________________'],
        ['부하 / 노즐 / 케이블 식별', '________________________________________'],
        ['모드 / FREQ CH / 주파수 / BIT', '________________________________________'],
        ['PWR / LOW / HIGH / DEF', '________________________________________'],
        ['RS-485 주소 / 속도 / 패리티', '________________________________________'],
        ['변경일 / 작업자 / 승인 조건', '________________________________________'],
    ], [43,57]),
    h('서비스 요청 시 준비할 정보'),
    p('모델·제조번호, 적용 문서 버전, LCD 알람 사진, 발생 시 I/V 표시, 사용한 FREQ/PWR, 변경 전후 설정, 외부 REMOTE 상태, 재현 순서를 전달하세요. 보증 범위와 서비스 연락처는 구매·공급 계약 및 장비 라벨을 따릅니다.'),
    h('문서 개정'),
    table(['버전', '일자', '내용'], [
        ['v4.1-KR', '2026-05-26', '참고 원본: 패널·기본값·LC 조합 설명'],
        ['v5.0-KR', DATE, '현재판 기능 상태 반영, CH10 2300 kHz, SET 복귀 약 3초, 실제 화면·RUN 조정·보호 표시·통신 설명 정리'],
    ], [19,24,57]),
    p('이 문서는 현재 저장된 소스와 원본 매뉴얼을 대조하여 작성했습니다. 실제 장비의 출고 시험·배선 검증·출력 교정 기록은 별도 관리합니다. 펌웨어나 하네스가 변경되면 적용 문서를 함께 확인하세요.'),
]),
]


def set_font(style, size, bold=False, color='25384B'):
    style.font.name = '맑은 고딕'
    style.font.size = Pt(size)
    style.font.bold = bold
    style.font.color.rgb = RGBColor.from_string(color)
    style.element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'), '맑은 고딕')


def shade(element, color):
    shd = OxmlElement('w:shd')
    shd.set(qn('w:fill'), color)
    element.append(shd)


def make_docx(path):
    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21), Cm(29.7)
    sec.top_margin, sec.bottom_margin = Cm(1.7), Cm(1.6)
    sec.left_margin = sec.right_margin = Cm(1.65)
    sec.header_distance = sec.footer_distance = Cm(0.75)
    set_font(doc.styles['Normal'], 9.5)
    doc.styles['Normal'].paragraph_format.space_after = Pt(7)
    doc.styles['Normal'].paragraph_format.line_spacing = 1.10
    for name, size in [('Title', 29), ('Heading 1', 19), ('Heading 2', 11.5)]:
        set_font(doc.styles[name], size, True, '154E70')
        doc.styles[name].paragraph_format.space_before = Pt(9)
        doc.styles[name].paragraph_format.space_after = Pt(7)
    header = sec.header.paragraphs[0]
    header.text = 'CSF JET  /  MULTI MEGASONIC  /  LCD USER MANUAL'
    for run in header.runs:
        run.font.size = Pt(8)
        run.font.color.rgb = RGBColor.from_string('547083')
    footer = sec.footer.paragraphs[0]
    footer.paragraph_format.tab_stops.add_tab_stop(Cm(17.6), WD_TAB_ALIGNMENT.RIGHT)
    footer.text = f'사용자 매뉴얼 v5.0-KR  |  {DATE}\t'
    for instruction in ['PAGE', 'NUMPAGES']:
        if instruction == 'NUMPAGES': footer.add_run(' / ')
        field = OxmlElement('w:fldSimple')
        field.set(qn('w:instr'), instruction)
        footer._p.append(field)
    for run in footer.runs: run.font.size = Pt(8)
    doc.core_properties.title = 'CSF JET LCD 패널 사용자 매뉴얼 v5.0-KR'
    doc.core_properties.subject = '현재 펌웨어 적용본의 조작·설정·원격 운전'
    doc.core_properties.author = '메가소닉 펌웨어 프로젝트'
    with ZipFile(REFERENCE) as ref:
        panel_data = ref.read('word/media/image1.png')
    for index, (title, blocks) in enumerate(PAGES):
        if index: doc.add_page_break()
        doc.add_paragraph(title, 'Title' if index == 0 else 'Heading 1')
        for block in blocks:
            kind = block[0]
            if kind == 'panel':
                para = doc.add_paragraph()
                para.alignment = WD_ALIGN_PARAGRAPH.CENTER
                para.add_run().add_picture(io.BytesIO(panel_data), width=Cm(15.8))
                caption = doc.add_paragraph(block[1])
                caption.alignment = WD_ALIGN_PARAGRAPH.CENTER
                caption.runs[0].font.size = Pt(8)
            elif kind == 'table':
                _, headers, rows, widths = block
                widths = widths or [100 / len(headers)] * len(headers)
                t = doc.add_table(rows=1, cols=len(headers))
                t.alignment = WD_TABLE_ALIGNMENT.CENTER
                t.autofit = False
                for col, width in zip(t.columns, widths): col.width = Cm(17.7 * width / 100)
                repeat = OxmlElement('w:tblHeader')
                t.rows[0]._tr.get_or_add_trPr().append(repeat)
                for ri, row_data in enumerate([headers] + rows):
                    row = t.rows[0] if ri == 0 else t.add_row()
                    row._tr.get_or_add_trPr().append(OxmlElement('w:cantSplit'))
                    for ci, value in enumerate(row_data):
                        cell = row.cells[ci]
                        cell.width = Cm(17.7 * widths[ci] / 100)
                        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
                        cell.text = value
                        shade(cell._tc.get_or_add_tcPr(), '154E70' if ri == 0 else ('F0F5F8' if ri % 2 else 'FAFCFD'))
                        for para in cell.paragraphs:
                            para.paragraph_format.space_before = Pt(3)
                            para.paragraph_format.space_after = Pt(3)
                            para.paragraph_format.line_spacing = 1.0
                            for run in para.runs:
                                run.font.size = Pt(8.5 if len(headers) >= 4 else 9)
                                run.bold = ri == 0
                                if ri == 0: run.font.color.rgb = RGBColor(255,255,255)
                spacer = doc.add_paragraph()
                spacer.paragraph_format.space_after = Pt(1)
                spacer.paragraph_format.space_before = Pt(0)
                spacer.paragraph_format.line_spacing = Pt(3)
                spacer.runs or spacer.add_run('')
            elif kind == 'h':
                doc.add_paragraph(block[1], 'Heading 2')
            elif kind == 'screen':
                caption = doc.add_paragraph(block[1])
                caption.paragraph_format.space_after = Pt(3)
                caption.paragraph_format.keep_with_next = True
                caption.runs[0].bold = True
                caption.runs[0].font.size = Pt(9)
                para = doc.add_paragraph(block[2].ljust(16) + '\n' + block[3].ljust(16))
                para.paragraph_format.left_indent = Cm(0.5)
                para.paragraph_format.right_indent = Cm(8.7)
                para.paragraph_format.keep_together = True
                shade(para._p.get_or_add_pPr(), 'E4EECF')
                para.runs[0].font.name = 'Consolas'
                para.runs[0].font.size = Pt(12)
                para.runs[0].font.bold = True
            else:
                para = doc.add_paragraph(block[1])
                if kind == 'subtitle':
                    para.runs[0].font.size = Pt(15)
                    para.runs[0].bold = True
                elif kind == 'note':
                    shade(para._p.get_or_add_pPr(), 'FFF1DD')
                    para.paragraph_format.keep_together = True
                    para.runs[0].font.size = Pt(9)
    doc.save(path)


def export_word_pdf(docx_path, pdf_path):
    import win32com.client
    app = None
    document = None
    try:
        app = win32com.client.DispatchEx('Word.Application')
        app.Visible = False
        app.DisplayAlerts = 0
        app.AutomationSecurity = 3
        document = app.Documents.Open(str(docx_path), ReadOnly=True, AddToRecentFiles=False)
        document.Repaginate()
        print('Word pages:', document.ComputeStatistics(2), flush=True)
        document.ExportAsFixedFormat(str(pdf_path), 17)
    finally:
        if document is not None: document.Close(0)
        if app is not None: app.Quit()


def validate(docx_path, pdf_path):
    normal = lambda s: re.sub(r'\s+', '', s).replace('\u00ad', '').replace('\u200b','')
    with ZipFile(docx_path) as z:
        assert z.testzip() is None
        xml = ElementTree.fromstring(z.read('word/document.xml'))
        word = normal(''.join(e.text or '' for e in xml.iter(qn('w:t'))))
    count = 0
    with fitz.open(pdf_path) as pdf:
        assert len(pdf) == len(PAGES), f'예상 {len(PAGES)}쪽 / 실제 {len(pdf)}쪽: 레이아웃 조정 필요'
        for i, (title, blocks) in enumerate(PAGES):
            expected = [title]
            for block in blocks:
                if block[0] == 'table':
                    expected += block[1] + [v for row in block[2] for v in row]
                elif block[0] == 'screen': expected += list(block[1:])
                else: expected.append(block[1])
            text = normal(pdf[i].get_text())
            for value in expected:
                assert normal(value) in word, f'Word 누락: {value[:50]}'
                assert normal(value) in text, f'PDF {i+1}쪽 누락: {value[:50]}'
                count += 1
    return count


def make_markdown(path):
    result = []
    for title, blocks in PAGES:
        result += ['# ' + title, '']
        for block in blocks:
            if block[0] == 'table':
                _, headers, rows, _ = block
                result.append('| ' + ' | '.join(headers) + ' |')
                result.append('| ' + ' | '.join('---' for _ in headers) + ' |')
                result += ['| ' + ' | '.join(v.replace('\n', '<br>') for v in row) + ' |' for row in rows]
            elif block[0] == 'h': result.append('## ' + block[1])
            elif block[0] == 'screen': result += [block[1], '', '```text', block[2], block[3], '```']
            elif block[0] == 'note': result.append('> ' + block[1])
            elif block[0] == 'panel': result.append('[Word 원본의 패널 이미지] ' + block[1])
            else: result.append(block[1])
            result.append('')
        result += ['---', '']
    path.write_text('\n'.join(result), encoding='utf-8')


def make_review():
    content = '''# 사용자 매뉴얼 v5.0-KR 작성·검토 메모

- 작성일: 2026-10-16
- 요청한 `manual_csf_fet_lcd_panel_v1.docx`와 정확히 같은 이름은 없었음.
- 가장 가까운 `docs/MANUAL_CSF_JET_LCD_Panel_v1.docx`(v4.1-KR)를 기준으로 작성. 원본은 보존.
- 사용자 배포 파일은 `docs/distribution/CSF_JET_LCD_User_Manual_KR_v5.0.docx` 및 `.pdf`.
- 소스 변경·장비 다운로드·하드웨어 시험은 수행하지 않음.

## 원본에서 정정한 주요 내용

| 항목 | 현재 소스 근거 | 문서 반영 |
| --- | --- | --- |
| CH10 기본 주파수 | include/params.h, FREQ_CH9_DEFAULT=23000 | 2300 kHz |
| SET 길게 복귀 | BTN_LONG_PRESS_MS=1500 + SET_EXIT_HOLD_EXTRA_MS=1500 | 약 3초 |
| Auto Tuning | board_runtime_config.h, AUTO_TUNE_ENABLED=0 | 비활성. 남아 있는 LCD 안내 문구 설명 |
| SENSOR 인터록 | params.h, SENSOR_RUN_INTERLOCK_ENABLED=0; IsSensorInputOk | 현재 자동 차단 없음 |
| LOW/HIGH 알람 | menu.c, MENU_LOW_ALARM_ENABLED=0 / MENU_HIGH_ALARM_ENABLED=0 | 설정값·전압 환산과 보호 정지를 구분 |
| 알람 릴레이 | gpio_init.c에 초기화, src/에서 운전 상태 연동 쓰기 미발견 | 핀 명칭만 참고, 접점 동작 보장 제외 |
| TX RX LED | menu_screen.c, UpdateLEDs에서 RX 비트 설정 없음 | 통신 판단 수단으로 사용하지 않음 |
| RUN 표시 | menu_screen.c, RenderSelect | STOP의 0000kHz, RUN I/V·추정 W, 주파수 조정 표시 |
| RUN 중 SET | StoreRunTunePointBySet, SaveStoreNow | 조정 지점 저장 및 일시 출력 정지 명시 |
| POWER 값 | MapPowerToSelectedVoltage01V | 실제 출력 보증과 구분. 프로파일 범위가 목표 전압에 영향 |
| REMOTE 시작 | ServiceRemoteRunRequest | OFF를 먼저 확인한 뒤 ON, 외부 ON 유지 시 재시작 주의 |
| 보호 표시 | ErrorText, RenderSelect | 과전류·전류센서·시작전압·Buck 과전압 반영 |
| TERM | SaveRs485EditBuffer | 설정 보관만 수행, 저항 전환 없음 |

## 장비 공급자가 확인해야 할 실제 미검증 사항

1. 현재 SENSOR/LOW/HIGH 보호가 비활성인 시운전 설정이다. 일반 출고 적용 가능 여부는 펌웨어·장비 책임자가 판단해야 한다. 문서 작성으로 보호가 활성화되지는 않는다.
2. `docs/SAFETY.md`의 1.00 W 제한과 현재 프로파일 기본값·상한(최대 2.50 W 수준)이 다르다. 매뉴얼에는 소프트웨어 설정 범위와 장비 승인 정격을 구분했다. 승인 출력 조건은 별도 확정해야 한다.
3. 25pin D-SUB 핀 번호는 참고 Word의 표에서 가져왔다. 현재 MCU 핀 정의만으로 외부 하네스 번호·접점 정격·극성·핀 관찰 방향은 검증할 수 없다. 출고 하네스와 대조 필요.
4. 알람/GO/OPERATE 접점의 실제 하드웨어 상태·Fail-safe 동작은 미검증이며 소프트웨어 연동도 현재 없다. 정상 시 SHORT라는 과거 보증 설명은 제거했다.
5. POWER 편집 경계는 LOW/HIGH 상호 보정에 의해 0.01 W 차이가 생길 수 있다. 일반 0.05 W 변화 단위와 경계 보정을 구분했다.
6. RS-485 타이머 분주값과 시스템 클럭의 불일치는 별도 통신 문서에 기재되어 있다. 실장비 통신 호환성 시험 필요.
7. 실제 출력·센서 계측·전원 재투입·알람 해제·REMOTE 재시작 및 RUN 중 SET의 출력 중단은 실제 보드에서 확인해야 한다.

## 문서 검증

- Word에서 직접 PDF로 변환하여 같은 페이지 배치 유지.
- 전체 페이지의 본문과 표 셀을 Word XML 및 해당 PDF 페이지의 추출 텍스트와 비교.
- 패널 이미지는 참고 Word에서 원본 바이트 그대로 재사용.
- 소스·참고 Word·생성 파일 SHA-256은 사용자 매뉴얼 전용 manifest JSON에 기록.
'''
    (ROOT / 'docs/manual_release_review_2026-10-16.md').write_text(content, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--word-pdf', action='store_true')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    docx_path, pdf_path = OUT / (BASE + '.docx'), OUT / (BASE + '.pdf')
    make_docx(docx_path)
    make_markdown(OUT / (BASE + '.md'))
    make_review()
    if args.word_pdf:
        export_word_pdf(docx_path, pdf_path)
        checked = validate(docx_path, pdf_path)
        manifest = {
            'document': BASE, 'date': DATE, 'pages': len(PAGES),
            'reference_file': str(REFERENCE.relative_to(ROOT)),
            'reference_sha256': hashlib.sha256(REFERENCE.read_bytes()).hexdigest(),
            'source_sha256': HASHES, 'text_blocks_checked': checked,
            'renderer': 'Microsoft Word', 'hardware_tested': False,
            'artifact_sha256': {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in [docx_path, pdf_path]},
        }
        (OUT / (BASE + '_manifest.json')).write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({'pages': len(PAGES), 'text_blocks_checked': checked,
                          'docx': str(docx_path), 'pdf': str(pdf_path)}, ensure_ascii=False, indent=2))
    else:
        print('DOCX/Markdown 생성 완료. PDF 생성·검증은 --word-pdf 사용.')


if __name__ == '__main__':
    main()
