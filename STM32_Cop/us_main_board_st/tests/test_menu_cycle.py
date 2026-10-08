#!/usr/bin/env python3
"""일반 시간 메뉴와 MODE+UP 보호 설정 메뉴의 정적 계약 테스트."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
menu_h = (ROOT / "include" / "menu.h").read_text(encoding="utf-8")
menu_c = (ROOT / "src" / "menu.c").read_text(encoding="utf-8")
screen = (ROOT / "src" / "menu_screen.c").read_text(encoding="utf-8")
button = (ROOT / "src" / "button.c").read_text(encoding="utf-8")
button_h = (ROOT / "include" / "button.h").read_text(encoding="utf-8")
params = (ROOT / "include" / "params.h").read_text(encoding="utf-8")
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
    "1.FREQUENCY",
    "2.SWEEP WIDTH",
    "3.SWEEP SPEED",
    "4.TUNING MODE",
    "MANUAL SWEEP",
    "AUTO TUNE READY",
    "5.OUTPUT MODE",
    "6.CURRENT SET",
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
assert "g_us_state.running ? g_us_state.target_freq : 0U" in screen
manual_case = menu_c.split("case MENU_TUNE_MANUAL:", 1)[1].split(
    "case MENU_TUNE_AUTO:", 1
)[0]
assert "UltrasonicCtrl_StartUntimed();" in manual_case
assert "ApplySweepParameters();" in manual_case
assert "UltrasonicCtrl_SetMode(MODE_SWEEP);" in manual_case
assert "BTN_PROTECTED_MENU_HOLD_MS 2000U" in params
assert "Button_IsPressed(ButtonId_t id)" in button_h
assert "Button_IsPressed(BTN_ID_MODE)" in menu_c
assert "Button_IsPressed(BTN_ID_UP)" in menu_c
assert "s_state = MENU_FREQUENCY;" in menu_c
protected_entry = menu_c.split("static bool HandleProtectedMenuCombo(void) {", 1)[1].split(
    "static void AdvanceMenu", 1
)[0]
assert "!s_run_input.stable_active && !g_us_state.running" in protected_entry
assert "const bool exiting = IsProtectedSettingState();" in protected_entry
assert "AbortTuningOutput();" in protected_entry
assert "s_state = MENU_MAIN;" in protected_entry
assert "menu_combo" in button
assert "menu_combo && i == BTN_ID_MODE" in button
supervisor_exit = menu_c.split("static bool HandleSupervisorExitCombo(void) {", 1)[1].split(
    "static void HandleSupervisorMenu", 1
)[0]
assert "Button_IsPressed(BTN_ID_MODE)" in supervisor_exit
assert "Button_IsPressed(BTN_ID_DOWN)" in supervisor_exit
assert "BTN_PROTECTED_MENU_HOLD_MS" in supervisor_exit
assert "s_supervisor_exit_combo_armed" in supervisor_exit
assert "SaveSupervisorSettings()" in supervisor_exit
assert "s_state = MENU_MAIN;" in supervisor_exit
supervisor_menu = menu_c.split("static void HandleSupervisorMenu", 2)[2].split(
    "static bool SaveSupervisorSettings", 1
)[0]
assert "s_state = MENU_SUPERVISOR_BAUD;" in supervisor_menu
assert "s_state = MENU_SUPERVISOR_ADDR;" in supervisor_menu
assert "s_state = MENU_SUPERVISOR_TERM;" in supervisor_menu
assert "s_state = MENU_SUPERVISOR_PL;" in supervisor_menu
assert "s_state = MENU_MAIN;" not in supervisor_menu

advance = menu_c.split("static void AdvanceMenu(void) {", 1)[1].split(
    "static void AbortTuningOutput", 1
)[0]
time_case = advance.split("case MENU_TIME:", 1)[1].split(
    "case MENU_FREQUENCY:", 1
)[0]
assert "s_state = MENU_MAIN;" in time_case
output_case = advance.split("case MENU_OUTPUT_MODE:", 1)[1].split(
    "case MENU_CURRENT_SET:", 1
)[0]
current_case = advance.split("case MENU_CURRENT_SET:", 1)[1].split(
    "default:", 1
)[0]
assert "s_state = MENU_FREQUENCY;" in output_case
assert "s_state = MENU_MAIN;" not in output_case
assert "s_state = MENU_FREQUENCY;" in current_case
assert "mode == BTN_EVT_LONG_PRESS && !IsProtectedSettingState()" in menu_c
protected_order = [
    "case MENU_FREQUENCY:",
    "case MENU_SWEEP_WIDTH:",
    "case MENU_SWEEP_RATE:",
    "case MENU_TUNE_SELECT:",
    "case MENU_TUNE_MANUAL:",
    "s_state = MENU_OUTPUT_MODE;",
    "case MENU_OUTPUT_MODE:",
]
positions = [advance.index(token) for token in protected_order]
assert positions == sorted(positions), "보호 설정 메뉴 순서 불일치"
assert "UltrasonicCtrl_SetFrequency" in menu_c
assert "ResonanceTuning_Start" in menu_c
assert "BTN_EVT_HOLD_3S" in button and "BTN_HOLD_3S_MS" in button
assert "UltrasonicCtrl_SetSweepParameters" in ctrl
assert "SWEEP_WIDTH_DEFAULT_HZ 500U" in params
assert "SWEEP_RATE_DEFAULT_HZ 100U" in params
assert "RUN_TIME_VALUE_DEFAULT 10U" in params
assert "32-bit phase accumulator" in ctrl
assert "SWEEP_UPDATE_RATE_HZ" in ctrl
assert "UltrasonicPWM_SetFrequencyHzFast" in ctrl
assert "TIM7_DAC_IRQHandler" in irq
assert "PERxR = period" in pwm and "CMP1xR = period / 2U" in pwm
assert "HRTIM_TIMERUPDATE_A" in pwm
assert "HRTIM_PRESCALERRATIO_MUL4" in pwm
assert "HRTIM_COUNTER_CLOCK_HZ / frequency_hz" in pwm

print("[PASS] MODE+UP 보호 메뉴 + 주파수/Tuning/Sweep 연결 검증")
