#!/usr/bin/env python3
"""
호스트 PC 단위 테스트 (Python 버전) — test_logic.c 와 동일 로직

실행:
    python tests/test_logic.py

검증 항목:
    - ARR 계산 정확도 (주파수 → 레지스터 역산)
    - CCR 계산 정확도 (듀티비 → 레지스터 역산)
    - 주파수-듀티 교차 일관성 (LCD 표시 검증 시뮬레이션)
    - ADC → 듀티 매핑 정확도
    - Modbus CRC-16 알려진 벡터
    - 파라미터 범위 상수 일관성
    - 소프트 스타트 시뮬레이션
"""

import sys

# ── params.h 상수 복제 ──
TIM1_CLOCK_HZ   = 170_000_000
FREQ_DEFAULT     = 280    # 28.0 kHz
FREQ_MIN         = 200    # 20.0 kHz
FREQ_MAX         = 500    # 50.0 kHz
FREQ_STEP        = 1
DUTY_DEFAULT     = 450    # 45.0%
DUTY_MIN         = 0
DUTY_MAX         = 1000   # 100.0%
DUTY_CLAMP_MAX   = 900    # 90.0%
ADC_RESOLUTION   = 4096
ADC_DEADZONE_LOW = 50
ADC_DEADZONE_HIGH= 4045

# ── 테스트 프레임워크 ──
tests_run = 0
tests_passed = 0
tests_failed = 0

def test_assert(cond, msg):
    global tests_run, tests_passed, tests_failed
    tests_run += 1
    if cond:
        tests_passed += 1
    else:
        tests_failed += 1
        print(f"  FAIL: {msg}")

def test_assert_eq(a, b, msg):
    global tests_run, tests_passed, tests_failed
    tests_run += 1
    if a == b:
        tests_passed += 1
    else:
        tests_failed += 1
        print(f"  FAIL: {msg} — expected {b}, got {a}")

def test_assert_near(a, b, tol, msg):
    global tests_run, tests_passed, tests_failed
    tests_run += 1
    diff = abs(a - b)
    if diff <= tol:
        tests_passed += 1
    else:
        tests_failed += 1
        print(f"  FAIL: {msg} — expected ~{b:.2f}, got {a:.2f}, diff={diff:.4f}")

# ── 테스트 대상 함수 (C 로직과 동일, 정수 산술) ──

def calc_arr(freq_01khz: int) -> int:
    """Center-aligned 모드 ARR 계산 (C 코드와 동일 정수 산술)"""
    freq_01khz = max(FREQ_MIN, min(FREQ_MAX, freq_01khz))
    freq_hz = freq_01khz * 100
    arr = TIM1_CLOCK_HZ // (2 * freq_hz) - 1
    return arr

def arr_to_freq_khz(arr: int) -> float:
    """ARR → 실제 주파수(kHz) 역산"""
    return TIM1_CLOCK_HZ / (2.0 * (arr + 1)) / 1000.0

def calc_ccr(arr: int, duty_01pct: int) -> int:
    """CCR 계산 (듀티비 클램핑 포함)"""
    if duty_01pct > DUTY_CLAMP_MAX:
        duty_01pct = DUTY_CLAMP_MAX
    ccr = (arr * duty_01pct) // 1000
    return ccr

def ccr_to_duty_pct(ccr: int, arr: int) -> float:
    """CCR → 실제 듀티비(%) 역산"""
    if arr == 0:
        return 0.0
    return ccr / arr * 100.0

def adc_to_duty_mapped(adc_raw: int) -> int:
    """ADC → 듀티 매핑 (데드존 적용)"""
    if adc_raw <= ADC_DEADZONE_LOW:
        return 0
    if adc_raw >= ADC_DEADZONE_HIGH:
        return 1000
    range_ = ADC_DEADZONE_HIGH - ADC_DEADZONE_LOW
    return ((adc_raw - ADC_DEADZONE_LOW) * 1000) // range_

