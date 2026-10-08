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
main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
screen = (ROOT / "src" / "menu_screen.c").read_text(encoding="utf-8")
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

check("LENGTH = 62K" in linker, "Dual-bank Bank 1 Application Flash가 62 KB로 제한되지 않음")
check("ORIGIN = 0x0800F800, LENGTH = 2K" in linker, "설정 Page 주소 불일치")
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
check("문서 버전 | 1.3" in public_guide, "PLC 사용자 가이드 문서 버전이 1.3이 아님")
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
check("REG_ADDR_ERROR_CODE          0x0009" in reg_header and
      "REG_ADDR_RUN_SWITCH_STATUS   0x000A" in reg_header and
      "REG_ADDR_COMM_CONTROL_READY  0x000B" in reg_header,
      "상세 오류/RUN 스위치/통신 제어 가능 Register 배치 불일치")
check("REG_ADDR_CENTER_FREQUENCY    0x000C" in reg_header and
      "REG_ADDR_SWEEP_WIDTH         0x000D" in reg_header and
      "REG_ADDR_SWEEP_SPEED         0x000E" in reg_header,
      "중심주파수/Sweep 폭/Sweep 속도 Read-only Register 배치 불일치")
check("#define PROTOCOL_BOOL_TRUE 0xFF00U" in params,
      "기존 Protocol Boolean True 값 불일치")
check("#define PLC_OUTPUT_MAX 500U" in params,
      "기존 Protocol 출력 Full-scale가 500이 아님")
check("if (s_plc_output_control)" in ctrl and
      "g_us_state.output_command * DUTY_CLAMP_MAX" in ctrl and
      "return g_us_state.target_duty" in ctrl,
      "Local PB1/PLC 절대 출력 제어권 분리 누락")
check("DutyToOutputCommand(g_us_state.target_duty)" in ctrl and
      "UltrasonicCtrl_SetPlcOutputControl" in ctrl,
      "PLC Lock 진입 시 현재 PB1 출력값 인계 누락")
check("PLC_POT_OVERRIDE_THRESHOLD" in ctrl and
      "s_plc_pot_override_active" in ctrl and
      "s_plc_pot_override_counter++" in ctrl and
      "next_command != g_us_state.output_command" in ctrl,
      "PLC Lock 중 PB1 수동 인계 후 연속 출력 반영 경로 누락")
check("PLC_POT_OVERRIDE_THRESHOLD 45U" in params,
      "통신 운전 중 PB1 Local 인계 기준이 약 5%가 아님")
write_impl = regs.split("bool ModbusRegs_Write", 1)[1]
check("UltrasonicCtrl_StartUntimed();" in write_impl,
      "Modbus RUN ON이 타이머 없는 Remote 방식으로 시작되지 않음")
check("UltrasonicCtrl_Start();" not in write_impl,
      "Modbus RUN ON에 Local 운전 타이머가 연결되어 있음")
check("if (g_us_state.running)" in ctrl and
      "g_us_state.timed_run = false;" in ctrl.split(
          "void UltrasonicCtrl_StartUntimed", 1
      )[1].split("static void StartOutput", 1)[0],
      "기존 Local RUN을 PLC 무시간 RUN으로 전환하는 경로 누락")
run_write = write_impl.split("case REG_ADDR_RUN_STATUS:", 1)[1].split(
    "case REG_ADDR_ERROR_STATUS_RESET:", 1
)[0]
check("UltrasonicCtrl_Stop();" in run_write and
      "s_modbus_run_controlled = false;" in run_write and
      "ModbusRegs_ReleaseLocalControl();" not in run_write,
      "Modbus RUN OFF가 출력만 정지하고 485 COMM 준비 상태를 유지하지 않음")
validate_body = regs.split("ModbusRegResult_t ModbusRegs_ValidateWrite", 1)[1]
lock_validation = validate_body.split(
    "case REG_ADDR_LOCAL_INPUT_LOCK:", 1
)[1].split("case REG_ADDR_RUN_STATUS:", 1)[0]
check("return MODBUS_REG_OK;" in lock_validation and
      "!g_us_state.running" not in lock_validation,
      "운전 중 Local Lock OFF 허용 경로 누락")
check("Modbus_IsCommunicationActive" in rtu and
      "MODBUS_COMM_DISPLAY_HOLD_MS" in rtu,
      "정상 RS485 Frame 통신 표시 Timer 누락")
