# Copilot 지침 — STM32G474RB/RC 초음파 메인보드

## 프로젝트 개요

**STM32G474RBT6 회로도 기준** 초음파 발진기 메인보드 펌웨어. **CMake + STM32Cube HAL** 프레임워크로 빌드.
HRTIM을 이용한 초음파 PWM 생성, 내부 고속 비교기(COMP)를 활용한 위상차 위상 동기(PLL),
LCD1602 디스플레이, 택트 스위치 메뉴 조작, 가변 저항 출력 레벨 제어, Modbus RTU(RS485) 통신을 지원한다.

- 타겟: STM32G474RBT6 회로도 기준 (Cortex-M4, 170 MHz, FPU, HRTIM 및 고속 아날로그 내장)
- 주의: 기존 코드/링커/일부 문서에는 `STM32G474RC/RCTx` 표기가 남아 있을 수 있으므로, CubeMX 재생성 시 실제 BOM과 Flash 용량에 맞게 동기화한다.
- 보드: 커스텀 보드 (외부 HSE 크리스탈 또는 고정밀 내부 HSI 16MHz 사용)
- 언어: C11 (STM32 HAL 드라이버 기반)
- 빌드 시스템: CMake (gcc-arm-none-eabi 크로스 컴파일)
- CubeMX 전환: `cmake/stm32cubemx/` 존재 시 자동 전환 (회로 설계 완료 후)

## 빌드 및 업로드

```bash
# 빌드 디렉토리 생성 및 CMake 구성
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug

# 빌드
cmake --build build

# ST-Link로 업로드 (st-flash)
cmake --build build --target flash

# OpenOCD로 업로드
cmake --build build --target flash_openocd

# 클린 빌드
cmake --build build --target clean

# 시리얼 모니터 (디버그용, USART1 사용)
# 별도 터미널 도구 사용 (minicom, putty 등 115200 bps)
```

## 프로젝트 구조

기능별로 세분화하여 모듈 단위 독립성을 확보한다.

```
us_main_board_st/
├── .github/
│   └── copilot-instructions.md    # 본 지침서
├── CMakeLists.txt                  # CMake 빌드 (CubeMX 자동/수동 듀얼 모드)
├── cmake/
│   ├── gcc-arm-none-eabi.cmake    # ARM 크로스 컴파일 툴체인
│   └── stm32cubemx/               # (CubeMX 생성 시 자동 배치)
├── ldscripts/
│   └── STM32G474RCTx_FLASH.ld     # 링커 스크립트 (실제 RBT6 사용 시 용량/파일명 동기화 필요)
├── startup/
│   └── startup_stm32g474xx.s      # 벡터 테이블 + Reset_Handler (HRTIM 벡터 포함)
├── Drivers/                        # STM32CubeG4 HAL/CMSIS (수동 모드 시)
├── include/
│   ├── config.h                   # 핀 정의, GPIO 매핑
│   ├── params.h                   # 파라미터 상수, 범위, 기본값
│   ├── stm32g4xx_hal_conf.h       # HAL 모듈 활성화 설정
│   ├── stm32g4xx_it.h             # 인터럽트 핸들러 선언
│   ├── system_clock.h             # 시스템 클럭 설정
│   ├── gpio_init.h                # 전체 GPIO 초기화
│   ├── ultrasonic_pwm.h           # HRTIM/TIM PWM HW 제어 (주파수/듀티/데드타임)
│   ├── ultrasonic_ctrl.h          # 초음파 상위 제어 (소프트스타트, 모드)
│   ├── lcd1602.h                  # LCD 상위 API (문자열, 커서)
│   ├── lcd1602_hw.h               # LCD 하위 GPIO 니블 전송
│   ├── button.h                   # 택트 스위치 (디바운스, 이벤트)
│   ├── adc_control.h              # ADC 가변저항 (필터링, 매핑)
│   ├── menu.h                     # 메뉴 FSM 상태 전이
│   ├── menu_screen.h              # LCD 화면 렌더링
│   ├── modbus_rtu.h               # Modbus RTU 프로토콜
│   ├── modbus_crc.h               # CRC-16 테이블 룩업
│   └── modbus_regs.h              # 레지스터 맵 R/W
├── src/
│   ├── main.c                     # 엔트리포인트, 메인 루프
│   ├── system_clock.c             # HSI/HSE→PLL→170MHz
│   ├── gpio_init.c                # 전체 핀 일괄 초기화
│   ├── ultrasonic_pwm.c           # HRTIM PA8/PA9 출력 + 데드타임 (기존 TIM1 코드 전환 필요)
│   ├── ultrasonic_ctrl.c          # 소프트스타트, 펄스/스윕 모드 관리
│   ├── lcd1602.c                  # HD44780 초기화, 문자열 출력
│   ├── lcd1602_hw.c               # 4비트 니블 전송, DWT μs 지연
│   ├── button.c                   # 디바운싱, 장기누름, 반복이벤트
│   ├── adc_control.c              # 이동평균 필터, 데드존, 히스테리시스
│   ├── menu.c                     # FSM 네비게이션, 값 편집
│   ├── menu_screen.c              # 메뉴 상태별 LCD 렌더링
│   ├── modbus_rtu.c               # USART2+TIM4, FC03/06/10 처리
│   ├── modbus_crc.c               # CRC-16 256-entry 테이블
│   ├── modbus_regs.c              # 레지스터 읽기/쓰기 핸들러
│   ├── stm32g4xx_it.c             # SysTick, USART2, TIM4 ISR
│   └── syscalls.c                 # Newlib 스텁
└── docs/                          # 문서 (한글)
```

### 모듈 세분화 원칙

| 영역   | 분리                                                                      | 이유                                   |
| ------ | ------------------------------------------------------------------------- | -------------------------------------- |
| 초음파 | `ultrasonic_pwm` (HW) + `ultrasonic_ctrl` (로직)                          | PWM 레지스터와 모드 관리 분리          |
| LCD    | `lcd1602` (API) + `lcd1602_hw` (GPIO)                                     | 하드웨어 의존성 격리                   |
| 메뉴   | `menu` (FSM) + `menu_screen` (렌더링)                                     | 로직과 표시 분리                       |
| Modbus | `modbus_rtu` (프로토콜) + `modbus_crc` (CRC) + `modbus_regs` (레지스터맵) | 기능별 독립 테스트 가능                |
| 설정   | `config.h` (핀 정의) + `params.h` (파라미터/범위)                         | 하드웨어 매핑과 애플리케이션 상수 분리 |

---

## CMake 빌드 설정

```cmake
# 듀얼 모드 빌드:
# 1) cmake/stm32cubemx/CMakeLists.txt 존재 시 → CubeMX 자동 생성 구성 사용
# 2) 없을 시 → Drivers/ 직접 참조 (수동 구성)

# 주요 정의
target_compile_definitions(... PRIVATE
    USE_HAL_DRIVER
    STM32G474xx
)
```

> 회로 설계 완료 후 CubeMX에서 CMake 프로젝트 생성 시 `cmake/stm32cubemx/` 폴더에 배치하면 자동으로 CubeMX 모드로 전환된다.

---

## 아키텍처

### 하드웨어 권장 구성 (초음파 및 디지털 파워 최적화)

기존 아날로그 로직 IC 위치를 STM32G4의 내장 하드웨어(고속 아날로그 블록)로 대체하여 원가 및 공간을 극적으로 절감한다.

- **전류 검출 (Dual Sensing 하이브리드 방식 권장)**:
  - **1) AC 입력단 정전류 제어용 (ACS722 등 3.3V 홀센서)**: 메인 220V 전원이 들어오는 초입부 또는 정류 보드 단에서 장비 전체의 소비 전류(RMS) 크기를 읽어내는 목적. 저주파(50/60Hz) 변동분을 읽어 조광기 위상 제어(정전류 제어)를 할 때 이상적입니다.
  - **2) 트랜스듀서 출력단 PLL 위상 검출용 (고주파 CT 센서)**: 수십 kHz로 스위칭하는 최종 초음파 출력 라인에서 전압과 전류의 나노초 단위 위상차(Phase)를 읽어내는 목적. 고주파 대역폭 왜곡이 없는 CT(Current Transformer)를 사용하여 내부 컴퍼레이터에 직결해야만 자동 공진점 추적(PLL)이 실패하지 않습니다.