def crc16_modbus(data: bytes) -> int:
    """CRC-16 Modbus (비트 연산 방식)"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc = crc >> 1
    return crc

# ═══════════════════════════════════════════
# ── 테스트 케이스 ──
# ═══════════════════════════════════════════

def test_arr_calculation():
    print("[TEST] ARR 계산 정확도")

    # 28.0 kHz
    arr_28k = calc_arr(280)
    actual_freq = arr_to_freq_khz(arr_28k)
    test_assert_near(actual_freq, 28.0, 0.05,
        "28.0kHz 설정 시 역산 주파수 오차 < 0.05kHz")

    # 40.0 kHz
    arr_40k = calc_arr(400)
    actual_freq = arr_to_freq_khz(arr_40k)
    test_assert_near(actual_freq, 40.0, 0.05,
        "40.0kHz 설정 시 역산 주파수 오차 < 0.05kHz")

    # 경계값: FREQ_MIN (20.0 kHz)
    arr_min = calc_arr(FREQ_MIN)
    actual_freq = arr_to_freq_khz(arr_min)
    test_assert_near(actual_freq, 20.0, 0.05,
        "FREQ_MIN(20.0kHz) 역산 정확도")

    # 경계값: FREQ_MAX (50.0 kHz)
    arr_max = calc_arr(FREQ_MAX)
    actual_freq = arr_to_freq_khz(arr_max)
    test_assert_near(actual_freq, 50.0, 0.1,
        "FREQ_MAX(50.0kHz) 역산 정확도")

    # 범위 이하 입력 → 클램핑
    arr_under = calc_arr(100)
    test_assert_eq(arr_under, arr_min,
        "FREQ_MIN 미만 입력 시 클램핑")

    # 범위 초과 입력 → 클램핑
    arr_over = calc_arr(600)
    test_assert_eq(arr_over, arr_max,
        "FREQ_MAX 초과 입력 시 클램핑")

def test_ccr_calculation():
    print("[TEST] CCR 계산 정확도 (듀티비)")

    arr = calc_arr(280)

    # 45.0% 듀티
    ccr_45 = calc_ccr(arr, 450)
    actual_duty = ccr_to_duty_pct(ccr_45, arr)
    test_assert_near(actual_duty, 45.0, 0.5,
        "45.0% 듀티 역산 오차 < 0.5%")

    # 0% 듀티
    ccr_0 = calc_ccr(arr, 0)
    test_assert_eq(ccr_0, 0, "0% 듀티 → CCR=0")

    # 100% 입력 → 90% 클램핑
    ccr_100 = calc_ccr(arr, 1000)
    ccr_90 = calc_ccr(arr, 900)
    test_assert_eq(ccr_100, ccr_90,
        "100% 입력 시 DUTY_CLAMP_MAX(90%)로 클램핑")

    # 클램핑된 듀티가 90% 이하인지 확인
    clamped_duty = ccr_to_duty_pct(ccr_100, arr)
    test_assert(clamped_duty <= 90.1,
        "클램핑 후 실제 듀티 ≤ 90%")

def test_frequency_duty_consistency():
    print("[TEST] 주파수-듀티 교차 일관성 (LCD 표시 검증 시뮬레이션)")

    # 시나리오: LCD에 "28.0 kHz" 표시 의도
    display_freq_01khz = 280

    # 1) display 값으로 ARR 계산
    arr = calc_arr(display_freq_01khz)

    # 2) ARR에서 실제 주파수 역산
    actual_freq_khz = arr_to_freq_khz(arr)

    # 3) 역산 주파수를 다시 ×0.1kHz 정수로 변환 (LCD 표시용)
    readback_01khz = round(actual_freq_khz * 10.0)

    # 4) 원래 설정값과 비교 — LCD 정합성 테스트
    test_assert_eq(readback_01khz, display_freq_01khz,
        "LCD 표시 주파수 = TIM1 ARR 역산 주파수 (28.0kHz)")

    # 모든 유효 주파수에 대해 반복 검증
    mismatch_count = 0
    mismatch_list = []
    for f in range(FREQ_MIN, FREQ_MAX + 1, FREQ_STEP):
        arr = calc_arr(f)
        actual_freq_khz = arr_to_freq_khz(arr)
        readback = round(actual_freq_khz * 10.0)
        if readback != f:
            mismatch_count += 1
            if len(mismatch_list) < 5:
                mismatch_list.append(f"  설정={f/10:.1f}kHz → 실제={readback/10:.1f}kHz")

    if mismatch_count > 0:
        print(f"  INFO: {mismatch_count}개 주파수에서 ±0.1kHz 불일치 발생 (정수 절사 오차)")
        for line in mismatch_list:
            print(line)

    # 허용 오차: ±0.1kHz 이내 불일치는 정수 절사로 인한 것이므로 
    # 실제 Hz 오차가 50Hz 미만인지 확인
    actual_error_count = 0
    for f in range(FREQ_MIN, FREQ_MAX + 1, FREQ_STEP):
        arr = calc_arr(f)
        actual_hz = TIM1_CLOCK_HZ / (2 * (arr + 1))
        expected_hz = f * 100
        if abs(actual_hz - expected_hz) > 50:  # 50Hz 오차 허용
            actual_error_count += 1

    test_assert_eq(actual_error_count, 0,
        "FREQ_MIN~FREQ_MAX 전 구간 실제 Hz 오차 < 50Hz")

def test_adc_mapping():
    print("[TEST] ADC → 듀티비 매핑")

    # 데드존 이하 → 0%
    test_assert_eq(adc_to_duty_mapped(0), 0, "ADC=0 → 0%")
    test_assert_eq(adc_to_duty_mapped(50), 0, "ADC=50(데드존 경계) → 0%")

    # 데드존 이상 → 100%
    test_assert_eq(adc_to_duty_mapped(4045), 1000, "ADC=4045(데드존 상한) → 100%")
    test_assert_eq(adc_to_duty_mapped(4095), 1000, "ADC=4095(최대) → 100%")

    # 중간값 → ~50%
    mid_adc = (ADC_DEADZONE_LOW + ADC_DEADZONE_HIGH) // 2
    mid_duty = adc_to_duty_mapped(mid_adc)
    test_assert_near(mid_duty, 500, 10, "ADC 중간값 → ~50%")

    # 단조 증가 확인
    prev = 0
    monotonic = True
    for a in range(0, 4096, 10):
        d = adc_to_duty_mapped(a)
        if d < prev:
            monotonic = False
            break
        prev = d
    test_assert(monotonic, "ADC→듀티 매핑이 단조 증가")

def test_crc16():
    print("[TEST] Modbus CRC-16")

    # 알려진 테스트 벡터: 슬레이브=1, FC=03, 주소=0x0000, 수량=0x000A
    frame1 = bytes([0x01, 0x03, 0x00, 0x00, 0x00, 0x0A])
    crc1 = crc16_modbus(frame1)
    # CRC-16/MODBUS: 0xCDC5 → 전송 시 CRC_LO=0xC5, CRC_HI=0xCD
    test_assert_eq(crc1, 0xCDC5, "CRC-16 테스트 벡터 1")

    # FC=06 Write Single Register
    frame2 = bytes([0x01, 0x06, 0x00, 0x01, 0x00, 0x03])
    crc2 = crc16_modbus(frame2)
    crc_lo = crc2 & 0xFF
    crc_hi = (crc2 >> 8) & 0xFF
    test_assert(crc_lo != crc_hi or crc_lo == 0,
        "CRC 상/하위 바이트 분리 가능")

    # 추가 벡터: 빈 바이트
    crc_empty = crc16_modbus(bytes([]))
    test_assert_eq(crc_empty, 0xFFFF, "빈 데이터 CRC = 0xFFFF (초기값)")

def test_parameter_ranges():
    print("[TEST] 파라미터 범위 상수 일관성")

    test_assert(FREQ_MIN < FREQ_MAX, "FREQ_MIN < FREQ_MAX")
    test_assert(DUTY_MIN < DUTY_MAX, "DUTY_MIN < DUTY_MAX")
    test_assert(DUTY_CLAMP_MAX <= DUTY_MAX, "DUTY_CLAMP_MAX ≤ DUTY_MAX")
    test_assert(DUTY_CLAMP_MAX > 0, "DUTY_CLAMP_MAX > 0")
    test_assert(ADC_DEADZONE_LOW < ADC_DEADZONE_HIGH, "ADC 데드존 순서 정상")
    test_assert(ADC_DEADZONE_HIGH < ADC_RESOLUTION, "ADC 데드존 상한 < 해상도")

    # ARR 범위: 16비트(65535) 이내인지 확인
    arr_at_min = calc_arr(FREQ_MIN)
    test_assert(arr_at_min <= 65535, "FREQ_MIN에서 ARR ≤ 16비트")

    arr_at_max = calc_arr(FREQ_MAX)
    test_assert(arr_at_max > 0, "FREQ_MAX에서 ARR > 0")

def test_soft_start_simulation():
    print("[TEST] 소프트 스타트 시뮬레이션")

    target_duty = 450  # 45.0%
    current_duty = 0
    steps = 500 // 10  # 50 단계

    for i in range(1, steps + 1):
        current_duty = target_duty * i // steps

    test_assert_eq(current_duty, target_duty,
        "소프트 스타트 완료 후 current_duty = target_duty")

    # 중간 단계 확인
    half_step_duty = target_duty * (steps // 2) // steps
    test_assert_near(half_step_duty, target_duty / 2, 10,
        "소프트 스타트 50% 시점 ≈ 목표의 절반")

    # 듀티 단조 증가 확인 (소프트 스타트 도중)
    monotonic = True
    prev = 0
    for i in range(1, steps + 1):
        d = target_duty * i // steps
        if d < prev:
            monotonic = False
            break
        prev = d
    test_assert(monotonic, "소프트 스타트 중 듀티 단조 증가")

# ═══════════════════════════════════════════
# ── 메인 ──
# ═══════════════════════════════════════════
if __name__ == "__main__":
    print("═══════════════════════════════════════════")
    print(" STM32G4 초음파 메인보드 — 호스트 단위 테스트")
    print("═══════════════════════════════════════════")
    print()

    test_arr_calculation()
    test_ccr_calculation()
    test_frequency_duty_consistency()
    test_adc_mapping()
    test_crc16()
    test_parameter_ranges()
    test_soft_start_simulation()

    print()
    print("═══════════════════════════════════════════")
    print(f" 결과: {tests_run} 실행, {tests_passed} 통과, {tests_failed} 실패")
    print("═══════════════════════════════════════════")

    sys.exit(1 if tests_failed > 0 else 0)
