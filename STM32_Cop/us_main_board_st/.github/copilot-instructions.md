# Copilot 지침 — STM32G474CBT6 초음파 메인보드

## 프로젝트 개요

**STM32G474CBT6 (LQFP48) 회로도 기준** 초음파 발진기 메인보드 펌웨어. **CMake + STM32Cube HAL** 프레임워크로 빌드.
HRTIM을 이용한 초음파 PWM 생성, 내부 고속 비교기(COMP)를 활용한 위상차 위상 동기(PLL),
LCD1602 디스플레이, 택트 스위치 메뉴 조작, 가변 저항 출력 레벨 제어, Modbus RTU(RS485) 통신을 지원한다.

- 타겟: STM32G474CBT6, LQFP48 회로도 기준 (Cortex-M4, 170 MHz, FPU, HRTIM 및 고속 아날로그 내장)
- 주의: 기존 코드/링커/일부 문서에는 `STM32G474RB/RC/RCTx` 표기가 남아 있다. CBT6은 LQFP48·128 KB Flash이므로 CubeMX 설정과 링커 메모리 용량을 실제 BOM에 맞게 동기화한다.
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
│   └── STM32G474RCTx_FLASH.ld     # 레거시 링커 스크립트 (CBT6 128 KB 용량으로 교체 필요)
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
│   ├── ultrasonic_pwm.h           # HRTIM/TIM PWM HW 제어 (주파수/동기 갱신)
│   ├── ultrasonic_ctrl.h          # 초음파 상위 제어 (소프트스타트, 모드)
│   ├── buzzer.h                   # PA5/TIM2_CH1 수동형 부저
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
│   ├── ultrasonic_pwm.c           # HRTIM PA8/PA9 역위상 출력 + 주기경계 갱신
│   ├── ultrasonic_ctrl.c          # 소프트스타트, 펄스/스윕 모드 관리
│   ├── buzzer.c                   # 2.7 kHz 버튼 확인음 비차단 재생
│   ├── lcd1602.c                  # HD44780 초기화, 문자열 출력
│   ├── lcd1602_hw.c               # 4비트 니블 전송, DWT μs 지연
│   ├── button.c                   # 디바운싱, 장기누름, 반복이벤트
│   ├── adc_control.c              # PB1 필터 + PA6 4kSPS DMA/200ms RMS
│   ├── resonance_tuning.c         # PA6 러프 + PA0/PA1 위상 정밀 공진 탐색
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
  - **1) AC 입력단 정전류 제어용 (SCT-13W + PA6/ADC2_IN3)**: 메인 220V 입력의 50/60Hz 소비전류를 1.65V 중심 AC로 직접 샘플링하고 software RMS로 계산한다. 수동/능동 정류회로는 사용하지 않는다.
  - **2) 트랜스듀서 출력단 PLL 위상 검출용 (고주파 CT 센서)**: 수십 kHz로 스위칭하는 최종 초음파 출력 라인의 전압과 전류 위상차를 읽는다. 전압은 `PA0/COMP3_INP(COMP_V)`, 전류는 `PA1/COMP1_INP(COMP_I)`에 입력하고 comparator 출력 timestamp로 공진점을 추적한다. 고주파 CT의 모델·권선비·burden은 별도 검증한다.
- **RS485 트랜시버**: 기존 5 V MAX485 옵션보드를 재사용한다. `PA2/TX`는 74AHCT125 U6C로 3.3→5 V 변환하고, `MAX485 RO`는 U6D와 PA3의 R80 4.7 kohm 외부 pull-up으로 5→3.3 V 변환한다. PA3에 5 V RO를 직접 연결하지 않는다.
- **FET 드라이버**:
  - **현재 확정: IR2104/IRS2104 2개 풀브리지**: `PA8/HRTIM1_CHA1`과 `PA9/HRTIM1_CHA2`를 각각 74AHCT125를 거쳐 U3/U7의 `IN`에 연결한다. 각 IR2104가 자신의 하프브리지 HO/LO를 상보 구동하고 고정 deadtime을 삽입한다.
  - **풀브리지/GDT 파형**: TA1/TA2는 50% 기준의 hardware complementary pair로 생성한다. 68 kHz 대역(중심주파수 최대 71.0 kHz)까지는 HRTIM 1.4 us, 80 kHz 대역(중심주파수 최소 77.0 kHz)부터는 0.8 us positive dead-time을 적용한다. 72.0 kHz를 판정 경계로 사용하며 Start 전에 중심주파수로 한 번만 선택하고 sweep 순간주파수에는 변경하지 않는다. 이는 IR2104 앞단 레그 보호와 별개로 TR1 뒤 IRFP460의 동시도통을 방지하기 위한 설정이다.
  - **TR1 권선비**: 1차 47T, 2차 42T. +12 V 풀브리지 기준 이상적인 2차는 약 `+/-10.7 V` (`21.4 Vpp`)이며 최종값은 IRFP460 Gate-Source 부하 상태에서 확인한다.
  - **안전 차단**: `PA15/SONIC_ON`을 74AHCT125를 거치지 않고 두 IR2104의 `SD`에 직접 공통 연결한다. PA15/공통 SD net에는 외부 pull-down 저항이 장착되어 reset·Hi-Z 상태를 LOW로 유지한다. Fault·정지 시에는 펌웨어가 PA15을 LOW로 내려 HO/LO를 모두 차단한다.
  - **FET**: IRF7351 듀얼 N채널 2개로 Q4A/Q3A, Q4B/Q3B 두 하프브리지를 구성한다. +12 V bus, SS24 + C72/C73 bootstrap을 사용한다.
- **제거된 외부 부품**: 외부 비교기(LM393/LM311 등), CD4046(VCO), HEF4520(카운터), AD5280(가변저항), SG3525(PWM 컨트롤러) - **모두 STM32G4 내부 하드웨어로 대체**

### 시스템 클럭 설정

- 소스: 내부 HSI (16 MHz) 또는 외부 HSE 크리스탈
- PLL: 내부 클럭을 뻥튀기 → SYSCLK = 170 MHz
- AHB = 170 MHz, APB1 = 170 MHz, APB2 = 170 MHz (타이머 클럭 기본 170 MHz)

### 핀 맵 (STM32G474CBT6 — LQFP48, 현재 회로도 기준)

> 이 표가 CBT6 회로도와 펌웨어의 기준이다. 이전 `docs/STM32G474RC_핀맵.md`는 RB/RC LQFP64 참고 자료이므로, PC0~PC12 또는 기존 PB0/PB1 RS485 방향 제어를 CBT6에 복사하지 않는다.

#### 이번 회로도의 핵심 배정

| 주제            | 현재 핀                      | 간단 설명                                        |
| --------------- | ---------------------------- | ------------------------------------------------ |
| 초음파 발진     | `PA8/PA9 = HRTIM1_CHA1/CHA2` | 고주파/스윕 발진과 주기경계 동기 갱신            |
| 위상제어 조광기 | `PA7 = TIM3_CH2(AF2)`        | 3kHz 위상제어 PWM 지령 출력                      |
| PWM 조정 VR     | `PB1 = ADC1_IN12`            | PA7 출력의 로컬 Duty limit 입력                  |
| LCD1602         | `PB10~PB15`, `PB0=CLK_LCD`   | 74HCT574 D 입력 6개와 CP latch clock             |
| 시스템 RUN LED  | `PA4=RUN_LED`                | 정상 동작 heartbeat, GPIO 주기 토글              |
| RS485           | `PA2/PA3`, `PB2~PB5`         | 5 V MAX485 옵션보드, U6C TX/U6D RX 레벨 변환     |
| SWD             | `PA13/PA14`                  | SWDIO/SWCLK, GPIO로 사용 금지                    |
| SONIC_ON        | `PA15`                       | 두 IR2104 SD 직접 공통 제어, 외부 pull-down 장착 |
| 공진 위상 입력  | `PA0=COMP_V`, `PA1=COMP_I`   | COMP3/COMP1 zero-cross와 ADC offset/진폭 측정    |

#### 초음파 발진 (HRTIM1 Timer A / 게이트 드라이버 연동)

| 핀   | 물리핀# | 포트/AF             | 회로도 Net | 기능                          |
| ---- | ------- | ------------------- | ---------- | ----------------------------- |
| PA8  | 30      | HRTIM1_CHA1(AF13)   | CHA1 / INA | 게이트 드라이버 INA           |
| PA9  | 31      | HRTIM1_CHA2(AF13)   | CHA2 / INB | 게이트 드라이버 INB           |
| PA15 | 39      | GPIO_Output, NOPULL | SONIC_ON   | 발진 Enable / 드라이버 Enable |

> PA9는 `UCPD1_DBCC1` 겸용 핀이므로 USB-C PD를 쓰지 않으면 초기화 초기에 `UCPD1_DBDIS=1` 설정을 넣는다. PA8/PA9는 `GPIO_AF13_HRTIM1`, `GPIO_SPEED_FREQ_VERY_HIGH`, `GPIO_NOPULL` 기준으로 설정한다.
>
> PA15/SONIC_ON에는 외부 pull-down 저항이 장착되어 있어 reset 또는 Hi-Z 상태에서 LOW가 유지된다. PA15는 `GPIO_MODE_OUTPUT_PP`, `GPIO_NOPULL`로 초기화하기 전에 output data를 LOW로 준비한다. HRTIM·deadtime·fault 초기화가 끝날 때까지 LOW를 유지하고, 정상 시작의 마지막 단계에서만 HIGH로 올린다. 정지/Fault에서는 PA15을 먼저 LOW로 내린 뒤 HRTIM 출력을 정지한다.