- **RS485 트랜시버**: **MAX3485, SP3485** (기존 5V용 MAX485 대신 3.3V 구동 및 3.3V 로직 레벨을 지원하는 핀투핀 호환 IC 사용 권장. 레벨 시프터 생략 가능)
- **FET 드라이버**:
  - **단일 입력형 (예: IR2104, IR2111)**: STM32에서 PWM 신호 1개만 입력받아 내부 로직으로 하이사이드/로우사이드를 쪼개고 **고정된 데드타임**을 삽입하여 구동하는 방식입니다. 단순하지만 MCU가 deadtime을 직접 조절하기 어렵습니다.
  - **독립 입력형 (예: IR2110, IR2113)**: STM32에서 2개의 PWM 신호(`PA8/HRTIM1_CHA1`, `PA9/HRTIM1_CHA2`)를 따로 받아 STM32 내부에서 설정한 매우 정밀한 가변 데드타임을 그대로 FET에 전달합니다. 주파수 스윕이나 듀티의 한계 제어를 MCU가 완벽히 통제할 수 있어 현재 설계에서 가장 권장됩니다.
- **제거된 외부 부품**: 외부 비교기(LM393/LM311 등), CD4046(VCO), HEF4520(카운터), AD5280(가변저항), SG3525(PWM 컨트롤러) - **모두 STM32G4 내부 하드웨어로 대체**

### 시스템 클럭 설정

- 소스: 내부 HSI (16 MHz) 또는 외부 HSE 크리스탈
- PLL: 내부 클럭을 뻥튀기 → SYSCLK = 170 MHz
- AHB = 170 MHz, APB1 = 170 MHz, APB2 = 170 MHz (타이머 클럭 기본 170 MHz)

### 핀 맵 (STM32G474RBT6 — LQFP64, 2026-06-13 회로도 기준)

> 상세 표는 `docs/STM32G474RC_핀맵.md`를 기준 문서로 삼는다. 파일명은 기존 RC를 유지하지만, 현재 회로도 U3는 `STM32G474RBT6`이다. RB/RC는 같은 LQFP64 패키지에서 핀 기능은 호환되며, Flash 용량/링커 설정은 실제 BOM에 맞춰 별도 확인한다.

#### 이번 회로도에서 새로 정리된 핵심 변경

| 주제 | 신규/변경 내용 | 간단 설명 |
| ---- | -------------- | --------- |
| 초음파 발진 | `PA8/PA9 = HRTIM1_CHA1/CHA2` | 고주파/스윕 발진과 deadtime 제어를 HRTIM 기준으로 통일 |
| 위상제어 조광기 | `PC9 = TIM3_CH4(AF2)` | 기존 `PB0/TIM3_CH3`에서 PC9로 이동. ATtiny85 3kHz PWM 지령 출력 |
| LCD1602 | `PB10~PB15` | LCD 6개 신호를 한 포트 묶음으로 정리해 배선 단순화 |
| RS485 | `PB0=DE`, `PB1=/RE` | USART2 TX/RX는 PA2/PA3 유지, 방향 제어만 PB0/PB1로 이동 |
| 디버그 UART | `PB6/PB7 = USART1_TX/RX` | PA9가 HRTIM 출력으로 사용되므로 USART1을 PB6/PB7로 이동 |
| SONIC_ON | `PB3` | 발진 Enable 또는 게이트 드라이버 Enable 제어용 출력 추가 |

#### 초음파 발진 (HRTIM1 Timer A / 게이트 드라이버 연동)

| 핀 | 물리핀# | 포트/AF | 회로도 Net | 기능 |
| --- | ------- | ------- | ---------- | ---- |
| PA8 | 42 | HRTIM1_CHA1(AF13) | CHA1 / INA | 게이트 드라이버 INA |
| PA9 | 43 | HRTIM1_CHA2(AF13) | CHA2 / INB | 게이트 드라이버 INB |
| PB3 | 56 | GPIO_Output | SONIC_ON | 발진 Enable / 드라이버 Enable |

> PA9는 `UCPD1_DBCC1` 겸용 핀이므로 USB-C PD를 쓰지 않으면 초기화 초기에 `UCPD1_DBDIS=1` 설정을 넣는다. PA8/PA9는 `GPIO_AF13_HRTIM1`, `GPIO_SPEED_FREQ_VERY_HIGH`, `GPIO_NOPULL` 기준으로 설정한다.

#### 위상제어 조광기 (ATtiny85 연동용 3kHz PWM 출력)

| 핀 | 물리핀# | 포트/AF | 기능 |
| --- | ------- | ------- | ---- |
| PC9 | 41 | TIM3_CH4(AF2) | ATtiny85 위상제어 듀티 지령 전달용 3kHz PWM 출력 |

> **중요 변경**: 위상제어 조광기 출력은 기존 `PB0 / TIM3_CH3`가 아니라 **`PC9 / TIM3_CH4(AF2)`**를 사용한다. `PB0`은 현재 RS485 `DE_RS485`로 재배치되었다.

#### LCD1602 (4비트 병렬 모드)

| 핀 | 물리핀# | 포트 | 기능 |
| --- | ------- | ---- | ---- |
| PB10 | 30 | GPIO_Output | LCD RS |
| PB11 | 33 | GPIO_Output | LCD E |
| PB12 | 34 | GPIO_Output | LCD D4 |
| PB13 | 35 | GPIO_Output | LCD D5 |
| PB14 | 36 | GPIO_Output | LCD D6 |
| PB15 | 37 | GPIO_Output | LCD D7 |

> RW 핀은 GND에 고정한다. 이번 구성은 LCD가 PB10~PB15로 연속 배치되어 이전 `PB12/PB14/PB15/PC6~PC8` 혼합 구성보다 아트웍이 단순하다.

#### 택트 스위치 (내부 풀업, Active LOW)

| 핀 | 물리핀# | 포트 | 기능 |
| --- | ------- | ---- | ---- |
| PC0 | 8 | GPIO_Input | START_STOP |
| PC1 | 9 | GPIO_Input | MODE |
| PC2 | 10 | GPIO_Input | UP |
| PC3 | 11 | GPIO_Input | DOWN |

#### ADC 입력 및 비교기 후보 (가변저항, 공진/레벨 검출)

| 핀 | 물리핀# | ADC/기능 | 회로도 Net | 용도 |
| --- | ------- | -------- | ---------- | ---- |
| PA0 | 12 | ADC1_IN1 | ADC1_IN1 | 공진/레벨 검출 입력 1 |
| PA1 | 13 | ADC1_IN2 / COMP1_INP | ADC1_IN2 | 공진/레벨 검출 입력 2, 전압 zero-cross 후보 |
| PA6 | 20 | ADC2_IN3 | ADC2_IN3 | 추가 아날로그 입력 |
| PA7 | 21 | ADC2_IN4 / COMP2_INP | PWM_VR | PWM/출력 설정 VR 입력 |

> **위상 검출 주의**: 전압/전류 위상 검출을 `COMP1/COMP2 + DAC3 + TIM2 capture`로 구현하려면 `PA1=전압 COMP1_INP`, `PA7=전류 COMP2_INP` 조합이 가장 자연스럽다. 하지만 현재 회로도에서는 PA7이 `PWM_VR`로 배정되어 있으므로, 실제 전류 zero-cross 입력을 넣으려면 `PWM_VR` 이동 또는 전류 검출 핀 재배치가 필요하다.
>
> MCU 아날로그 입력은 정상 동작 중 약 `0.3V~3.0V` 안쪽으로 제한한다. 진동자 양단 20~600V 신호는 고저항 분압, 1.65V bias, 직렬저항, 저용량 클램프를 거쳐야 한다.

#### 내장 아날로그 블록 할당 (COMP 기준전압 · DAC · OPAMP)

| 블록 | 권장 연결 | 용도 | 상태 |
| ---- | --------- | ---- | ---- |
| COMP1_INP | PA1 | 전압(V) zero-cross 검출 후보 | 권장 |
| COMP2_INP | PA7 | 전류(I) zero-cross 검출 후보 | PA7이 PWM_VR과 충돌하므로 재검토 필요 |
| DAC3_CH1 | 내부 → COMP1_INM | COMP1 기준전압 약 1.65V | 사용 권장 |
| DAC3_CH2 | 내부 → COMP2_INM | COMP2 기준전압 약 1.65V | 사용 권장 |
| TIM2_IC1/IC2 | 내부 라우팅 | COMP edge 타임스탬프 캡처 | PLL 구현 시 사용 |

> DAC3는 외부 핀이 없는 내부 전용 DAC다. COMP 기준전압을 소프트웨어로 보정할 수 있으므로 외부 기준전압 저항망을 줄일 수 있다.

#### Modbus RTU (RS485, 선택 사양 보드 연동)

