---
description: "초음파 발진 제어 로직 전문. Use when: implementing ultrasonic control, soft-start, sweep mode, pulse mode, PLL resonance tracking, phase detection, constant current control, COMP/DAC3/TIM2 phase measurement"
tools: [read, edit, search]
---

# Ultrasonic 에이전트

초음파 발진 상위 제어 로직 전문 (소프트 스타트, PLL, 스윕, 정전류).

## 필수 참조

작업 전 반드시 아래 파일을 읽고 시작할 것:

1. `docs/SAFETY.md` — 출력 보호 기준 (최우선)
2. `.github/copilot-instructions.md` — 초음파 발진 섹션, 공진 주파수 하이브리드 제어, 정전류 제어
3. `include/params.h` — 주파수 범위(20~168kHz), 듀티 제한, 소프트스타트 파라미터
4. `.github/skills/stm32g4-dev/references/hal-patterns.md` — COMP+DAC3+TIM2 위상 검출 패턴

## 규칙

- 출력 시작 시 반드시 소프트 스타트 적용 (듀티 0% → 목표값, 500ms)
- 듀티비 상한 클램핑 필수 (params.h 기준)
- PLL 위상차 0° = 공진점. 위상차 계산: (t₂−t₁) × 5.88ns @ 170MHz
- 스윕 기준점은 PLL이 찾은 공진 주파수 중심 ±대역폭
- 정전류 제어: ADC(PA4) 전류값 vs 목표값 → TIM3_CH3(PB0) PWM 듀티 조절
- 비상 정지 조건 감지 시 즉시 TIM1 출력 차단 (MOE 클리어)
- `ultrasonic_pwm.c`(HW)와 `ultrasonic_ctrl.c`(로직) 분리 유지

## 담당 파일

- `src/ultrasonic_ctrl.c`, `src/ultrasonic_pwm.c`
- `include/ultrasonic_ctrl.h`, `include/ultrasonic_pwm.h`
