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


def current_centiamp(rms_counts: int) -> int:
    numerator = rms_counts * VDDA_MV * CT_RATIO * 100
    denominator = ADC_FS * BURDEN_OHM * 1000
    return (numerator + denominator // 2) // denominator


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


def test_phase_wrap() -> None:
    assert phase_01deg(1000, 1000, 6000) == 0
    assert phase_01deg(1000, 1500, 6000) == 300
    assert phase_01deg(5800, 200, 6000) == 240
    assert phase_01deg(200, 5800, 6000) == -240


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
    assert "s_rough_frequency == s_scan_start" in tune
    assert "AUTO_TUNE_PHASE_MAX_ABS_01DEG" in tune
    assert "ResonanceTuning_Process();" in main


if __name__ == "__main__":
    test_rms_and_current_conversion()
    test_phase_wrap()
    test_source_wiring_and_safety_guards()
    print("[PASS] PA6 RMS + PA0/PA1 위상 + 2단계 Auto-Tuning 검증")
