#!/usr/bin/env python3
"""일반 GDT 정지의 안전 계약 검사. 실제 파형/소자 지연의 실측을 대체하지 않는다."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
pwm = (ROOT / "src/ultrasonic_pwm.c").read_text(encoding="utf-8")
ctrl = (ROOT / "src/ultrasonic_ctrl.c").read_text(encoding="utf-8")
params = (ROOT / "include/params.h").read_text(encoding="utf-8")


def body(source, name):
    start = re.search(r"\b" + name + r"\([^;{}]*\)\s*\{", source)
    assert start, name
    depth = 1
    for end in range(start.end(), len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start.end():end]
    raise AssertionError(name)


def constant(name):
    return int(re.search(r"#define " + name + r"\s+(\d+)U", params)[1])


normal = body(pwm, "UltrasonicPWM_StopNormal")
immediate = body(pwm, "UltrasonicPWM_Stop")
boundary = body(pwm, "ClampAtPeriodBoundary")
hold = body(pwm, "HoldGdtClamp")

# STOP 처리에서 어떤 경로도 SD나 HRTIM 출력을 다시 켜면 안 된다.
for part in (normal, immediate, boundary, hold):
    assert not re.search(r"HAL_GPIO_WritePin\([^;]*GPIO_PIN_SET", part)
    assert "WaveformOutputStart" not in part
    assert "OENR" not in part
    assert "HAL_Delay" not in part

# 보호 정지는 주기/클램프 대기 없이 유지한다.
assert "StopOutput(true);" in body(ctrl, "UltrasonicCtrl_EmergencyStop")
assert "ClampAtPeriodBoundary" not in immediate and "HoldGdtClamp" not in immediate
assert immediate.index("GPIO_PIN_RESET") < immediate.index("ODISR")
stop = body(ctrl, "StopOutput")
assert stop.index("SweepTimerStop();") < stop.index("UltrasonicPWM_StopNormal();")
assert re.search(r"if \(immediate \|\| g_us_state.error_active\)\s*\{\s*"
                 r"UltrasonicPWM_Stop\(\);\s*\}\s*else\s*\{\s*"
                 r"UltrasonicPWM_StopNormal\(\);", stop)

# 정상 정지도 PA7은 첫 단계에서 차단하고 다시 쓸 수 없도록 한다.
assert normal.index("UltrasonicPWM_ForceControlOutputOff();") < normal.index("ClampAtPeriodBoundary()")
assert normal.index("s_output_running = false;") < normal.index("__disable_irq();")
for gate in ("!s_ready", "!was_running", "!s_stop_clock_ready",
             "__get_IPSR() != 0U", "__get_PRIMASK() != 0U",
             "CoreDebug_DEMCR_TRCENA_Msk", "DWT_CTRL_CYCCNTENA_Msk"):
    assert gate in normal, gate

# 경계 검출 중만 IRQ 차단. 실패하면 IRQ 복원 전 즉시 차단한다.
assert re.search(r"if \(!clamped\)\s*\{\s*UltrasonicPWM_Stop\(\);\s*\}"
                 r"\s*__enable_irq\(\);", normal)
assert normal.index("__enable_irq();") < normal.index("HoldGdtClamp();")
assert re.search(r"HoldGdtClamp\(\);\s*UltrasonicPWM_Stop\(\);", normal)

# 타이머/DWT 고장 시 무한 루프 방지, 32-bit counter wrap에 안전한 경과시간.
for part in (boundary, hold):
    assert "attempts-- != 0U" in part
    assert "(uint32_t)(DWT->CYCCNT - started)" in part
assert "current < previous" in boundary
assert "current >= window_counts" in boundary
assert boundary.index("ODISR = HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2") < boundary.index("sMasterRegs.MCR")
assert "HAL_GPIO_ReadPin(SONIC_ON_PORT, SONIC_ON_PIN) == GPIO_PIN_RESET" in hold

# ODISR의 IDLE 상태가 HIGH나 Hi-Z가 아닌 양 출력 LOW여야 한다.
init = body(pwm, "UltrasonicPWM_Init")
assert "output_cfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;" in init
assert "output_cfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;" in init
assert init.count("output_cfg.IdleLevel =") == 1
assert init.count("output_cfg.Polarity =") == 1

# 실제 회로 감쇠시간을 모델링하지 않고 설정된 시간 상한과 전 대역 주기만 확인한다.
timeout_us = constant("GDT_STOP_SYNC_TIMEOUT_US")
clamp_us = constant("GDT_STOP_CLAMP_US")
assert 0 < clamp_us <= 1000
assert 1_000_000 / (constant("FREQ_MIN") * 100) < timeout_us <= 100
assert 26 < 1_000_000 / 37700 < 27
assert "__HAL_HRTIM_SETCOUNTER(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, 0U)" in body(
    pwm, "UltrasonicPWM_Start")
print(f"[PASS] GDT 일반 정지 계약: 경계 timeout {timeout_us} us, LOW 유지 {clamp_us} us, 보호 정지 즉시")
