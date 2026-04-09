---
name: stm32g4-dev
description: "STM32G4 시리즈 펌웨어 개발 지원. Use when: writing STM32G4 HAL driver code, configuring peripherals (HRTIM, TIM, ADC, COMP, DAC, USART, GPIO, DMA), debugging register-level issues, setting up clocks (PLL 170MHz + HRTIM DLL 5.44GHz), implementing HRTIM PWM/dead-time (184ps), ADC calibration, Modbus RTU RS-485, LCD HD44780 4-bit GPIO, 6-button debouncing, 8-LED indicator control. Covers G4-specific differences from F1/F4 series and HRTIM-specific patterns."
---

# STM32G4 시리즈 개발 스킬 (메가소닉 발진기 대응)

## 적용 대상

- STM32G474MET6 (LQFP80) — Cortex-M4, 170MHz, Flash 512KB, SRAM 128KB
- **HRTIM 6개 Timer Unit** (5.44GHz 유효 클럭, 184ps 분해능)
- CMake + gcc-arm-none-eabi 빌드 환경
- STM32 HAL 드라이버 기반 C11 펌웨어

## 핵심 체크리스트

모든 STM32G4 코드 작성/리뷰 시 아래 항목을 반드시 확인:

1. **HRTIM DLL 캘리브레이션 필수** — `HAL_HRTIM_DLLCalibrationStart()` + `HAL_HRTIM_PollForDLLCalibration()` 호출 전 HRTIM 사용 금지. 주기적 자동 캘리브레이션 활성화 권장.
2. **HRTIM 출력 활성화** — `HAL_HRTIM_WaveformOutputStart(&hhrtim, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2)` 호출 필수
3. **HRTIM Period/Compare 유효 범위** — 0x0003~0xFFFD (DLL 캘리브레이션 미완료 시 ×32 배율 미적용)
4. **HRTIM 핀 AF13** — PA8(HRTIM1_CHA1), PA9(HRTIM1_CHA2) → AF13 설정
5. **ADC 캘리브레이션 필수** — `HAL_ADCEx_Calibration_Start()` 없이 ADC 사용 금지
6. **USART 레지스터 구조** — G4는 `ISR/ICR/TDR/RDR` 분리 구조 (F1의 `SR/DR` 아님)
7. **Flash 쓰기 단위** — 더블워드(64비트) 정렬 필수, 페이지 크기 4KB (G474 기준)
8. **GPIO 속도** — HRTIM PWM/고속 통신 핀은 `GPIO_SPEED_FREQ_VERY_HIGH` 설정
9. **volatile 선언** — ISR ↔ main 공유 변수 반드시 `volatile`
10. **동적 메모리 금지** — `malloc`/`free`/`calloc` 사용하지 않음
11. **LCD LQFP80 재배치** — PD3~PD5 미존재 → LCD D5=PB12, D6=PB13, D7=PB15

## 절차

### 주변장치 초기화 코드 작성 시

1. [HAL 코딩 패턴](./references/hal-patterns.md) 참조하여 초기화 순서 확인
2. [G4 레지스터 차이점](./references/register-quirks.md) 참조하여 F1/F4 대비 변경점 확인
3. [코드 템플릿](./references/code-templates.md) 에서 주변장치별 보일러플레이트 활용

### 디버깅 시

1. 클럭 설정 확인 (HSE 8MHz → PLL → 170MHz, APB1/APB2 모두 170MHz)
2. **HRTIM DLL 캘리브레이션 완료 상태** 확인 (미완료 시 Period 값이 ×1 모드로 동작)
3. GPIO AF(Alternate Function) 번호 확인 — HRTIM은 AF13, USART2는 AF7
4. 인터럽트 우선순위 충돌 여부 확인
5. 레지스터 직접 접근 시 G4 레퍼런스 매뉴얼(RM0440) 기준으로 작성

### 빌드/플래시

```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target flash          # ST-Link
cmake --build build --target flash_openocd  # OpenOCD
```

## 코딩 규칙 요약

- 한글 주석 표준
- 함수명: `모듈명_동작()` (예: `HRTIM_PWM_SetFrequency()`, `LCD_SetCursor()`)
- 파일명: 소문자 + 언더스코어 (예: `hrtim_pwm.c`, `megasonic_ctrl.c`)
- 매직 넘버 금지 — `#define` 또는 `enum` 사용
- ISR에서 플래그만 설정, 메인 루프에서 처리
- 모듈 간 getter/setter 함수로 데이터 교환