| 핀 | 물리핀# | 포트/모드 | 회로도 Net | 기능 |
| --- | ------- | --------- | ---------- | ---- |
| PA2 | 14 | USART2_TX | USART2_TX | RS485 TXD |
| PA3 | 17 | USART2_RX | USART2_RX | RS485 RXD |
| PB0 | 24 | GPIO_Output | DE_RS485 | Driver Enable, HIGH=송신 |
| PB1 | 25 | GPIO_Output | /RE_RS485 | Receiver Enable, LOW=수신 |
| PC4 | 22 | GPIO_Output | RTERM | 종단저항 ON/OFF |
| PC5 | 23 | GPIO_Input | 485BD_DETECT | 옵션보드 감지 |

> 선택 사양 보드 장착 시 `485BD_DETECT` 핀 상태를 읽어 인식하며, 옵션 보드가 감지될 때만 LCD 메뉴 시스템에 Modbus 설정 메뉴 하위 트리가 표출된다.

#### 외부 제어 인터페이스 (스위치 입력 및 상태 출력)

| 핀 | 물리핀# | I/O | 회로도 Net | 기능 |
| --- | ------- | --- | ---------- | ---- |
| PC6 | 38 | Input | REMOTE | 외부 리모트 입력 |
| PC7 | 39 | Input | RUN_SW | RUN 스위치 |
| PC8 | 40 | Input | SWEEP_SW | Sweep 스위치 |
| PA10 | 44 | Output | GOING | 운전 상태 출력 |
| PC13 | 2 | Output | END_BZ | 종료 알림 |
| PC14 | 3 | Output | BZ_OUT | 부저 출력 |

#### 디버그 / 기타

| 핀 | 물리핀# | 포트/모드 | 기능 |
| --- | ------- | --------- | ---- |
| PA13 | 49 | SWDIO | ST-Link SWD, GPIO 사용 금지 |
| PA14 | 50 | SWCLK | ST-Link SWD, GPIO 사용 금지 |
| PB6 | 59 | USART1_TX(AF7) | USB-UART 디버그 TX |
| PB7 | 60 | USART1_RX(AF7) | USB-UART 디버그 RX |
| NRST | 7 | RESET | ST-Link 및 DTR 리셋 회로 연결 |
| PB8 | 61 | BOOT0 / GPIO | 부트 설정 저항 정책 확정 필요 |

> **ST-Link V2 다운로드 커넥터 필수 결선 팁**: 최소 동작 핀은 VDD(3.3V 참조), GND, SWDIO, SWCLK이다. 안정적인 디버깅/다운로드를 위해 NRST까지 연결한다.

#### PCB 아트웍 배선 라우팅 가이드 (LQFP64 물리 핀 기준)

```text
좌측 핀: PC0~PC3 버튼, PC4~PC9 외부 제어/RS485/PWM, PC13~PC14 부저
우측 핀: PA0~PA7 아날로그/USART2, PA8~PA10 HRTIM/GOING, PA13~PA14 SWD
하측 핀: PB10~PB15 LCD 6핀 묶음
상측 핀: PB3 SONIC_ON, PB6~PB7 USART1, PB8 BOOT0, PB9 예비/I2C 후보
```

| PCB 영역 | 배치 부품 | MCU 핀 |
| -------- | --------- | ------ |
| 조작부 | START/STOP, MODE, UP, DOWN | PC0~PC3 |
| RS485 커넥터/트랜시버 | USART2, DE, /RE, RTERM, Board Detect | PA2, PA3, PB0, PB1, PC4, PC5 |
| 아날로그 검출부 | 공진/레벨 검출, PWM_VR | PA0, PA1, PA6, PA7 |
| 게이트 드라이버 근처 | HRTIM 출력, SONIC_ON | PA8, PA9, PB3 |
| LCD 커넥터 | LCD1602 4bit | PB10~PB15 |
| 디버그 커넥터 | SWD, USART1, NRST | PA13, PA14, PB6, PB7, NRST |

**핀 변경 이력 (기존 대비):**

| 변경 내용 | 이전 핀 | 현재 핀 | 사유 |
| --------- | ------- | ------- | ---- |
| 초음파 출력 | PA8+PB13 / TIM1 | PA8+PA9 / HRTIM1 | HRTIM pair deadtime 및 정밀 스윕 |
| 위상제어 PWM | PB0 / TIM3_CH3 | PC9 / TIM3_CH4 | PB0을 RS485 DE로 사용 |
| LCD | PB12, PB14, PB15, PC6~PC8 | PB10~PB15 | LCD 배선 단순화 |
| RS485 DE/RE | PA5/PA6 | PB0/PB1 | 회로도 기준 변경 |
| Debug UART | PA9/PA10 | PB6/PB7 | PA9를 HRTIM1_CHA2로 사용 |
| SONIC_ON | 미정 | PB3 | 발진 Enable 제어 추가 |

> **PCB 아트웍 핵심 포인트 1**: PA8/PA9 HRTIM 출력은 게이트 드라이버 INA/INB까지 짧고 나란히 배선한다.
>
> **PCB 아트웍 핵심 포인트 2**: PA0/PA1/PA6/PA7 아날로그 입력은 고전압 스위칭 루프와 분리하고, 정상 동작 중 MCU 입력이 0.3V~3.0V 범위를 벗어나지 않게 보호한다.
>
> **PCB 아트웍 핵심 포인트 3**: PB10~PB15 LCD 버스는 스위칭 노드와 평행 장거리 배선을 피한다.

---

## 모듈별 상세

### 1. 초음파 발진 (`ultrasonic_pwm` + `ultrasonic_ctrl`)

HRTIM1 Timer A를 사용하여 산업용 초음파 주파수(20–168 kHz)를 생성한다.
`ultrasonic_pwm`이 HRTIM 하드웨어 레벨(주파수, 듀티, 데드타임)을 담당하고,
`ultrasonic_ctrl`이 상위 로직(소프트 스타트, 펄스/스윕 모드 관리, PLL 공진 추적)을 담당한다.

> 현재 저장소의 일부 기존 코드는 TIM1 기반일 수 있다. 새 회로도 기준 출력은 `PA8/HRTIM1_CHA1`, `PA9/HRTIM1_CHA2`이므로 CubeMX/펌웨어 재생성 시 HRTIM으로 전환한다.

**지원 주파수 대역:**

- STM32G474RB/RC (170MHz, HRTIM 내장) 기준: 20kHz~168kHz 발진, 중심주파수 기준 ±100Hz~±1000Hz 스윕 지원 목표
- HRTIM은 TIM1보다 주파수/edge 조정 해상도가 높아, 작은 스윕 폭에서도 주파수 계단이 덜 거칠다.

**핵심 설정:**

- **PWM 출력**: `HRTIM1_CHA1(PA8)` + `HRTIM1_CHA2(PA9)` pair 출력
- **데드 타임**: HRTIM deadtime insertion으로 설정 — FET 관통 전류 방지 (실측 기준으로 200–500 ns부터 검토)
- **주파수 설정**: HRTIM Timer A period 값 변경

```
주파수 = HRTIM 타이머 클럭 / period
실제 period 계산은 CubeMX/HAL HRTIM clock 설정과 prescaler를 기준으로 맞춘다.
```

- **듀티비 설정**: Timer A compare 값 변경 (가변저항 ADC 값 또는 Modbus 명령에 매핑)
- **출력 Enable**: HRTIM 출력 enable + `PB3/SONIC_ON` 게이트 드라이버 Enable을 함께 관리

**안전 기능 및 자동 튜닝(Auto-Tuning):**

| 기능          | 설명                                                          |
| ------------- | ------------------------------------------------------------- |
| 소프트 스타트 | 출력 개시 시 듀티비를 0%에서 목표값까지 점진적 증가 (500 ms)  |
| 비상 정지     | Modbus 명령, 외부 제어 입력 또는 이상 감지 시 즉시 출력 차단  |
| 출력 제어     | START_STOP(PC0) 또는 메뉴 명령으로 초음파 출력 시작/종료 제어 |
| 출력 제한     | 듀티비 상한 클램핑 (하드웨어 보호)                            |
| 이상 감지     | 타이머 브레이크 입력(BRK) 활용 가능 (확장 시)                 |
| **오토 튜닝** | **가변 부하(세제, 세척물, 온도) 대응 공진점 자동 추적 (PLL)** |

**공진 주파수 하이브리드 제어 (디지털 PLL + 기준점 스윕 💡):**

