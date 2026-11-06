"""현재 펌웨어 기준 RS-485 설명서 생성: python docs/generate_rs485_protocol.py

필요 패키지: python-docx, PyMuPDF. PDF에는 Windows 맑은 고딕을 포함한다.
Microsoft Word와 pywin32가 있으면 --word-pdf로 Word와 같은 배치의 PDF를 생성한다.
코드 변경 시 아래 본문도 재검토해야 하며, 자동 실행만으로 사양이 갱신되지는 않는다.
"""
from __future__ import annotations

import hashlib
import html
import io
import json
import re
import argparse
from pathlib import Path

import fitz
from docx import Document
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'docs' / 'distribution'
BASE = 'Megasonic_RS485_Protocol_KR_v1.0'
DATE = '2026-10-16'
SOURCES = [
    'include/modbus_regs.h', 'include/modbus_rtu.h', 'include/params.h',
    'include/board_runtime_config.h', 'include/config.h', 'include/menu.h',
    'src/modbus_regs.c', 'src/modbus_rtu.c', 'src/modbus_crc.c',
    'src/megasonic_ctrl.c', 'src/menu.c', 'src/menu_screen.c',
    'src/adc_control.c', 'src/settings_store.c', 'src/system_clock.c',
    'src/stm32g4xx_it.c', 'src/main.c',
]
HASHES = {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in SOURCES}
SNAPSHOT = hashlib.sha256(json.dumps(HASHES, sort_keys=True).encode()).hexdigest()


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


