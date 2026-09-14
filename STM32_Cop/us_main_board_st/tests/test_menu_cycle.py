#!/usr/bin/env python3
"""순환형 LCD 메뉴 구성과 제어 연결의 정적 계약 테스트."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
menu_h = (ROOT / "include" / "menu.h").read_text(encoding="utf-8")
menu_c = (ROOT / "src" / "menu.c").read_text(encoding="utf-8")
screen = (ROOT / "src" / "menu_screen.c").read_text(encoding="utf-8")
button = (ROOT / "src" / "button.c").read_text(encoding="utf-8")
ctrl = (ROOT / "src" / "ultrasonic_ctrl.c").read_text(encoding="utf-8")
pwm = (ROOT / "src" / "ultrasonic_pwm.c").read_text(encoding="utf-8")
irq = (ROOT / "src" / "stm32g4xx_it.c").read_text(encoding="utf-8")

states = [
    "MENU_TIME",
    "MENU_FREQUENCY",
    "MENU_TUNE_SELECT",
    "MENU_TUNE_MANUAL",
    "MENU_TUNE_AUTO",
    "MENU_SWEEP_WIDTH",
    "MENU_SWEEP_RATE",
    "MENU_OUTPUT_MODE",
    "MENU_CURRENT_SET",
]
for state in states:
    assert state in menu_h, state
    assert state in screen, state

for text in [
    "MODE    TIME:%02uM",
    "TIME SET: %02u MIN",
    "2.FREQUENCY",
    "3.TUNING MODE",
    "MANUAL TUNE",
    "AUTO TUNE READY",
    "4.SWEEP WIDTH",
    "5.SWEEP SPEED",
    "6.OUTPUT MODE",
    "7.CURRENT SET",
]:
    assert text in screen, text

for default in [
    "FREQ_BAND_28_DEFAULT",
    "FREQ_BAND_40_DEFAULT",
    "FREQ_BAND_68_DEFAULT",
    "FREQ_BAND_80_DEFAULT",
]:
    assert default in menu_c, default
assert "s_frequency_choices[s_frequency_index] = frequency" in menu_c
assert "SettingsStorage_Save(&settings)" in menu_c
assert "Menu_GetFrequencyBandIndex() == 3U" in screen
assert "uint16_t frequency = g_us_state.target_freq" in screen
manual_case = menu_c.split("case MENU_TUNE_MANUAL:", 1)[1].split(
    "case MENU_TUNE_AUTO:", 1
)[0]
assert "UltrasonicCtrl_StartUntimed();" in manual_case
assert "!s_sweep_input.stable_active" not in manual_case
assert "UltrasonicCtrl_SetFrequency" in menu_c
assert "ResonanceTuning_Start" in menu_c
assert "BTN_EVT_HOLD_3S" in button and "BTN_HOLD_3S_MS" in button
assert "UltrasonicCtrl_SetSweepParameters" in ctrl
assert "32-bit phase accumulator" in ctrl
assert "SWEEP_UPDATE_RATE_HZ" in ctrl
assert "UltrasonicPWM_SetFrequencyHzFast" in ctrl
assert "TIM7_DAC_IRQHandler" in irq
assert "PERxR = period" in pwm and "CMP1xR = period / 2U" in pwm
assert "HRTIM_TIMERUPDATE_A" in pwm
assert "HRTIM_PRESCALERRATIO_MUL4" in pwm
assert "HRTIM_COUNTER_CLOCK_HZ / frequency_hz" in pwm

print("[PASS] MODE 순환 메뉴 + 주파수/Tuning/Sweep 연결 검증")