초음파 세척기는 최대 수십 개의 BLT(진동자)를 병렬 결합하여 사용하므로, 각 진동자의 개별 공진 주파수 편차가 존재합니다. 특정 단일 공진점(예: 28.0kHz)에만 고정하면 전체 효율이 떨어집니다. 이를 극복하기 위해 **"위상차 기반 자동 공진점 탐색"**과 **"기준점 기반 수동 스윕(Sweep)"**을 결합한 하이브리드 방식을 적용합니다.

1. **내장 고속 비교기(COMP)를 활용한 위상 검출**:
   - 외부 아날로그 비교기(LM393 등)를 **완전히 제거**합니다. 대신 진동자에 인가되는 전압(V)과 전류(I) 파형을 하드웨어로 적절히 감쇄/변환하여 STM32G4의 하드웨어 핀으로 직접 입력합니다.
   - **전압(V) 파형**: 진동자 직전의 수백 볼트 고전압을 다단계 1MΩ 수준의 저항 분배 회로 및 쇼트키 다이오드(BAT54S 등) 클램핑 라인을 거쳐 안전한 3.3V 로직 레벨의 저전압 AC 신호로 축소한 뒤 COMP1 핀에 인가.
   - **전류(I) 파형**: 진동자로 향하는 출력 선로에 결합시킨 **고주파 대응 관통형 CT(Current Transformer)**로 추출된 교류 신호에 Burden 저항 및 오프셋(예: 1.65V 바이어스)을 걸어 COMP2 핀에 그대로 연결.
   - MCU 내부에 탑재된 **약 15ns 응답속도의 고속 컴퍼레이터 모듈(COMP)**이 위 두 AC 파형의 제로크로싱을 받아 내부 로직(하드웨어 크로스바 룩업)으로 즉시 구형파 펄스를 생성하고, 타이머의 입력 캡처 채널로 넘깁니다. 168kHz의 폭주파수에서도 심각한 위상 지연 및 왜곡이 없는 CT 기반 위상차 획득이 가능합니다.
   - **기준전압(INM)은 DAC3 내부 전용 DAC**로 공급합니다. DAC3_CH1→COMP1_INM, DAC3_CH2→COMP2_INM이 칩 내부 크로스바로 직결되어 외부 핀이 전혀 필요 없습니다. 기준전압을 바이어스 중점(~1.65V = VDDA/2)으로 설정하며, 온도 드리프트 시 소프트웨어로 실시간 보정합니다.
   - **위상차 캡처는 TIM2(32비트) 듀얼 입력 캡처**로 수행합니다. COMP1 출력→TIM2_IC1, COMP2 출력→TIM2_IC2가 내부 라우팅으로 연결되며, 두 제로크로싱 시점의 타임스탬프 차이(170MHz 기준 5.88ns/tick)로 위상차를 나노초 정밀도로 산출합니다.
2. **소프트웨어 제어 알고리즘 (Hybrid Mode)**:
   - **Step 1 (Auto-Tuning)**: 동작 초기 또는 큰 부하 변동 감지 시, 위상차가 0(전류와 전압 동위상)이 되는 **실시간 메인 공진 주파수(기준점)**를 신속하게 탐색(Tracking)하여 고정합니다.
   - **Step 2 (Base-point Sweep)**: 탐색된 기준점(예: 27.8kHz)을 중심으로, 설정된 대역폭(예: ±200Hz) 내에서 초당 100~300회 주파수를 오르내리며 **고속 스윕(Sweep) 발진**합니다.
   - **Step 3 (Continuous Tracking)**: 온도나 세제 투입 등으로 기준점 자체가 크게 이동하면(위상차 오버플로우 감지), 즉시 Step 1으로 돌아가 새로운 기준점을 다시 잡고 스윕을 재개합니다.
3. **효과**:
   - 무작정 고정된 주파수 대역만 스윕할 때 발생하던 극단적인 임피던스 불일치(유도성/용량성 과도 구간)를 회피하여 FET 스위칭 발열 및 소손을 방지합니다.
   - 동시에 스윕 기능을 유지하여 병렬 연결된 여러 진동자 각각의 편차를 모두 커버하고, 세척기 내부의 정주파(Standing Wave) 제거에 따른 세척 효율 극대화 트렌드를 완벽히 충족합니다.

### 🌟 정전류 (Constant Current) 유지 제어 로직 (AC 위상 제어 연동)

수위의 변화, 세척물의 입출, 세제의 종류에 따라 기계적 부하가 변하면 초음파 출력(결과적으로 소비 전류)이 크게 요동칩니다. 이를 방지하고 항상 균일한 세척력을 유지하기 위해 **설정된 목표 전류(예: 4A)를 유지하는 정전류 제어(Closed-Loop AC Phase Control)** 기능을 병행합니다.

1. **AC 전원 위상 제어 (Phase Control) 원리**:
   - 메인 AC 입력 전원(220V 60Hz) 단에 트라이액(TRIAC)이나 SCR 기반의 조광기(Dimmer) 회로를 배치합니다.
   - 전압의 제로 크로싱(Zero-Crossing) 시점을 기준으로 언제 스위치를 켤 것인가(Trigger Delay)를 조절하여, 전원 인가 단면적(RMS 전압)을 잘라냅니다.
   - 딜레이 타임 0(지연 없이 켬) = 출력 100%. 딜레이 타임 약 8.3ms(반주기 끝) = 출력 10%.
2. **STM32 <-> ATtiny85 간의 정전류 듀티 제어 흐름**:
   - STM32는 초당 여러 번 전류 센서 또는 정류된 CT envelope 값으로부터 실제 소비 전류값(A)을 필터링하여 읽습니다.
   - 실제 전류가 목표 전류(예: 4.0A)보다 **낮으면**, STM32는 ATtiny85로 보내는 3kHz PWM 신호의 듀티비를 올려줍니다 (트리거 타임을 제로크로싱 가깝게 당겨 RMS 전압/출력을 상승시킵니다).
   - 실제 전류가 목표 전류보다 **높으면**, 듀티비를 내립니다 (트리거 타임을 뒤로 미루어 전력을 즉각 깎아냅니다).
3. **효과**: PID 제어기나 단순 증감 로직을 통해 구현되며, 사용자가 원하는 타겟 전류를 메뉴나 터치 패널로 입력해 두기만 하면, 물이 출렁거리거나 대형 세척물이 들어와도 실시간으로 출력을 보정하여 **요동치지 않는 매우 일정한 초음파 출력과 전류량**을 보장하는 최고급 기능이 완성됩니다.

### 2. LCD1602 디스플레이 (`lcd1602` + `lcd1602_hw`)

16×2 캐릭터 LCD를 HD44780 호환 4비트 모드로 구동한다.
`lcd1602_hw`가 GPIO 니블 전송 및 DWT 기반 마이크로초 지연을 처리하고,
`lcd1602`가 초기화, 문자열 출력, 커서 제어 등 상위 API를 제공한다.

**드라이버 인터페이스:**

```c
void LCD_Init(void);                           // 4비트 모드 초기화
void LCD_Clear(void);                          // 화면 전체 지움
void LCD_SetCursor(uint8_t row, uint8_t col);  // 커서 위치 지정 (row: 0-1, col: 0-15)
void LCD_WriteString(const char *str);         // 문자열 출력
void LCD_WriteChar(char ch);                   // 단일 문자 출력
void LCD_CreateChar(uint8_t addr, const uint8_t *pattern); // 사용자 정의 문자 (CGRAM)
```

**화면 레이아웃 (메인 화면 예시):**

```
┌────────────────┐
│ FREQ:28.0k  RUN│  ← 1행: 주파수 + 동작 상태
│ DUTY:45%  AUTO │  ← 2행: 듀티비 + 제어 모드
└────────────────┘
```

**구현 주의사항:**

- 명령/데이터 기록 후 최소 37 µs 대기 (Clear/Home은 1.52 ms)
- 화면 갱신은 값 변경 시에만 수행 (불필요한 깜빡임 방지)
- 초기화 시퀀스: 전원 ON → 15 ms 대기 → 4비트 모드 진입 (3회 반복 규격 준수)

### 3. 메뉴 시스템 (`menu` + `menu_screen`)

FSM(Finite State Machine) 기반 계층 메뉴 구조.
`menu`가 상태 전이 및 값 편집 로직을 담당하고,
`menu_screen`이 메뉴 상태별 LCD 화면 렌더링을 담당한다.

