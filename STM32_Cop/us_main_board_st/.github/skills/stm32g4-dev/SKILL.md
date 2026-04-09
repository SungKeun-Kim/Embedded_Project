---
name: stm32g4-dev
description: "STM32G4 시리즈 펌웨어 개발 지원. Use when: writing STM32G4 HAL driver code, configuring peripherals (TIM, ADC, COMP, DAC, USART, GPIO, DMA), debugging register-level issues, setting up clocks (PLL 170MHz), implementing PWM/dead-time, ADC calibration, comparator zero-crossing, Modbus RTU RS485, LCD HD44780, button debouncing. Covers G4-specific differences from F1/F4 series."
---

# STM32G4 시리즈 개발 스킬

## 적용 대상

- STM32G4xx (특히 G474) Cortex-M4 MCU
- CMake + gcc-arm-none-eabi 빌드 환경
- STM32 HAL 드라이버 기반 C11 펌웨어

## 핵심 체크리스트

모든 STM32G4 코드 작성/리뷰 시 아래 항목을 반드시 확인:

1. **ADC 캘리브레이션 필수** — `HAL_ADCEx_Calibration_Start()` 없이 ADC 사용 금지
2. **TIM1(고급 타이머) MOE 비트** — `__HAL_TIM_MOE_ENABLE()` 또는 `HAL_TIM_PWM_Start()` + `HAL_TIMEx_PWMN_Start()` 호출 필수
3. **USART 레지스터 구조** — G4는 `ISR/ICR/TDR/RDR` 분리 구조 (F1의 `SR/DR` 아님)
4. **Flash 쓰기 단위** — 더블워드(64비트) 정렬 필수 (F1의 하프워드 아님)
5. **GPIO 속도** — 고속 PWM/통신 핀은 `GPIO_SPEED_FREQ_VERY_HIGH` 설정
6. **DAC3 = 내부 전용** — 외부 핀 없음, COMP INM 기준전압 전용
7. **volatile 선언** — ISR ↔ main 공유 변수 반드시 `volatile`
8. **동적 메모리 금지** — `malloc`/`free`/`calloc` 사용하지 않음

## 절차

### 주변장치 초기화 코드 작성 시

1. [HAL 코딩 패턴](./references/hal-patterns.md) 참조하여 초기화 순서 확인
2. [G4 레지스터 차이점](./references/register-quirks.md) 참조하여 F1/F4 대비 변경점 확인
3. [코드 템플릿](./references/code-templates.md) 에서 주변장치별 보일러플레이트 활용

### 디버깅 시

1. 클럭 설정 확인 (HSI 16MHz → PLL → 170MHz, APB1/APB2 모두 170MHz)
2. GPIO AF(Alternate Function) 번호 확인 — G4는 F1과 AF 매핑이 다름
3. 인터럽트 우선순위 충돌 여부 확인
4. 레지스터 직접 접근 시 G4 레퍼런스 매뉴얼(RM0440) 기준으로 작성

### 빌드/플래시

```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target flash          # ST-Link
cmake --build build --target flash_openocd  # OpenOCD
```

## 코딩 규칙 요약

- 한글 주석 표준
- 함수명: `모듈명_동작()` (예: `LCD_WriteString()`)
- 파일명: 소문자 + 언더스코어 (예: `modbus_rtu.c`)
- 매직 넘버 금지 — `#define` 또는 `enum` 사용
- ISR에서 플래그만 설정, 메인 루프에서 처리
- 모듈 간 getter/setter 함수로 데이터 교환