CRC_SOURCE = (ROOT / 'src/modbus_crc.c').read_text(encoding='utf-8-sig')
CRC_TABLE = [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', CRC_SOURCE.split('CRC_TABLE[256] = {')[1].split('};')[0])]
assert len(CRC_TABLE) == 256


def firmware_crc(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc = (crc >> 8) ^ CRC_TABLE[(crc ^ value) & 0xFF]
    return crc


EXAMPLES: list[dict] = []


def frame(payload: str) -> str:
    data = bytes.fromhex(payload)
    crc = crc16(data)
    assert crc == firmware_crc(data)
    full = data + crc.to_bytes(2, 'little')
    assert firmware_crc(full) == 0
    result = full.hex(' ').upper()
    EXAMPLES.append({'payload': payload, 'frame': result, 'crc_verified': True})
    return result


def p(text): return ('p', text)
def h(text): return ('h', text)
def note(text): return ('note', text)
def code(text): return ('code', text)
def table(headers, rows, widths=None): return ('table', headers, rows, widths)
def address(value): return f'0x{value:04X}\n({value})'


BASIC_ROWS = [
    [address(0), 'R/W', '출력 RUN/STOP', '0=정지, 1=시작', '읽기: 운전 상태. 시작은 EXT에서만 허용.'],
    [address(1), 'R/W', '목표 주파수', '3000~23000\n×0.1 kHz', '4000=400.0 kHz. 채널 번호가 아님.'],
    [address(2), 'R/W', 'Buck 제어 목표값', '0~1000\n×0.1%', '901~1000 입력은 900으로 제한. 게이트 PWM 듀티가 아님.'],
    [address(3), 'R', '주파수 읽기', '×0.1 kHz', '0x0001과 같은 목표값. 실측 주파수가 아님.'],
    [address(4), 'R', '현재 Buck 제어값', '×0.1%', '소프트 스타트·추종 중의 current_duty.'],
    [address(5), 'R', '상태 플래그', '비트 필드', 'bit0=RUN, bit1=소프트 스타트.'],
    [address(6), 'R/W', '제어부 운전 모드', '0=NORMAL\n1=REMOTE, 2=EXT', '패널 선택 모드·저장값과 자동 동기화되지 않음.'],
    [address(7), 'R/W', '펄스 ON 시간', '100~60000 ms', '호환용 보관값. 현재 펄스 동작 미구현.'],
    [address(8), 'R/W', '펄스 OFF 시간', '100~60000 ms', '호환용 보관값. 현재 펄스 동작 미구현.'],
    [address(9), 'R/W', '스윕 시작 주파수', '3000~23000\n×0.1 kHz', '호환용 보관값. 현재 스윕 동작 미구현.'],
    [address(10), 'R/W', '스윕 종료 주파수', '3000~23000\n×0.1 kHz', '시작값과 종료값의 대소 관계 검사 없음.'],
    [address(11), 'R/W', '스윕 시간', '500~60000 ms', '호환용 보관값. 현재 스윕 동작 미구현.'],
]
COMM_ROWS = [
    [address(16), 'R/W', '슬레이브 주소', '1~247', '쓰기 직후 새 주소 적용.'],
    [address(17), 'R/W', '통신 속도 인덱스', '0~3', '0=9600, 1=19200, 2=38400, 3=115200 bps'],
    [address(18), 'R/W', '패리티', '0~2', '0=None, 1=Even, 2=Odd. Odd 저장 제한 참조.'],
    [address(19), 'R', '전류 ADC raw', '0~4095', 'ADC2_IN3 필터링 결과.'],
    [address(20), 'R', '전류 센서 핀 전압', '×0.1 mV', '값 1000=100.0 mV. 버스 전압이 아님.'],
    [address(21), 'R', '환산 전류', 'mA', '가장 가까운 mA로 반올림, 최대 65535.'],
    [address(22), 'R', '전압 ADC raw', '0~4095', 'ADC2_IN4 필터링 결과. 전압[V]이 아님.'],
    [address(23), 'R', '전류·전압 기반 추정 전력', '0~999\n×0.01 W', '값 50=0.50 W. 영점 보정·무신호 처리 포함.'],
    [address(24), 'R', '순방향 ADC raw', '0~4095', 'ADC1_IN1. W 단위로 변환된 값이 아님.'],
    [address(25), 'R', '반사 ADC raw', '0~4095', 'ADC1_IN2. W 단위로 변환된 값이 아님.'],
    [address(255), 'W', 'MCU 재시작', '0x1234', '정지 상태에서 즉시 리셋. 정상 응답 보장 없음.'],
]

PAGES = [
('RS-485 통신 프로토콜', [
    ('subtitle', '메가소닉 메인보드 · Modbus RTU 사용 설명서'),
    p('PC·PLC·HMI 연동 담당자를 위한 현재 펌웨어 구현 기준 문서'),
    table(['문서 정보', '내용'], [
        ['대상', 'ms_main_board_st / STM32G474 메가소닉 메인보드'],
        ['문서 버전 / 작성일', f'v1.0 / {DATE}'],
        ['펌웨어 버전 매크로', '1.0.0 (동일 버전명이라도 코드가 다를 수 있음)'],
        ['코드 식별자', SNAPSHOT[:16]],
        ['적용 기준', '작성일 현재 로컬 src/·include/의 저장된 소스'],
    ], [32, 68]),
    h('빠른 설정'),
    table(['항목', '설정'], [
        ['통신 방식', 'RS-485 2선식 반이중 / Modbus RTU 슬레이브'],
        ['기본 통신', '주소 1 / 9600 bps / 데이터 8비트 / 패리티 None / 정지 1비트'],
        ['지원 기능', 'FC03 읽기, FC06 단일 쓰기, FC10 다중 쓰기'],
        ['첫 연결 시험', 'FC03 / 시작 주소 0x0000(십진수 0) / 수량 6'],
        ['정지 명령', 'FC06 / 0x0000에 0 쓰기'],
        ['시작 명령', '패널에서 EXT 선택·저장 후 FC06 / 0x0000에 1 쓰기'],
    ], [32, 68]),
    note('저장된 통신 설정이 있으면 기본값보다 우선합니다. 연결 전 패널에서 실제 주소·속도·패리티를 확인하세요.'),
    h('문서 범위'),
    p('레지스터 주소, 수치 범위, 접근 제한, 응답 및 예제 CRC를 현재 코드와 대조했습니다. 실제 보드의 통신 파형·응답 시간·출력 동작 검증 결과를 의미하지 않습니다.'),
    p('기존 Modbus_레지스터맵.md에는 현재 구현과 다른 주소·기능이 있습니다. 이 문서의 주소표를 사용하고, 펌웨어 변경 시 문서도 함께 갱신하세요.'),
    p('구성: 연결 → 프레임 → 레지스터 → 운전 → 예제 → 제한·문제 해결 → 적용 근거'),
]),
('1. 연결과 최초 설정', [
    h('배선 및 패널 설정'),
    p('PC의 USB–RS-485 변환기 또는 PLC의 RS-485 포트를 보드의 RS-485 A/B 단자에 연결합니다. A/B의 극성 명칭은 장치마다 다를 수 있으므로 양쪽 제품의 신호 정의를 맞추세요. 커넥터 핀 번호와 신호 기준선 연결은 해당 보드 회로도·배선도를 따릅니다.'),
    code('PC / PLC (마스터)  ── RS-485 A/B ──  메가소닉 보드 (슬레이브)'),
    p('PA2/PA3는 MCU와 트랜시버 사이의 UART 신호이며 외부 A/B 단자가 아닙니다. 종단 저항은 버스 양 끝 장치의 실제 하드웨어 구성으로 확인합니다. 패널 TERM ON/OFF는 현재 설정 기록만 수행하며 저항을 전기적으로 전환하지 않습니다.'),
    table(['순서', '조작'], [
        ['1', '출력을 정지하고 패널 MODE에서 EXT를 선택한 뒤 SET으로 저장합니다.'],
        ['2', '정지 화면에서 MODE를 길게 눌러 SETTING MODE로 진입하고 3.EXT:RS485를 선택합니다.'],
        ['3', 'MODE로 BAUD → ADDR → TERM → PARITY 항목을 이동하고 UP/DOWN으로 수정합니다.'],
        ['4', 'SET으로 저장한 뒤 설정 화면에서 나옵니다. PC/PLC도 같은 조건으로 맞춥니다.'],
    ], [12, 88]),
    note('패널에서 BAUD·ADDR·PARITY를 변경하면 통신에 즉시 반영됩니다. 설정 도중 마스터의 접속이 끊길 수 있습니다.'),
    h('지원 통신 조건'),
    table(['항목', '현재 구현'], [
        ['주소', '1~247. 주소 0 브로드캐스트는 처리하지 않음.'],
        ['통신 속도', '9600 / 19200 / 38400 / 115200 bps'],
        ['데이터 / 정지 비트', '데이터 8비트, 정지 1비트 고정'],
        ['패리티', '패널: None 또는 Even. 레지스터: None / Even / Odd'],
        ['흐름 제어', '없음. 송수신 방향은 보드가 자동 전환.'],
    ], [30, 70]),
    h('첫 읽기 시험'),
    p('마스터에서 Holding Registers(FC03), 시작 주소 0, 수량 6을 선택합니다. 연결 시에는 출력 시작 명령 없이 읽기만 시험하세요. 예제 요청은 다음과 같습니다.'),
    code(frame('01 03 00 00 00 06')),
    p('응답의 데이터 12바이트는 순서대로 RUN, 목표 주파수, Buck 목표값, 주파수 읽기, 현재 Buck 제어값, 상태 플래그입니다. 값의 의미는 3장을 참조하세요.'),
]),
('2. 프레임과 통신 처리', [
    h('바이트 형식'),
    code('[주소 1B] [기능 1B] [데이터 N B] [CRC Low 1B] [CRC High 1B]'),
    p('모든 레지스터는 unsigned 16-bit입니다. 레지스터 주소·수량·값은 상위 바이트부터 보내고, CRC만 하위 바이트부터 보냅니다. 예제의 01 03 등은 16진수 바이트이며 ASCII 문자열이 아닙니다.'),
    p('CRC-16/Modbus: 초기값 0xFFFF, 반전 다항식 0xA001. CRC는 주소부터 데이터 마지막 바이트까지 계산합니다.'),
    table(['기능', '요청 데이터(CRC 앞)', '정상 응답 데이터(CRC 앞)'], [
        ['03 읽기', '시작 주소(2B) + 수량(2B)', '바이트 수(1B) + 값(2B × 수량)'],
        ['06 단일 쓰기', '주소(2B) + 값(2B)', '요청 8바이트 전체를 그대로 에코'],
        ['10 다중 쓰기', '시작 주소(2B) + 수량(2B) + 바이트 수(1B) + 값들', '시작 주소(2B) + 수량(2B)'],
    ], [19, 43, 38]),
    h('요청 크기와 주소 범위'),
    p('FC03은 수량 1~61을 검사하지만 모든 주소가 실제로 존재해야 합니다. 읽기 가능한 연속 구간은 0x0000~0x000B(12개), 0x0010~0x0019(10개)입니다. 0x000C~0x000F를 가로지르는 요청은 실패합니다.'),
    p('FC10은 RX 버퍼 128바이트 안에서 사용해야 하므로 전송 수량 상한은 59개(9+2×59=127바이트)입니다. 이는 주소 59개가 지원된다는 뜻이 아닙니다. 실제 쓰기 가능 구간과 운전 조건을 모두 만족해야 합니다.'),
    note('FC10은 앞 레지스터부터 순서대로 적용합니다. 뒤에서 오류가 나도 앞에서 변경된 값은 되돌리지 않습니다. 시작·정지·통신 설정·리셋은 FC06으로 각각 전송하세요.'),
    h('마스터 운용 시작값'),
    table(['항목', '연동 시험용 권장 시작값'], [
        ['요청 간격', '한 번에 한 요청만 전송. 응답 완료 후 최소 10 ms 유휴.'],
        ['상태 폴링', '200 ms 간격부터 시작하여 실측 후 조정.'],
        ['읽기 응답 대기', '1000 ms부터 시작. 최대 응답 시간 보장값은 아님.'],
        ['START 응답 대기', '5000 ms부터 시작. 선충전 대기(최대 2500 ms)·설정 저장 등이 포함됨.'],
    ], [30, 70]),
    p('프레임의 바이트는 중간 지연 없이 한 번에 전송하세요. START 응답이 늦어도 다른 요청을 겹쳐 보내지 마세요. 응답 누락 시 통신을 복구하고 RUN 상태를 읽어 결과를 확인합니다. 현재 타이머 제한은 8장에 설명합니다.'),
]),
('3. 운전 레지스터', [
    p('주소는 실제 프레임에 넣는 0 기반 주소입니다. 괄호는 십진수입니다. R=FC03 읽기, W=FC06/FC10 쓰기. W는 아래 표와 별개로 운전 중 제한을 받습니다.'),
    table(['주소\n(십진수)', '권한', '항목', '값 / 단위', '설명'], BASIC_ROWS, [14, 9, 22, 24, 31]),
    note('운전 중에는 0x0000=0(정지)만 쓸 수 있습니다. 운전 중 0x0000=1을 다시 보내도 예외 0x02가 반환됩니다. 읽기는 운전 모드와 무관하게 허용됩니다.'),
    h('주소 표기와 단위'),
    p('40001 형식을 사용하는 마스터라면 표시 번호=40001+실제 주소입니다. 예: 0x0000 → 40001, 0x0006 → 40007, 0x0010 → 40017. 통신 프레임에는 40001을 그대로 넣지 않습니다. 도구가 0 기반인지 1 기반인지 확인하세요.'),
    p('주파수 4000은 400.0 kHz, Buck 제어값 450은 45.0%입니다. Buck 값은 전력[W]이나 실제 FET 게이트 듀티가 아닙니다. 0x0001·0x0002의 값은 START/STOP에서 패널 프로파일로 다시 설정될 수 있습니다.'),
]),
('4. 통신·계측 레지스터', [
    table(['주소\n(십진수)', '권한', '항목', '값 / 단위', '설명'], COMM_ROWS, [14, 9, 24, 19, 34]),
    h('계측값 읽는 방법'),
    p('0x0013부터 7개를 읽으면 전류 raw·센서 핀 전압·전류 mA·전압 raw·추정 전력·순방향 raw·반사 raw를 한 번에 얻습니다. 0x0017은 펌웨어 버전이 아니라 추정 전력입니다.'),
    p('0x0017은 전류 영점 보정, 무신호 판정, 작은 전류 무시, 9.99 W 상한 처리를 포함합니다. 순방향/반사 RF 전력의 교정된 측정값이나 실제 초음파 출력 보증값으로 해석하지 마세요.'),
    h('현재 제공하지 않는 항목'),
    p('전력[W] 목표값 쓰기, 주파수/출력 채널 선택, LC 릴레이 선택, 알람 상세 코드·리셋, 실제 게이트 듀티, 펌웨어 버전, 통신 종단 제어, 명시적 Flash 저장 명령은 현재 레지스터 맵에 없습니다.'),
    p('0x00FF는 읽기 불가입니다. 이 표와 3장에 없는 주소는 사용하지 마세요.'),
]),
('5. 운전 순서와 저장 동작', [
    h('권장 연동 순서'),
    table(['단계', '동작'], [
        ['1. 패널 준비', '정지 상태에서 EXT 모드와 사용할 주파수·출력 프로파일을 선택하고 저장합니다. 설정 화면에서 나옵니다.'],
        ['2. 통신 확인', 'FC03으로 0x0000(RUN), 0x0005(상태), 0x0006(모드=2)를 확인합니다.'],
        ['3. 출력 시작', 'FC06으로 0x0000=1 전송. 정상 응답 뒤 RUN과 소프트 스타트 상태를 읽습니다.'],
        ['4. 운전 감시', 'RUN·상태·계측값을 폴링합니다. RUN=0이면 LCD의 알람·운전 상태도 확인합니다.'],
        ['5. 출력 정지', 'FC06으로 0x0000=0 전송. FC03으로 RUN=0을 확인합니다.'],
    ], [22, 78]),
    p('START는 알람, SENSOR 입력, 전류 센서 상태, 설정 저장, Buck 선충전 등 내부 조건을 통과해야 성공합니다. 성공 응답은 그 시점의 시작 처리를 뜻하며 이후 보호 정지까지 보장하지 않습니다.'),
    h('접근 제한'),
    table(['상태 / 명령', '현재 코드 동작'], [
        ['읽기', '모든 모드에서 허용.'],
        ['정지 상태의 설정 쓰기', 'NORMAL·REMOTE·EXT 모두에서 주소·값이 유효하면 허용.'],
        ['출력 시작(0x0000=1)', '정지 상태이고 제어부 모드가 EXT일 때만 허용.'],
        ['운전 중 쓰기', '0x0000=0만 허용. 나머지는 예외 0x02.'],
    ], [38, 62]),
    note('0x0006 쓰기는 제어부 모드만 바꾸며 패널의 선택 모드와 저장값을 함께 바꾸지 않습니다. REMOTE 접점 동작과 충돌하지 않도록 실제 운전 모드는 패널에서 EXT로 설정하세요.'),
    h('설정 유지 및 전원 재투입'),
    p('Modbus 쓰기 자체는 Flash 저장을 요청하지 않습니다. 통신 주소·속도·패리티는 나중에 패널 저장이나 START 전 저장 경로에 포함될 수 있으므로, 항상 휘발성 또는 항상 영구 저장이라고 가정하면 안 됩니다. 영구 통신 설정은 패널에서 설정·저장 후 전원을 재투입하여 확인하세요.'),
    p('직접 쓴 주파수·Buck 목표값은 패널 프로파일을 편집하지 않습니다. START에서 선택된 출력 프로파일을 적용하면서 두 값이 다시 설정되고, STOP에서도 선택된 주파수·출력 프로파일이 적용됩니다. 통신만으로 임의 주파수·전력을 지정해 운전하는 용도로 사용하지 마세요.'),
]),
('6. 정상 요청·응답 예제', [
    p('모든 예제는 슬레이브 주소 1 기준이며 CRC를 포함합니다. 실제 설정과 다르면 주소를 바꾸고 CRC를 다시 계산해야 합니다.'),
    h('6.1 상태 플래그 1개 읽기'),
    code('요청: ' + frame('01 03 00 05 00 01') + '\n응답 예: ' + frame('01 03 02 00 00')),
    p('응답 예는 정지·소프트 스타트 아님(0x0000)입니다. 상태값 0x0001=RUN, 0x0003=RUN+소프트 스타트. bit2~15는 현재 0이며 알람 상세나 통신 활성 상태로 사용할 수 없습니다.'),
    h('6.2 통신 설정 3개 읽기'),
    code('요청: ' + frame('01 03 00 10 00 03') + '\n기본 설정 응답: ' + frame('01 03 06 00 01 00 00 00 00')),
    p('데이터 0001 / 0000 / 0000은 주소 1 / 속도 인덱스 0(9600) / 패리티 0(None)을 뜻합니다.'),
    h('6.3 계측값 7개 읽기'),
    code('요청: ' + frame('01 03 00 13 00 07')),
    p('응답: 01 03 0E 뒤에 레지스터 값 7개(14바이트), 마지막에 CRC 2바이트가 옵니다. 예를 들어 0x0015의 값 00 7B는 123 mA, 0x0017의 값 00 32는 0.50 W입니다.'),
    h('6.4 출력 정지'),
    code('요청 및 정상 응답: ' + frame('01 06 00 00 00 00')),
    p('모든 모드에서 허용됩니다. 통신 프레임이 처리될 때 정지하므로 통신 불능 시에는 실행할 수 없습니다. REMOTE에서 외부 RUN 접점이 활성 상태이면 정지 후 다시 시작될 수 있습니다. 정지 후 RUN을 다시 읽으세요.'),
    h('6.5 출력 시작'),
    code('요청 및 정상 응답: ' + frame('01 06 00 00 00 01')),
    p('전제: 패널 EXT 선택·저장, 정지 상태, 유효한 프로파일과 시작 조건. 조건을 만족하지 않으면 예외 0x02입니다. 응답 대기 중 START를 반복 전송하지 마세요.'),
]),
('7. 설정 쓰기와 예외 예제', [
    h('7.1 단일 레지스터 쓰기 형식'),
    code('목표 주파수 400.0 kHz 요청/응답: ' + frame('01 06 00 01 0F A0')),
    p('정지 상태에서만 허용됩니다. 0x0FA0=4000입니다. 이는 레지스터 형식 예제이며 다음 START에서 주파수가 유지된다는 뜻은 아닙니다. START는 패널 프로파일을 다시 적용합니다.'),
    h('7.2 여러 레지스터 쓰기 형식'),
    code('요청: ' + frame('01 10 00 07 00 02 04 03 E8 03 E8') + '\n응답: ' + frame('01 10 00 07 00 02')),
    p('0x0007·0x0008에 각각 1000 ms를 저장하는 형식 예제입니다. 정지 상태에서만 허용되며 현재 펄스 출력 기능은 실행되지 않습니다.'),
    h('7.3 예외 응답'),
    code('[주소] [요청 기능 OR 0x80] [예외 코드] [CRC Low] [CRC High]'),
    table(['코드', '현재 반환 조건'], [
        ['0x01', '미지원 기능. 예: FC04, FC05 등.'],
        ['0x02', '잘못된 읽기 주소 또는 쓰기 실패. 범위 오류·읽기 전용 주소·운전 중 제한·시작 거부도 포함.'],
        ['0x03', '프레임 최소 길이 부족, FC03 수량 0 또는 61 초과, FC10 수량/바이트 수/길이 오류.'],
    ], [13, 87]),
    table(['예', '요청 → 응답 (CRC 포함)'], [
        ['없는 주소 읽기', frame('01 03 00 0C 00 01') + '\n→ ' + frame('01 83 02')],
        ['범위 초과 쓰기\nBuck 값 1001', frame('01 06 00 02 03 E9') + '\n→ ' + frame('01 86 02')],
        ['수량 0 읽기', frame('01 03 00 00 00 00') + '\n→ ' + frame('01 83 03')],
    ], [27, 73]),
    p('CRC 불일치, 다른 슬레이브 주소, 주소 0, 4바이트 미만 프레임은 응답 없이 무시합니다. 예외 0x02만으로 알람 원인을 구분할 수 없으므로 LCD와 RUN 상태를 함께 확인하세요.'),
]),
('8. 현재 구현의 제한과 문제 해결', [
    h('통신 설정 변경 시 주의'),
    p('주소(0x0010)는 쓰기 처리 중 즉시 변경됩니다. FC06 정상 응답은 요청을 복사하므로 이전 주소로 돌아오고 다음 요청부터 새 주소를 사용합니다. 속도(0x0011)와 패리티(0x0012)는 정상 응답을 보내기 전에 UART가 변경되어 마스터가 응답을 놓칠 수 있습니다. 한 항목씩 변경하고 새 설정으로 재접속하여 읽어 확인하세요.'),
    p('Odd(2)는 UART에서 즉시 사용할 수 있지만 패널의 정규화·저장·복원 경로에서는 None(0)으로 바뀝니다. 지속 사용은 패널이 지원하는 None 또는 Even으로 맞추세요. 재시작(0x00FF=0x1234)은 응답을 보내기 전에 리셋되며 전원 투입 때처럼 저장된 설정을 복원합니다.'),
    h('프레임 타이밍 제한'),
    p('현재 시스템은 170 MHz인데 TIM4 분주값은 72 MHz를 전제로 한 71로 고정되어 있습니다. 따라서 코드가 의도한 3.5문자 유휴시간과 실제 타임아웃이 일치하지 않습니다. 연속된 바이트 전송을 사용하고 프레임 중간에 지연을 넣지 마세요. 정확한 프레임 경계·최대 응답 시간·통신 호환성은 보드에서 실측해야 합니다.'),
    p('통신 단절 자동 정지 타이머는 현재 Modbus 경로에 없습니다. RUN=0과 실제 알람 종류는 별개이고, 상태 bit2도 알람 표시로 구현되어 있지 않습니다. 통신 연결 상태나 알람 유무를 예약 비트로 판단하지 마세요.'),
    h('증상별 확인'),
    table(['증상', '확인할 내용'], [
        ['응답 없음', '주소·속도·패리티·정지 1비트, A/B 극성, 실제 종단 배치, CRC, 단일 마스터 여부.'],
        ['통신이 간헐적으로 끊김', '프레임을 한 번에 전송하는지, 요청이 겹치는지, 바이트 사이 지연·타이머 영향이 있는지 확인.'],
        ['읽기 예외 0x02', '미정의 주소 0x000C~0x000F 또는 0x00FF를 읽는지 확인.'],
        ['쓰기 예외 0x02', 'RUN 여부, 쓰기 가능 주소와 범위, START의 EXT·센서·알람 조건 확인.'],
        ['START 후 설정값이 달라짐', '패널 프로파일 재적용 동작. 0x0001·0x0002 직접 쓰기는 프로파일 편집이 아님.'],
        ['재부팅 후 통신 조건이 달라짐', 'Modbus 쓰기의 저장 보장 없음, Odd→None 정규화, 패널 저장 상태 확인.'],
    ], [32, 68]),
    note('현재 구현의 제한을 문서화한 것이며 펌웨어를 변경한 것은 아닙니다. 현장 연동은 읽기 시험 → 정지 시험 → 준비된 프로파일로 시작 시험 순서로 확인하세요.'),
]),
('9. 적용 근거와 배포 관리', [
    h('검토 기준'),
    table(['근거 파일', '확인한 내용'], [
        ['include/modbus_regs.h\nsrc/modbus_regs.c', '주소 전체, 읽기 의미, 쓰기 범위, 운전 중 제한, 시작 모드 제한'],
        ['src/modbus_rtu.c\nsrc/modbus_crc.c', '기능 코드, UART 형식, 버퍼, 예외, 프레임 응답, CRC'],
        ['include/params.h\ninclude/board_runtime_config.h', '기본값, 주파수·듀티 범위, 통신 속도, 현재 기능 설정'],
        ['src/menu.c\nsrc/menu_screen.c', '패널 조작, 시작/정지, 프로파일 재적용, 통신 저장·패리티 정규화'],
        ['src/megasonic_ctrl.c\nsrc/adc_control.c', '상태 비트, Buck 제어값, 계측 단위와 전력 제한'],
        ['src/system_clock.c\nsrc/stm32g4xx_it.c', 'TIM4 클럭 전제와 수신 인터럽트 처리'],
    ], [47, 53]),
    h('기존 문서와 혼용하면 안 되는 항목'),
    p('기본값은 8N1이며 None에서 정지 2비트가 아닙니다. 주파수 레지스터는 채널 번호가 아닌 ×0.1 kHz입니다. 정지 중 설정 쓰기는 EXT 외 모드에서도 허용됩니다. 쓰기 값 범위 오류는 현재 예외 0x02이며, 상태 알람 비트·전력 설정·버전 읽기 등 과거 문서의 항목은 현재 제공되지 않습니다.'),
    h('검증 범위'),
    p('모든 표의 레지스터 주소를 헤더 정의와 대조했습니다. 모든 예제 CRC는 독립 비트 연산 방식과 실제 펌웨어 CRC 테이블 방식으로 교차 확인했습니다. Word와 PDF는 동일한 본문 데이터에서 생성합니다. 실제 장비의 송수신·전원 재투입·출력 운전 시험은 별도로 필요합니다.'),
    h('버전 식별'),
    p(f'문서: {BASE} / 작성일: {DATE} / 펌웨어 매크로: 1.0.0'),
    p('소스 집합 SHA-256: ' + SNAPSHOT),
    p('동봉된 source_manifest.json에는 검토한 소스별 SHA-256과 CRC 확인 결과가 있습니다. 소스가 변경되면 기존 문서의 동작 설명을 재검토한 뒤 문서 버전을 올려 배포하세요.'),
    p('배포 파일: .docx(편집용), .pdf(열람용). .md는 본문 확인용이며 생성 스크립트는 docs/generate_rs485_protocol.py입니다.'),
]),
]


def verify_registers():
    header = (ROOT / 'include/modbus_regs.h').read_text(encoding='utf-8-sig')
    values = {int(v, 16) for v in re.findall(r'^#define REG_ADDR_\w+\s+(0x[\dA-Fa-f]+)', header, re.M)}
    documented = set(range(12)) | set(range(16, 26)) | {255}
    assert values == documented, (values, documented)
    assert crc16(b'123456789') == 0x4B37


def set_font(style, size, bold=False, color='20334A'):
    style.font.name = '맑은 고딕'
    style.font.size = Pt(size)
    style.font.bold = bold
    style.font.color.rgb = RGBColor.from_string(color)
    style.element.get_or_add_rPr().rFonts.set(qn('w:eastAsia'), '맑은 고딕')


def shade(cell, color):
    shd = OxmlElement('w:shd')
    shd.set(qn('w:fill'), color)
    cell._tc.get_or_add_tcPr().append(shd)


def make_docx(path):
    doc = Document()
    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21), Cm(29.7)
    sec.top_margin, sec.bottom_margin = Cm(1.8), Cm(1.7)
    sec.left_margin = sec.right_margin = Cm(1.65)
    sec.header_distance = sec.footer_distance = Cm(0.8)
    set_font(doc.styles['Normal'], 9)
    normal = doc.styles['Normal'].paragraph_format
    normal.space_after = Pt(6)
    normal.line_spacing = 1.1
    for name, size in [('Title', 27), ('Heading 1', 19), ('Heading 2', 11.5)]:
        set_font(doc.styles[name], size, True, '154E70')
        doc.styles[name].paragraph_format.space_before = Pt(8)
        doc.styles[name].paragraph_format.space_after = Pt(6)
    sec.header.paragraphs[0].text = 'MEGASONIC  /  RS-485 MODBUS RTU'
    for run in sec.header.paragraphs[0].runs:
        run.font.size = Pt(8)
        run.font.color.rgb = RGBColor.from_string('547083')
    footer = sec.footer.paragraphs[0]
    footer.text = f'프로토콜 v1.0  |  {DATE}                                          '
    for instruction in ['PAGE', 'NUMPAGES']:
        if instruction == 'NUMPAGES':
            footer.add_run(' / ')
        field = OxmlElement('w:fldSimple')
        field.set(qn('w:instr'), instruction)
        footer._p.append(field)
    for run in footer.runs:
        run.font.size = Pt(8)
    doc.core_properties.title = '메가소닉 RS-485 통신 프로토콜'
    doc.core_properties.subject = '현재 펌웨어 구현 기준 Modbus RTU 사용 설명서'
    doc.core_properties.author = '메가소닉 펌웨어 프로젝트'
    for page_no, (title, blocks) in enumerate(PAGES):
        if page_no:
            doc.add_page_break()
        doc.add_paragraph(title, 'Title' if page_no == 0 else 'Heading 1')
        for block in blocks:
            kind = block[0]
            if kind == 'table':
                _, headers, rows, widths = block
                t = doc.add_table(rows=1, cols=len(headers))
                t.alignment = WD_TABLE_ALIGNMENT.CENTER
                t.autofit = False
                widths = widths or [100 / len(headers)] * len(headers)
                for col, width in zip(t.columns, widths):
                    col.width = Cm(17.7 * width / 100)
                repeat = OxmlElement('w:tblHeader')
                t.rows[0]._tr.get_or_add_trPr().append(repeat)
                for ri, data in enumerate([headers] + rows):
                    row = t.rows[0] if ri == 0 else t.add_row()
                    cant_split = OxmlElement('w:cantSplit')
                    row._tr.get_or_add_trPr().append(cant_split)
                    for ci, value in enumerate(data):
                        cell = row.cells[ci]
                        cell.width = Cm(17.7 * widths[ci] / 100)
                        cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
                        cell.text = value
                        shade(cell, '154E70' if ri == 0 else ('F0F5F8' if ri % 2 else 'FAFCFD'))
                        for para in cell.paragraphs:
                            para.paragraph_format.space_before = Pt(3)
                            para.paragraph_format.space_after = Pt(3)
                            para.paragraph_format.line_spacing = 1.0
                            for run in para.runs:
                                run.font.size = Pt(8 if len(headers) >= 5 else 8.5)
                                run.bold = ri == 0
                                if ri == 0:
                                    run.font.color.rgb = RGBColor(255, 255, 255)
                doc.add_paragraph().paragraph_format.space_after = Pt(0)
            elif kind == 'h':
                doc.add_paragraph(block[1], 'Heading 2')
            else:
                para = doc.add_paragraph(block[1])
                if kind == 'subtitle':
                    para.runs[0].bold = True
                    para.runs[0].font.size = Pt(13)
                if kind in ('note', 'code'):
                    shd = OxmlElement('w:shd')
                    shd.set(qn('w:fill'), 'FFF1DD' if kind == 'note' else 'EDF3F7')
                    para._p.get_or_add_pPr().append(shd)
                    para.paragraph_format.keep_together = True
                    if kind == 'code':
                        para.runs[0].font.name = 'Consolas'
                        para.runs[0].font.size = Pt(8.5)
    doc.save(path)


def page_html(title, blocks, cover=False):
    esc = lambda value: html.escape(value).replace('\n', '<br>')
    parts = [f'<h1 class="{"cover" if cover else ""}">{esc(title)}</h1>']
    for block in blocks:
        kind = block[0]
        if kind == 'table':
            _, headers, rows, widths = block
            widths = widths or [100 / len(headers)] * len(headers)
            parts.append('<table class="dense">' if len(headers) >= 5 else '<table>')
            parts.append('<tr>' + ''.join(f'<th style="width:{w}%">{esc(v)}</th>' for v, w in zip(headers, widths)) + '</tr>')
            for ri, row in enumerate(rows):
                parts.append(f'<tr class="r{ri % 2}">' + ''.join(f'<td>{esc(v)}</td>' for v in row) + '</tr>')
            parts.append('</table>')
        elif kind == 'h':
            parts.append(f'<h2>{esc(block[1])}</h2>')
        else:
            parts.append(f'<p class="{kind}">{esc(block[1])}</p>')
    return '\n'.join(parts)


CSS = '''
@font-face {font-family: Malgun; src: url(malgun.ttf);}
@font-face {font-family: Malgun; src: url(malgunbd.ttf); font-weight: bold;}
body {font-family: Malgun; font-size: 9pt; color: #20334A; line-height: 1.35; margin: 0;}
h1 {font-size: 19pt; color:#154E70; margin:0 0 13pt;}
h1.cover {font-size:29pt; margin-top:17pt;}
h2 {font-size:11.3pt; color:#154E70; margin:12pt 0 5pt;}
p {margin:0 0 8pt; overflow-wrap:break-word;}
p.subtitle {font-size:13pt; font-weight:bold; margin-bottom:14pt;}
p.note {background-color:#FFF1DD; padding:8pt; font-size:8.7pt;}
p.code {background-color:#EDF3F7; padding:8pt; font-size:8.4pt; line-height:1.45;}
table {border-collapse:collapse; width:100%; margin:6pt 0 10pt; font-size:8.5pt;}
th {background-color:#154E70; color:white; text-align:left; padding:6pt; font-weight:bold;}
td {padding:5pt 6pt; vertical-align:middle; border-bottom:0.5pt solid #D9E4EA;}
tr.r0 {background-color:#F0F5F8;}
tr.r1 {background-color:#FAFCFD;}
table.dense {font-size:8pt; line-height:1.28;}
table.dense td {padding:5pt 4pt;}
'''


def make_pdf(path):
    buffer = io.BytesIO()
    writer = fitz.DocumentWriter(buffer)
    archive = fitz.Archive('C:/Windows/Fonts')
    for index, (title, blocks) in enumerate(PAGES):
        story = fitz.Story(html=page_html(title, blocks, index == 0), user_css=CSS, archive=archive)
        dev = writer.begin_page(fitz.paper_rect('a4'))
        more, _ = story.place(fitz.Rect(44, 53, 551, 790))
        if more:
            raise RuntimeError(f'PDF {index + 1}쪽 내용 초과: {title}')
        story.draw(dev)
        writer.end_page()
    writer.close()
    pdf = fitz.open(stream=buffer.getvalue(), filetype='pdf')
    for index, page in enumerate(pdf):
        page.draw_line((44, 37), (551, 37), color=(0.08, 0.31, 0.44), width=0.6)
        page.insert_text((44, 29), 'MEGASONIC  /  RS-485 MODBUS RTU', fontsize=8, color=(0.30, 0.42, 0.50))
        page.draw_line((44, 802), (551, 802), color=(0.75, 0.82, 0.86), width=0.5)
        page.insert_text((44, 817), f'Protocol v1.0  |  {DATE}  |  Source {SNAPSHOT[:16]}', fontsize=8, color=(0.30, 0.42, 0.50))
        page.insert_text((512, 817), f'{index + 1} / {len(PAGES)}', fontsize=8, color=(0.30, 0.42, 0.50))
    pdf.subset_fonts()
    pdf.set_metadata({'title': '메가소닉 RS-485 통신 프로토콜 v1.0', 'author': '메가소닉 펌웨어 프로젝트', 'subject': '현재 구현 기준 Modbus RTU 사용 설명서'})
    pdf.save(path, garbage=4, deflate=True)
    pdf.close()


def make_markdown(path):
    result = []
    for title, blocks in PAGES:
        result += [f'# {title}', '']
        for block in blocks:
            kind = block[0]
            if kind == 'table':
                _, headers, rows, _ = block
                result.append('| ' + ' | '.join(v.replace('\n', '<br>') for v in headers) + ' |')
                result.append('| ' + ' | '.join('---' for _ in headers) + ' |')
                result += ['| ' + ' | '.join(v.replace('\n', '<br>') for v in row) + ' |' for row in rows]
            elif kind == 'h':
                result.append('## ' + block[1])
            elif kind == 'code':
                result += ['```text', block[1], '```']
            elif kind == 'note':
                result.append('> ' + block[1])
            else:
                result.append(block[1])
            result.append('')
        result += ['---', '']
    path.write_text('\n'.join(result), encoding='utf-8')


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
        assert document.ComputeStatistics(2) == len(PAGES), 'Word 페이지 수 확인 필요'
        document.ExportAsFixedFormat(str(pdf_path), 17)
    finally:
        if document is not None:
            document.Close(0)
        if app is not None:
            app.Quit()


def validate_outputs(docx_path, pdf_path):
    from zipfile import ZipFile
    from xml.etree import ElementTree
    normalize = lambda text: re.sub(r'\s+', '', text).replace('\u00ad', '').replace('\u200b', '')
    with ZipFile(docx_path) as z:
        assert z.testzip() is None
        xml = ElementTree.fromstring(z.read('word/document.xml'))
        docx_text = normalize(''.join(el.text or '' for el in xml.iter(qn('w:t'))))
    checked = 0
    with fitz.open(pdf_path) as pdf:
        assert len(pdf) == len(PAGES)
        for page_index, (title, blocks) in enumerate(PAGES):
            pdf_text = normalize(pdf[page_index].get_text())
            expected = [title]
            for block in blocks:
                if block[0] == 'table':
                    expected += block[1]
                    expected += [value for row in block[2] for value in row]
                else:
                    expected.append(block[1])
            for text in expected:
                value = normalize(text)
                assert value in docx_text, f'Word 본문 누락: {text[:60]}'
                assert value in pdf_text, f'PDF {page_index + 1}쪽 본문 누락: {text[:60]}'
                checked += 1
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--word-pdf', action='store_true', help='Microsoft Word로 PDF 내보내기')
    args = parser.parse_args()
    verify_registers()
    OUT.mkdir(parents=True, exist_ok=True)
    docx_path = OUT / (BASE + '.docx')
    pdf_path = OUT / (BASE + '.pdf')
    make_docx(docx_path)
    if args.word_pdf:
        export_word_pdf(docx_path, pdf_path)
    else:
        make_pdf(pdf_path)
    make_markdown(OUT / (BASE + '.md'))
    text_checks = validate_outputs(docx_path, pdf_path)
    manifest = {
        'document': BASE, 'date': DATE, 'firmware_version_macro': '1.0.0',
        'source_set_sha256': SNAPSHOT, 'source_sha256': HASHES,
        'register_addresses_checked': 23, 'examples': EXAMPLES,
        'pdf_pages': len(PAGES), 'text_blocks_checked_in_both_formats': text_checks,
        'pdf_renderer': 'Microsoft Word' if args.word_pdf else 'PyMuPDF Story',
        'artifact_sha256': {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                            for path in [docx_path, pdf_path]},
        'validation_scope': '소스 대조 및 CRC 교차 검증. 실제 보드 통신·출력 시험은 포함하지 않음.',
    }
    (OUT / 'source_manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'files': [str(OUT / (BASE + ext)) for ext in ['.docx', '.pdf', '.md']],
                      'pages': len(PAGES), 'crc_examples': len(EXAMPLES),
                      'text_blocks_checked': text_checks}, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