```
MENU_MAIN (메인 화면 — 주파수/듀티비/상태 표시)
│
├── [MODE] → MENU_FREQ    — 주파수 설정 (20.0–168.0 kHz, 0.1 kHz 단위)
├── [MODE] → MENU_DUTY    — 듀티비 설정 (0–100%, 가변저항 우선 또는 수동)
├── [MODE] → MENU_MODE    — 동작 모드 선택
│                            ├── 연속 (Continuous)
│                            ├── 펄스 (Pulse: ON/OFF 시간 설정)
│                            └── 스윕 (Sweep: 시작/끝 주파수, 스윕 시간)
├── [MODE] → MENU_MODBUS  — Modbus 설정 (RS485 Board_Detect 시에만 활성화)
│                            ├── 슬레이브 주소 (1–247)
│                            ├── 통신 속도 (9600/19200/38400/115200)
│                            └── 패리티 (None/Even/Odd)
└── [MODE] → MENU_INFO    — 시스템 정보 (FW 버전, 동작 시간, 온도 등)
```

**네비게이션 규칙:**

- START_STOP(PC0): 메인 화면에서는 출력 시작/정지, 값 편집 중에는 확정/저장
- MODE(PC1): 메뉴 진입, 항목 선택 또는 다음 화면 이동
- MODE 장기 누름: 상위 메뉴로 복귀 (뒤로가기)
- UP/DOWN(PC2/PC3): 메뉴 항목 이동 또는 값 증감

### 4. 택트 스위치 입력 (`button`)

**디바운싱 알고리즘:**

- 소프트웨어 디바운싱: 10 ms 주기 폴링, 20 ms 연속 안정 시 확정
- SysTick 인터럽트 또는 기본 타이머(TIM6/TIM7 등) 기반 주기 호출 권장. TIM3는 PC9 조광기 PWM용으로 예약한다.

**현재 회로도 버튼 핀:**

| 버튼 | MCU 핀 | 입력 방식 |
| ---- | ------ | --------- |
| START_STOP | PC0 | 내부 풀업, Active LOW |
| MODE | PC1 | 내부 풀업, Active LOW |
| UP | PC2 | 내부 풀업, Active LOW |
| DOWN | PC3 | 내부 풀업, Active LOW |

**이벤트 유형:**

| 이벤트         | 조건                                      |
| -------------- | ----------------------------------------- |
| BTN_PRESS      | 눌림 → 놓임 (단일 클릭)                   |
| BTN_LONG_PRESS | 1초 이상 누름 유지                        |
| BTN_REPEAT     | 장기 누름 시 200 ms 간격 반복 이벤트 발생 |

**인터페이스:**

```c
void     Button_Init(void);          // GPIO 초기화 (내부 풀업)
void     Button_Process(void);       // 주기적 호출 (10 ms마다)
uint8_t  Button_GetEvent(uint8_t id); // 이벤트 읽기 및 소비 (읽으면 클리어)
```

### 5. ADC 및 외부 위상제어 출력 (`adc_control` & `phase_pwm`)

**ADC 설정 (센서 및 다중 가변저항):**

- ADC 후보: `PA0/ADC1_IN1`, `PA1/ADC1_IN2`, `PA6/ADC2_IN3`, `PA7/ADC2_IN4`
- 현재 회로도 Net: `ADC1_IN1`, `ADC1_IN2`, `ADC2_IN3`, `PWM_VR`
- 해상도: 12비트 (0–4095), G4는 하드웨어 오버샘플링(최대 256배) 지원으로 최대 16비트 유효 해상도 가능
- 샘플링 타임: 47.5 cycles 이상 권장 (고임피던스 소스는 92.5~247.5 cycles, G4 ADC는 최대 640.5 cycles 선택 가능)
- 변환 방식: Scan 모드 + Continuous 변환 + DMA 전송 권장
- G4 ADC 특이사항: 사용 전 반드시 `HAL_ADCEx_Calibration_Start()` 캘리브레이션 호출 필수 (싱글엔드/디퍼렌셜 각각)

**노이즈 필터링:**

- `PWM_VR(PA7/ADC2_IN4)` 또는 메뉴 설정값: 이동 평균 필터(16 샘플), 데드존(양 끝 50 카운트), 히스테리시스(±2 카운트) 적용
- 공진/레벨 검출용 `PA0/PA1/PA6`: RC 시정수와 소스 임피던스에 맞춰 sampling time을 길게 잡고, 여러 샘플 평균 후 판단
- 과전류/과전압 보호용 ADC는 정상 동작 범위에서 0.3V~3.0V 안쪽에 머물도록 하드웨어 감쇠/클램프를 먼저 설계

**위상제어용 3kHz PWM 출력 (ATtiny85 연동):**

- 하드웨어: **TIM3_CH4 (PC9)** 일반 타이머 채널 사용
- 주파수: 3 kHz 고정 (ATtiny85가 읽어갈 지령 주파수)
- 듀티비 연동: `PWM_VR(PA7)` 또는 메뉴/Modbus 설정값(0~100%)을 TIM3 CH4의 CCR 레지스터에 반영하여 출력 1~100% 범위로 변조 전송
- **핀 변경 주의**: 기존 문서/코드의 `PB0/TIM3_CH3` 기준 구현은 새 회로도에서 틀리다. `PB0`은 `DE_RS485`이며, 위상제어 조광기 PWM은 `PC9/TIM3_CH4`로 구현한다.

**전류 센싱의 오해와 이중 검출법(Dual Sensing - 홀센서와 CT의 역할 분리):**

초음파 세척기 시스템에서 전류라는 개념은 크게 **"1) 장비가 소모하는 총 파워 트렌드 파악 (AC 메인 입력 전류)"**와 **"2) 진동자가 뛰는 찰나의 박자 파악 (초음파 고주파 출력 전류)"**로 나뉩니다. 이 두 가지를 측정하는 센서 소자는 특성에 따라 완벽히 분리되어야 합니다.

- **1. 크기 및 정전류 제어용 (ACS722, 또는 범용 CT + 정류회로)**
  - **위치**: 상용 220V 50/60Hz가 들어오는 인입 단이나 SMPS(브릿지 다이오드 전단 혹은 후단) 배선 로(경로).
  - **목적**: 기기가 현재 전체적으로 전류를 3A 먹고 있는지 5A 먹고 있는지를 알아내어 ATtiny85 조광기의 위상(전력 출력)에 피드백을 주기 위함.
  - **센서 1안 (홀센서)**: **ACS722, ACS723** 등 사용. 부품이 적어 회로가 간단하고 직류 스케일로 바로 읽히나 외부 노이즈에 변동이 있을 수 있습니다.
  - **센서 2안 (범용 저주파 CT + LM358 능동 정류) [권장]**:
    - 일반 50/60Hz용 소형 범용 CT(예: ZMPT, KCT 시리즈 등)를 인입 단에 관통시킵니다.
    - CT 2차측에서 나오는 교류 파형(수십~수백mV)을 저렴한 **LM358 OP-AMP**를 활용한 **"능동 전파 정류 회로(Precision Full-Wave Rectifier)"** 에 통과시킨 뒤, 커패시터로 평활하여 0~3.3V DC 아날로그 전압으로 스케일링하여 여유 ADC 핀에 넣습니다.
    - **[설계 팁] LM358 능동 정류 회로**: 일반 다이오드(0.6V)나 쇼트키 다이오드(0.3V)를 그냥 직렬로 쓰면 강하 전압(Vf) 미만의 낮은 신호(기초 대기 전력)는 읽히지 않고 데드존이 생깁니다. 이를 방지하기 위해 다이오드를 OP-AMP의 피드백 루프 안에 넣어 Vf 손실을 '0(Zero)'으로 만들어 버리는 회로입니다. 구글에 "Op-amp Precision Rectifier" 또는 "LM358 정전류 정류"로 검색하면 나오는 아주 표준적인 회로로, 저항 몇 개와 1N4148 다이오드 2개면 완벽한 리니어(직선) 비례 특성을 가진 RMS DC 전압을 뽑아낼 수 있습니다. 산업 현장에서 내구성과 노이즈 절연성을 동시에 챙기는 가장 가성비 좋은 하드웨어 설계입니다.

- **2. 공진점(위상) 추적용 (CT 센서, COMP2_IN 활용)**
  - **위치**: 매칭 트랜스포머를 거쳐 실제 여러 개의 진동자(BLT) 덩어리로 나가는 20kHz ~ 168kHz의 고주파 출력 배선로.
  - **목적**: 전압(V) 파형 대비 전류(I) 파형 커브가 먼저 들어오는지 나중에 들어오는지 위상(Phase) 딜레이 시간을 나노초(ns) 단위로 읽어서, 이 딜레이가 "0번(동위상)"이 되도록 HRTIM 주파수를 조절(PLL)하기 위함. 파형의 크기(전압 강하/증폭률)는 전혀 중요하지 않으며 오직 파형의 곡선 모양과 제로 크로싱 타이밍만 중요합니다.
  - **센서**: **관통형 고주파 AC CT(Current Transformer)** 적용 필수. 고주파 대역폭 왜곡이 없으므로 타이밍 지연(위상 어긋남) 없이 신속하게 본래의 나노초 단위 파형을 컴퍼레이터 비교기(COMP)로 밀어 넣습니다.

