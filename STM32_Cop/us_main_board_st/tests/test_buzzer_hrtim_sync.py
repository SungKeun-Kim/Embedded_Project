#!/usr/bin/env python3
"""PA5 수동형 부저와 HRTIM 동기 주파수 갱신 정적 계약 테스트."""

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
params = (ROOT / "include" / "params.h").read_text(encoding="utf-8")
buzzer = (ROOT / "src" / "buzzer.c").read_text(encoding="utf-8")
button = (ROOT / "src" / "button.c").read_text(encoding="utf-8")
menu = (ROOT / "src" / "menu.c").read_text(encoding="utf-8")
main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
gpio = (ROOT / "src" / "gpio_init.c").read_text(encoding="utf-8")
pwm = (ROOT / "src" / "ultrasonic_pwm.c").read_text(encoding="utf-8")
ctrl = (ROOT / "src" / "ultrasonic_ctrl.c").read_text(encoding="utf-8")

assert "BUZZER_RESONANCE_HZ 2700U" in params
assert "BUZZER_TIMER_COUNTER_HZ 1000000U" in params
assert "GPIO_AF1_TIM2" in gpio
assert "TIM2" in buzzer and "TIM_CHANNEL_1" in buzzer
assert "Buzzer_RequestLongPress(any_long_pressed);" in button
assert "s_btn[i].event = BTN_EVT_DOWN;" in button
assert "Buzzer_RequestButtonClick();" in menu
assert "Buzzer_RequestAdjustTick();" in menu
assert "Buzzer_RequestInvalidButton();" in menu
assert "Buzzer_Play(BUZZER_STARTUP_BEEP_MS);" in main
assert "BUZZER_INVALID_PULSE_COUNT 3U" in params
assert "BUZZER_ADJUST_BEEP_MS 25U" in params
assert "BTN_ADJUST_HOLD_MS 500U" in params
assert "BTN_REPEAT_INTERVAL_MS 120U" in params

counter_hz = 1_000_000
period_counts = round(counter_hz / 2700)
actual_hz = counter_hz / period_counts
assert abs(actual_hz - 2700) / 2700 < 0.002

assert "HRTIM_UPDATEONREPETITION_ENABLED" in pwm
assert "HRTIM_TIMDEADTIMEINSERTION_ENABLED" in pwm
assert "HAL_HRTIM_DeadTimeConfig" in pwm
assert "HRTIM_TIMDEADTIME_PRESCALERRATIO_DIV1" in pwm
assert "HRTIM_GDT_DEADTIME_SPLIT_01KHZ 720U" in params
assert "HRTIM_GDT_DEADTIME_LOW_NS 1400U" in params
assert "HRTIM_GDT_DEADTIME_LOW_COUNTS 238U" in params
assert "HRTIM_GDT_DEADTIME_HIGH_NS 800U" in params
assert "HRTIM_GDT_DEADTIME_HIGH_COUNTS 136U" in params
assert "GDT_PRIMARY_TURNS 47U" in params
assert "GDT_SECONDARY_TURNS 42U" in params
assert "HRTIM_OUTPUTSET_NONE" in pwm
assert "HRTIM_OUTPUTRESET_NONE" in pwm
assert "HRTIM_CR1_TAUDIS" in pwm

low_deadtime_ns = 238 * 1_000_000_000 / 170_000_000
high_deadtime_ns = 136 * 1_000_000_000 / 170_000_000
assert abs(low_deadtime_ns - 1400.0) < 0.01
assert abs(high_deadtime_ns - 800.0) < 0.01

secondary_peak_v = 12.0 * 42 / 47
assert 10.5 <= secondary_peak_v <= 11.0

# 고주파 대역 0.8 us 적용 시 120/128 kHz 명령 ON 시간을 확인한다.
half_period_ns_at_120k = 1_000_000_000 / (2 * 120_000)
half_period_ns_at_128k = 1_000_000_000 / (2 * 128_000)
assert half_period_ns_at_120k - high_deadtime_ns > 3_300
assert half_period_ns_at_128k - high_deadtime_ns > 3_100

select_body = re.search(
    r"void UltrasonicPWM_SelectDeadTimeForCenter\(.*?\n\}", pwm, re.S
)
assert select_body is not None
assert "DeadTimeCountsForFrequency" in select_body.group(0)
assert "!s_output_running" in select_body.group(0)

selector_body = re.search(
    r"static uint16_t DeadTimeCountsForFrequency\(uint16_t freq_01khz\) \{.*?\n\}",
    pwm,
    re.S,
)
assert selector_body is not None
assert "HRTIM_GDT_DEADTIME_SPLIT_01KHZ" in selector_body.group(0)
assert "HRTIM_GDT_DEADTIME_HIGH_COUNTS" in selector_body.group(0)
assert "HRTIM_GDT_DEADTIME_LOW_COUNTS" in selector_body.group(0)

fast_body = re.search(
    r"void UltrasonicPWM_SetFrequencyHzFast\(.*?\n\}", pwm, re.S
)
assert fast_body is not None
assert "HRTIM_TIMERUPDATE_A" not in fast_body.group(0)
assert "DeadTime" not in fast_body.group(0)

start_body = re.search(r"static void StartOutput\(bool use_timer\) \{.*?\n\}", ctrl, re.S)
assert start_body is not None
assert "UltrasonicPWM_SelectDeadTimeForCenter(g_us_state.target_freq);" in start_body.group(0)
assert start_body.group(0).index("UltrasonicPWM_SelectDeadTimeForCenter") < start_body.group(0).index("UltrasonicPWM_Start")

print("[PASS] PA5 버튼음 + HRTIM 동기 갱신 + GDT 대역별 dead-time 검증")