#### 위상제어 조광기 (3kHz PWM 출력)

| 핀  | 물리핀# | 포트/AF       | 기능                                    |
| --- | ------- | ------------- | --------------------------------------- |
| PA7 | 15      | TIM3_CH2(AF2) | 위상제어 듀티 지령 전달용 3kHz PWM 출력 |

> **중요**: 위상제어 PWM은 **`PA7 / TIM3_CH2(AF2) / PWM_OUTPUT`**이고 조정 입력은 **`PB1 / ADC1_IN12 / PWM_VR`**이다. PA7은 ADC2_IN4와 COMP2_INP 기능을 동시에 사용할 수 없다. 기존 `PB0=GOING`은 `PB0=CLK_LCD`로 변경하고 상태 출력은 `PA4=RUN_LED`, `PC13=GOING`, `PC14=END_BZ`, `PC15=SPARE`로 사용한다.

#### LCD1602 (4비트 병렬 모드)

| 핀   | 물리핀# | 포트        | 기능                                     |
| ---- | ------- | ----------- | ---------------------------------------- |
| PB0  | 16      | GPIO_Output | CLK_LCD, 74HCT574 CP(상승 에지에서 저장) |
| PB10 | 22      | GPIO_Output | 74HCT574 D 입력 -> LCD RS                |
| PB11 | 25      | GPIO_Output | 74HCT574 D 입력 -> LCD E                 |
| PB12 | 26      | GPIO_Output | 74HCT574 D 입력 -> LCD D4                |
| PB13 | 27      | GPIO_Output | 74HCT574 D 입력 -> LCD D5                |
| PB14 | 28      | GPIO_Output | 74HCT574 D 입력 -> LCD D6                |
| PB15 | 29      | GPIO_Output | 74HCT574 D 입력 -> LCD D7                |

> RW 핀은 GND에 고정한다. PB10~PB15에 데이터를 먼저 설정한 뒤 PB0/CLK_LCD에 LOW->HIGH 상승 에지를 만들고 다시 LOW로 복귀한다. PB0은 reset 중 오동작 방지를 위해 외부 10 kohm pull-down을 권장하며 CP 직렬 100~330 ohm과 74HCT574 VCC 근처 100 nF를 선택 적용한다. 74HCT574 `/OE`는 정상 동작 중 활성 상태로 고정하고 미사용 D 입력은 GND 또는 정의된 논리에 연결한다. LCD 출력값과 남은 채널 상태는 하나의 shadow byte로 보존한다.

#### 택트 스위치 (외부 풀업, Active LOW)

| 핀  | 물리핀# | 포트       | 기능       |
| --- | ------- | ---------- | ---------- |
| PB6 | 43      | GPIO_Input | MODE       |
| PB7 | 44      | GPIO_Input | DOWN       |
| PB8 | 45      | GPIO_Input | UP         |
| PB9 | 46      | GPIO_Input | START_STOP |

> MODE(PB6), DOWN(PB7), UP(PB8), START_STOP(PB9)에는 보드 외부 pull-up 저항이 실장되어 있다. 네 핀은 `GPIO_NOPULL`로 초기화하며 LOW를 눌림, HIGH를 해제로 해석한다. 내부 `GPIO_PULLUP`을 중복 적용하지 않는다.

#### ADC 및 comparator 입력 (공진/레벨 검출)

| 핀  | 물리핀# | ADC/기능             | 회로도 Net | 용도                                     |
| --- | ------- | -------------------- | ---------- | ---------------------------------------- |
| PA0 | 8       | ADC1_IN1 / COMP3_INP | COMP_V     | 초음파 출력전압 zero-cross, offset/진폭  |
| PA1 | 9       | ADC1_IN2 / COMP1_INP | COMP_I     | 초음파 출력전류 zero-cross, offset/진폭  |
| PA6 | 14      | ADC2_IN3             | CT_ADC     | 220 VAC 입력 CT RMS 전류 센싱            |
| PA7 | 15      | TIM3_CH2(AF2)        | PWM_OUTPUT | 위상제어용 3kHz PWM 출력                 |
| PB1 | 17      | ADC1_IN12            | PWM_VR     | PA7 출력의 로컬 Duty limit 가변저항 입력 |

> **핀 기능 주의**: PA0은 `COMP1_INM` 겸용이지만 이 설계에서는 반드시 `COMP3_INP`로 선택한다. PA1은 `COMP1_INP`로 선택한다. PB1은 ADC analog/no-pull로 설정하고, PA7은 PWM 전용이므로 `ADC2_IN4` 또는 `COMP2_INP`로 초기화하지 않는다.
> CubeMX `.ioc`에서는 PA0/PA1의 대표 기능을 `COMP3_INP/COMP1_INP`로 기록한다. 동일 analog pad의 ADC1_IN1/IN2 offset 측정은 펌웨어에서 `HAL_ADC_ConfigChannel()`로 명시하며, CubeMX 코드 재생성 후에도 USER CODE 구간에 유지한다.
>
> **CT 입력 주의**: PA6은 `SCT-13W`로 220 VAC 입력전류의 RMS 크기와 보호를 판단하는 ADC 입력이다. AC 250V/20A max, 1:1000 전류출력형·내부 burden 없음 사양을 전제로 R29/R30 240 ohm 병렬(합성 120 ohm), 1.65V bias, R57 1 kohm, C33 100 nF 및 BAT54WS rail clamp를 사용한다. 정류회로는 사용하지 않는다.
>
> MCU 아날로그 입력은 정상 동작 중 약 `0.3V~3.0V` 안쪽으로 제한한다. T2 nominal 50~550Vpp 신호는 확정된 300 kohm/1 kohm 분압, bias, 직렬저항과 저용량 clamp를 거쳐야 한다. L2 뒤 Y3 양단의 공진 상승전압은 이 PA0 경로가 직접 측정하지 않는다.

#### 내장 아날로그 블록 할당 (COMP 기준전압 · DAC · OPAMP)

| 블록      | 권장 연결        | 용도                               | 상태 |
| --------- | ---------------- | ---------------------------------- | ---- |
| COMP3_INP | PA0 / COMP_V     | 전압(V) zero-cross                 | 확정 |
| COMP1_INP | PA1 / COMP_I     | 전류(I) zero-cross                 | 확정 |
| DAC3_CH1  | 내부 → COMP3_INM | COMP_V 기준, 계산 초기값 약 1.618V | 확정 |
| DAC1_CH1  | 내부 → COMP1_INM | COMP_I 기준, 계산 초기값 약 1.623V | 확정 |

> COMP_V/COMP_I의 기준전압은 고정 1.65V가 아니다. 출력 정지/무전류 상태에서 ADC1_IN1/IN2 중심전압을 측정하고 DAC3_CH1/DAC1_CH1에 각각 보정값을 적용한다. 외부 DAC 출력 핀은 사용하지 않는다.

> DAC1/DAC3 출력은 외부 핀으로 내보내지 않고 comparator 내부 INM 경로로만 연결한다. COMP 기준전압은 소프트웨어에서 채널별로 보정한다.

#### Modbus RTU (RS485, 선택 사양 보드 연동)

| 핀  | 물리핀# | 포트/모드   | 회로도 Net   | 기능                         |
| --- | ------- | ----------- | ------------ | ---------------------------- |
| PA2 | 10      | USART2_TX   | USART2_TX    | U6C를 거쳐 5 V 보드 TX/DI    |
| PA3 | 11      | USART2_RX   | USART2_RX    | U6D+R80 4.7k 외부 pull-up RX |
| PB4 | 41      | GPIO_Output | DE_RS485     | Driver Enable, HIGH=송신     |
| PB5 | 42      | GPIO_Output | /RE_RS485    | Receiver Enable, LOW=수신    |
| PB2 | 19      | GPIO_Output | RTERM        | 종단저항 ON/OFF              |
| PB3 | 40      | GPIO_Input  | 485BD_DETECT | 외부 pull-up, LOW=보드 장착  |

> `PA3`은 `USART2_RX(AF7)`, `GPIO_NOPULL`로 설정한다. MAX485 RO가 LOW이면 U6D가 LOW를 구동하고, RO가 HIGH이면 U6D 출력이 Hi-Z가 되어 R80 4.7 kohm가 PA3을 3.3 V HIGH로 만든다. `PB3/485BD_DETECT`도 MCU 보드의 외부 3.3 V pull-up을 사용하므로 `GPIO_NOPULL`로 설정한다. 옵션보드의 J12 pin 8이 GND이므로 미장착=HIGH, 장착=LOW이며, LOW일 때만 Modbus 메뉴를 활성화한다.

#### 외부 제어 인터페이스 (스위치 입력 및 상태 출력)

| 핀   | 물리핀# | I/O           | 회로도 Net | 기능                             |
| ---- | ------- | ------------- | ---------- | -------------------------------- |
| PA10 | 32      | Input         | REMOTE     | 외부 리모트 접점 입력            |
| PA11 | 33      | Input         | RUN_SW     | RUN lock 스위치 입력             |
| PA12 | 34      | Input         | SWEEP_SW   | Sweep ON/OFF 스위치 입력         |
| PA15 | 39      | Output        | SONIC_ON   | 발진 ON/OFF, IR2104 SD 공통 제어 |
| PA4  | 12      | Output        | RUN_LED    | MCU 정상 동작 heartbeat LED      |
| PC13 | 2       | Output        | GOING      | 초음파 정상 발진 중 ON           |
| PC14 | 3       | Output        | END_BZ     | 내부 타이머 종료 후 2초 ON       |
| PA5  | 13      | TIM2_CH1(AF1) | BZ_OUT     | 수동형 부저 2.7 kHz PWM          |

