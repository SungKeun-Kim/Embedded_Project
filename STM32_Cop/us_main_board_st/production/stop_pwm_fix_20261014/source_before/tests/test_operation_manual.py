#!/usr/bin/env python3
"""기본조작 매뉴얼 LCD/운전 상태의 정적 계약 테스트."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
failures: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)


def check_line(text: str) -> None:
    check(len(text) <= 16, f"LCD 16자 초과: {text!r} ({len(text)}자)")


for sample in (
    "   ULTRASONIC",
    "    K-SONICS",
    "STANDBY TIME:10M",
    "MODE    TIME:09M",
    "TIME SET: 09 MIN",
    "MODE    TIME:05S",
    "TIME SET: 05 SEC",
    "MODE   TIME:CONT",
    "TIME SET:   CONT",
    "RUN/SWP   REMOTE",
    "RUN/SWP   485 ON",
    "RUN|    TIME:19M",
    "RDY/    TIME:20M",
    "PL:100%  39.5kHz",
    "STANDBY 485 COMM",
    "RDY/SWP 485 COMM",
    "VR: 50% MANUAL",
    "RDY/SWP TIME:20M",
    "RUN/SWP TIME:19M",
    "RDY/SWP TIMEOVER",
    "P:060% F:028.0K",
):
    check_line(sample)

params = (ROOT / "include" / "params.h").read_text(encoding="utf-8")
ctrl = (ROOT / "src" / "ultrasonic_ctrl.c").read_text(encoding="utf-8")
adc = (ROOT / "src" / "adc_control.c").read_text(encoding="utf-8")
menu = (ROOT / "src" / "menu.c").read_text(encoding="utf-8")
screen = (ROOT / "src" / "menu_screen.c").read_text(encoding="utf-8")
main = (ROOT / "src" / "main.c").read_text(encoding="utf-8")
system_clock = (ROOT / "src" / "system_clock.c").read_text(encoding="utf-8")
storage = (ROOT / "src" / "settings_storage.c").read_text(encoding="utf-8")
ioc = (ROOT / "us_main_board_st.ioc").read_text(encoding="utf-8")
config = (ROOT / "include" / "config.h").read_text(encoding="utf-8")

check("#define RUN_TIME_VALUE_DEFAULT 10U" in params, "기본 시간이 10M이 아님")
check("PL_DISPLAY_CONFIRM_REFRESHES 1U" in params and
      "s_power_display_candidate_count" in screen,
      "PWM 볼륨 표시의 빠른 안정값 갱신 누락")
lcd = (ROOT / "src" / "lcd1602.c").read_text(encoding="utf-8")
check("LCD_CHAR_SPINNER_BACKSLASH" in screen and
      "spinner_backslash" in lcd and "LCD_CreateChar" in lcd,
      "엔화 기호 없는 4단계 사용자 정의 회전 문자 누락")
check("RUN_TIME_MINUTES" in params and "RUN_TIME_SECONDS" in params and
      "RUN_TIME_CONTINUOUS" in params, "MIN/SEC/CONT 타이머 모드 누락")
check("MenuScreen_ShowSplash();" in main and
      "HAL_Delay(LCD_SPLASH_TIME_MS);" in main, "2초 부팅 화면 호출 누락")
check("UltrasonicCtrl_StartUntimed" in ctrl and
      "g_us_state.timed_run" in ctrl, "수동 무제한/자동 타이머 분리 누락")
check("g_us_state.time_over = true" in ctrl, "TIMEOVER 상태 설정 누락")
check("s_remote_input.activated" in menu, "자동 REMOTE edge 토글 누락")
check("!s_run_input.stable_active" in menu and
      "UltrasonicCtrl_StartUntimed" in menu, "수동 REMOTE level 운전 누락")
check("EXTERNAL_DEBOUNCE_COUNT 3U" in menu, "외부 접점 30ms 디바운스 누락")
check("spinner" in screen and "TIMEOVER" in screen and
      "TRANSDUCER ERROR" in screen and "OVER CURRENT" in screen,
      "운전/종료/보호 Error 화면 누락")
check("#define LCD_MAIN_REFRESH_MS 200U" in params,
      "RUN 회전 문자 200ms 갱신 설정 누락")
check('snprintf(prefix, 8U, "RUN%c", motion);' in screen and
      'sweep ? "RDY/SWP" : "RDY/"' in screen,
      "Sweep OFF RUN 회전 또는 RDY/ 표시 누락")
check("FormatRunPrefix(prefix, g_us_state.running, sweep)" in screen and
      '"485 COMM"' in screen,
      "수동 REMOTE Sweep 회전 또는 RS485 통신 표시 누락")
check(screen.count('SetLine(s_line1, "    K-SONICS");') >= 3,
      "K-SONICS 가운데 정렬이 부팅/대기/통신준비 화면에 적용되지 않음")
check("SettingsRecordV1_t" in storage and "SettingsRecordV2_t" in storage,
      "기존 Flash 설정과 v2 호환 처리 누락")

for pin, label in (
    ("PA10", "REMOTE"),
    ("PA11", "RUN_SW"),
    ("PA12", "SWEEP_SW"),
    ("PB6", "MODE"),
    ("PB7", "DOWN"),
    ("PB8-BOOT0", "UP"),
    ("PB9", "START_STOP"),
    ("PB0", "CLK_LCD"),
    ("PB1", "PWM_VR"),
    ("PB10", "LCD_E"),
    ("PB11", "LCD_RS"),
    ("PB12", "LCD_D4"),
    ("PB13", "LCD_D5"),
    ("PB14", "LCD_D6"),
    ("PB15", "LCD_D7"),
    ("PA4", "RUN_LED"),
    ("PA5", "BZ_OUT"),
    ("PA7", "PWM_OUTPUT"),
    ("PC15", "SPARE"),
):
    check(f"{pin}.GPIO_Label={label}" in ioc, f".ioc {pin}/{label} 누락")

check("PB1.Signal=ADC1_IN12" in ioc and
      "PA7.Signal=TIM3_CH2" in ioc,
      ".ioc PWM_VR 입력/PWM_OUTPUT 출력 기능 불일치")
check("ADC1.ClockPrescaler=ADC_CLOCK_ASYNC_DIV4" in ioc and
      "ADC2.ClockPrescaler=ADC_CLOCK_ASYNC_DIV4" in ioc,
      ".ioc ADC1/ADC2 42.5MHz 분주 설정 누락")
check("RCC_PERIPHCLK_ADC12" in system_clock and
      "RCC_ADC12CLKSOURCE_SYSCLK" in system_clock and
      "HAL_RCCEx_PeriphCLKConfig" in system_clock,
      "펌웨어 ADC12 kernel clock 선택 누락")

check("PF0-OSC_IN.Signal=RCC_OSC_IN" in ioc and
      "PF1-OSC_OUT.Signal=RCC_OSC_OUT" in ioc,
      ".ioc HSE PF0/PF1 설정 누락")
check("RCC.OscillatorType=RCC_OSCILLATORTYPE_HSE" in ioc and
      "RCC.PLLSourceVirtual=RCC_PLLSOURCE_HSE" in ioc,
      ".ioc HSE PLL 설정 누락")
check("#define LCD_RS_PIN GPIO_PIN_11" in config and
      "#define LCD_EN_PIN GPIO_PIN_10" in config,
      "74HCT574 Q0/Q1 기준 LCD RS/E 교차 매핑 불일치")
check("#define PWM_VR_PIN GPIO_PIN_1" in config and
      "#define PWM_VR_ADC_CHANNEL ADC_CHANNEL_12" in config and
      "#define PWM_OUTPUT_PIN GPIO_PIN_7" in config,
      "config.h PB1 PWM_VR/PA7 PWM_OUTPUT 매핑 불일치")
check("ADC_Control_GetPwmVrDutyLimit()" in main and
      "UltrasonicCtrl_SetDuty" in main,
      "PB1 PWM_VR에서 PA7 Duty로 이어지는 main 데이터 흐름 누락")
check("ADC_Control_GetPwmVrNormalized()" in screen and
      "ADC_Control_GetCurrentNormalized()" not in screen,
      "LCD PL이 PB1 PWM_VR 값이 아닌 다른 ADC를 표시함")
check("PWM_VR_SAMPLE_INTERVAL_MS 5U" in params and
      "ADC_FILTER_SAMPLES 8U" in params and
      "ADC_HYSTERESIS 3U" in params,
      "PB1 5ms/40ms 이동평균 또는 0.3% deadband 누락")
check("s_pwm_vr_sample_tick" in adc and
      "filter->initialized" in adc,
      "PB1 고정주기 샘플링 또는 최초 window 초기화 누락")
check("GetStablePowerPercent" in screen and
      "PL_DISPLAY_HYSTERESIS_01PCT" in screen,
      "LCD PL 표시 히스테리시스 누락")
check("MapCommandToOutputPercent" in screen and
      "PWM_OUTPUT_DUTY_MIN" in screen and
      "PWM_OUTPUT_DUTY_MAX" in screen,
      "LCD PL 5~100% 재매핑 누락")
check("#define SOFT_START_DURATION_MS 1500U" in params,
      "소프트 스타트 1.5초 설정 누락")
check("g_us_state.running ? g_us_state.target_freq : 0U" in screen,
      "출력 정지 중 00.0kHz 표시 조건 누락")
check("effective_duty * elapsed" in ctrl and
      "SOFT_START_DURATION_MS" in ctrl,
      "볼륨 목표값까지 선형 소프트 스타트 처리 누락")
check("if (!g_us_state.running)" in ctrl and
      "g_us_state.current_duty = EffectiveTargetDuty();" in ctrl,
      "정지 상태 Local/PLC 출력의 PA7 즉시 반영 누락")
check('status = "485 ON"' in screen,
      "Modbus RUN 중 485 ON 표시 누락")
check('"RDY/SWP 485 COMM" : "RDY/    485 COMM"' in screen,
      "Local Lock 정지 중 TIME 대신 485 COMM 표시 누락")
check("IsLocalVrPreviewActive" in screen and '"VR:%3u%% MANUAL"' in screen,
      "통신 중 PB1 수동 조작 Preview 누락")
check("UltrasonicCtrl_GetPotOverrideCounter" in screen,
      "실제 PB1 수동 출력 반영과 LCD Preview 연결 누락")
check("UltrasonicCtrl_IsPlcOutputControl()" in screen and
      "g_us_state.output_command * 100U" in screen,
      "통신 중 LCD PL에 실제 PLC 출력값 반영 누락")
check("Modbus_IsCommunicationActive" not in screen,
      "Lock 해제 후 최근 통신 Frame 때문에 485 COMM 화면이 잠시 남음")
check("SetAlignedFields(s_line0, prefix, status);" in screen and
      "SetAlignedFields(line, power_text, frequency_text);" in screen,
      "485 ON 또는 운전 주파수의 LCD 우측 끝 정렬 누락")

if failures:
    print("[FAIL] 기본조작 매뉴얼 계약 테스트")
    for failure in failures:
        print(f"  - {failure}")
    sys.exit(1)

print("[PASS] 기본조작 매뉴얼 계약 테스트")
print("  LCD1602 16x2, 기본 10 MIN, MIN/SEC/CONT")
print("  RUN_SW 수동/자동, REMOTE level/edge, TIMEOVER")
