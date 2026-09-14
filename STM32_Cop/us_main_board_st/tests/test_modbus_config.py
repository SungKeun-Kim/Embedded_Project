#!/usr/bin/env python3
"""Modbus 보드 설정과 Flash 배치의 정적 계약 테스트."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
failures: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


params = (ROOT / "include" / "params.h").read_text(encoding="utf-8")
linker = (ROOT / "ldscripts" / "STM32G474CBTx_FLASH.ld").read_text(
    encoding="utf-8"
)
ioc = (ROOT / "us_main_board_st.ioc").read_text(encoding="utf-8")
regs = (ROOT / "src" / "modbus_regs.c").read_text(encoding="utf-8")
reg_header = (ROOT / "include" / "modbus_regs.h").read_text(encoding="utf-8")
menu = (ROOT / "src" / "menu.c").read_text(encoding="utf-8")
menu_header = (ROOT / "include" / "menu.h").read_text(encoding="utf-8")
storage = (ROOT / "src" / "settings_storage.c").read_text(encoding="utf-8")
rtu = (ROOT / "src" / "modbus_rtu.c").read_text(encoding="utf-8")
ctrl = (ROOT / "src" / "ultrasonic_ctrl.c").read_text(encoding="utf-8")
public_guide = (ROOT / "docs" / "PLC_Modbus_RTU_사용자_가이드.md").read_text(
    encoding="utf-8"
)

check("#define MODBUS_ADDR_DEFAULT 1U" in params, "기본 Slave ID가 1이 아님")
check(
    "#define MODBUS_BAUD_DEFAULT 9600U" in params,
    "기본 Baud rate가 9600이 아님",
)
check(
    "#define MODBUS_PARITY_DEFAULT 1U" in params,
    "기본 Parity가 Even(1)이 아님",
)

baud_match = re.search(
    r"MODBUS_BAUD_TABLE\[MODBUS_BAUD_INDEX_COUNT\]\s*=\s*\{([^}]+)\}",
    params,
    re.S,
)
check(baud_match is not None, "Baud table을 찾을 수 없음")
if baud_match:
    baud_values = [
        int(value)
        for value in re.findall(r"(\d+)U", baud_match.group(1))
    ]
    check(
        baud_values == [9600, 19200, 38400, 57600, 115200],
        f"Baud table 불일치: {baud_values}",
    )

check("LENGTH = 126K" in linker, "Application Flash가 126 KB로 제한되지 않음")
check("ORIGIN = 0x0801F800, LENGTH = 2K" in linker, "설정 Page 주소 불일치")
check("__settings_flash_start__" in linker, "설정 Page 시작 Symbol 누락")
check("__settings_flash_end__" in linker, "설정 Page 끝 Symbol 누락")

check("USART2.BaudRate=9600" in ioc, ".ioc Baud rate 불일치")
check("USART2.WordLength=WORDLENGTH_9B" in ioc, ".ioc 8-E-1 WordLength 불일치")
check("USART2.Parity=PARITY_EVEN" in ioc, ".ioc Even parity 누락")
check("USART2.StopBits=STOPBITS_1" in ioc, ".ioc Stop bit 불일치")

write_body = regs.split("bool ModbusRegs_Write", 1)[1].split(
    "ModbusRegResult_t ModbusRegs_ValidateWrite", 1
)[0]
check(
    "case REG_ADDR_MODBUS_ADDR" not in write_body,
    "PLC Write 경로에서 Slave ID가 변경될 수 있음",
)
check(
    "case REG_ADDR_MODBUS_BAUD" not in write_body,
    "PLC Write 경로에서 Baud rate가 변경될 수 있음",
)
check(
    "case REG_ADDR_MODBUS_PARITY" not in write_body,
    "PLC Write 경로에서 Parity가 변경될 수 있음",
)

# Source의 공개 Register가 PLC 배포 가이드에 빠지지 않았는지 확인
register_definitions = re.findall(
    r"#define\s+(REG_ADDR_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+)", reg_header
)
for name, address in register_definitions:
    check(address.upper() in public_guide.upper(), f"배포 가이드에 {name} {address} 누락")

check("Firmware | v1.2.0" in public_guide, "배포 가이드 Firmware 버전 불일치")
check("기본값 |" not in public_guide or "9600" in public_guide,
      "배포 가이드 기본 Baud 누락")
check("`0x0010~0x0012`" in public_guide and "쓸 수는 없다" in public_guide,
      "배포 가이드에 통신 설정 Read-only 정책 누락")
check("REG_ADDR_FREQ_SET" not in reg_header and "REG_ADDR_DUTY_SET" not in reg_header,
      "PLC Map에 직접 Frequency/Duty 설정 Register가 남아 있음")
check("REG_ADDR_FREQUENCY_VALUE     0x0000" in reg_header,
      "발진 주파수 읽기 주소 불일치")
check("REG_ADDR_OUTPUT_VALUE        0x0001" in reg_header,
      "기존 Protocol 출력 주소 불일치")
check("REG_ADDR_DISPLAY_RANGE       0x0002" in reg_header,
      "표시 출력 범위 주소 불일치")
check("REG_ADDR_RESERVED_1          0x0009" in reg_header and
      "REG_ADDR_RESERVED_2          0x000A" in reg_header and
      "REG_ADDR_RESERVED_3          0x000B" in reg_header,
      "향후 명령용 예비 Register 3개 배치 불일치")
check("#define PROTOCOL_BOOL_TRUE 0xFF00U" in params,
      "기존 Protocol Boolean True 값 불일치")
check("#define PLC_OUTPUT_MAX 500U" in params,
      "기존 Protocol 출력 Full-scale가 500이 아님")
check("g_us_state.target_duty *" in ctrl and
      "g_us_state.output_command / PLC_OUTPUT_MAX" in ctrl,
      "Local Duty limit × PLC Output / 500 제한식 누락")
check("Modbus_IsCommunicationActive" in rtu and
      "MODBUS_COMM_DISPLAY_HOLD_MS" in rtu,
      "정상 RS485 Frame 통신 표시 Timer 누락")
check("Menu_GetPowerRangeWatts" in regs,
      "DISPLAY_OUTPUT_RANGE에 Supervisor 전력 범위 반영 누락")
check("START/STOP 장기 누름 비상정지는 허용" in menu,
      "Local Lock 중 장기 누름 비상정지 경로 누락")
check("`FREQ_SET`, `DUTY_SET` Register를 제공하지 않는다" in public_guide,
      "배포 가이드에 Frequency/Duty Board-only 정책 누락")

check("MENU_SELECT" in menu_header, "LCD 메인 메뉴 선택 상태 누락")
check("Modbus_ApplyLocalConfig(" in rtu and "SettingsStorage_Save(&settings)" in rtu,
      "ID/Baud/Parity 통신 설정 저장 경로가 존재하지 않음")
check("SettingsStorage_Load(&settings)" in rtu and
      "SettingsStorage_SetDefaults(&settings)" in rtu,
      "통신 설정 저장 시 LCD 운전 시간 보존 경로가 없음")
check("SETTINGS_MAGIC" in storage and
      "offsetof(SettingsRecordV4_t, crc32)" in storage,
      "Flash 설정 Record의 Magic/CRC 검증 누락")
check("SETTINGS_VERSION_V2" in storage and "RecordV1IsValid" in storage and
      "RecordV2IsValid" in storage,
      "기존 v1/v2 Flash 설정 호환 읽기 누락")
check("band_frequency[FREQ_BAND_COUNT]" in storage and
      "selected_frequency_band" in storage,
      "대역별 중심주파수 v3 저장 항목 누락")
check("power_range_index" in storage and "rterm_enabled" in storage,
      "Supervisor 전력 범위/종단저항 v4 저장 항목 누락")
check("LegacyBaudIndexToCurrent" in storage,
      "기존 v1~v3의 115200 Baud index 변환 누락")
for state in ("MENU_SUPERVISOR_PL", "MENU_SUPERVISOR_BAUD",
              "MENU_SUPERVISOR_ADDR", "MENU_SUPERVISOR_TERM"):
    check(state in menu and state in menu_header,
          f"Supervisor 메뉴 상태 누락: {state}")

# 11-bit Character 기준 RTU timing 확인
for baud, expected_t35 in ((9600, 4011), (19200, 2006)):
    char_us = (11_000_000 + baud - 1) // baud
    t35_us = (char_us * 35 + 9) // 10
    check(
        abs(t35_us - expected_t35) <= 2,
        f"{baud} bps t3.5 계산 오류: {t35_us} us",
    )


def modbus_crc(frame: bytes) -> int:
    crc = 0xFFFF
    for byte in frame:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


# 배포 가이드 Raw Frame CRC 재검증
for frame_hex in (
    "01 03 00 10 00 03 04 0E",
    "01 03 00 00 00 09 85 CC",
    "01 06 00 04 FF 00 89 FB",
    "01 06 00 01 00 FA 58 49",
    "01 06 00 05 FF 00 D8 3B",
    "01 06 00 05 00 00 99 CB",
    "01 06 00 08 FF 00 49 F8",
    "01 06 00 04 00 00 C8 0B",
):
    frame = bytes.fromhex(frame_hex)
    expected = frame[-2] | (frame[-1] << 8)
    check(modbus_crc(frame[:-2]) == expected,
          f"배포 가이드 Frame CRC 오류: {frame_hex}")

if failures:
    print("[FAIL] Modbus 설정 계약 테스트")
    for failure in failures:
        print(f"  - {failure}")
    sys.exit(1)

print("[PASS] Modbus 설정 계약 테스트")
print("  Slave ID=1, 9600 bps, 8-E-1")
print("  Flash settings page=0x0801F800 (2 KB)")
print("  Baud table=9600/19200/38400/57600/115200")