> PA4/RUN_LED는 push-pull 출력으로 초기화하고 정상 main loop 또는 주기 task에서 `HAL_Delay()` 없이 tick 기준으로 토글한다. 500 ms마다 토글하면 약 1 Hz 점멸이 된다. 기본 문서에서는 `HIGH=ON`으로 간주하되 LED가 3.3 V 쪽에 연결된 sink 구성이면 매크로에서 극성을 반전한다. PA5/BZ_OUT은 내부 발진회로가 없는 수동형 부저이므로 TIM2_CH1의 2.7 kHz PWM으로 구동하며, 직접 구동 전류가 크면 트랜지스터 드라이버를 사용한다. PC13/PC14는 LSE를 사용하지 않을 때 GPIO로 사용하고 PC15는 SPARE 입력이다. PA10은 UCPD1_DBCC2, PA11/PA12는 USB DM/DP 겸용이므로 USB-C/USB 기능을 활성화하지 않는다.
>
> REMOTE(PA10), RUN_SW(PA11), SWEEP_SW(PA12)에는 보드 외부 pull-up 저항이 실장되어 있다. 세 핀 모두 `GPIO_NOPULL`, Active LOW 입력으로 설정하며 LOW를 접점 ON으로 해석한다. PB6~PB9의 네 버튼까지 합쳐 일곱 입력 모두 같은 외부 pull-up 정책을 사용한다.

#### 디버그 / 기타

| 핀   | 물리핀# | 포트/모드 | 기능                          |
| ---- | ------- | --------- | ----------------------------- |
| PA13 | 37      | SWDIO     | ST-Link SWD, GPIO 사용 금지   |
| PA14 | 38      | SWCLK     | ST-Link SWD, GPIO 사용 금지   |
| PF0  | 5       | OSC_IN    | 8MHz HSE 크리스탈 입력        |
| PF1  | 6       | OSC_OUT   | 8MHz HSE 크리스탈 출력        |
| NRST | 7       | RESET     | ST-Link 및 DTR 리셋 회로 연결 |

> **ST-Link V2 다운로드 커넥터 필수 결선 팁**: 최소 동작 핀은 VDD(3.3V 참조), GND, SWDIO, SWCLK이다. 안정적인 디버깅/다운로드를 위해 NRST까지 연결한다.

#### PCB 아트웍 배선 라우팅 가이드 (LQFP48 물리 핀 기준)

```text
좌측 핀: PA0~PA7 아날로그/USART2/위상제어 PWM/RUN_LED, PF0/PF1 HSE, PC13~PC15 상태·부저
상단 핀: PB0 CLK_LCD, PB2~PB5 RS485, PB6~PB9 버튼
우측 핀: PB10~PB15 LCD, PA8~PA15 HRTIM·외부입력·SWD·SONIC_ON
```

| PCB 영역              | 배치 부품                            | MCU 핀                       |
| --------------------- | ------------------------------------ | ---------------------------- |
| 조작부                | MODE, DOWN, UP, START/STOP           | PB6~PB9                      |
| RS485 커넥터/트랜시버 | USART2, DE, /RE, RTERM, Board Detect | PA2, PA3, PB4, PB5, PB2, PB3 |
| 아날로그 검출부       | 공진/레벨 검출, CT 전류              | PA0, PA1, PA6                |
| 위상제어 인터페이스   | 3kHz PWM 지령                        | PA7                          |
| RUN 상태 표시         | MCU heartbeat LED                    | PA4                          |
| 게이트 드라이버 근처  | HRTIM 출력, SONIC_ON                 | PA8, PA9, PA15               |
| LCD 레벨변환/커넥터   | 74HCT574 CP와 LCD1602 4bit           | PB0, PB10~PB15               |
| 디버그 커넥터         | SWD, NRST                            | PA13, PA14, NRST             |

**핀 변경 이력 (기존 대비):**

| 변경 내용    | 이전 핀                   | 현재 핀          | 사유                                    |
| ------------ | ------------------------- | ---------------- | --------------------------------------- |
| 초음파 출력  | PA8+PB13 / TIM1           | PA8+PA9 / HRTIM1 | GDT용 1.4/0.8 us 대역별 DT 및 정밀 스윕 |
| 위상제어 PWM | PC9 / TIM3_CH4            | PA7 / TIM3_CH2   | CBT6 LQFP48 배정                        |
| LCD          | PB12, PB14, PB15, PC6~PC8 | PB0, PB10~PB15   | 74HCT574 CP 및 데이터 배선              |
| RUN LED      | PB0/GOING 등 기존 혼용    | PA4/RUN_LED      | MCU heartbeat 전용 출력 분리            |
| RS485 DE/RE  | PB0/PB1                   | PB4/PB5          | 회로도 기준 분리 제어                   |
| 버튼         | PC0~PC3                   | PB6~PB9          | MODE, DOWN, UP, START/STOP 순서         |
| SONIC_ON     | PB3                       | PA15             | CBT6 LQFP48 배정                        |

> **PCB 아트웍 핵심 포인트 1**: PA8/PA9 HRTIM 출력은 게이트 드라이버 INA/INB까지 짧고 나란히 배선한다.
>
> **PCB 아트웍 핵심 포인트 2**: PA0/PA1/PA6/PB1 아날로그 입력은 고전압 스위칭 루프와 분리하고, 정상 동작 중 MCU 입력이 0.3V~3.0V 범위를 벗어나지 않게 보호한다. PA7 PWM은 CT/ADC 및 PB1 PWM_VR 배선과 분리한다.
>
> **PCB 아트웍 핵심 포인트 3**: PB10~PB15 LCD 버스는 스위칭 노드와 평행 장거리 배선을 피한다.

---

## 모듈별 상세

### 1. 초음파 발진 (`ultrasonic_pwm` + `ultrasonic_ctrl`)

HRTIM1 Timer A를 사용하여 현재 제품 범위의 산업용 초음파 주파수(15–128 kHz)를 생성한다.
`ultrasonic_pwm`이 HRTIM 하드웨어 레벨(주파수, 듀티, 데드타임)을 담당하고,
`ultrasonic_ctrl`이 상위 로직(소프트 스타트, 펄스/스윕 모드 관리, PLL 공진 추적)을 담당한다.

> 현재 저장소의 일부 기존 코드는 TIM1 기반일 수 있다. 새 회로도 기준 출력은 `PA8/HRTIM1_CHA1`, `PA9/HRTIM1_CHA2`이므로 CubeMX/펌웨어 재생성 시 HRTIM으로 전환한다.

**지원 주파수 대역:**

- STM32G474CBT6 (170MHz, HRTIM 내장) 기준: 15kHz~128kHz 발진, 중심주파수 기준 ±100Hz~±1000Hz 스윕 지원 목표
- HRTIM은 TIM1보다 주파수/edge 조정 해상도가 높아, 작은 스윕 폭에서도 주파수 계단이 덜 거칠다.

**핵심 설정:**

- **PWM 출력**: `HRTIM1_CHA1(PA8)` + `HRTIM1_CHA2(PA9)` pair 출력
- **Timer A clock**: `HRTIM_PRESCALERRATIO_MUL4`, counter 680 MHz. 전체
  15~128 kHz에서 `PER=45333~5312` 범위를 사용해 100 kHz 이상에서도 충분한
  주기 해상도를 유지한다. 주파수 계산과 자기검증은 반드시
  `HRTIM_COUNTER_CLOCK_HZ`를 사용한다.
- **데드 타임**: 각 IR2104의 내부 약 0.5 us급 dead-time은 앞단 IRF7351
  한 레그의 상·하 MOSFET을 보호한다. TR1 뒤 IRFP460에는 HRTIM이 별도로
  zero-vector를 만든다. 중심주파수 72.0 kHz 미만은 1.4 us(238 count),
  72.0 kHz 이상은 0.8 us(136 count)를 적용한다. 메뉴상 68 kHz 대역은
  65.0~71.0 kHz, 80 kHz 대역은 77.0~128.0 kHz여서 서로 겹치지 않는다.
  dead-time은 Start 전에 선택하며 sweep 순간주파수 변화에는 고정한다.
- **주파수 설정**: HRTIM Timer A period 값 변경

```
주파수 = HRTIM 타이머 클럭 / period
실제 period 계산은 CubeMX/HAL HRTIM clock 설정과 prescaler를 기준으로 맞춘다.
```

- **듀티비 설정**: Timer A compare 값 변경 (가변저항 ADC 값 또는 Modbus 명령에 매핑)
- **출력 Enable**: HRTIM 출력 enable + `PA15/SONIC_ON` 게이트 드라이버 Enable을 함께 관리

**안전 기능 및 자동 튜닝(Auto-Tuning):**

| 기능          | 설명                                                          |
| ------------- | ------------------------------------------------------------- |
| 소프트 스타트 | 출력 개시 시 듀티비를 0%에서 목표값까지 점진적 증가 (500 ms)  |
| 비상 정지     | Modbus 명령, 외부 제어 입력 또는 이상 감지 시 즉시 출력 차단  |
| 출력 제어     | START_STOP(PB9) 또는 메뉴 명령으로 초음파 출력 시작/종료 제어 |
| 출력 제한     | 듀티비 상한 클램핑 (하드웨어 보호)                            |
| 이상 감지     | 타이머 브레이크 입력(BRK) 활용 가능 (확장 시)                 |
| **오토 튜닝** | **가변 부하(세제, 세척물, 온도) 대응 공진점 자동 추적 (PLL)** |