**엔지니어 조언 요약:**
생각하신 설계 방향이 **매우 훌륭하고 이상적이며, 가장 정석적인 산업용 설계 방식**입니다!
위상 제어(Dimmer)를 위한 매크로한 RMS 전류 검출은 **저주파 친화적인 ACS 센서**에게 맡겨서 ADC로 필터링해 읽고,
나노초 단위의 오차도 용납할 수 없는 초음파 마이크로 위상 비교(PLL)를 위한 파형 픽업은 **고주파 친화적인 CT 센서**에 맡겨 COMP 하드웨어에 직결하는 이 "이중 역할 분담 방식"을 사용하면,
파워 제어의 안정성(전류 흔들림 방지)과 자동 공진 추적의 정밀도를 동시에 극도로 끌어올릴 수 있습니다. 설계 사상에 전혀 문제가 없으며 모범 답안에 가깝습니다.

**가변저항 매핑:**

```
PWM_VR(PA7/ADC2_IN4) 또는 선택 ADC (0–4095) → 듀티비 (0–100%) 또는 사용자 지정 범위
실제 적용범위: ADC_MIN(50) ~ ADC_MAX(4045) → 0% ~ 100%
```

### 6. Modbus RTU 통신 (`modbus_rtu` + `modbus_crc` + `modbus_regs`)

`modbus_rtu`가 USART2 + TIM4로 프레임 수신/파싱/응답을 처리하고,
`modbus_crc`가 CRC-16 테이블 룩업을 제공하며,
`modbus_regs`가 레지스터 맵 읽기/쓰기 핸들러를 담당한다.

**물리 계층:** RS485 반이중 통신 (MAX3485 / SP3485 등 3.3V 로직 지원품 권장)

**USART 설정:**

- USART2: 9600–115200 bps (메뉴에서 설정 가능), 기본 9600 bps
- 데이터 포맷: 8N1 / 8E1 / 8O1 (메뉴에서 설정 가능)
- 슬레이브 주소: 1–247 (메뉴에서 설정, 기본 1)

**RS485 방향 및 제어 핀 상태:**

```text
USART2 TX/RX: PA2/PA3
DE_RS485: PB0, /RE_RS485: PB1
RTERM: PC4, 485BD_DETECT: PC5
옵션 보드 인식: 부팅 시 및 주기적으로 485BD_DETECT 핀 읽음 → 인식 시 메뉴 활성화
종단 저항 (RTERM): 메뉴에서 활성화 시 PC4 HIGH 출력 (필요 시)
대기 / 수신 시: PB0(DE) = LOW, PB1(/RE) = LOW → RXNE 인터럽트로 바이트 수신 대기
데이터 송신 시: PB0(DE) = HIGH, PB1(/RE) = HIGH → 데이터 송신 → TC 플래그 대기 → 완료 후 DE=LOW, /RE=LOW 복귀
```

**프레임 감지:**

- 프레임 간 묵음 구간: 3.5 캐릭터 시간 (9600 bps 기준 약 4 ms)
- USART IDLE 인터럽트 또는 타이머(TIM4 등)로 프레임 종료 감지

**지원 펑션 코드:**

| 코드 | 기능                     | 설명                    |
| ---- | ------------------------ | ----------------------- |
| 0x03 | Read Holding Registers   | 파라미터/상태 읽기      |
| 0x06 | Write Single Register    | 단일 파라미터 쓰기      |
| 0x10 | Write Multiple Registers | 복수 파라미터 일괄 쓰기 |

**예외 응답 코드:**

| 코드 | 의미                 |
| ---- | -------------------- |
| 0x01 | Illegal Function     |
| 0x02 | Illegal Data Address |
| 0x03 | Illegal Data Value   |
| 0x04 | Slave Device Failure |

**Modbus 레지스터 맵 (그룹별 0x10 블록 정렬, 상세: `docs/Modbus_레지스터맵.md`):**

_그룹 0x00: 운전 제어 — 10개 연속 읽기 가능_

| 주소   | R/W | 내용                | 단위/범위                            |
| ------ | --- | ------------------- | ------------------------------------ |
| 0x0000 | R/W | 동작 ON/OFF         | 0=OFF, 1=ON                          |
| 0x0001 | R/W | 주파수 설정값       | ×0.1 kHz (200–1680 → 20.0–168.0 kHz) |
| 0x0002 | R/W | 출력 설정값         | 0–500 (Fullscale=500 정규화)         |
| 0x0003 | R   | 현재 출력 주파수    | ×0.1 kHz                             |
| 0x0004 | R   | 현재 출력값         | 0–500                                |
| 0x0005 | R   | 동작 상태 플래그    | 비트 필드 (아래 참조)                |
| 0x0006 | R/W | 동작 모드           | 0=연속, 1=펄스, 2=스윕               |
| 0x0007 | R   | 출력 표시 범위      | 표시용 Fullscale 값 (W 또는 %)       |
| 0x0008 | R/W | 로컬 입력 금지      | 0=허용, 1=금지                       |
| 0x0009 | R   | 외부 입력 동작 상태 | 0=OFF, 1=ON                          |

_그룹 0x10: 펄스/스윕 설정 — 5개 연속 읽기/쓰기 가능_

| 주소   | R/W | 내용             | 단위/범위           |
| ------ | --- | ---------------- | ------------------- |
| 0x0010 | R/W | 펄스 ON 시간     | ms (10–10000)       |
| 0x0011 | R/W | 펄스 OFF 시간    | ms (10–10000)       |
| 0x0012 | R/W | 스윕 시작 주파수 | ×0.1 kHz (200–1680) |
| 0x0013 | R/W | 스윕 끝 주파수   | ×0.1 kHz (200–1680) |
| 0x0014 | R/W | 스윕 시간        | ms (100–60000)      |

_그룹 0x20: 에러/진단_

| 주소   | R/W | 내용               | 단위/범위                                |
| ------ | --- | ------------------ | ---------------------------------------- |
| 0x0020 | R   | 에러 상태 코드     | 비트 필드 (OC/OV/OT/PLL/COMM/HW)         |
| 0x0021 | W   | 에러 리셋          | 0x0001 기록 시 에러 클리어               |
| 0x0022 | R   | 누적 운전 시간 (H) | 시간 (32비트 중 상위)                    |
| 0x0023 | R   | 누적 운전 시간 (L) | 분 (32비트 중 하위)                      |
| 0x0024 | R   | MCU 내부 온도      | ×0.1 °C                                  |
| 0x0025 | R   | 입력 전류값        | ×0.01 A                                  |
| 0x0026 | R   | 통신 에러 카운트   | 누적 CRC/프레임 에러 횟수                |
| 0x0027 | R   | 펌웨어 버전        | 상위=Major, 하위=Minor (예: 0x0102=v1.2) |

_그룹 0x30: 통신 설정 — 3개 연속 읽기/쓰기 가능_

| 주소   | R/W | 내용                 | 단위/범위                          |
| ------ | --- | -------------------- | ---------------------------------- |
| 0x0030 | R/W | Modbus 슬레이브 주소 | 1–247                              |
| 0x0031 | R/W | 통신 속도 인덱스     | 0=9600, 1=19200, 2=38400, 3=115200 |
| 0x0032 | R/W | 패리티 설정          | 0=None, 1=Even, 2=Odd              |

_그룹 0x40: 타이머/사이클 제어_

| 주소   | R/W | 내용               | 단위/범위               |
| ------ | --- | ------------------ | ----------------------- |
| 0x0040 | R/W | 타이머 사용 여부   | 0=미사용, 1=사용        |
| 0x0041 | R/W | 타이머 설정 시간   | 초 (1–36000)            |
| 0x0042 | R   | 타이머 잔여 시간   | 초 (카운트다운)         |
| 0x0043 | R/W | 정전류 목표값      | ×0.01 A (예: 400=4.00A) |
| 0x0044 | R/W | 정전류 제어 활성화 | 0=수동, 1=자동(정전류)  |

_시스템 명령_

| 주소   | R/W | 내용        | 단위/범위                  |
| ------ | --- | ----------- | -------------------------- |
| 0x00FF | W   | 시스템 리셋 | 0x1234 기록 시 소프트 리셋 |

**상태 플래그 (0x0005) 비트 정의:**