check("Menu_GetPowerRangeWatts" in regs,
      "DISPLAY_OUTPUT_RANGE에 Supervisor 전력 범위 반영 누락")
read_impl = regs.split("bool ModbusRegs_Read", 1)[1].split(
    "ModbusRegResult_t ModbusRegs_ValidateWrite", 1
)[0]
check("case REG_ADDR_SWEEP_STATUS:" in read_impl and
      "Menu_IsSweepActive()" in read_impl,
      "Sweep 상태가 물리 SWEEP_SW에서 읽히지 않음")
check("case REG_ADDR_SWEEP_STATUS:" not in write_impl,
      "통신에서 Sweep Mode를 변경하는 쓰기 경로가 남아 있음")
validate_impl = regs.split("ModbusRegResult_t ModbusRegs_ValidateWrite", 1)[1].split(
    "bool ModbusRegs_Write", 1
)[0]
check("case REG_ADDR_SWEEP_STATUS:" in validate_impl and
      "MODBUS_REG_ILLEGAL_ADDRESS" in validate_impl,
      "Sweep 상태 Register가 Read-only로 검증되지 않음")
check("case REG_ADDR_CENTER_FREQUENCY:" in read_impl and
      "Menu_GetCenterFrequency01kHz()" in read_impl and
      "case REG_ADDR_SWEEP_WIDTH:" in read_impl and
      "Menu_GetSweepWidthHz()" in read_impl and
      "case REG_ADDR_SWEEP_SPEED:" in read_impl and
      "Menu_GetSweepRateHz()" in read_impl,
      "중심주파수/Sweep 폭/Sweep 속도 읽기 경로 누락")
check("case REG_ADDR_CENTER_FREQUENCY:" not in write_impl and
      "case REG_ADDR_SWEEP_WIDTH:" not in write_impl and
      "case REG_ADDR_SWEEP_SPEED:" not in write_impl,
      "주파수/Sweep 설정값 통신 쓰기 경로가 남아 있음")
check("UltrasonicCtrl_EmergencyStop();" not in menu and
      "s_state == MENU_TUNE_AUTO" in menu,
      "START/STOP 장기 누름이 Auto-Tuning 전용으로 분리되지 않음")
check("if (s_run_input.changed)" in menu and
      "PA11/RUN_SW 토글은 출력 정지" in menu and
      "UltrasonicCtrl_Stop();" in menu and
      "ModbusRegs_ReleaseLocalControl();" in menu,
      "PA11/RUN_SW 토글 긴급정지 및 Local Lock 해제 경로 누락")
check("통신 제어 준비/운전 중에는 PA10 REMOTE ON/OFF만 무시" in menu and
      "if (ModbusRegs_IsLocalInputLocked())" in menu,
      "Local Lock의 REMOTE 선택 차단 경로 누락")
check("bool ModbusRegs_IsRunControlled" in regs and
      "s_modbus_run_controlled = true;" in regs,
      "LCD 485 ON용 실제 Modbus RUN 출처 상태 누락")
check("IsModbusBoardDetected()" in regs and
      "IsCommunicationControlReady()" in regs and
      "!Menu_IsAutomaticMode()" in regs,
      "485 보드 감지 및 RUN_SW OFF 통신 준비 조건 누락")
check("Menu_GetState() == MENU_MAIN" in regs,
      "메인화면에서만 통신 제어 진입하는 조건 누락")
check("case REG_ADDR_ERROR_CODE:" in regs and
      "g_us_state.error_code" in regs,
      "Transducer/Over current 상세 오류 코드 Register 누락")
check("case REG_ADDR_RUN_SWITCH_STATUS:" in regs and
      "Menu_IsAutomaticMode()" in regs,
      "RUN_SW 상태 Register 누락")
check("case REG_ADDR_COMM_CONTROL_READY:" in regs and
      "IsCommunicationControlReady()" in regs,
      "통신 제어 가능 상태 Register 누락")
check("통신 준비 진입은 기존 Local/REMOTE 출력을 정지" in regs,
      "Local Lock 최초 진입 시 기존 출력 정지 누락")
check("void ModbusRegs_Update" in regs and
      "ModbusRegs_Update();" in main,
      "RS485 보드 분리 시 출력/Lock 해제 감시 누락")
check('"RDY/SWP 485 COMM" : "RDY/    485 COMM"' in screen and
      'SetLine(s_line1, "    K-SONICS")' in screen,
      "통신 준비 화면에서 485 COMM 표시/출력정보 숨김 누락")
