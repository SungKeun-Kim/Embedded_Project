# Codex 지침 — ms_main_board_st

## 프로젝트 요약

이 저장소는 커스텀 메가소닉 발진기 메인보드용 STM32G474 펌웨어입니다. 커스텀 LQFP64 보드를 대상으로 하며, STM32Cube HAL, CMake, Ninja, `gcc-arm-none-eabi` 기반으로 빌드합니다.

현재 코드 기준은 오래된 문서에 남아 있는 512 KB RETx 사양이 아니라, STM32G474RBT6/RB 계열 메모리 제약에 맞춰져 있습니다. 활성 링커 스크립트는 `ldscripts/STM32G474RBTx_FLASH.ld`이며, 애플리케이션 Flash는 112 KB로 제한되고 마지막 16 KB는 설정 저장 영역으로 예약됩니다. RAM은 일반 SRAM 96 KB와 CCM 32 KB로 나누어 취급합니다.

펌웨어가 제어하는 주요 기능:

- PA8/PA9의 HRTIM Timer A 상보 PWM으로 메가소닉 출력 생성.
- `include/params.h` 기준 400 kHz부터 2.2 MHz까지 10개 주파수 채널.
- PC9/PC8/PC7/PC6의 LC 릴레이 선택으로 공진 매칭.
- PA4의 DAC1_OUT1로 LM5005 Buck 출력 제어.
- PB10-PB15 공유 버스와 PC14/PC15 래치로 LCD1602 및 LED 6개 제어.
- PC0-PC4의 Active LOW 버튼 5개.
- PA0/PA1/PA6/PA7의 ADC 계측.
- USART2 PA2/PA3 및 PB0/PB1 DE/RE를 사용하는 RS-485 Modbus RTU.
- `settings_store.c`의 `0x0800F000`, `0x0800F800` ping-pong slot 기반 Flash 설정 저장.

## 문서 작성 언어

- 지침서, 설명서, 작업 요약, 코드 리뷰 메모는 가능한 한 한글로 작성합니다.
- 파일명, 함수명, 타입명, 레지스터명, 명령어, 빌드 타겟, 표준 용어처럼 영문 표기가 더 정확한 항목은 영문 그대로 둡니다.
- 기존 코드에 한글 주석이 있으면 유지하고, 새 주석도 문맥상 자연스러우면 한글로 작성합니다.
- 사용자에게 답변할 때도 특별한 요청이 없으면 한글을 기본으로 사용합니다.

## 소스 구조

- `src/`, `include/`: 현재 활성 애플리케이션 펌웨어 모듈.
- `CMakeLists.txt`: 루트 빌드 로직. 애플리케이션 소스는 `file(GLOB APP_SOURCES "src/*.c")`로 수집됩니다.
- `CMakePresets.json`: 권장 프리셋. 주로 `stm32g474-debug`, `stm32g474-release`를 사용합니다.
- `Drivers/`: STM32 HAL/CMSIS 소스. 이 트리는 의도적으로 일부만 남아 있을 수 있습니다.
- `.cache/STM32CubeG4/Drivers`: `Drivers/`가 불완전할 때 사용하는 fallback 드라이버 위치.
- `cmake/stm32cubemx/`: CubeMX 생성물 연동 자료.
- `Core/`: CubeMX 생성 참조 소스. 사용자가 명시하지 않는 한 주 애플리케이션 소스로 보지 않습니다.
- `docs/`: 아키텍처, 안전 기준, 워크플로우, 매뉴얼, 핀 테이블, 생성 이미지 자료.

## 빌드 및 플래시

프리셋 사용을 우선합니다.

```powershell
cmake --preset stm32g474-debug
cmake --build --preset stm32g474-debug
```

사용 가능한 경우 유용한 타겟:

```powershell
cmake --build --preset stm32g474-debug --target flash_stm32prog
cmake --build --preset stm32g474-debug --target flash
cmake --build --preset stm32g474-debug --target flash_openocd
cmake --build --preset stm32g474-debug --target docs_html
```

프리셋은 `ARM_TOOLCHAIN_PATH`를 `C:/ST/STM32CubeCLT_1.19.0/GNU-tools-for-STM32`로 설정합니다. 로컬 환경이 다르면 애플리케이션 코드를 바꾸지 말고 프리셋 또는 툴체인 경로를 조정합니다.

## 기준 문서 우선순위

문서끼리 충돌하면 아래 순서로 판단합니다.

1. `src/`, `include/`의 현재 활성 소스와 헤더.
2. `CMakeLists.txt`, `CMakePresets.json`, `ldscripts/STM32G474RBTx_FLASH.ld`.
3. 보호 및 안전 규칙은 `docs/SAFETY.md`.
4. 모듈 간 계약은 `docs/ARCHITECTURE.md`.
5. `.github/copilot-instructions.md`, `docs/WORKFLOW.md`, `docs/PLANS.md`는 배경 자료 또는 과거 계획으로 참고.

주의할 오래된 내용: 일부 문서에는 아직 STM32G474RET6/512 KB, 7개 주파수 채널, `hrtim_pwm.*`, 오래된 핀맵이 남아 있습니다. 현재 코드는 `megasonic_pwm.*`, 10개 주파수 채널, RB 계열 메모리 및 저장소 제약을 우선합니다.

## 코딩 규칙