| 비트  | 명칭          | 의미                    |
| ----- | ------------- | ----------------------- |
| 0     | RUN           | 출력 동작 중 (Running)  |
| 1     | SOFT_START    | 소프트 스타트 진행 중   |
| 2     | FAULT         | 에러/이상 발생          |
| 3     | MODBUS_ACTIVE | Modbus 통신 활성        |
| 4     | LOCAL_LOCK    | 로컬 입력 금지 상태     |
| 5     | EXT_INPUT     | 외부 리모트 입력 ON     |
| 6     | SWEEP_ACTIVE  | 스윕 동작 중            |
| 7     | PULSE_ACTIVE  | 펄스 모드 동작 중       |
| 8     | TIMER_ACTIVE  | 타이머 카운트다운 중    |
| 9     | CC_ACTIVE     | 정전류 제어 동작 중     |
| 10    | PLL_LOCKED    | PLL 공진 추적 잠금 완료 |
| 11–15 | (예약)        | 향후 확장용 (0 고정)    |

**CRC-16 (Modbus):** 다항식 0xA001, LSB-first. 테이블 룩업 방식 권장 (256 엔트리).

---

## 메인 루프 구조

```c
int main(void)
{
    HAL_Init();                    // HAL 라이브러리 초기화 (SysTick 1 ms)
    SystemClock_Config();          // HSI 또는 HSE → PLL → 170 MHz
    GPIO_Init_All();               // 전체 GPIO 초기화
    UltrasonicPWM_Init();          // HRTIM PA8/PA9 PWM 설정
    UltrasonicCtrl_Init();         // 초음파 제어 초기화
    ADC_Control_Init();            // ADC 다중 채널 및 DMA 초기화
    COMP_Init();                   // 내부 고속 비교기 (위상차 검출용) 초기화
    DAC3_Init();                   // DAC3 내부 전용 — COMP1/2 INM 기준전압 (VDDA/2 ≈ 1.65V)
    TIM2_IC_Init();                // TIM2 듀얼 입력 캡처 — COMP1→IC1, COMP2→IC2 위상차 측정
    LCD_Init();                    // LCD1602 초기화
    Button_Init();                 // 택트 스위치 초기화
    Menu_Init();                   // 메뉴 FSM 초기 상태
    Modbus_Init();                 // Modbus USART2 + 타이머 초기화

    while (1) {
        // 버튼 입력은 SysTick ISR에서 10 ms마다 Button_Process() 호출
        ADC_Control_Process();     // 가변저항 값 읽기 및 필터링
        Menu_Update();             // 메뉴 상태 갱신 (버튼 이벤트 소비)
        MenuScreen_Refresh();      // LCD 화면 갱신 (변경 시에만)
        UltrasonicCtrl_Update();   // 소프트스타트, 펄스/스윕 처리
        Modbus_Process();          // Modbus 수신 프레임 처리
    }
}
```

### 인터럽트 우선순위

| 우선순위 (숫자 낮을수록 높음) | 인터럽트         | 용도                               |
| ----------------------------- | ---------------- | ---------------------------------- |
| 0 (최고)                      | HRTIM Fault/BRK  | 초음파 출력 하드웨어 차단 (확장용) |
| 1                             | USART2 RXNE/IDLE | Modbus 바이트 수신 및 프레임 감지  |
| 2                             | TIM4             | Modbus 3.5T 타임아웃 (필요 시)     |
| 3                             | SysTick          | 버튼 폴링, HAL 틱 (1 ms)           |
| 4                             | ADC1             | ADC 변환 완료 (DMA 사용 시 불필요) |

---

## 코딩 규칙

- **한글 주석**이 표준이며, 코드 수정 시에도 한글 주석을 유지할 것.
- HAL 함수 사용을 기본으로 하되, 타이밍 임계 경로(ISR 내부 등)에서는 LL 드라이버 또는 레지스터 직접 접근 허용.
- 인터럽트 핸들러에서 **플래그만 설정**하고, 무거운 처리는 메인 루프에서 수행 (ISR 최소화 원칙).
- 함수 명명: `모듈명_동작()` 형식 (예: `LCD_WriteString()`, `Modbus_ProcessFrame()`).
- 파일 명명: 소문자 + 언더스코어 (예: `modbus_rtu.c`, `adc_control.h`).
- 상수: `#define` 또는 `enum` 사용; 매직 넘버 금지.
- 전역 변수 최소화; 모듈 간 데이터 교환은 getter/setter 함수 권장.
- 공유 변수(ISR ↔ main): `volatile` 선언 필수, 필요 시 `__disable_irq()`/`__enable_irq()` 사용하되 임계 구간 최소화.
- **동적 메모리 할당 금지** (`malloc`/`free`/`calloc` 사용하지 않음).
- 각 모듈은 `.c`/`.h` 쌍으로 분리; 헤더에는 include guard (`#ifndef ... #endif`) 사용.
- 변수 선언은 함수 상단 또는 모듈 스코프 `static` — C99 이상 허용.

---

## 핵심 제약사항

- STM32G474RB/RC 계열은 LQFP64 핀 기능이 호환되지만 Flash 용량은 실제 BOM에 맞춰 확인한다. 현재 회로도 U3는 `STM32G474RBT6` 기준이며, 기존 `STM32G474RCTx_FLASH.ld` 사용 시 링커 용량 동기화가 필요하다.
- **초음파 출력은 HRTIM 기준**으로 구현한다:
  - `PA8/HRTIM1_CHA1`, `PA9/HRTIM1_CHA2`를 `GPIO_AF13_HRTIM1`로 설정한다.
  - HRTIM output enable, Timer A counter start, deadtime 설정, fault 입력 정책을 함께 확인한다.
  - 기존 TIM1 상보출력 예제 코드를 그대로 복사하지 말고, HRTIM HAL/LL 설정으로 전환한다.
- **위상제어 조광기 출력은 TIM3_CH4(PC9)** 이다. 예전 `PB0/TIM3_CH3` 기준 코드는 사용하지 않는다. `PB0`은 현재 `DE_RS485`다.
- 타이머 클럭: G4 시리즈는 버스 아키텍처가 최적화되어, APB1/APB2 모두 170MHz로 동작하여 압도적 듀티 분해능 제공.
- RS485 반이중 특성: 송신 완료(TC) 확인 후 DE → LOW 전환 필수.
- LCD1602 HD44780: 명령 실행 타이밍 준수 필수 — `HAL_Delay()` 또는 마이크로초 지연 함수 사용.
- ADC 레퍼런스: VDDA = 3.3 V; 가변저항은 GND–3.3 V 사이에 연결.
- **OPAMP LQFP64 제한**: 내장 OPAMP 외부 핀은 현재 HRTIM, USART2, LCD, RS485, 아날로그 검출 핀과 충돌 가능성이 높다. 이번 보드는 전압 분압, CT burden, bias, 클램프 같은 외부 신호 컨디셔닝을 우선한다.
- **DAC3 권장**: COMP1/2의 반전 입력(INM) 기준전압은 DAC3(내부 전용 DAC)로 공급한다. DAC1/DAC2 외부 출력은 현재 핀맵과 충돌 가능성이 있으므로 별도 핀 검토 없이 사용하지 않는다.
- **SWD 핀 예약**: PA13(SWDIO), PA14(SWCLK)은 GPIO로 사용 금지. PB3은 이번 회로도에서 `SONIC_ON`으로 사용하므로 SWO 기능을 켜지 않는다.
- **G4 USART 레지스터 차이**: F1의 `USART_SR`/`USART_DR` 단일 레지스터 구조와 달리, G4는 `USART->ISR`(상태), `USART->ICR`(플래그 클리어), `USART->TDR`/`USART->RDR`(송수신 분리) 구조. 레지스터 직접 접근 시 반드시 G4 레퍼런스 매뉴얼(RM0440) 참조.
- **G4 GPIO 속도**: G4는 GPIO 출력 속도 설정이 Low/Medium/High/Very High 4단계. 고속 통신이나 PWM 출력 핀은 `GPIO_SPEED_FREQ_VERY_HIGH` 설정 권장.
- Modbus CRC-16: 테이블 룩업 방식 사용 (속도 최적화).

---

## 양산용 펌웨어 다운로드 편의성 (플래싱 방법론)

IDE와 ST-Link(SWD)를 꽂아 펌웨어를 굽는 것은 개발/디버깅 시에는 필수적이나 양산(생산) 단계에서는 매우 번거롭습니다. STM32 칩에는 공장 출고 시부터 롬(ROM)에 구워져 있는 **"시스템 메모리 부트로더(System Memory Bootloader)"** 기능이 있어, 별도의 프로그래머 장비 없이도 손쉽게 롬 라이팅이 가능합니다.

