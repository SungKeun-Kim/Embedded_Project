#!/usr/bin/env python3
"""PA6 RMS 환산과 PA0/PA1 위상/Auto-Tuning 선택 로직 검증."""

import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ADC_FS = 4095
VDDA_MV = 3300
CT_RATIO = 1000
BURDEN_OHM = 120
SAMPLE_RATE = 4000
WINDOW = 800
CURRENT_AVERAGE_WINDOWS = 5
OVERCURRENT_LIMIT_CENTIAMP = 700
OVERCURRENT_HOLD_WINDOWS = 25


def current_centiamp(rms_counts: int) -> int:
    numerator = rms_counts * VDDA_MV * CT_RATIO * 100
    denominator = ADC_FS * BURDEN_OHM * 1000
    raw = (numerator + denominator // 2) // denominator
    return (raw * 1000 + 500) // 1000


def moving_average(values: list[int], window: int = CURRENT_AVERAGE_WINDOWS) -> list[int]:
    history: list[int] = []
    result: list[int] = []
    for value in values:
        history.append(value)
        if len(history) > window:
            history.pop(0)
        result.append((sum(history) + len(history) // 2) // len(history))
    return result


def overcurrent_trips(values: list[int]) -> bool:
    consecutive = 0
    for current in moving_average(values):
        if current >= OVERCURRENT_LIMIT_CENTIAMP:
            consecutive += 1
        else:
            consecutive = 0
        if consecutive >= OVERCURRENT_HOLD_WINDOWS:
            return True
    return False


def phase_01deg(voltage_capture: int, current_capture: int, period: int) -> int:
    delta = current_capture - voltage_capture
    if delta > period // 2:
        delta -= period
    elif delta < -(period // 2):
        delta += period
    return delta * 3600 // period


def test_rms_and_current_conversion() -> None:
    # 5 A: burden RMS=0.6 V, ADC RMS 약 744.5 count
    amplitude = (0.6 * math.sqrt(2.0)) / 3.3 * ADC_FS
    samples = [
        2048 + round(amplitude * math.sin(2.0 * math.pi * 60.0 * n / SAMPLE_RATE))
        for n in range(WINDOW)
    ]
    mean = sum(samples) // WINDOW
    rms = math.isqrt(sum((sample - mean) ** 2 for sample in samples) // WINDOW)
    measured = current_centiamp(rms)
    assert 495 <= measured <= 505, (rms, measured)


def test_overcurrent_uses_filtered_rms() -> None:
    # 정상 1.76 A 운전 중 단 한 번 12 A로 계산되는 spike가 있어도 Trip하지 않는다.
    assert not overcurrent_trips([176] * 20 + [1200] + [176] * 30)

    # 200 ms마다 spike가 한 번씩 발생하더라도 RMS 평균 전류가 낮으면 Trip하지 않는다.
    noisy_normal = [1200 if i % 5 == 0 else 176 for i in range(100)]
    assert not overcurrent_trips(noisy_normal)

    # 1초 평균값이 7 A 이상인 상태가 5초 지속되면 반드시 Trip한다.
    assert overcurrent_trips([176] * 10 + [800] * 35)


def test_phase_wrap() -> None:
    assert phase_01deg(1000, 1000, 6000) == 0
    assert phase_01deg(1000, 1500, 6000) == 300
    assert phase_01deg(5800, 200, 6000) == 240
    assert phase_01deg(200, 5800, 6000) == -240


def test_direction_search_reaches_parabola_minimum() -> None:
    frequency = 40000
    direction = -1
    previous = None
    best_frequency = frequency
    best_current = 0xFFFF
    best_low = frequency
    best_high = frequency
    for _ in range(100):  # 200 ms x 100 = 20 s
        current = 300 + ((frequency - 39730) ** 2) // 4000
        if current < best_current:
            best_current = current
            best_frequency = frequency
            best_low = frequency
            best_high = frequency
        elif current == best_current:
            best_low = min(best_low, frequency)
            best_high = max(best_high, frequency)
            best_frequency = (best_low + best_high) // 2
        if previous is not None and current > previous + 2:
            direction = -direction
        previous = current
        frequency += direction * 10
    assert abs(best_frequency - 39730) <= 10
    assert best_frequency + 200 == 39930


def test_source_wiring_and_safety_guards() -> None:
    adc = (ROOT / "src" / "adc_control.c").read_text(encoding="utf-8")
    tune = (ROOT / "src" / "resonance_tuning.c").read_text(encoding="utf-8")
    main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
    assert "ADC_EXTERNALTRIG_T6_TRGO" in adc
    assert "DMA_REQUEST_ADC2" in adc
    assert "__HAL_RCC_DMAMUX1_CLK_ENABLE" in adc
    assert "CT_RMS_WINDOW_SAMPLES" in adc
    assert "HRTIM_EEV4SRC_COMP1_OUT" in tune  # PA1 전류위상
    assert "HRTIM_EEV5SRC_COMP3_OUT" in tune  # PA0 전압위상
    ctrl = (ROOT / "src" / "ultrasonic_ctrl.c").read_text(encoding="utf-8")
    params = (ROOT / "include" / "params.h").read_text(encoding="utf-8")
    assert "AUTO_TUNE_STEP_HZ 10U" in params
    assert "AUTO_TUNE_DURATION_MS 20000U" in params
    assert "AUTO_TUNE_RESULT_OFFSET_HZ 200U" in params
    assert "AUTO_TUNE_OUTPUT_01PCT 480U" in params
    assert "CT_CURRENT_CALIBRATION_PERMILLE 1000U" in params
    assert "CT_CURRENT_AVERAGE_WINDOWS 5U" in params
    assert "ADC_Control_GetCurrentWindowCentiAmp()" in tune
    assert "s_scan_direction = (int8_t)-s_scan_direction" in tune
    assert "ADC_Control_GetCurrentCentiAmp() >=" in ctrl
    assert "CT_OVERCURRENT_LIMIT_CENTIAMP" in ctrl
    assert "ADC_Control_GetCurrentPeakRaw() >=" not in ctrl
    assert "s_transducer_low_signal_windows" in ctrl
    assert "TRANSDUCER_SIGNAL_LOSS_WINDOWS" in ctrl
    assert "OutputDuty01Percent(g_us_state.current_duty)" in ctrl
    assert "ULTRASONIC_ERROR_OVER_CURRENT" in ctrl
    assert "ULTRASONIC_ERROR_TRANSDUCER" in ctrl
    assert "UltrasonicPWM_ForceControlOutputOff" in ctrl
    assert "ResonanceTuning_Process();" in main


if __name__ == "__main__":
    test_rms_and_current_conversion()
    test_overcurrent_uses_filtered_rms()
    test_phase_wrap()
    test_direction_search_reaches_parabola_minimum()
    test_source_wiring_and_safety_guards()
    print("[PASS] PA6 RMS + 보호정지 + 10 Hz/20초 Auto-Tuning 검증")