check("옵션 RS485 보드가 실제로 감지된 경우에만" in rtu,
      "미장착 RS485 요청 무시 경로 누락")
check("`FREQ_SET`, `DUTY_SET` Register를 제공하지 않는다" in public_guide,
      "배포 가이드에 Frequency/Duty Board-only 정책 누락")
check("보드의 M/S/CONT 운전시간" in public_guide and
      "OFF될 때까지 계속" in public_guide,
      "배포 가이드에 Modbus 무시간 운전 정책 누락")
check("Start Address=4" in public_guide and
      "Local Lock만" in public_guide and
      "Start Address=5" in public_guide,
      "배포 가이드에 Lock과 RUN 주소 구분 설명 누락")
check("FC06 쓰기 시험에서는" in public_guide and
      "`Scan`을 중지" in public_guide,
      "배포 가이드에 QModMaster FC06 단발 쓰기 절차 누락")
check("통신 단절 Watchdog이 없다" in public_guide,
      "배포 가이드에 무시간 RUN 통신 단절 주의 누락")
check("TRANSDUCER ERROR" in public_guide and
      "OVER CURRENT" in public_guide and
      "1초 평균 RMS 전류" in public_guide and
      "7.00A" in public_guide and "약 5초" in public_guide,
      "배포 가이드에 보호 조건과 상세 Error 코드 설명 누락")
check("PLC 운전 Sequence" in public_guide and
      "Stop → Lock 해제" in public_guide,
      "배포 가이드에 완전한 PLC 운전/종료 순서 누락")
check("전원 재인가와 설정 복원" in public_guide and
      "LOCAL_INPUT_LOCK`: 해제" in public_guide and
      "약 3초 기다린 다음" in public_guide,
      "배포 가이드에 전원 복구 후 PLC 자동 재명령 절차 누락")
check("현재 PB1" in public_guide and "마지막으로 조작" in public_guide and
      "485 COMM" in public_guide and "상태를 유지" in public_guide,
      "배포 가이드에 PLC 출력 제어권/RUN OFF 후 준비 상태 설명 누락")
check("약 5%" in public_guide and "주소 1 쓰기를 생략" in public_guide,
      "배포 가이드에 Local 기본 출력/5% PB1 수동 인계 정책 누락")
check("pulse_target_duty" in ctrl and
      "s_pulse_on_phase ? EffectiveTargetDuty() : 0U" in ctrl,
      "Pulse ON 구간에서 Modbus 출력 변경 즉시 반영 경로 누락")

check("MENU_SELECT" in menu_header, "LCD 메인 메뉴 선택 상태 누락")
check("Modbus_ApplyLocalConfig(" in rtu and "SettingsStorage_Save(&settings)" in rtu,
      "ID/Baud/Parity 통신 설정 저장 경로가 존재하지 않음")
check("SettingsStorage_Load(&settings)" in rtu and
      "SettingsStorage_SetDefaults(&settings)" in rtu,
      "통신 설정 저장 시 LCD 운전 시간 보존 경로가 없음")
check("SETTINGS_MAGIC" in storage and
      "offsetof(SettingsRecordV6_t, crc32)" in storage,
      "Flash 설정 Record의 Magic/CRC 검증 누락")
check("SETTINGS_VERSION_V2" in storage and "RecordV1IsValid" in storage and
      "RecordV2IsValid" in storage,
      "기존 v1/v2 Flash 설정 호환 읽기 누락")
check("band_frequency[FREQ_BAND_COUNT]" in storage and
      "selected_frequency_band" in storage,
      "대역별 중심주파수 v3 저장 항목 누락")
check("power_range_index" in storage and "rterm_enabled" in storage,
      "Supervisor 전력 범위/종단저항 v4 저장 항목 누락")
check("band_frequency_fine_10hz" in storage and
      "RecordV5IsValid" in storage,
      "Auto-Tuning 10 Hz 정밀도 v5 저장 항목 누락")
check("sweep_width_hz" in storage and "sweep_rate_hz" in storage and
      "RecordV6IsValid" in storage,
      "Sweep 폭/속도 v6 저장 항목 누락")
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
    "01 03 00 00 00 0C 45 CF",
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
print("  Flash settings page=0x0800F800 (Bank 1, 2 KB)")
print("  Baud table=9600/19200/38400/57600/115200")