**공진 주파수 하이브리드 제어 (디지털 PLL + 기준점 스윕 💡):**

초음파 세척기는 최대 수십 개의 BLT(진동자)를 병렬 결합하여 사용하므로, 각 진동자의 개별 공진 주파수 편차가 존재합니다. 특정 단일 공진점(예: 28.0kHz)에만 고정하면 전체 효율이 떨어집니다. 이를 극복하기 위해 **"위상차 기반 자동 공진점 탐색"**과 **"기준점 기반 수동 스윕(Sweep)"**을 결합한 하이브리드 방식을 적용합니다.

1. **내장 고속 비교기(COMP)를 활용한 위상 검출**:
   - 외부 아날로그 비교기(LM393 등)를 **완전히 제거**합니다. 대신 진동자에 인가되는 전압(V)과 전류(I) 파형을 하드웨어로 적절히 감쇄/변환하여 STM32G4의 하드웨어 핀으로 직접 입력합니다.
   - **전압(V) 파형**: T2 18:32의 nominal 50~550Vpp 출력을 R35/R38/R39=100 kohm 직렬과 R41=1 kohm으로 감쇠하고, R59=1 kohm, C49=22 pF, BAT54WS rail clamp를 거쳐 `PA0/ADC1_IN1/COMP3_INP(COMP_V)`에 인가한다. 550Vpp에서 예상 입력은 약 0.704~2.531V이다.
   - **전류(I) 파형**: 출력 선로의 고주파 CT 신호에 burden, bias, 직렬 보호와 저용량 clamp를 적용하여 `PA1/ADC1_IN2/COMP1_INP(COMP_I)`에 인가한다. 고주파 CT의 모델·권선비·burden 값은 별도 검증 항목이다.
   - COMP3/COMP1은 두 AC 파형의 zero-cross를 디지털 edge로 바꾸고, 출력은 timer input capture 또는 HRTIM 내부 event/capture에 연결한다. main loop GPIO polling이나 ADC sample index만으로 edge 시간을 결정하지 않는다.
   - **기준전압(INM)**: DAC3_CH1→COMP3_INM(COMP_V), DAC1_CH1→COMP1_INM(COMP_I)을 사용한다. 계산 초기값은 약 1.618V/1.623V이지만 출력 정지·무전류 상태에서 ADC로 중심전압을 측정하여 개별 보정한다. 일괄 1.65V 설정을 금지한다.
   - 사용할 comparator→timer/HRTIM 내부 interconnect는 RM0440 및 CubeMX에서 실제 선택 가능한 경로를 확인한다. 과거의 `COMP1/COMP2→TIM2_IC1/IC2` 고정 가정은 사용하지 않는다.
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
   - STM32는 `SCT-13W`의 1.65V 중심 AC 파형을 고정 주기로 샘플링하고, offset 제거와 RMS 계산으로 실제 소비 전류값(A)을 구합니다.
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
│RUN     TIME:19M│  ← 1행: 동작 상태 + 남은/설정 시간
│PL: 60%  28.0kHz│  ← 2행: 출력 레벨 + 현재 주파수
└────────────────┘
```

**구현 주의사항:**

- 명령/데이터 기록 후 최소 37 µs 대기 (Clear/Home은 1.52 ms)
- 화면 갱신은 값 변경 시에만 수행 (불필요한 깜빡임 방지)
- 초기화 시퀀스: 전원 ON → 15 ms 대기 → 4비트 모드 진입 (3회 반복 규격 준수)

### 3. 메뉴 시스템 (`menu` + `menu_screen`)

16×2 LCD와 `MODE/DOWN/UP/START_STOP` 네 버튼에 맞춘 FSM 계층 메뉴를 사용한다.
`menu`는 상태 전이와 임시 편집값을, `menu_screen`은 표시만 담당한다. 평상시에는
메인 운전 화면만 보이고, 설정 메뉴는 출력이 완전히 정지했을 때만 진입시킨다.

#### 권장 메인 화면

```text
RUN     TIME:19M
PL: 60%  28.0kHz
```

- 운전 중 항상 주파수, 남은/설정 시간, `PL`을 한 화면에 유지한다.
- 위상제어 모드의 `PL`은 PB1 가변저항 0~100%를 표시한다.
- 정전류 모드의 `PL`은 제어기가 PA7로 실제 출력하는 0~100% 명령을 표시한다.
  PB1은 정전류 제어의 로컬 최대 출력 제한으로 유지하고, 목표 전류에 도달하지
  못하면 보조 상태 화면에 `LIMIT`을 표시한다.
- 정전류 모드에서도 기본 화면은 유지하고, 운전 중 `MODE` 짧게 누르기로
  `I:4.9/5.0A OK`와 Sweep 상태를 보여주는 보조 상태 화면만 전환한다.
- 운전 중에는 설정값을 편집하지 않는다. 설정 시도 시 `STOP TO SET`을 표시한다.

#### 권장 메뉴 트리

```text
MENU_MAIN
└── MODE 짧게 (출력 정지 상태)
    ├── 1 TIME             — 기존 M/S/CONT 타이머
    ├── 2 FREQUENCY
    │   ├── BAND           — 28k / 40k / 68k / 80k+
    │   ├── MANUAL TUNE    — UP/DOWN으로 중심주파수 조정 후 저장
    │   └── AUTO TUNE      — 준비 화면에서 START 3초로 탐색 시작
    ├── 3 SWEEP
    │   ├── WIDTH          — 중심주파수 기준 ±100~±1000 Hz, 100 Hz step
    │   └── RATE           — 삼각 Sweep 50~200 Hz, 10 Hz step
    ├── 4 OUTPUT CTRL
    │   ├── PHASE/VOLUME   — PB1 → PA7 3 kHz PWM 5~100%
    │   └── CONST CURRENT  — PA6 RMS, 목표 1.0~6.0 A, 0.1 A step
    └── 5 SYSTEM           — Modbus / FW 정보 / 진단값
