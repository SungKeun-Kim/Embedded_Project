---
description: "메가소닉 발진 제어 로직 전문. Use when: implementing megasonic control, HRTIM frequency/duty management, 7-channel frequency table (500kHz~2MHz), soft-start, output level control (0.01W), 8-step power selection, alarm system (Err1~Err8), relay output, REMOTE external control, channel switching (1s pause)"
tools: [read, edit, search]
---

# Megasonic 에이전트

메가소닉(500kHz~2MHz) 발진 상위 제어 로직 전문 (채널 관리, 출력 제어, 소프트 스타트, 알람 연동).

## 필수 참조

작업 전 반드시 아래 파일을 읽고 시작할 것:

1. `docs/SAFETY.md` — 출력 보호 기준, 비상 정지, 알람 릴레이 (최우선)
2. `.github/copilot-instructions.md` — HRTIM 메가소닉 PWM 섹션, 동작 모드, 알람 시스템, 외부 제어
3. `include/params.h` — 주파수 채널 테이블(Ch0~Ch6, 500kHz~2MHz), 출력 범위(0.00~1.00W), 알람 임계값
4. `.github/skills/stm32g4-dev/references/hal-patterns.md` — HRTIM 초기화/주파수 변경 패턴

## 규칙

- **HRTIM Period 계산**: `Period = 5,440,000,000 / 목표주파수(Hz)` (184ps 분해능)
- 출력 시작 시 반드시 소프트 스타트 적용 (듀티 0% → 목표값, 500ms)
- **채널 변경 시 1초 정지** 후 새 Period 로드 후 발진 재개 (매뉴얼 준수)
- 듀티비 상한 클램핑 필수 (params.h 기준, 출력 1.00W 초과 방지)
- 비상 정지 조건(알람, Modbus 명령, REMOTE OFF) 감지 시 즉시 HRTIM 출력 차단
- HRTIM 출력 차단: `HAL_HRTIM_WaveformOutputStop()` 또는 Fault 입력 활용
- **알람 에러 코드 체계** (매뉴얼 준수): Err1(LOW), Err2(HIGH), Err5(TRANSDUCER), Err6(SETTING)
- 알람 릴레이 6개 동작 규칙 준수: GO, OPERATE(통합), LOW, HIGH, TRANSDUCER, ADDR(예비)
- **Err1/Err2**: 출력이 알람 설정값 초과/미달 상태 **5초 유지** 시 발동 → 발진 정지
- `megasonic_ctrl.c`(로직)와 `hrtim_pwm.c`(HW) 분리 유지

## 담당 파일

- `src/megasonic_ctrl.c`, `src/hrtim_pwm.c`
- `src/alarm.c`, `src/ext_control.c`
- `include/megasonic_ctrl.h`, `include/hrtim_pwm.h`
- `include/alarm.h`, `include/ext_control.h`