- 변경 범위는 요청된 모듈 또는 동작에 가깝게 제한합니다.
- 기존 모듈 API, 이름 규칙, 파일 구조를 우선합니다.
- 기본은 HAL 사용입니다. 직접 레지스터 접근은 기존 모듈이 이미 그렇게 하거나 타이밍상 필요한 경우에만 사용합니다.
- 동적 메모리 할당은 사용하지 않습니다. ISR 또는 hot path에서 큰 스택 배열을 피합니다.
- ISR에서는 플래그 설정, 바이트 저장 등 최소 작업만 수행하고 무거운 처리는 메인 루프로 넘깁니다.
- ISR과 main 사이 공유 변수는 `volatile`을 사용하고, 다중 바이트 일관성이 필요하면 보호 구간을 둡니다.
- 기존 한글 주석은 유지합니다. 새 주석은 복잡한 임베디드 동작을 설명할 때만 짧게 추가합니다.
- PA13/PA14 SWD 핀은 보존합니다.
- 사용자가 명시하지 않는 한 생성 파일, HAL/CMSIS vendor 파일은 수정하지 않습니다.
- 사용자가 명시하지 않는 한 이 펌웨어 프로젝트 밖의 KiCad 파일은 수정하지 않습니다.

## 안전 규칙

출력, 전력, ADC, 알람, Modbus, HRTIM 관련 작업은 `docs/SAFETY.md`를 따릅니다.

핵심 보호 규칙:

- HRTIM 출력은 초기화와 명시적 Start가 완료되기 전까지 꺼진 상태를 유지합니다.
- 고해상도 PWM 사용 전 HRTIM DLL calibration이 필요합니다.
- Period 레지스터 쓰기 전 HRTIM period 범위를 검증합니다.
- deadtime은 `HRTIM_DEADTIME_MIN_NS`, `HRTIM_DEADTIME_MAX_NS` 범위로 제한합니다.
- 출력 시작 및 주파수 변경 후 재시작에는 soft start를 적용합니다.
- emergency stop 경로에서는 PWM을 즉시 정지합니다.
- Modbus 쓰기는 값 범위와 모드별 접근 제한을 검증합니다.
- ADC 기반 판단은 calibration과 안정된 filtering 이후에만 사용합니다.
- Flash 쓰기는 8-byte alignment를 지키고 애플리케이션 Flash와 겹치지 않게 합니다.

## 현재 메인 루프 구조

`src/main.c`의 현재 초기화 순서:

```text
HAL_Init
SystemClock_Config
GPIO_Init_All
BuckDAC_Init
MegasonicPWM_Init
MegasonicCtrl_Init
ADC_Control_Init
LCD_Init
Button_Init
Menu_Init
Modbus_Init
```

현재 메인 루프:

```text
ADC_Control_Process
Menu_Update
MenuScreen_Refresh
MegasonicCtrl_Update
Modbus_Process
```

버튼 처리는 SysTick에서 `Button_Process`를 통해 수행되는 구조입니다.

## 주요 모듈

- `config.h`: GPIO 및 peripheral pin map의 기준.
- `params.h`: 상수, 범위, 기본값, mode enum, 주파수 profile, deadtime table, Modbus 기본값, firmware version의 기준.
- `system_clock.*`: HSE 8 MHz에서 SYSCLK 170 MHz 설정.
- `gpio_init.*`: 보드 GPIO 초기화와 안전 시작 상태.
- `megasonic_pwm.*`: HRTIM Timer A 하드웨어 PWM, deadtime, start/stop, frequency/duty 적용.
- `megasonic_ctrl.*`: run/stop, soft start, operating mode, frequency/duty state, LC relay 제어, resonance scan API.
- `buck_dac.*`: DAC 기반 Buck 출력 제어.
- `adc_control.*`: ADC sampling/filtering 및 계측 getter.
- `button.*`: Active LOW 버튼 debounce/event 처리.
- `lcd1602.*`, `lcd1602_hw.*`: 래치 버스 기반 LCD1602 제어.
- `menu.*`, `menu_screen.*`: UI 상태, profile 편집, 화면 렌더링, auto-tune 상태.
- `modbus_crc.*`, `modbus_rtu.*`, `modbus_regs.*`: Modbus RTU protocol 및 register map.
- `settings_store.*`: Flash 설정 저장. 주소, page 번호, CRC, sequence 처리를 매우 조심합니다.

## Git 및 작업공간 주의

이 작업공간은 이미 변경 사항이 많을 수 있습니다. 편집 전 `git status --short`로 상태를 확인합니다. 사용자의 기존 변경을 되돌리지 않습니다. 작업 대상 파일에 기존 변경이 있으면 내용을 읽고 그 변경을 보존하며 작업합니다.

`build/` 아래 산출물과 `.bin`, `.hex`, `.elf`, `.map` 파일은 빌드 결과일 수 있습니다. 사용자가 명시하지 않는 한 소스 변경으로 취급하지 않습니다.

## 권장 검증

펌웨어 변경 후 최소 검증:

```powershell
cmake --build --preset stm32g474-debug
```

CMake 또는 빌드 시스템 변경 후:

```powershell
cmake --preset stm32g474-debug
cmake --build --preset stm32g474-debug
```

안전 민감 출력 변경은 `docs/SAFETY.md` 기준으로 영향 경로를 직접 점검하고, 오실로스코프, ST-Link, Modbus Poll, 실제 보드 테스트처럼 하드웨어에서만 가능한 검증이 남아 있으면 결과 보고에 명확히 적습니다.