```

`80k+`는 하나의 고정 주파수가 아니다. 선택 후 nominal center를
`77.0~128.0 kHz`에서 지정할 수 있어 80.0 kHz 아래쪽 공진점도 찾을 수 있다.
`28k/40k/68k` preset도 단순 표시값이
아니라 해당 진동자와 매칭회로에 허용된 수동/자동 튜닝 범위를 선택하는 항목으로
취급한다. 각 Band의 절대 탐색 상·하한은 시제품 측정 후 별도 상수로 확정한다.

현재 구현 FSM은 RUN SW OFF·출력 정지 상태에서 MODE를 짧게 누를 때 아래 순서로
순환한다.

`MAIN → TIME → FREQUENCY → TUNE SELECT → MANUAL/AUTO TUNE → SWEEP WIDTH →
SWEEP SPEED → OUTPUT MODE → (CONST I일 때 CURRENT SET) → MAIN`

상태명은 `MENU_MAIN`, `MENU_TIME`, `MENU_FREQUENCY`, `MENU_TUNE_SELECT`,
`MENU_TUNE_MANUAL`, `MENU_TUNE_AUTO`, `MENU_SWEEP_WIDTH`, `MENU_SWEEP_RATE`,
`MENU_OUTPUT_MODE`, `MENU_CURRENT_SET`이다. UP/DOWN은 현재 화면의 값 변경,
MODE는 다음 화면 이동, MODE 1초는 메인 복귀로 통일한다.

#### 버튼 역할

| 화면                  | MODE                      | UP/DOWN              | START_STOP               |
| --------------------- | ------------------------- | -------------------- | ------------------------ |
| 메인/정지(RUN SW OFF) | 다음 설정 화면            | 사용 안 함           | REMOTE 운전 사용         |
| 메인/운전             | 보조 상태 화면            | 사용 안 함           | 짧게=정지, 1초=비상정지  |
| 일반 설정             | 짧게=다음 화면, 길게=메인 | 값 증감, hold repeat | 화면별 보조 선택         |
| MANUAL TUNE           | 다음 화면/출력 정지       | 주파수 ±100 Hz       | 시험 출력 ON/OFF         |
| AUTO TUNE 준비        | 다음 화면                 | 사용 안 함           | **3초 유지 시에만 시작** |
| AUTO TUNE 실행        | 취소 후 다음 화면         | 사용 안 함           | 짧게=중단 및 출력 차단   |

`button.c`에는 1초 `BTN_EVT_LONG_PRESS`와 별도로 3초 `BTN_EVT_HOLD_3S`가
구현되어 있다. START 3초 Auto-Tuning은 반드시 출력이 정지된
`AUTO TUNE` 준비 화면에서만 해석한다. 다른 화면에서 START 장기 누름은 기존
비상정지 우선권을 유지하여 두 동작이 충돌하지 않게 한다.

#### 주파수 선택과 수동 튜닝

- `FREQUENCY`: UP/DOWN으로 `28k → 40k → 68k → 80k+`를 선택하며 값은 즉시
  `UltrasonicCtrl_SetFrequency()`와 Sweep 중심주파수에 반영된다. 각 Band는
  마지막 튜닝값을 따로 유지하므로 40 kHz Band를 38.5 kHz로 맞췄다면 다시
  선택할 때 38.5 kHz가 표시되고 출력된다.
- `MANUAL TUNE`: 선택 Band 범위 안에서 UP/DOWN 짧게는 100 Hz, hold repeat도
  100 Hz씩 연속 변경한다. 첫째 줄은 `MANUAL TUNE`, 둘째 줄은
  `F:28.0 I:3.2A`처럼 현재 주파수와 PA6 RMS 전류를 표시한다.
- 수동 튜닝 화면의 START는 외부 `SWEEP_SW` 상태와 무관하게 Sweep을 끄고 선택한
  중심주파수의 고정 시험출력을 ON/OFF한다. 작업자는 PA6 표시와 외부 전류계를
  보면서 주파수를 조정할 수 있다.
- 네 Band별 중심주파수와 마지막 선택 Band는 Flash settings v3에 저장한다.
  Manual 화면을 MODE로 벗어나거나 START로 시험출력을 정지할 때 저장하며,
  변경이 없으면 Flash를 다시 쓰지 않는다. 기존 v1/v2 설정은 자동 호환한다.

#### Auto-Tuning 권장 순서

1. 선택 Band, 제한된 Tune 출력, 과전류/Fault 조건을 확인한다.
2. `AUTO TUNE READY` 화면에서 START를 3초 유지한다.
3. 중심주파수 ±1 kHz를 100 Hz 간격으로 coarse scan한다. 각 지점에서 주파수
   변경 뒤 완전히 새로 수집된 PA6 RMS 200 ms window 2개를 기다리고, 입력
   소비전류의 국부 최저점을 러프 공진 후보로 잡는다.
4. 러프 후보 ±300 Hz를 100 Hz 간격으로 다시 scan하며 PA0 전압위상과 PA1
   전류위상의 절대 위상차가 가장 작은 지점을 정밀 공진점으로 선택한다.
   검색 구간 양 끝값, CT 무신호, ADC 포화값은 러프 후보로 채택하지 않는다.
5. 위상 입력이 유효하지 않거나 최저 위상차가 45도를 넘으면 PA6 러프 후보를
   결과로 유지하되 `ROUGH` 상태로 표시한다. 완료 결과는 해당 Band의 중심값으로
   즉시 Flash v3에 저장한다.
6. START/MODE, REMOTE OFF, 과전류, 위상 이상 또는 ADC 오류가 발생하면 즉시
   `PA15/SONIC_ON`과 HRTIM 출력을 차단하고 기존 중심주파수를 보존한다.

> **현 보드의 확정 전략:** 작업자가 기존 CD4046 VCO와 입력 전류미터로 확인한
> 경험에 따라 PA6/SCT-13W 입력 소비전류의 국부 최저점을 러프 공진점으로 사용한다.
> 최종 정밀점은 PA0(COMP_V)/PA1(COMP_I)의 위상차 0점에 가장 가까운 주파수로
> 보정한다. 보드/진동자 조합이 바뀌면 PA6 최저점과 실제 출력파형의 관계를 다시
> 확인하며, Band 전체 최저값이나 검색 끝점은 자동 채택하지 않는다.

#### Sweep 정의

- `WIDTH`는 저장된 중심주파수의 양쪽 폭이다. 예: center 28.0 kHz,
  width ±500 Hz이면 27.5~28.5 kHz를 왕복한다.
- `RATE`는 한 번의 `최저→최고→최저` 삼각파 왕복 주기 수다. 100 Hz이면 초당
  100회 왕복한다. 기존 `SWEEP_TIME`과 혼용하지 않는다.
- 50~200 Hz Sweep은 1 ms main-loop tick만으로 만들지 않는다. 최소 32 point/cycle
  기준 1.6~6.4 kHz update timer 또는 HRTIM/DMA와 phase accumulator를 사용하고,
  period/compare는 preload 후 동기 update한다.
- 현재 `UltrasonicPWM_SetFrequency()`의 100 Hz 단위 API와 별도로 내부 Sweep용
  Hz/fixed-point API를 두어 작은 폭에서도 계단과 끝점 jump가 생기지 않게 한다.
- Sweep 시작/끝은 중심주파수와 선택 Band의 안전 범위를 넘지 않도록 clamp한다.
- **현재 구현:** TIM7 10 kHz ISR과 32-bit phase accumulator로 Hz 단위 삼각파를
  생성한다. `UltrasonicPWM_SetFrequencyHzFast()`는 Timer A update를 잠근 동안
  PER/CMP1 preload를 함께 기록하고 다음 repetition(PWM 주기 경계)에서 반영한다.
  출력 중 software update로 현재 주기를 자르는 동작은 금지한다.

#### 출력 제어 모드

- `PHASE/VOLUME`: PB1 0~3.3 V를 PA7 3 kHz PWM 5~100%에 매핑한다.
- `CONST CURRENT`: PA6 RMS 전류를 목표 1.0~6.0 A와 비교하여 PA7 PWM을 PI 또는
  제한된 증감 제어로 보정한다. 출력 slew-rate, deadband, 적분 anti-windup,
  최소/최대 PWM clamp, 센서 단선/포화 검출을 반드시 둔다.
- 정전류 모드에서도 PB1 값을 로컬 최대 출력 limit으로 사용한다. 제어기 출력이
  이 limit에 계속 걸려 목표 전류에 도달하지 못하면 `CURRENT LIMIT` 상태를 만들고
  보조 화면에는 `LIMIT`을 표시한다. PB1을 무시하고 출력을 자동으로 100%까지
  올리지 않는다.
- PA6는 `TIM6 TRGO → ADC2 → DMA1_Channel1 circular` 경로로 4 kSPS 샘플링하고,
  offset을 제거한 200 ms RMS를 사용한다. `ADC_Control_GetCurrentCentiAmp()`의
  단위는 0.01 A이며 `500=5.00 A`이다.
- 정전류 loop는 여러 Sweep cycle의 평균 전류를 천천히 제어한다. Sweep 폭 자동
  보정은 빠른 PI loop와 분리된 supervisory loop로 두고, 불안정/과전류 시 폭을
  줄이며 충분히 안정된 뒤에만 사용자가 설정한 최대 폭까지 천천히 늘린다.

#### 권장 구현 순서

1. MODE 순환 메뉴 FSM/화면과 출력 정지 상태 값 편집을 검증한다. (구현 완료)
2. settings v3 migration과 Band별 중심주파수 저장을 검증한다. (구현 완료,
   Sweep/제어모드 저장은 추후 확장)
3. Manual Tune 시험출력과 중심주파수 저장을 구현한다. (구현 완료)
4. Hz 단위 삼각 Sweep generator를 구현하고 PA8/PA9 동기 파형을 실측한다.
   (펌웨어 구현 완료, 실부하 측정 필요)
5. PA6 timer-trigger + DMA RMS와 A 단위 calibration을 완료한다. (펌웨어 구현 완료,
   0.2/1/3/5/6/7 A 실측 gain 보정 필요)
6. `resonance_tuning.c`의 PA6 러프/PA0·PA1 정밀 탐색을 AUTO TUNE 화면의
   START 3초와 연결한다. (구현 및 Band별 Flash 결과 저장 완료)
7. 정전류 loop를 Sweep OFF에서 먼저 검증한 뒤 Sweep supervisory loop를 추가한다.
8. PA6 최소 전류 Auto-Tuning을 제한적으로 검증하고, 최종적으로 PA0/PA1 위상 기반
   탐색과 보호 조건을 통합한다.

### 4. 택트 스위치 입력 (`button`)

**디바운싱 알고리즘:**

- 소프트웨어 디바운싱: 10 ms 주기 폴링, 20 ms 연속 안정 시 확정
- SysTick 인터럽트 또는 기본 타이머(TIM6/TIM7 등) 기반 주기 호출 권장. TIM3 CH2는 PA7 위상제어 PWM용으로 예약한다.

**현재 회로도 버튼 핀:**

| 버튼       | MCU 핀 | 입력 방식                                 |
| ---------- | ------ | ----------------------------------------- |
| MODE       | PB6    | 외부 풀업 실장, Active LOW, `GPIO_NOPULL` |
| DOWN       | PB7    | 외부 풀업 실장, Active LOW, `GPIO_NOPULL` |
| UP         | PB8    | 외부 풀업 실장, Active LOW, `GPIO_NOPULL` |
| START_STOP | PB9    | 외부 풀업 실장, Active LOW, `GPIO_NOPULL` |

**이벤트 유형:**

| 이벤트         | 조건                                      |
| -------------- | ----------------------------------------- |
| BTN_PRESS      | 눌림 → 놓임 (단일 클릭)                   |
| BTN_LONG_PRESS | 1초 이상 누름 유지                        |
| BTN_REPEAT     | 장기 누름 시 200 ms 간격 반복 이벤트 발생 |
| BTN_HOLD_3S    | START 3초 유지, AUTO TUNE 준비 화면 전용  |

`BTN_HOLD_3S`는 기존 1초 `BTN_LONG_PRESS` 비상정지와 다른 event로 처리한다.

**인터페이스:**

```c
void     Button_Init(void);          // GPIO 초기화 (외부 풀업 사용, GPIO_NOPULL)
void     Button_Process(void);       // 주기적 호출 (10 ms마다)
uint8_t  Button_GetEvent(uint8_t id); // 이벤트 읽기 및 소비 (읽으면 클리어)
```

### 5. ADC 및 외부 위상제어 출력 (`adc_control` & `phase_pwm`)

**ADC 설정 (센서 및 가변저항):**

- ADC/COMP 입력: `PA0/ADC1_IN1/COMP3_INP=COMP_V`, `PA1/ADC1_IN2/COMP1_INP=COMP_I`, `PA6/ADC2_IN3=CT_ADC`, `PB1/ADC1_IN12=PWM_VR`
- 현재 회로도 Net: `COMP_V`, `COMP_I`, `CT_ADC`, `PWM_VR`
- 해상도: 12비트 (0–4095), G4는 하드웨어 오버샘플링(최대 256배) 지원으로 최대 16비트 유효 해상도 가능
- 샘플링 타임: 47.5 cycles 이상 권장 (고임피던스 소스는 92.5~247.5 cycles, G4 ADC는 최대 640.5 cycles 선택 가능)
- 변환 방식: PB1은 software-trigger 단일변환/이동평균, PA6은 TIM6-trigger DMA
- G4 ADC 특이사항: 사용 전 반드시 `HAL_ADCEx_Calibration_Start()` 캘리브레이션 호출 필수 (싱글엔드/디퍼렌셜 각각)
- ADC1/ADC2 동시 사용 시 170 MHz kernel clock은 `ADC_CLOCK_ASYNC_DIV4`로 나누어 42.5 MHz로 사용한다.

**COMP_V/COMP_I 및 펌웨어 설정:**

- PA0/PA1 GPIO는 `Analog`, `No pull`로 설정한다. PA0은 `COMP3_INP`, PA1은 `COMP1_INP`이며 PA0을 `COMP1_INM`으로 잘못 선택하지 않는다.
- `.ioc`는 PA0/PA1을 COMP 대표 기능으로 기록하므로 ADC1_IN1/ADC1_IN2 regular channel은 펌웨어에서 명시적으로 구성한다. GPIO는 두 기능 모두 동일한 analog/no-pull 상태를 사용한다.
- COMP3/COMP1은 high-speed mode, non-inverted polarity로 시작하고 hysteresis는 `None` 또는 `Low`부터 실측한다.
- 출력 정지/무전류 상태에서 PA0/PA1 ADC를 여러 번 측정하여 중심전압을 구한다. DAC3_CH1→COMP3_INM과 DAC1_CH1→COMP1_INM에 각각 반영한다. 계산 초기값 1.618V/1.623V를 고정 상수로 사용하지 않는다. DAC가 안정된 뒤 COMP를 enable하고 pending flag를 지운 다음 capture를 시작한다.
- COMP output edge는 timer input capture 또는 HRTIM event/capture로 timestamp하고 DMA 또는 짧은 ISR로 수집한다. main loop polling은 15~128kHz 위상 측정에 사용하지 않는다.
- `period = V[n]-V[n-1]`, `delta = I[n]-V[n]`, `phase = 360*delta/period`로 계산하고 delta를 `-period/2~+period/2`로 wrap한다. 상승/하강 edge 평균, CT 극성, 주파수별 고정 지연을 보정한다.
- C48/C49=22pF, BAT54WS 접합용량 및 PCB 기생용량 때문에 채널 지연이 달라질 수 있다. 15kHz와 128kHz를 포함해 실측하고 DNP/10pF/22pF 옵션으로 조정한다.

**SCT-13W CT 입력 및 필터링:**

- 적용 CT는 AC 250V, 최대 20 A, 1:1000, 50/60 Hz 전류출력형이며 내부 burden이 없는 `SCT-13W`를 전제로 한다. 동일 이름의 전압출력형 또는 내부 burden 내장품으로 대체하지 않는다.
- `20 A`는 CT 부품 정격이고 현재 ADC 회로의 선형 측정범위가 아니다. 120 ohm burden의 정상 목표범위는 0.2~7 A이며, 보수적으로 약 8 A부터 과전류 영역으로 취급한다. 20 A까지 선형 측정하려면 burden을 약 47 ohm 이하로 재설계해야 한다.
- `R29/R30=240 ohm` 병렬로 burden 120 ohm을 만들고, `R27/R28=10 kohm` 및 `C31=10 uF`, `C34=100 nF`로 1.65V bias를 만든다. 기존 R14/R15는 DNP이며 BOM/Pick & Place에서 제외한다.
- PA6 앞에는 `R57=1 kohm`, `C33=100 nF`, BAT54WS의 VDDA/GND rail clamp를 둔다. `R57`은 ADC를 저항분압하지 않으며 60 Hz 감쇠는 약 0.1% 이하다.
- 0.2 A에서 ADC 파형은 약 1.616~1.684V, 7 A에서는 약 0.462~2.838V가 된다. 이 범위는 3.3V ADC의 정상 범위 안이다.
- ADC2는 `TIM6 TRGO` timer trigger와 `DMA1_Channel1` circular 전송으로 4 kSPS,
  200 ms/800 sample RMS window를 수집한다. window 평균을 DC offset으로 빼고
  제곱평균제곱근을 계산하며 raw 이동평균을 전류값으로 사용하지 않는다.
- RMS 결과에는 실측 gain 보정, 상한·하한 hysteresis 및 과전류 지속시간 판정을 적용한다.
- 공진 위상 검출용 PA0/PA1 ADC는 중심전압 및 진폭 검증에 사용하고, zero-cross 시간은 COMP3/COMP1 hardware edge로 측정한다.
- 과전류/과전압 보호용 ADC는 정상 동작 범위에서 0.3V~3.0V 안쪽에 머물도록 하드웨어 감쇠/클램프를 먼저 설계

**위상제어용 3kHz PWM 출력:**

- 하드웨어: **TIM3_CH2 (PA7)** 일반 타이머 채널 사용
- 주파수: 3 kHz 고정 위상제어 지령 출력
- 듀티비 연동: PB1/PWM_VR 로컬 Duty limit과 Modbus OUTPUT_VALUE를 곱해 TIM3 CH2의 CCR에 반영하고 출력 5~100% 범위로 변조 전송
- **핀 변경 주의**: 위상제어 PWM은 **`PA7/TIM3_CH2(AF2)`**다. PA7을 ADC2_IN4 또는 COMP2_INP로 중복 초기화하지 않는다.

**저주파 입력전류 센싱과 고주파 위상 센싱의 역할 분리:**

- **AC 입력전류 RMS/보호**: 220 VAC 인입선의 한 가닥만 `SCT-13W` 1차에 통과시킨다. PA6 ADC는 1.65V 중심 AC 파형을 읽어 RMS를 계산하고, 정전류 제어와 과전류 보호에 사용한다. 브리지 정류나 LM358 능동 정류회로는 현재 확정 회로에 포함하지 않는다.
- **초음파 출력 위상 검출**: PA0/COMP3의 COMP_V와 PA1/COMP1의 COMP_I를 사용한다. `SCT-13W/PA6`는 이 용도를 대신하지 않는다.
- PB1은 `ADC1_IN12/PWM_VR`, PA7은 `TIM3_CH2/PWM_OUTPUT`으로 유지한다. 공진 위상 검출에 COMP2를 사용하지 않는다.

**가변저항 매핑:**

```
PB1/PWM_VR (0–3.3V) → LCD PL 표시 (0–100%)
PB1/PWM_VR (0–3.3V) → 로컬 Duty limit (0–90%)
로컬 Duty limit × Modbus OUTPUT_VALUE / 500 → TIM3_CH2(PA7) PWM (5–100%)
```

- `PA6/ADC2_IN3` CT 값은 전류 측정/보호 전용이며 LCD `PL`에 표시하지 않는다.

### 6. Modbus RTU 통신 (`modbus_rtu` + `modbus_crc` + `modbus_regs`)

`modbus_rtu`가 USART2 + TIM4로 프레임 수신/파싱/응답을 처리하고,
`modbus_crc`가 CRC-16 테이블 룩업을 제공하며,
`modbus_regs`가 레지스터 맵 읽기/쓰기 핸들러를 담당한다.

**물리 계층:** 기존 5 V MAX485 옵션보드를 74AHCT125 U6C/U6D 레벨 변환을 통해 재사용하는 RS485 반이중 통신

**USART 설정:**

- USART2: 9600/19200/38400/115200 bps (메뉴에서 설정 가능), 기본 19200 bps
- 데이터 포맷: 8N2 / 8E1 / 8O1 (메뉴에서 설정 가능), 기본 8E1
- 슬레이브 주소: 1–247 (메뉴에서 설정, 기본 1)
- 주소/Baud/Parity는 보드 메뉴에서만 변경하고 내장 Flash 마지막 2 KB page에 저장한다. PLC에서는 0x0010~0x0012를 읽기만 한다.

**RS485 방향 및 제어 핀 상태:**

```text
USART2 TX: PA2 -> U6C A(pin 9), U6C /OE(pin 10)=GND, U6C Y(pin 8) -> 옵션보드 TX -> 74AHC02 -> MAX485 DI
USART2 RX: MAX485 RO -> U6D /OE(pin 13), U6D A(pin 12)=GND, U6D Y(pin 11) -> PA3
PA3 외부 pull-up: R80 4.7 kohm -> +3.3 V, PA3=AF7 USART2_RX + GPIO_NOPULL
DE_RS485: PB4, /RE_RS485: PB5
RTERM: PB2, 485BD_DETECT: PB3
옵션 보드 인식: PB3 외부 3.3 V pull-up + 옵션보드 J12 pin 8 GND, HIGH=미장착, LOW=장착
SPARE1: 옵션보드 RT3 10 kohm로 5 V HIGH 유지, MCU에는 연결하지 않는 SPARE1/NC
종단 저항 (RTERM): 메뉴에서 활성화 시 PB2 HIGH 출력 (필요 시)
대기 / 수신 시: PB4(DE) = LOW, PB5(/RE) = LOW → RXNE 인터럽트로 바이트 수신 대기
데이터 송신 시: PB5(/RE) = HIGH로 수신기를 끈 뒤 PB4(DE) = HIGH → 데이터 송신 → TC 플래그 대기 → PB4=LOW, PB5=LOW 복귀
```

> `PB4(DE)`와 `PB5(/RE)`에는 각각 외부 풀다운을 둔다. MCU 리셋·부팅 중에도 송신 드라이버를 끄고 수신기를 켜기 위한 하드웨어 기본 상태다. PB3은 외부 pull-up이 있으므로 `GPIO_NOPULL`로 읽고 `HAL_GPIO_ReadPin(...)=GPIO_PIN_RESET`일 때 옵션보드 장착으로 판단한다.
>
> U6D RX 회로는 `/OE`를 데이터 입력으로 쓰는 특수 구성이다. MAX485 RO=LOW이면 A=GND인 U6D가 PA3을 LOW로 구동하고, RO=HIGH이면 U6D Y가 Hi-Z가 되어 R80이 PA3을 3.3 V HIGH로 만든다. PA3은 5 V tolerant 입력으로 간주하지 말고 MAX485 RO를 직접 연결하지 않는다. SPARE1은 RT3에 의해 옵션보드 내부 HIGH로 유지되는 예약 신호이므로 MCU나 GND에 연결하지 않는다.

**프레임 감지:**

- 프레임 간 묵음 구간: 19200 bps 이하는 11-bit character 기준 3.5T, 그보다 빠르면 1.750 ms 고정
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

**현재 구현 레지스터 맵 (Firmware v1.2.0):**

`docs/Modbus_레지스터맵.md`를 유일한 상세 기준으로 사용한다. 구현 주소는 다음과 같다.

| 주소 범위       | 접근       | 내용                                     |
| --------------- | ---------- | ---------------------------------------- |
| `0x0000`        | R          | 현재 Frequency (×0.1 kHz)                |
| `0x0001`        | R/W        | 기존 Protocol 출력값 0~500               |
| `0x0002~0x0003` | R          | 표시 범위, Sweep 주파수                  |
| `0x0004~0x0006` | R 또는 R/W | Local Lock, Run, 외부 입력               |
| `0x0007`        | R          | 물리 SWEEP_SW 상태                       |
| `0x0008`        | R/W        | Error 상태/Reset                         |
| `0x0009`        | R          | Error 원인: 0=None, 1=Transducer, 2=Over current |
| `0x000A~0x000B` | R          | RUN_SW 상태, 통신 제어 가능 상태         |
| `0x000C~0x000E` | R          | 설정 중심주파수, Sweep 폭, Sweep 속도    |
| `0x0010~0x0012` | R          | Slave ID, Baud index, Parity             |

Frequency와 Local Duty limit은 보드에서만 설정한다. PLC에는 직접적인
`FREQ_SET`, `DUTY_SET`을 추가하지 않는다. PLC 출력은 0~500 정규화값이며
Local Duty limit보다 높게 출력할 수 없다. Boolean은 기존 Protocol과 같이
False=`0x0000`, True=`0xFF00`을 쓴다.

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
    ResonanceDAC_Init();           // DAC3_CH1→COMP3_INM, DAC1_CH1→COMP1_INM 개별 threshold
    ResonanceThreshold_Calibrate();// 출력 OFF에서 ADC 중심값 측정 후 DAC threshold 반영
    COMP_Init();                   // DAC 안정 후 COMP3/COMP1 enable, pending flag clear
    ResonanceCapture_Init();       // COMP3/COMP1 edge용 timer 또는 HRTIM 내부 capture
    LCD_Init();                    // LCD1602 초기화
    Button_Init();                 // 택트 스위치 초기화
    Modbus_Init();                 // Flash 통신 설정 Load + USART2/TIM4 초기화
    Menu_Init();                   // Load된 통신 설정으로 메뉴 FSM 초기화

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

- STM32G474CBT6는 LQFP48·128 KB Flash 기준이다. 기존 RB/RC LQFP64 핀맵과 `STM32G474RCTx_FLASH.ld`는 그대로 사용하지 말고, CubeMX 설정·링커 메모리 용량을 CBT6에 맞춘다.
- **초음파 출력은 HRTIM 기준**으로 구현한다:
  - `PA8/HRTIM1_CHA1`, `PA9/HRTIM1_CHA2`를 `GPIO_AF13_HRTIM1`로 설정한다.
  - HRTIM output enable, Timer A counter start, IR2104 내부 레그 deadtime과 GDT용 HRTIM 1.4/0.8 us 대역별 deadtime, fault 입력 정책을 함께 확인한다.
  - 기존 TIM1 상보출력 예제 코드를 그대로 복사하지 말고, HRTIM HAL/LL 설정으로 전환한다.
- **PWM 조정 입력은 PB1/ADC1_IN12, 출력은 TIM3_CH2(PA7)** 이다. PB1은 analog/no-pull, PA7은 AF2로 설정한다.
- 타이머 클럭: G4 시리즈는 버스 아키텍처가 최적화되어, APB1/APB2 모두 170MHz로 동작하여 압도적 듀티 분해능 제공.
- RS485 반이중 특성: 송신 완료(TC) 확인 후 DE → LOW 전환 필수.
- **RS485 5 V 레벨 변환**: PA2 TX는 U6C의 정상 버퍼 경로를 사용하고 PA3 RX는 `RO→U6D /OE`, `U6D A=GND`, `U6D Y→PA3`, `R80 4.7 kohm→3.3 V` 경로를 사용한다. PA3은 `GPIO_NOPULL`이며 5 V RO를 직접 연결하지 않는다.
- **외부 pull-up 입력**: `PB6/MODE`, `PB7/DOWN`, `PB8/UP`, `PB9/START_STOP`, `PA10/REMOTE`, `PA11/RUN_SW`, `PA12/SWEEP_SW`, `PB3/485BD_DETECT`는 외부 pull-up이 실장되어 있으므로 모두 `GPIO_NOPULL` 및 Active LOW로 처리한다.
- LCD1602 HD44780: PB10~PB15 데이터 설정 후 PB0/CLK_LCD 상승 에지로 74HCT574를 갱신한다. LCD 명령 실행 타이밍은 마이크로초 지연 함수로 준수하고 74HCT574 전체 출력 상태는 shadow byte로 보존한다.
- **RUN LED**: `PA4/RUN_LED`는 정상 동작 heartbeat 출력이다. main loop를 막는 `HAL_Delay()`를 사용하지 말고 시스템 tick 기준으로 500 ms마다 토글한다.
- ADC 레퍼런스: VDDA = 3.3 V; PWM_VR 가변저항은 GND–3.3 V 사이에 연결하고 wiper는 PB1에 연결한다.
- **OPAMP LQFP48 제한**: 내장 OPAMP 외부 핀은 현재 HRTIM, USART2, LCD, RS485, 아날로그 검출 핀과 충돌 가능성이 높다. 이번 보드는 전압 분압, CT burden, bias, 클램프 같은 외부 신호 컨디셔닝을 우선한다.
- **COMP/DAC 확정 배정**: `PA0/COMP3_INP=COMP_V`, `PA1/COMP1_INP=COMP_I`, `DAC3_CH1→COMP3_INM`, `DAC1_CH1→COMP1_INM`을 사용한다. DAC 출력은 외부 GPIO로 내보내지 않는다. 두 threshold는 ADC로 측정한 채널별 중심전압에 맞춰 보정한다.
- **SWD 핀 예약**: PA13(SWDIO), PA14(SWCLK)은 GPIO로 사용 금지. J3가 표준 10핀 Cortex Debug 커넥터라면 NRST는 10번 핀에 연결한다. PB3은 `485BD_DETECT`, PA15는 `SONIC_ON`이므로 SWO/JTAG 대체 기능을 켜지 않는다.
- **G4 USART 레지스터 차이**: F1의 `USART_SR`/`USART_DR` 단일 레지스터 구조와 달리, G4는 `USART->ISR`(상태), `USART->ICR`(플래그 클리어), `USART->TDR`/`USART->RDR`(송수신 분리) 구조. 레지스터 직접 접근 시 반드시 G4 레퍼런스 매뉴얼(RM0440) 참조.
- **G4 GPIO 속도**: G4는 GPIO 출력 속도 설정이 Low/Medium/High/Very High 4단계. 고속 통신이나 PWM 출력 핀은 `GPIO_SPEED_FREQ_VERY_HIGH` 설정 권장.
- Modbus CRC-16: 테이블 룩업 방식 사용 (속도 최적화).

---

## 양산용 펌웨어 다운로드 편의성 (플래싱 방법론)

IDE와 ST-Link(SWD)를 꽂아 펌웨어를 굽는 것은 개발/디버깅 시에는 필수적이나 양산(생산) 단계에서는 매우 번거롭습니다. STM32 칩에는 공장 출고 시부터 롬(ROM)에 구워져 있는 **"시스템 메모리 부트로더(System Memory Bootloader)"** 기능이 있어, 별도의 프로그래머 장비 없이도 손쉽게 롬 라이팅이 가능합니다.

### 추천 1. UART(RS485) 부트로더 활용 (강력 추천)

STM32G474는 부팅 시 특정 조건이 맞으면 내장 부트로더가 활성화되어 `USART1`, `USART2` 등을 통해 플래싱 대기 상태가 됩니다.

- **하드웨어 준비**: 현재 회로도에서 PB8은 UP 버튼으로 사용한다. UART 부트로더 채택 전에는 CBT6의 BOOT0 option byte와 시스템 메모리 부트 조건을 확인하고, 별도의 진입 회로 또는 SWD 양산 방식을 확정한다.
- **PC 작업**: RS485 단자의 데이터 핀은 `PA2/PA3`이고 방향 제어는 `PB4/PB5`다. 시스템 부트로더가 사용하는 USART와 트랜시버 방향 제어의 초기 상태는 별도로 검증한다.
- PC에서 ST 공식 무료 배포 프로그램인 **"STM32CubeProgrammer"**를 켜서 펌웨어(.hex 또는 .bin) 파일 1개만 선택하고 업로드를 누르면 끝입니다.

### 추천 2. Custom (사용자) 부트로더 개발 (통신 업데이트 / OTA)

부팅 영역(가령 플래시의 0x0800_0000 ~ 0x0800_4000)에 우리가 직접 짠 통신 부트로더 코드를 심어두고, 메인 양산 코드는 그 이후 주소(0x0800_4000)부터 할당하는 방식입니다.

- 평소에는 메인 앱으로 동작하다가, "펌웨어 업데이트 모드 진입" 메뉴나 Modbus 특수 명령이 들어오면 메인 앱이 칩을 리셋시키고 부트로더 구역으로 점프합니다.
- 부트로더가 현재 연결된 RS485를 통해 새로운 펌웨어 바이너리 데이터를 패킷으로 쪼개어 수신한 뒤 메인 앱 플래시 구역을 지우고 다시 씁니다. PC쪽에는 간단한 펌웨어 송신용 C# 프로그램(보통 YMODEM 프로토콜 등 사용) 하나만 띄워주면 현장 조작자가 마우스 클릭 한 번으로 통신선을 통해 간편하게 패치할 수 있습니다.

> **하드웨어 설계 요약**: CBT6 보드의 PB8은 UP 버튼으로 예약되어 있다. UART 부트로더는 BOOT0 option byte·시스템 부트 조건·RS485 방향 기본 상태를 별도로 검증한 뒤 채택하며, 현재 확정된 양산/복구 인터페이스는 SWD다.

### 추천 3. ST-Link + STM32CubeProgrammer (개발~양산 겸용, 가장 간편)

ST-Link(SWD)는 STM32CubeProgrammer에서도 그대로 사용 가능합니다. IDE 없이 독립 실행하여 .hex/.bin 파일만 선택 후 다운로드할 수 있어, 양산 담당자도 간단히 조작할 수 있습니다.

- **BOOT0 핀 조작 불필요**: SWD로 플래시에 직접 접근하므로 부트로더 모드 진입이 필요 없음
- **어떤 상태에서든 플래싱 가능**: NRST가 연결되어 있으면 슬립/잠금 상태에서도 강제 리셋 후 다운로드
- **필요 장비**: ST-Link V2 (개당 3,000~5,000원 수준) + USB 케이블
- **필요 연결**: SWDIO(PA13), SWCLK(PA14), GND, NRST (총 4~5선)

> **정리**: 개발~양산 전 단계에서 ST-Link + STM32CubeProgrammer가 **가장 간편**합니다. 추천 1(UART 부트로더)은 ST-Link 없이 해야 할 때의 보험용 대안입니다.

---

## 흔한 실수

- **HRTIM PWM이 출력 안 됨** → PA8/PA9가 `GPIO_AF13_HRTIM1`인지, HRTIM Timer A counter와 output enable이 모두 켜졌는지, `PA15/SONIC_ON`이 활성 상태인지 확인. PA9는 USB-C PD 미사용 시 `UCPD1_DBDIS=1` 설정도 확인한다.
- **위상제어 PWM이 출력 안 됨** → 회로도 기준 출력은 `PA7/TIM3_CH2(AF2)/PWM_OUTPUT`, 조정 입력은 `PB1/ADC1_IN12/PWM_VR`이다. 두 핀의 GPIO mode가 뒤바뀌지 않았는지 확인한다.
- **RS485 수신 불가** → `PB4(DE)`와 `PB5(/RE)`가 아이들 시 모두 LOW인지 확인하고, U6D의 A=GND, `/OE=MAX485_RO`, Y=PA3 배선과 PA3의 R80 4.7 kohm 외부 3.3 V pull-up을 확인한다. PA3은 AF7 USART2_RX + `GPIO_NOPULL`이어야 한다. 송신 후 `USART->ISR`의 `TC`를 확인한 뒤 PB4/PB5를 LOW로 복귀한다.
- **RS485 옵션보드 감지 반대/불안정** → PB3은 외부 pull-up, 옵션보드 J12 pin 8은 GND이므로 HIGH=미장착, LOW=장착이다. PB3을 `GPIO_NOPULL`로 설정하고 SPARE1은 MCU에 연결하지 않는다.
- **LCD 아무것도 표시 안 됨** → PB0/CLK_LCD가 기본 LOW이고 데이터 설정 뒤 상승 에지가 발생하는지, 74HCT574 `/OE`가 활성인지 확인한다. V0 대비 조절과 초기화 시퀀스 타이밍(15 ms → 4.1 ms → 100 µs)도 확인한다.
- **RUN_LED가 점멸하지 않음** → PA4가 GPIO push-pull output인지, 회로의 LED active polarity와 펌웨어 매크로가 일치하는지, tick 기반 heartbeat 코드가 main loop에서 주기적으로 실행되는지 확인한다.
- **Modbus CRC 불일치** → CRC 바이트 순서: CRC Low 먼저, CRC High 나중 (LSB-first). 다항식 0xA001.
- **ADC 값 불안정** → 샘플링 타임 증가 (최소 47.5 cycles, 고임피던스는 92.5~247.5 cycles 권장), 이동 평균 필터 적용, VDDA 바이패스 커패시터 확인.
- **스위치/외부입력 오동작** → MODE/DOWN/UP/START_STOP/REMOTE/RUN_SW/SWEEP_SW의 외부 pull-up 저항과 접점-GND 배선을 확인하고, GPIO는 내부 pull-up이 아닌 `GPIO_NOPULL`인지 확인한다. 입력은 Active LOW이며 디바운스 시간 20 ms를 적용한다.
- **`HAL_Delay()` 동작 안 함** → SysTick 인터럽트 우선순위 확인. 다른 높은 우선순위 ISR에서 블록되면 HAL_Delay 무한 루프.
- **170 MHz 클럭 설정 실패** → HSE가 없는 환경에서는 HSI(16 MHz) + PLL 구성 사용. G4의 PLL은 PLLM/PLLN/PLLP/PLLQ/PLLR 5개 분주기가 있으므로 정확한 설정 필요. `RCC_OscInitStruct.OscillatorType` 및 `RCC_PLLCFGR` 확인.
- **ADC가 동작하지 않음** → G4 ADC는 사용 전 반드시 `HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED)` 캘리브레이션 호출 필수. F1과 달리 캘리브레이션 없이는 정확한 값을 읽을 수 없음.
- **HardFault 발생** → 스택 오버플로 가능성 확인. 재귀 호출이나 큰 로컬 배열 금지. 링커 스크립트에서 스택 크기 확인 (기본 0x400).

---

## 파라미터 저장

Firmware v1.2.0은 `settings_storage.c`로 Modbus 주소, 통신 속도, 패리티를 저장한다.

- STM32G474CBT6 마지막 Flash page `0x0801F800~0x0801FFFF` 2 KB를 설정 전용으로 예약한다.
- Application Linker 영역은 126 KB로 제한해 설정 page와 겹치지 않게 한다.
- 16-byte CRC/sequence record를 순차 추가해 한 page에서 128회 저장하고, 가득 찬 경우에만 erase한다.
- G4는 더블워드(64-bit/8-byte) 단위로 2회 프로그램한다.
- 전원 차단으로 마지막 record가 손상되면 이전 CRC-valid record를 사용한다.

향후 메뉴 확장 시 settings record를 v3로 올리고 기존 v1/v2 migration을 유지한다.
v3에는 `frequency_band`, `center_frequency_hz`, `sweep_width_hz`,
`sweep_rate_hz`, `output_control_mode`, `current_target_01a`를 저장한다.
수동 조정 중에는 RAM 값만 변경하고 사용자가 START로 확정하거나 Auto-Tuning 결과를
승인할 때만 Flash에 기록한다.

---

## 문서 파일 (한글)

| 파일                                                       | 내용                       |
| ---------------------------------------------------------- | -------------------------- |
| `docs/코드_설명서.md`                                      | 펌웨어 전체 사양서         |
| `docs/회로도_설명.md`                                      | 회로 설계 및 부품 목록     |
| `docs/Modbus_레지스터맵.md`                                | Modbus 레지스터 상세 사양  |
| `docs/PLC_Modbus_RTU_사용자_가이드.md`                     | PLC 사용자 배포 원본       |
| `docs/STM32G474CBT6_Modbus_RTU_PLC_사용자_가이드.docx`     | PLC 사용자 배포본(DOCX)    |
| `docs/STM32G474CBT6_Modbus_RTU_PLC_사용자_가이드_v1.2.pdf` | PLC 사용자 배포본(PDF)     |
| `docs/메뉴_구조.md`                                        | 메뉴 트리 및 UI 사양       |
| `docs/초음파_설계.md`                                      | 초음파 발진 회로 설계 상세 |