### 추천 1. UART(RS485) 부트로더 활용 (강력 추천)

STM32G474는 부팅 시 특정 조건이 맞으면 내장 부트로더가 활성화되어 `USART1`, `USART2` 등을 통해 플래싱 대기 상태가 됩니다.

- **하드웨어 준비**: `PB8/BOOT0` 핀을 기본 풀다운으로 두고, 버튼이나 점퍼로 3.3V에 올릴 수 있게 외부로 빼놓습니다.
- **다운로드 방법**: `PB8/BOOT0`를 `HIGH(3.3V)`로 만든 상태에서 전원을 켭니다(또는 NRST 리셋). 칩이 사용자 코드가 아닌 공장 부트로더로 부팅합니다.
- **PC 작업**: ST-Link 프로그래머 없이, 시중의 저렴한 USB-to-RS485 변환기나 USB-to-UART(TTL) 젠더를 보드의 RS485 단자(`PA2/PA3`, `PB0/PB1` 방향 제어)나 디버그 UART 핀(`PB6/PB7`)에 꽂습니다.
- PC에서 ST 공식 무료 배포 프로그램인 **"STM32CubeProgrammer"**를 켜서 펌웨어(.hex 또는 .bin) 파일 1개만 선택하고 업로드를 누르면 끝입니다.

### 추천 2. Custom (사용자) 부트로더 개발 (통신 업데이트 / OTA)

부팅 영역(가령 플래시의 0x0800_0000 ~ 0x0800_4000)에 우리가 직접 짠 통신 부트로더 코드를 심어두고, 메인 양산 코드는 그 이후 주소(0x0800_4000)부터 할당하는 방식입니다.

- 평소에는 메인 앱으로 동작하다가, "펌웨어 업데이트 모드 진입" 메뉴나 Modbus 특수 명령이 들어오면 메인 앱이 칩을 리셋시키고 부트로더 구역으로 점프합니다.
- 부트로더가 현재 연결된 RS485를 통해 새로운 펌웨어 바이너리 데이터를 패킷으로 쪼개어 수신한 뒤 메인 앱 플래시 구역을 지우고 다시 씁니다. PC쪽에는 간단한 펌웨어 송신용 C# 프로그램(보통 YMODEM 프로토콜 등 사용) 하나만 띄워주면 현장 조작자가 마우스 클릭 한 번으로 통신선을 통해 간편하게 패치할 수 있습니다.

> **하드웨어 설계 요약**: 양산 시 UART 부트로더를 쓸 가능성이 있으면 `PB8/BOOT0`를 기본 풀다운으로 고정하고, 버튼/딥스위치/점퍼로 3.3V에 연결할 수 있게 둔다. BOOT0를 누른 채 전원을 켜면 ST-Link 없이도 시리얼 계열 인터페이스로 펌웨어 업데이트를 진행할 수 있다.

### 추천 3. ST-Link + STM32CubeProgrammer (개발~양산 겸용, 가장 간편)

ST-Link(SWD)는 STM32CubeProgrammer에서도 그대로 사용 가능합니다. IDE 없이 독립 실행하여 .hex/.bin 파일만 선택 후 다운로드할 수 있어, 양산 담당자도 간단히 조작할 수 있습니다.

- **BOOT0 핀 조작 불필요**: SWD로 플래시에 직접 접근하므로 부트로더 모드 진입이 필요 없음
- **어떤 상태에서든 플래싱 가능**: NRST가 연결되어 있으면 슬립/잠금 상태에서도 강제 리셋 후 다운로드
- **필요 장비**: ST-Link V2 (개당 3,000~5,000원 수준) + USB 케이블
- **필요 연결**: SWDIO(PA13), SWCLK(PA14), GND, NRST (총 4~5선)

> **정리**: 개발~양산 전 단계에서 ST-Link + STM32CubeProgrammer가 **가장 간편**합니다. 추천 1(UART 부트로더)은 ST-Link 없이 해야 할 때의 보험용 대안입니다.

---

## 흔한 실수

- **HRTIM PWM이 출력 안 됨** → PA8/PA9가 `GPIO_AF13_HRTIM1`인지, HRTIM Timer A counter와 output enable이 모두 켜졌는지, `PB3/SONIC_ON`이 활성 상태인지 확인. PA9는 USB-C PD 미사용 시 `UCPD1_DBDIS=1` 설정도 확인한다.
- **위상제어 조광기 PWM이 출력 안 됨** → 새 회로도 기준은 `PC9/TIM3_CH4(AF2)`다. `PB0/TIM3_CH3`로 초기화하면 출력이 나오지 않고 RS485 DE 핀과 충돌한다.
- **RS485 수신 불가** → `PB0(DE)`/`PB1(/RE)`가 아이들 시 LOW(수신 모드)인지 확인. 송신 후 `USART->ISR`의 `TC` 플래그(Transmission Complete) 확인 후 DE → LOW 전환. (G4의 USART 레지스터는 F1의 `USART_SR`이 아닌 `USART->ISR`/`USART->ICR` 구조임에 주의)
- **LCD 아무것도 표시 안 됨** → V0 핀의 대비 조절 가변저항 확인. 초기화 시퀀스 타이밍(15 ms → 4.1 ms → 100 µs) 준수.
- **Modbus CRC 불일치** → CRC 바이트 순서: CRC Low 먼저, CRC High 나중 (LSB-first). 다항식 0xA001.
- **ADC 값 불안정** → 샘플링 타임 증가 (최소 47.5 cycles, 고임피던스는 92.5~247.5 cycles 권장), 이동 평균 필터 적용, VDDA 바이패스 커패시터 확인.
- **택트 스위치 오동작** → 디바운스 시간 20 ms 적용, 내부 풀업 활성화 확인 (`GPIO_PULLUP`).
- **`HAL_Delay()` 동작 안 함** → SysTick 인터럽트 우선순위 확인. 다른 높은 우선순위 ISR에서 블록되면 HAL_Delay 무한 루프.
- **170 MHz 클럭 설정 실패** → HSE가 없는 환경에서는 HSI(16 MHz) + PLL 구성 사용. G4의 PLL은 PLLM/PLLN/PLLP/PLLQ/PLLR 5개 분주기가 있으므로 정확한 설정 필요. `RCC_OscInitStruct.OscillatorType` 및 `RCC_PLLCFGR` 확인.
- **ADC가 동작하지 않음** → G4 ADC는 사용 전 반드시 `HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED)` 캘리브레이션 호출 필수. F1과 달리 캘리브레이션 없이는 정확한 값을 읽을 수 없음.
- **HardFault 발생** → 스택 오버플로 가능성 확인. 재귀 호출이나 큰 로컬 배열 금지. 링커 스크립트에서 스택 크기 확인 (기본 0x400).

---

## 파라미터 저장 (확장)

사용자 설정값은 STM32G474RB/RC 내장 Flash 백업 또는 외부 EEPROM(I2C)에 저장 가능:

- 내장 Flash: 페이지 단위 소거 (**2 KB/page**, G4 기준), 쓰기 단위는 **더블워드(64비트/8바이트)**, 쓰기 수명 10,000회 → 마모 평준화 고려
  - G4 Flash 쓰기 절차: 언락(`HAL_FLASH_Unlock()`) → 페이지 소거(`HAL_FLASHEx_Erase()`) → 더블워드 프로그램(`HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ...)`) → 락(`HAL_FLASH_Lock()`)
  - F1과 달리 하프워드(16비트) 단위 쓰기 불가, 반드시 64비트 정렬 필요
- 외부 EEPROM (AT24C02 등): I2C 핀은 현재 미확정. `PB8`은 `BOOT0` 정책과 충돌하므로, EEPROM이 필요하면 `PB9`와 다른 여유 I2C 후보 핀을 CubeMX에서 다시 검토한다.

저장 대상: 주파수, 출력값, 동작 모드, Modbus 주소, 통신 속도, 패리티, 타이머 설정, 정전류 목표값.

---

## 문서 파일 (한글)

| 파일                        | 내용                       |
| --------------------------- | -------------------------- |
| `docs/코드_설명서.md`       | 펌웨어 전체 사양서         |
| `docs/회로도_설명.md`       | 회로 설계 및 부품 목록     |
| `docs/Modbus_레지스터맵.md` | Modbus 레지스터 상세 사양  |
| `docs/메뉴_구조.md`         | 메뉴 트리 및 UI 사양       |
| `docs/초음파_설계.md`       | 초음파 발진 회로 설계 상세 |
