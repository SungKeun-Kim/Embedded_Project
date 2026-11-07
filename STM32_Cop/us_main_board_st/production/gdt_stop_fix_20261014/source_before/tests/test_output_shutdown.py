#!/usr/bin/env python3
"""출력 차단 회귀 검사: 실제 C 소스의 차단 경로/재활성화 경계를 검사한다.

하드웨어 에뮬레이션이 아니므로 핀 전압/레지스터의 전기적 동작은 실측이 필요하다.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
pwm = (ROOT / "src/ultrasonic_pwm.c").read_text(encoding="utf-8")
ctrl = (ROOT / "src/ultrasonic_ctrl.c").read_text(encoding="utf-8")
gpio = (ROOT / "src/gpio_init.c").read_text(encoding="utf-8")
selftest = (ROOT / "src/selftest.c").read_text(encoding="utf-8")


def body(source, name):
    match = re.search(r"\b" + name + r"\([^;{}]*\)\s*\{", source)
    assert match, f"함수 없음: {name}"
    start = match.end()
    depth = 1
    for pos in range(start, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[start:pos]
    raise AssertionError(f"함수 끝 없음: {name}")


def ordered(source, *tokens):
    pos = 0
    for token in tokens:
        found = source.find(token, pos)
        assert found >= 0, f"순서/호출 누락: {token}"
        pos = found + len(token)


# 부팅 및 부분 초기화 실패: PA7은 GPIO LOW, 타이머는 0%로 먼저 준비한다.
pa7_gpio = gpio.split("gpio.Pin = PHASE_PWM_PIN;", 1)[1].split(
    "HAL_GPIO_Init(PHASE_PWM_PORT, &gpio);", 1)[0]
assert "GPIO_MODE_OUTPUT_PP" in pa7_gpio
assert "HAL_GPIO_WritePin(PHASE_PWM_PORT, PHASE_PWM_PIN, GPIO_PIN_RESET)" in pa7_gpio
init = body(pwm, "UltrasonicPWM_Init")
ordered(init, "phase_pwm_cfg.Pulse = 0U", "HAL_TIM_PWM_Start",
        "s_phase_ready = true", "UltrasonicPWM_ForceControlOutputOff()",
        "HAL_GPIO_Init(PHASE_PWM_PORT", "s_ready = true")

# 정지 뒤 직접 SetDuty 호출도 출력 재개를 우회할 수 없다.
duty = body(pwm, "UltrasonicPWM_SetDuty")
guard = re.search(r"if \(!s_ready \|\| !s_output_running\)\s*\{([^}]+)\}", duty)
assert guard and "UltrasonicPWM_ForceControlOutputOff();" in guard[1]
assert "return;" in guard[1]
assert duty.index(guard[0]) < duty.index("__HAL_TIM_SET_COMPARE")

# 실제 LOW 강제는 preload 갱신보다 앞선다. 채널 Disable로 Hi-Z를 만들지 않는다.
off = body(pwm, "UltrasonicPWM_ForceControlOutputOff")
ordered(off, "if (!s_phase_ready)", "return;", "TIM_OCMODE_FORCED_INACTIVE",
        "__HAL_TIM_SET_COMPARE(&htim3_phase, TIM_CHANNEL_2, 0U)", "TIM_EGR_UG")
assert "HAL_TIM_PWM_Stop" not in off
assert "TIM_CCMR1_OC2M_3" in off

# 일반 정지/보호 정지/시간 만료가 같은 하위 차단 함수를 사용한다.
stop = body(pwm, "UltrasonicPWM_Stop")
ordered(stop, "GPIO_PIN_RESET", "s_output_running = false",
        "UltrasonicPWM_ForceControlOutputOff()", "ODISR =", "sMasterRegs.MCR")
assert "HAL_HRTIM_WaveformOutputStop" not in stop  # HAL_BUSY로 차단 누락 방지
upper_stop = body(ctrl, "StopOutput")
assert "UltrasonicPWM_Stop();" in upper_stop
assert "UltrasonicPWM_SetDuty" not in upper_stop
for caller in ("UltrasonicCtrl_Stop", "UltrasonicCtrl_EmergencyStop",
               "UltrasonicCtrl_Update"):
    assert "StopOutput();" in body(ctrl, caller), caller

# PB1/PLC 출력, 제어권 변경, 오류 Reset은 대기 중 출력을 다시 올리지 않는다.
for setter in ("UltrasonicCtrl_SetDuty", "UltrasonicCtrl_SetOutputCommand",
               "UltrasonicCtrl_SetPlcOutputControl"):
    setter_body = body(ctrl, setter)
    assert "UltrasonicPWM_SetDuty" not in setter_body, setter
    assert "current_duty =" not in setter_body, setter
reset = body(ctrl, "UltrasonicCtrl_ResetError")
assert "UltrasonicPWM_SetDuty" not in reset
ordered(reset, "if (g_us_state.running)", "return false;",
        "g_us_state.current_duty = 0U", "UltrasonicPWM_Stop()")

# Start의 실패 경로는 모두 차단하고 실패를 상위로 전달한다.
start = body(pwm, "UltrasonicPWM_Start")
failures = re.findall(r"UltrasonicPWM_Stop\(\);\s*return false;", start)
assert len(failures) == 4  # 미초기화, dead-time, counter, output 시작 실패
assert len(re.findall(r"return false;", start)) == len(failures)
ordered(start, "HAL_HRTIM_WaveformCountStart", "HAL_HRTIM_WaveformOutputStart",
        "s_output_running = true", "UltrasonicPWM_SetDuty(0U)", "GPIO_PIN_SET")
upper_start = body(ctrl, "StartOutput")
ordered(upper_start, "if (!UltrasonicPWM_Start())",
        "TripProtection(ULTRASONIC_ERROR_OUTPUT)", "return;",
        "g_us_state.running = true", "SweepTimerStart()", "GOING_PORT")

# Pulse OFF에는 5%도 없어야 하고, 목표값 0인 ON 복귀도 반드시 적용되어야 한다.
update = body(ctrl, "UltrasonicCtrl_Update")
pulse = update.split("case MODE_PULSE:", 1)[1].split("case MODE_SWEEP:", 1)[0]
assert re.search(r"if \(s_pulse_on_phase\)\s*\{\s*"
                 r"UltrasonicPWM_SetDuty\(g_us_state.current_duty\);", pulse)
assert "UltrasonicPWM_ForceControlOutputOff();" in pulse
assert "current_duty !=" not in pulse
assert "g_us_state.mode != MODE_PULSE || s_pulse_on_phase" in body(
    ctrl, "UltrasonicCtrl_SetTuningDutyOverride")

# 정지 상태도 자기검증: PA7과 단일 HRTIM 채널 잔류를 무시하지 않는다.
assert "TIM_OCMODE_FORCED_INACTIVE" in body(selftest, "SelfTest_CheckDuty")
state_check = body(selftest, "CheckOutputState")
assert "== 0U" in state_check and "GPIO_PIN_RESET" in state_check
assert "CheckOutputState()" in body(selftest, "SelfTest_RunAll")

print("[PASS] 출력 차단 소스 계약: 부팅/Stop/보호/타이머/PLC/Reset/Pulse/시작 실패")
