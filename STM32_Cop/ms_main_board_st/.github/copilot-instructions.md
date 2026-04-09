# Copilot 지침 — STM32G474ME 메가소닉 메인보드

## 프로젝트 개요

**STM32G474MET6** 기반 메가소닉(Megasonic) 발진기 메인보드 펌웨어. **CMake + STM32Cube HAL** 프레임워크로 빌드.
HRTIM 고해상도 타이머(184ps 분해능)를 이용한 500kHz~2MHz 메가소닉 PWM 생성,
LCD1602 캐릭터 디스플레이(74HCT574 ×2, 20핀 커넥터), 6개 택트 스위치 + 6개 LED 표시등,
7채널 주파수 선택 (500kHz~2MHz, 250kHz 단위), 8단계 외부 출력 제어, 다중 알람 릴레이 출력,
Modbus RTU(RS-485) 통신을 지원한다.

기존 경일메가소닉 HS AUP Multi Generator 매뉴얼(`docs/MANUAL- HS AUP Multi Generator.pdf`)의
동작 사양을 충실히 재현하되, 통신 물리 계층을 RS-422에서 **RS-485(Modbus RTU)**로 변경하고,
MCU 내장 HRTIM으로 외부 아날로그 발진 IC(VCO 등)를 완전히 대체한다.

- 타겟: STM32G474MET6 (Cortex-M4, 170 MHz, Flash 512 KB, SRAM 128 KB, FPU, **HRTIM**, COMP×7, OpAmp×4)
- 보드: 커스텀 보드 LQFP80 (외부 HSE 8MHz 크리스탈 사용, ±20ppm 정밀도)
- 언어: C11 (STM32 HAL 드라이버 기반)
- 빌드 시스템: CMake (gcc-arm-none-eabi 크로스 컴파일)
- CubeMX 전환: `cmake/stm32cubemx/` 존재 시 자동 전환 (회로 설계 완료 후)
- 참조 매뉴얼: `docs/MANUAL- HS AUP Multi Generator.pdf` (기존 HS AUP Multi 메가소닉 발진기)

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
ms_main_board_st/
├── .github/
│   └── copilot-instructions.md        # 본 지침서
├── CMakeLists.txt                      # CMake 빌드 (CubeMX 자동/수동 듀얼 모드)
├── cmake/
│   ├── gcc-arm-none-eabi.cmake        # ARM 크로스 컴파일 툴체인
│   └── stm32cubemx/                   # (CubeMX 생성 시 자동 배치)
├── ldscripts/
│   └── STM32G474METx_FLASH.ld         # 링커 스크립트 (512K Flash, 128K SRAM)
├── startup/
│   └── startup_stm32g474xx.s          # 벡터 테이블 + Reset_Handler
├── Drivers/                            # STM32CubeG4 HAL/CMSIS (수동 모드 시)
├── include/
│   ├── config.h                       # 핀 정의, GPIO 매핑
│   ├── params.h                       # 파라미터 상수, 범위, 기본값
│   ├── stm32g4xx_hal_conf.h           # HAL 모듈 활성화 설정
│   ├── stm32g4xx_it.h                 # 인터럽트 핸들러 선언
│   ├── system_clock.h                 # 시스템 클럭 설정
│   ├── gpio_init.h                    # 전체 GPIO 초기화
│   ├── hrtim_pwm.h                    # HRTIM 고해상도 PWM HW 제어
│   ├── megasonic_ctrl.h               # 메가소닉 상위 제어 (채널, 출력, 소프트스타트)
│   ├── lcd1602.h                      # LCD1602 디스플레이 상위 API
│   ├── lcd1602_hw.h                   # LCD1602 4비트 GPIO 하드웨어 드라이버 (니블 전송)
│   ├── button.h                       # 택트 스위치 (디바운스, 이벤트)
│   ├── led_indicator.h                # LED 표시등 6개 상태 관리
│   ├── adc_control.h                  # ADC 전력/전류/전압 계측
│   ├── alarm.h                        # 알람 관리 (Err1~Err7, 릴레이 출력)
│   ├── ext_control.h                  # 외부 제어 (REMOTE, 8POWER BCD)
│   ├── menu.h                         # 모드 FSM 및 설정 네비게이션
│   ├── modbus_rtu.h                   # Modbus RTU 프로토콜
│   ├── modbus_crc.h                   # CRC-16 테이블 룩업
│   └── modbus_regs.h                  # 레지스터 맵 R/W
├── src/
│   ├── main.c                         # 엔트리포인트, 메인 루프
│   ├── system_clock.c                 # HSE 8MHz→PLL→170MHz, HRTIM DLL 초기화
│   ├── gpio_init.c                    # 전체 핀 일괄 초기화
│   ├── hrtim_pwm.c                    # HRTIM Timer A 상보 PWM + 초정밀 데드타임
│   ├── megasonic_ctrl.c               # 채널 관리, 출력 제어, 소프트 스타트
│   ├── lcd1602.c                      # LCD1602 상위 API (2행 16칸럼 문자열 출력)
│   ├── lcd1602_hw.c                   # LCD1602 4비트 GPIO 하드웨어 (니블 전송)
│   ├── button.c                       # 디바운싱, 장기누름, 반복이벤트
│   ├── led_indicator.c                # 6개 LED 상태 표시 로직
│   ├── adc_control.c                  # 이동평균 필터, 전력/임피던스 계산
│   ├── alarm.c                        # 알람 조건 판정, 릴레이 제어, 에러 코드 관리
│   ├── ext_control.c                  # REMOTE/8POWER 외부 입력 처리
│   ├── menu.c                         # 모드 FSM (NORMAL/REMOTE/EXT/H-L SET/8POWER/FREQ SET)
│   ├── modbus_rtu.c                   # USART2+TIM4, FC03/06/10 처리
│   ├── modbus_crc.c                   # CRC-16 256-entry 테이블
│   ├── modbus_regs.c                  # 레지스터 읽기/쓰기 핸들러
│   ├── stm32g4xx_it.c                 # SysTick, USART2, TIM4 ISR
│   └── syscalls.c                     # Newlib 스텁
└── docs/
    ├── MANUAL-  HS AUP Multi Generator.pdf  # 기존 메가소닉 제품 매뉴얼 (참조)
    ├── Modbus_레지스터맵.md                  # Modbus 레지스터 상세 사양
    ├── 코드_설명서.md                        # 펌웨어 전체 사양서
    ├── 회로도_설명.md                        # 회로 설계 및 부품 목록
    └── 메가소닉_설계.md                      # 메가소닉 발진 회로 설계 상세
```

### 모듈 세분화 원칙

| 영역       | 분리                                                                      | 이유                                   |
| ---------- | ------------------------------------------------------------------------- | -------------------------------------- |
| 메가소닉   | `hrtim_pwm` (HRTIM HW) + `megasonic_ctrl` (채널/출력 로직)                | HRTIM 레지스터와 상위 제어 분리        |
| 디스플레이 | `lcd1602` (상위 API) + `lcd1602_hw` (4비트 GPIO 니블 드라이버)            | 하드웨어 드라이버 독립                 |
| 표시       | `led_indicator` (6개 LED 상태)                                            | 디스플레이와 LED 분리                  |
| 모드       | `menu` (FSM) + `ext_control` (외부 입력)                                  | 내부 조작과 외부 제어 분리             |
| 알람       | `alarm` (알람 판정 + 릴레이)                                              | 에러 관리 독립 모듈                    |
| Modbus     | `modbus_rtu` (프로토콜) + `modbus_crc` (CRC) + `modbus_regs` (레지스터맵) | 기능별 독립 테스트 가능                |
| 설정       | `config.h` (핀 정의) + `params.h` (파라미터/범위)                         | 하드웨어 매핑과 애플리케이션 상수 분리 |

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

### 기존 HS AUP 발진기 IC → STM32G474 설계 대응표

기존 HS AUP Multi 메가소닉 발진기 보드의 주요 IC 13종(`docs/HS_AUP_메가소닉_부품구성_260323.xlsx`)과
STM32G474MET6 신규 설계에서의 대체/유지/추가 방안을 정리한다.

#### 제거 대상 (STM32G474 내장 주변장치로 대체)

| 기존 IC        | 기능                                        | 대체 수단                                                              | 비고                                                                                                 |
| -------------- | ------------------------------------------- | ---------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------- |
| **PIC16F873A** | 8비트 MCU (제어 중추)                       | **STM32G474MET6**                                                      | Cortex-M4F 170MHz, HRTIM, 512KB Flash, 128KB SRAM. 성능 수백 배 향상                                 |
| **SN74LS624N** | VCO (전압 제어 발진기)                      | **HRTIM Timer A**                                                      | HRTIM 5.44GHz 유효 클럭으로 500kHz~2MHz 직접 생성. 외부 VCO 완전 불필요                              |
| **MC145163**   | PLL 주파수 합성기                           | **HRTIM Timer A**                                                      | Period 레지스터 값 변경으로 184ps 분해능 주파수 설정. 외부 PLL 불필요                                |
| **MCP3202**    | 12비트 SPI ADC (2채널, 듀얼레인지 전력검출) | **STM32G474 내장 ADC** (5개, 12비트)                                   | 듀얼 레인지 전압 진폭 측정 → ADC1 2채널로 직결 대체. SPI 버스 제거로 배선 단순화. 아래 상세 참조     |
| **MCP4922**    | 12비트 SPI DAC (2채널)                      | **STM32G474 내장 DAC** (3채널, 12비트) 또는 **HRTIM 듀티비** 직접 제어 | 기존에는 DAC→VCO 경로로 주파수 설정했으나, HRTIM이 직접 주파수를 생성하므로 DAC→VCO 경로 자체가 소멸 |
| **74HC574**    | 옥탈 D 플립플롭 래치                        | **불필요**                                                             | PIC의 부족한 GPIO를 확장하기 위한 병렬 버스 래치. STM32G474 LQFP80은 ~51개 GPIO로 충분               |
| **74HC138D**   | 3-to-8 라인 디코더                          | **불필요**                                                             | GPIO 확장/주소 디코딩용. LQFP80 패키지의 GPIO로 직접 연결                                            |
| **74HC04N**    | 헥스 인버터                                 | **불필요**                                                             | VCO/PLL 클럭 체인의 위상 반전용. HRTIM 상보 출력(CHA1+CHA2)이 데드타임 포함 자동 생성                |
| **74HC390N**   | 듀얼 디케이드 카운터 (분주기)               | **불필요**                                                             | VCO 출력 분주용. HRTIM이 정확한 목표 주파수를 직접 생성하므로 분주 불필요                            |
| **74HC14D**    | 헥스 슈미트 트리거 인버터                   | **불필요**                                                             | STM32G474 GPIO 입력에 슈미트 트리거 내장. 외부 입력 신호 정형이 필요한 경우에만 선택적 유지          |
| **CANTUS512**  | EPROM/ROM (설정 데이터 저장 추정)           | **STM32G474 내장 Flash**                                               | 512KB Flash 마지막 페이지(4KB 단위)를 파라미터 저장 영역으로 사용                                    |

> **제거되는 IC: 11종** — PIC16F873A, SN74LS624N, MC145163, MCP3202, MCP4922, 74HC574, 74HC138D, 74HC04N, 74HC390N, 74HC14D, CANTUS512

##### MCP3202 듀얼 레인지 전력 검출 회로 상세

기존 HS AUP 보드의 MCP3202는 **듀얼 레인지 전압 측정**에 사용됨 (위상 검출이 아님).
J1-1, J1-2 커넥터로 외부 전압 신호 입력 → 반파 정류(UF4007) → RC 필터 → DC 전압 출력.
(CT(CTL-6-P)는 별도 회로이며, J1-1/J1-2 커넥터와는 직접 연결되어 있지 않음)

```
기존 회로 (docs/SNxx1203_connetion.jpg 참조):

외부 전압 신호 ──▶ J1-2 ──▶ D1-2(UF4007) ──▶ R4-2(220K) ──▶ 필터 ──▶ CH0 (저감도, 고출력용)

외부 전압 신호 ──▶ J1-1 ──▶ D1-1(UF4007) ──▶ R4-1(120K) ──▶ 필터 ──▶ CH1 (고감도, 저출력용)
```

| 채널 | 분압 저항 | 감도   | 측정 범위            |
| ---- | --------- | ------ | -------------------- |
| CH0  | 220KΩ     | 저감도 | 고출력 (포화 방지)   |
| CH1  | 120KΩ     | 고감도 | 저출력 (분해능 향상) |

**핵심 원리**: 출력 **전압의 크기(진폭)**를 반파 정류 후 DC 전압으로 변환하여 ADC로 읽음.
**위상 검출이 아님**. 전압 진폭으로부터 출력 전력을 계산.

**STM32G474 대체 방안 (방법 1: 2채널 ADC 직결)**:

- 기존 분압/정류 회로 그대로 유지
- MCP3202 제거 → STM32G474 내장 ADC1 2채널로 직결
- PA0 (ADC1_IN1) ← CH0 경로 (저감도, 220K 분압)
- PA1 (ADC1_IN2) ← CH1 경로 (고감도, 120K 분압)
- SPI 버스 완전 제거, 배선 단순화

#### 유지 대상 (동등 부품으로 교체)

| 기존 IC                            | 기능                                             | 신규 설계 대응                                                  | 비고                                                                                                                       |
| ---------------------------------- | ------------------------------------------------ | --------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| **ULN2003A** (ILN2003AD)           | 달링턴 트랜지스터 어레이 (7채널 릴레이 드라이버) | **ULN2003A ×2 유지** 또는 **개별 N-ch MOSFET (2N7002, BSS138)** | #1: 알람 릴레이 5개 = 5채널 (IN6/IN7 여유 2채널). #2: LC 탱크 릴레이 4개 = 4채널(3채널 여유). 부저는 PB5→KEC105S 직접 구동 |
| **ATMLH412** (AT24Cxx EEPROM 추정) | I2C EEPROM (파라미터 저장)                       | **선택적 유지**: AT24C02/04 (I2C1: PB8/PB9)                     | 내장 Flash로 기본 저장하되, 쓰기 수명(10만회) 초과 우려 시 외부 EEPROM(100만회) 확장 가능                                  |

#### 신규 추가 부품 (메가소닉 MHz 대역 대응)

| 부품                     | 역할                              | 권장 사양                                                                                            | 비고                                                                                                        |
| ------------------------ | --------------------------------- | ---------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------- |
| **전력 FET**             | 하프브리지 전력 스위칭 (MHz 대역) | **옵션A**: IPB200N15N3 (Infineon, D2PAK, 150V, 손납땜 용이) / **옵션B**: EPC2036 (GaN, 100V, 고성능) | 옵션A: Si MOSFET, 데드타임 50~100ns, 저비용. 옵션B: GaN, 데드타임 5~20ns, 고성능                            |
| **절연 게이트 드라이버** | 듀얼 채널 절연 구동               | **UCC21520** (TI, 5.7kVrms 절연, 4A peak, 18ns 지연)                                                 | HRTIM CHA1/CHA2 → INA/INB 입력. 하이사이드(VDDA)는 절연 전원 필수. `docs/circuit_ref.md` 참조               |
| **절연 DC-DC**           | 하이사이드 게이트 전원            | **R1215S** (RECOM, 12V→15V, 1W 절연) 또는 SN6505A+트랜스                                             | VDDA/VSSA용 절연 전원 공급. VSSA=SW 노드 연결. 로우사이드(VDDB)는 직접 공급 가능                            |
| **방향성 커플러**        | 순방향/역방향 RF 전력 분리        | 브릿지형 전류 감지 회로 (CTL-6-P + R21~R26) 또는 Mini-Circuits ZGBDC20-372HP+ (500kHz~2MHz 대역)     | 메가소닉 MHz 대역에서는 저항/CT 직접 측정 불가 → RF 커플러 필수                                             |
| **RF 전력 검출기**       | 커플러 출력 → DC 전압 변환        | **AD8318** (로그 검출, 1MHz~8GHz) 또는 **LTC5507** (RMS 검출, 100kHz~1GHz)                           | 순방향/역방향 각 1개씩 총 2개 필요. DC 출력 → ADC1_IN1(PA0), ADC1_IN2(PA1)                                  |
| **RS-485 트랜시버**      | Modbus RTU 반이중 통신            | **MAX3485** 또는 **SP3485** (3.3V, 반이중)                                                           | 기존 RS-422(전이중) → RS-485(반이중) 변경. DE/RE 핀: PC13/PC14                                              |
| **BUCK 컨버터**          | 48V→가변 DC 전력 제어             | **LM5005** (TI, SOIC-14, 7~75V, 동기 정류)                                                           | DAC(PA4)로 FB 전압 조절, 0.1W~10W 출력 제어. `docs/circuit_ref.md` 섹션 9 참조                              |
| **LCD1602**              | 캐릭터 디스플레이 (2행×16칸럼)    | HD44780 호환 LCD1602 모듈                                                                            | 74HCT574 ×2 방법 C (6비트 DATA 버스 PB10~PB15 + CLK_LCD/CLK_LED). 20핀 커넥터. 배경 조명 LED 별도 전원 필요 |
| **릴레이 모듈**          | 알람 출력 6채널 + LC 탱크 4채널   | DC 5V 코일, 접점 AC100V/DC100V 0.3A                                                                  | 알람: GO/OPERATE/LOW/HIGH/TRANSDUCER/ADDR. LC: 인덕턴스 L1~L4 스위칭                                        |
| **전원 레귤레이터**      | MCU 및 로직 3.3V 공급             | LDO (AMS1117-3.3 등) 또는 DCDC (TPS5430 등)                                                          | 입력 12~24V → 3.3V 변환. LCD 배경조명은 5V 별도 필요                                                        |

> **회로 상세**: `docs/circuit_ref.md` 파일에 UCC21520 + IPB200N15N3 + 절연 DC-DC 회로도 및 BOM 수록.

#### IC 수량 비교 요약

| 항목                    | 기존 HS AUP                                          | 신규 STM32G474 설계                                                 |
| ----------------------- | ---------------------------------------------------- | ------------------------------------------------------------------- |
| MCU                     | PIC16F873A ×1                                        | **STM32G474MET6 ×1**                                                |
| 발진/PLL/분주 IC        | SN74LS624N + MC145163 + 74HC04N + 74HC390N = **4종** | **모두 HRTIM 내장 (0종)**                                           |
| ADC/DAC IC              | MCP3202 + MCP4922 = **2종**                          | **모두 내장 ADC/DAC (0종)**                                         |
| 로직 IC                 | 74HC574 + 74HC138D + 74HC14D = **3종**               | **GPIO 직결 (0종)**                                                 |
| 저장 IC                 | CANTUS512 + ATMLH412 = **2종**                       | **내장 Flash + EEPROM(선택) = 0~1종**                               |
| 드라이버 IC             | ULN2003A ×1                                          | **ULN2003A ×2 (알람릴레이+부저 / LC탱크릴레이)**                    |
| **소계 (로직/디지털)**  | **13종**                                             | **1~2종 (MCU + 선택적 EEPROM)**                                     |
| 신규 추가 (아날로그/RF) | —                                                    | GaN FET, 게이트 드라이버, 커플러, RF 검출기 ×2, RS-485, LCD, 릴레이 |

> **핵심 효과**: 기존 13개 IC 중 **11개를 STM32G474 단일 칩으로 통합**하여,
> 디지털 로직 IC(VCO, PLL, 분주기, ADC, DAC, 래치, 디코더) 전체를 제거.
> 신규 추가되는 부품은 대부분 **아날로그/RF 전력단**(GaN FET, 커플러, RF 검출기)으로,
> 기존 설계에도 등가 부품이 존재했던 영역(Si MOSFET, 아날로그 계측 회로)의 현대화임.

### 하드웨어 구성 개요

기존 HS AUP Multi 메가소닉 발진기에서 외부 아날로그 발진 IC(VCO, PLL IC 등) 및 이산 로직을
STM32G474 내장 **HRTIM**(5.44GHz 유효 분해능)으로 완전 대체하여 원가 및 공간을 절감한다.

- **HRTIM Timer A**: 500kHz~2MHz 상보 PWM 생성 (CHA1 + CHA2)
- **절연 게이트 드라이버**: UCC21520 (5.7kVrms 절연) + 절연 DC-DC (R1215S), 또는 LMG1210 (부트스트랩)
- **전력 FET**: IPB200N15N3 (Si MOSFET, D2PAK, 손납땜 용이) 또는 EPC2036 (GaN, 최고 성능)
- **전력 검출**: 방향성 커플러(Directional Coupler) + RF 전력 검출기(AD8318, LTC5507 등)
- **LCD1602 디스플레이**: HD44780 LCD1602 캐릭터 LCD (2행×16칸럼, 74HCT574 ×2 방법 C, 20핀 커넥터)
- **RS-485 트랜시버**: MAX3485, SP3485 (3.3V 호환)
- **알람 릴레이**: 6개 릴레이 출력 (GO/OPERATE/LOW/HIGH/TRANSDUCER/ADDR) — ULN2003A #1
- **LC 탱크 릴레이**: 4개 (인덕턴스 조합 → 공진주파수 미세 조정) — ULN2003A #2
- **외부 제어**: REMOTE, 8POWER BCD(3비트 + COM) — 광커플러(TLP281-4) 절연 입력
- **제거된 외부 부품**: VCO IC, 아날로그 PLL IC, SG3525 PWM 컨트롤러 — **모두 STM32G4 HRTIM으로 대체**

### STM32G474MET6 vs STM32G431RBT6 비교

| 항목         | STM32G431RBT6 (초음파용) | **STM32G474MET6 (메가소닉용)** |
| ------------ | ------------------------ | ------------------------------ |
| 코어         | Cortex-M4 170MHz         | Cortex-M4 170MHz (동일)        |
| Flash / SRAM | 128KB / 32KB             | **512KB / 128KB** (4배)        |
| **HRTIM**    | ❌ 없음                  | ✅ **6개 Timer Unit, 184ps**   |
| 비교기(COMP) | 3개                      | **7개**                        |
| Op-Amp       | 3개                      | **4개**                        |
| DAC          | 3채널                    | 3채널                          |
| 패키지       | LQFP64                   | **LQFP80** (충분한 핀 여유)    |
| GPIO         | ~51개                    | **~51개** (PD0~PD2 추가)       |
| HAL          | STM32CubeG4              | STM32CubeG4 **(동일)**         |

### 시스템 클럭 설정

- 소스: 외부 HSE 8 MHz 크리스탈 (PF0=OSC_IN, PF1=OSC_OUT, ±20ppm)
- PLL: HSE 8MHz / 2 × 85 / 2 = SYSCLK 170 MHz
- AHB = 170 MHz, APB1 = 170 MHz, APB2 = 170 MHz
- **HRTIM DLL 캘리브레이션**: 부팅 시 반드시 `HAL_HRTIM_DLLCalibrationStart()` + `HAL_HRTIM_PollForDLLCalibration()` 호출
- HRTIM 유효 클럭: 170 MHz × 32 (DLL) = **5.44 GHz** (184 ps 분해능)

### 핀 맵 (STM32G474MET6 — LQFP80)

#### 메가소닉 발진 (HRTIM Timer A)

| 핀  | 포트        | 기능                                                |
| --- | ----------- | --------------------------------------------------- |
| PA8 | HRTIM1_CHA1 | 메가소닉 PWM 출력 (하이사이드 게이트 드라이버)      |
| PA9 | HRTIM1_CHA2 | 메가소닉 PWM 상보 출력 (로우사이드 게이트 드라이버) |

> **HRTIM AF 설정**: PA8, PA9 → AF13 (HRTIM1)

#### LCD1602 + LED 제어 (방법 C: 74HCT574 ×2, 20핀 커넥터)

| 핀   | 포트        | 기능                                    |
| ---- | ----------- | --------------------------------------- |
| PB10 | GPIO_Output | DATA[0] — 공유 버스 (74HCT574 #1/#2 D0) |
| PB11 | GPIO_Output | DATA[1] — 공유 버스 (74HCT574 #1/#2 D1) |
| PB12 | GPIO_Output | DATA[2] — 공유 버스 (74HCT574 #1/#2 D2) |
| PB13 | GPIO_Output | DATA[3] — 공유 버스 (74HCT574 #1/#2 D3) |
| PB14 | GPIO_Output | DATA[4] — 공유 버스 (74HCT574 #1/#2 D4) |
| PB15 | GPIO_Output | DATA[5] — 공유 버스 (74HCT574 #1/#2 D5) |
| PD0  | GPIO_Output | CLK_LCD — 74HCT574 #1 래치 클럭 (LCD용) |
| PD1  | GPIO_Output | CLK_LED — 74HCT574 #2 래치 클럭 (LED용) |

> PB10~PB15: 6비트 공유 DATA 버스. CLK_LCD(PD0) 상승엣지로 #1에 {RS,EN,D4~D7} 래치 → LCD1602.
> CLK_LED(PD1) 상승엣지로 #2에 {LED1~6} 래치. 74HCT574 VCC=5V, /OE=GND 고정.
> 디스플레이 보드 분리 구조, 20핀 커넥터로 연결. 버튼 풀업(외부 10kΩ), 부저(KEC105S) 포함.
> PD2, PB0, PB1, PB2: 여유 핀 (이전 LCD_D4, LED_NORMAL/HL_SET/8POWER 해제).

#### 택트 스위치 (6개, 외부 10kΩ 풀업, Active LOW)

| 핀  | 포트       | 기능                                                                    |
| --- | ---------- | ----------------------------------------------------------------------- |
| PC0 | GPIO_Input | BTN_START_STOP (발진 시작/정지, EXT모드에서 ADDR 설정 겸용)             |
| PC1 | GPIO_Input | BTN_MODE (모드 변경: NORMAL→H/L SET→8POWER→NORMAL…)                     |
| PC2 | GPIO_Input | BTN_UP (값 증가, 0.01W 단위)                                            |
| PC3 | GPIO_Input | BTN_DOWN (값 감소, 0.01W 단위)                                          |
| PC4 | GPIO_Input | BTN_SET (설정 확인/저장)                                                |
| PC5 | GPIO_Input | BTN_FREQ (짧게 누름: 현재 주파수 5초 표시, 5초 장기누름: FREQ SET 모드) |

#### LED 표시등 (6개, 74HCT574 #2 Q 출력, 560Ω 저항)

LED는 74HCT574 #2 Q0~Q5에 연결. DATA 버스(PB10~PB15)에 마스크 셋팅 후 CLK_LED(PD1) 상승엣지로 래치.

| LED ID | 기능       | 74HCT574 #2 Q핀     | 점등 조건                  |
| ------ | ---------- | ------------------- | -------------------------- |
| LED1   | LED_NORMAL | Q0 (DATA[0] = PB10) | NORMAL 모드                |
| LED2   | LED_HL_SET | Q1 (DATA[1] = PB11) | H/L SET 모드               |
| LED3   | LED_8POWER | Q2 (DATA[2] = PB12) | 8POWER 모드                |
| LED4   | LED_REMOTE | Q3 (DATA[3] = PB13) | REMOTE 모드 / 발진 중 점멸 |
| LED5   | LED_EXT    | Q4 (DATA[4] = PB14) | EXT 모드                   |
| LED6   | LED_RX     | Q5 (DATA[5] = PB15) | RS-485 수신 시 점멸        |

> LED 전류 제한 저항: **560Ω** (5V 기준, ~4.5mA). 74HCT574 Q 정격: 35mA 절대 최대.
> LED_W(출력 표시), LED_kHz(주파수 표시) 기능은 LCD1602에서 직접 표시하므로 별도 LED 불필요.

#### RS-485 Modbus RTU 통신

| 핀   | 포트        | 기능                                          |
| ---- | ----------- | --------------------------------------------- |
| PA2  | USART2_TX   | RS-485 TXD                                    |
| PA3  | USART2_RX   | RS-485 RXD                                    |
| PC13 | GPIO_Output | RS-485 DE (Driver Enable, HIGH=송신 활성화)   |
| PC14 | GPIO_Output | RS-485 /RE (Receiver Enable, LOW=수신 활성화) |

> 기존 제품의 RS-422 (D-SUB 9핀) 를 RS-485 반이중으로 변경. Modbus RTU 프로토콜 적용.
> **v2 변경**: DE/RE 핀을 PA4/PA5에서 PC13/PC14로 이동 (PA4를 DAC로 사용).

#### BUCK 전력 컨버터 제어 (LM5005)

| 핀  | 포트      | 기능                                                 |
| --- | --------- | ---------------------------------------------------- |
| PA4 | DAC1_OUT1 | BUCK FB 전압 조절 (0~3.3V로 출력 전력 0.1W~10W 가변) |
| PA5 | ADC2_IN13 | BUCK 출력 전압 보조 ADC (옵션, 피드백 루프 보정 용)  |

> **전력 제어 원리**: DAC 출력으로 LM5005 FB 핀에 전류 주입, 48V→2.5~25V 가변.
> 출력 전력 = V²/R (R=50Ω 트랜스듀서) → 0.1W(2.24V) ~ 10W(22.4V) 범위.
> 회로 상세: `docs/circuit_ref.md` 섹션 9 참조.

#### ADC 계측 입력 (전력, 전류, 전압)

| 핀  | 포트     | 기능                                                 |
| --- | -------- | ---------------------------------------------------- |
| PA0 | ADC1_IN1 | 순방향 전력 검출 (방향성 커플러 → RF 검출기 DC 출력) |
| PA1 | ADC1_IN2 | 역방향 전력 검출 (방향성 커플러 → RF 검출기 DC 출력) |
| PA6 | ADC2_IN3 | BUCK 출력 전류 측정 (0.1Ω 센스 저항 → 10mV/100mA)    |
| PA7 | ADC2_IN4 | BUCK 출력 전압 측정 (저항 분압 1:10 → 0~2.5V)        |

> **메가소닉 전력 검출**: MHz 대역에서는 저항/CT 직접 측정 불가 → 방향성 커플러 + RF 전력 검출기 필수.
> PA0(순방향), PA1(역방향) ADC로 VSWR 및 출력 전력 계산.
> PA6, PA7은 BUCK 컨버터의 출력 전류/전압을 측정하여 PI 피드백 제어에 사용.

#### 외부 제어 입력 (REMOTE, 8POWER BCD — 4개, 광커플러 TLP281-4)

| 핀   | 포트       | 기능                                                      |
| ---- | ---------- | --------------------------------------------------------- |
| PC6  | GPIO_Input | REMOTE 입력 (SHORT=발진ON, OPEN=발진OFF, 4-COM 단자 겸용) |
| PC10 | GPIO_Input | 8POWER BCD BIT1 (Step 선택 비트 1)                        |
| PC11 | GPIO_Input | 8POWER BCD BIT2 (Step 선택 비트 2)                        |
| PC12 | GPIO_Input | 8POWER BCD BIT3 (Step 선택 비트 3)                        |

> **8POWER BCD 선택 테이블** (매뉴얼 준수):
>
> | REMOTE(4) | BIT3(3) | BIT2(2) | BIT1(1) | 동작                   |
> | --------- | ------- | ------- | ------- | ---------------------- |
> | OPEN      | -       | -       | -       | 발진 정지              |
> | SHORT     | OPEN    | OPEN    | OPEN    | Step 0 (NORMAL 설정값) |
> | SHORT     | OPEN    | OPEN    | SHORT   | Step 1 설정값 출력     |
> | SHORT     | OPEN    | SHORT   | OPEN    | Step 2 설정값 출력     |
> | SHORT     | OPEN    | SHORT   | SHORT   | Step 3 설정값 출력     |
> | SHORT     | SHORT   | OPEN    | OPEN    | Step 4 설정값 출력     |
> | SHORT     | SHORT   | OPEN    | SHORT   | Step 5 설정값 출력     |
> | SHORT     | SHORT   | SHORT   | OPEN    | Step 6 설정값 출력     |
> | SHORT     | SHORT   | SHORT   | SHORT   | Step 7 설정값 출력     |
>
> **⚠️ 주의**: 2대 이상 발진기를 동시 제어 시, 반드시 **독립 접점(4-COM)** 사용. 병렬 접점 사용 시 오동작/고장 발생.
> **⚠️ 주의**: REMOTE ON/OFF 간 최소 **3초 이상** 간격 유지 (설정값 도달 보장).

#### 알람 릴레이 출력 (6개, ULN2003A #1로 구동)

| 핀   | 포트        | 기능                                                                    |
| ---- | ----------- | ----------------------------------------------------------------------- |
| PA11 | GPIO_Output | RELAY_GO (출력이 정상 범위 내: 알람 시 OPEN)                            |
| PA12 | GPIO_Output | RELAY_OPERATE (통합 알람 릴레이, **정상 시 SHORT / 알람·단전 시 OPEN**) |
| PA15 | GPIO_Output | RELAY_LOW (Err1 LOW ALARM 시 SHORT)                                     |
| PB3  | GPIO_Output | RELAY_HIGH (Err2 HIGH ALARM 시 SHORT)                                   |
| PB4  | GPIO_Output | RELAY_TRANSDUCER (Err5 진동자 알람 시 SHORT)                            |
| PA10 | GPIO_Input  | SENSOR_INPUT (TLP181 광커플러 절연 입력, Err7/Err8 발생)                |

> **릴레이 접점 사양** (매뉴얼 준수): 접점 용량 AC PEAK 100V, DC 100V, 0.3A.
> 전원 OFF 시 모든 릴레이 단자 OPEN. OPERATE 릴레이는 24V 0.1A 이상 스위치 접점 사용 권장.
> ULN2003A #1: 알람 릴레이 5채널 사용 (PA10은 SENSOR_INPUT 광커플러로 재할당, IN6/IN7 여유 2채널).
> **부저(PB5)**: ULN2003A #1에서 제거 → KEC105S(NPN, 내장 베이스 저항) 직접 구동 (디스플레이 보드 탑재).

#### LC 탱크 인덕턴스 조합 릴레이 (4개, ULN2003A #2로 구동)

| 핀   | 포트        | 기능                                              |
| ---- | ----------- | ------------------------------------------------- |
| PC9  | GPIO_Output | LC_RELAY1 (인덕턴스 L1 스위칭, PB8→PC9 v4 재배치) |
| PC15 | GPIO_Output | LC_RELAY2 (인덕턴스 L2 스위칭)                    |
| PC7  | GPIO_Output | LC_RELAY3 (인덕턴스 L3 스위칭, PF0→PC7 재배치)    |
| PC8  | GPIO_Output | LC_RELAY4 (인덕턴스 L4 스위칭, PF1→PC8 재배치)    |

> 릴레이 ON/OFF 조합으로 LC 탱크 인덕턴스 값 변경 → 공진주파수 미세 조정.
> ULN2003A #2: LC 릴레이 4개 사용 (3채널 여유).

#### 부저

| 핀  | 포트        | 기능                                                                                     |
| --- | ----------- | ---------------------------------------------------------------------------------------- |
| PB5 | GPIO_Output | KEC105S Base 구동 (디스플레이 보드 내 부저 트랜지스터, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL) |

#### 디버그 / 기타

| 핀   | 포트      | 기능                                                |
| ---- | --------- | --------------------------------------------------- |
| PB6  | USART1_TX | 디버그 메시지용 UART TX (선택사항, 115200 bps, AF7) |
| PB7  | USART1_RX | 디버그 메시지 수신용 UART RX (선택사항, AF7)        |
| PA13 | SWDIO     | SWD 데이터 입출력 (GPIO 사용 금지)                  |
| PA14 | SWCLK     | SWD 클럭 (GPIO 사용 금지)                           |
| NRST | RESET     | MCU 리셋 (ST-Link 연결 권장)                        |

#### HSE 크리스탈 — PF0/PF1

| 핀  | 기능    | 비고                |
| --- | ------- | ------------------- |
| PF0 | OSC_IN  | 8 MHz 크리스탈 입력 |
| PF1 | OSC_OUT | 8 MHz 크리스탈 출력 |

> HSE 8MHz → PLL → 170MHz SYSCLK. 정밀도 ±20ppm.

#### 여유(Spare) 핀 — LQFP80

config.h 기준 현재 할당되지 않은 여유 핀. **7개** (v4 방법 C 적용 후 PD2, PB0~PB2, PB8 해제).

| 핀  | 비고                                             |
| --- | ------------------------------------------------ |
| PA5 | ADC2_IN13 또는 GPIO (BUCK 보조 ADC 옵션)         |
| PD2 | GPIO (구 LCD_D4, 74HCT574 방법 C 전환 후 해제)   |
| PB0 | GPIO (구 LED_NORMAL, 74HCT574 #2로 전환 후 해제) |
| PB1 | GPIO (구 LED_HL_SET, 74HCT574 #2로 전환 후 해제) |
| PB2 | GPIO (구 LED_8POWER, 74HCT574 #2로 전환 후 해제) |
| PB8 | GPIO (구 LC_RELAY1, PC9로 재배치 후 해제)        |
| PB9 | I2C1_SDA(AF4) 또는 GPIO (EEPROM 확장 옵션)       |

> **GPIO 총계**: LQFP80에서 약 51개 GPIO 사용 가능 (SWD 2핀 제외 시 ~49개).
> 현재 GPIO 39개 + HSE 2개 = 41핀 사용, **여유 7개** (PA5, PD2, PB0~PB2, PB8, PB9).
> PF0/PF1은 HSE 크리스탈 전용. LC_RELAY3/4는 PC7/PC8로 재배치. LC_RELAY1은 PC9로 재배치.
> PC13/PC14는 RS-485 DE/RE로 사용. LQFP80에서는 PD3~PD15, PE 포트 전체가 미존재.

#### 🔧 PCB 아트웍 배선 라우팅 가이드 (LQFP80 물리 핀 기준)

```text
              ┌──── Top (61~80) ───────────────────────┐
              │ PB3~PB5(릴레이H/T+부저) PB6~PB9(I2C+여유)│
              │ BOOT0                                    │
   Left       │                                         │  Right
  (1~20)      │        STM32G474MET6                     │  (41~60)
  버튼 6ea    │          LQFP80                          │  LED W/kHz
  (PC0~PC5)   │                                         │  HRTIM PWM (PA8,PA9)
              │                                         │  SWD (PA13,PA14)
  ADC 4ch     │                                         │  릴레이 GO/OPER
  (PA0,PA1,   │                                         │  (PA11,PA12,PA15)
   PA6,PA7)   │                                         │
  RS485       │                                         │
  (PA2~PA5)   │                                         │
              └──── Bottom (21~40) ─────────────────────┘
                DATA버스(PB10~PB15) CLK_LCD(PD0) CLK_LED(PD1)
                20핀 커넥터→디스플레이 보드(LCD+LED+BTN+BUZZER)
                외부제어(PC6,PC10~PC12)
```

**부품 배치 권장:**

| PCB 영역      | 배치 부품                                                      | MCU 핀                                    |
| ------------- | -------------------------------------------------------------- | ----------------------------------------- |
| **좌측 상단** | 택트 스위치 6개                                                | PC0~PC5                                   |
| **좌측 하단** | MAX3485 + RS-485 커넥터                                        | PA2~PA5                                   |
| **좌측**      | RF 전력 검출기(ADC)                                            | PA0, PA1, PA6, PA7                        |
| **하측 중앙** | 20핀 커넥터 → 디스플레이 보드 (74HCT574×2, LCD+LED+BTN+BUZZER) | PB10~PB15, PD0, PD1, PC0~PC5, PB5         |
| **하측 우**   | 외부 제어 커넥터 (D-SUB 25P)                                   | PC6, PC10~PC12                            |
| **우측**      | HRTIM PWM → GaN 드라이버                                       | PA8, PA9                                  |
| **우측**      | 알람 릴레이 5개 + LC 릴레이 4개                                | PA11,PA12,PA15,PB3,PB4 / PC9,PC15,PC7,PC8 |
| **우측**      | ST-Link SWD 커넥터                                             | PA13, PA14                                |
| **상측**      | BOOT0 점퍼, I2C EEPROM 확장                                    | BOOT0, PB8(SCL), PB9(SDA)                 |

> **PCB 핵심 포인트**: HRTIM PWM 트레이스(PA8, PA9)는 GaN 드라이버까지 최단 거리로 배선하고,
> 그라운드 플레인 바로 위/아래 레이어에 배치. 디지털 신호(SPI, GPIO)와의 크로스토크 방지를 위해
> 별도 레이어 또는 가드 트레이스 삽입 필수.

---

## 동작 모드 상세 (매뉴얼 준수)

### 운전 모드 (RUN MODE)

기존 매뉴얼의 3가지 운전 모드를 충실히 재현한다.

#### 1. NORMAL 모드 (기본)

- **진입**: 전원 ON (기본), 또는 (SET+DOWN) 동시 누른 채 전원 ON
- **기능**:
  - START/STOP 버튼으로 발진 시작/정지
  - UP/DOWN 버튼으로 출력 전력 조정 (0.01W 단위)
  - SET 버튼으로 현재 출력값 저장 (전원 OFF까지 유지, Flash 저장 시 영구)
  - REMOTE 외부 입력으로 ON/OFF 제어 가능
  - 8POWER 외부 BCD 입력으로 8단계 출력 선택 가능
  - RS-485 Modbus **모니터링만 가능** (쓰기 불가)
  - FREQ 버튼 짧게 누름: 현재 설정 주파수를 5초간 표시
- **LED**: LED_NORMAL 점등
- **디스플레이**: 정지 시 설정 출력값 점멸(blink), 발진 시 실측 출력값 상시 표시

#### 2. REMOTE 모드

- **진입**: (SET+MODE) 동시 누른 채 전원 ON
- **기능**:
  - REMOTE 외부 입력으로만 ON/OFF 제어
  - 8POWER 외부 BCD 입력으로 8단계 출력 선택
  - RS-485 Modbus **모니터링만 가능** (쓰기 불가)
  - 전면 패널 START/STOP, UP/DOWN, SET 버튼 비활성화
- **LED**: LED_REMOTE 점등, 발진 중 LED_REMOTE 깜빡임

#### 3. EXT 모드 (외부 통신 제어)

- **진입**: (UP+MODE) 동시 누른 채 전원 ON
- **기능**:
  - RS-485 Modbus **읽기/쓰기 모두 가능** (출력 설정, 채널 변경 등)
  - REMOTE 외부 입력으로 ON/OFF 제어 가능
  - 8POWER 외부 BCD 입력으로 8단계 출력 선택 가능
  - ADDR(START/STOP) 버튼으로 슬레이브 주소 설정 (0~14)
  - UP/DOWN으로 주소 변경, SET로 확정
- **LED**: LED_EXT 점등, LED_RX 수신 시 점멸
- **통신 속도**: 9600 bps 기본 (Modbus 설정 레지스터로 변경 가능)

> **⚠️ 주의**: EXT에서 NORMAL 모드로 전환 시, 반드시 발진 정지 후 전원 OFF → (SET+DOWN) 누른 채 전원 ON.
> 설정값 변경(H/L SET, 8-POWER)은 반드시 RS-485 또는 NORMAL 모드로 전환 후 수행.

### 설정 모드 (SET MODE)

NORMAL 모드에서 MODE 버튼으로 진입하는 설정 하위 모드.

#### 4. H/L SET 모드 (최고/최저 알람 설정)

- **진입**: NORMAL 모드에서 MODE 버튼 1회 (LED_HL_SET 점등)
- **전제**: 발진 정지 상태에서만 진입 가능
- **기능**:
  - SET 버튼으로 HIGH/LOW 알람 토글 표시
  - UP/DOWN 버튼으로 알람 임계값 조정 (0.01W 단위)
  - SET 버튼으로 확정/저장
  - HIGH 알람 범위: (설정 출력 + 0.05W) ~ 1.50W
  - LOW 알람 범위: (설정 출력 - 0.05W) ~ 0.00W
- **디스플레이**: HIGH 또는 LOW 알람값 표시

#### 5. 8 POWER 모드 (8단계 출력 설정)

- **진입**: H/L SET 모드에서 MODE 버튼 1회 (LED_8POWER 점등)
- **기능**:
  - Step 0은 NORMAL 설정값과 동일 (자동 연동)
  - Step 1~7에 대해 각각 출력값, HIGH/LOW 알람값 개별 설정
  - SET 버튼으로 Step 선택, UP/DOWN으로 값 조정
  - SET 장기 누름으로 다음 Step 이동
- **디스플레이**: 현재 Step 번호 및 설정값 표시

#### 6. FREQ SET 모드 (주파수 채널 변경)

- **진입**: NORMAL 모드에서 FREQ 버튼 **5초 장기 누름**
- **전제**: NORMAL 모드에서만 진입 가능
- **기능**:
  - UP/DOWN 버튼으로 채널 번호 선택 (Ch0~Ch6)
  - SET 버튼으로 채널 변경 확정 및 저장
  - **채널 변경 시 발진 1초 정지** 후 새 주파수로 재개
- **디스플레이**: 채널 번호 또는 주파수값(kHz) 표시
- **FREQ 짧게 누름** (NORMAL 모드): 현재 설정 주파수를 약 5초간 표시 후 자동 복귀

```
MODE 전이도:
                    ┌──────────────────────────────────────┐
                    │                                      │
                    ▼                                      │
NORMAL ──MODE──▶ H/L SET ──MODE──▶ 8 POWER ──MODE──▶ (NORMAL 복귀)
  │
  ├── FREQ(5초 장기누름) ──▶ FREQ SET ──SET──▶ (NORMAL 복귀)
  │
  ├── START/STOP ──▶ 발진 시작/정지 토글
  │
  └── FREQ(짧게) ──▶ 5초간 주파수 표시 ──▶ 자동 복귀
```

---

## 모듈별 상세

### 1. HRTIM 메가소닉 PWM (`hrtim_pwm` + `megasonic_ctrl`)

HRTIM Timer A를 사용하여 500kHz~2MHz 메가소닉 주파수를 생성한다.
`hrtim_pwm`이 HRTIM 하드웨어 레벨(주파수, 듀티, 데드타임)을 담당하고,
`megasonic_ctrl`이 상위 로직(채널 관리, 출력 제어, 소프트 스타트)을 담당한다.

**HRTIM 유효 분해능:**

```
HRTIM 유효 클럭 = HRTIM_CLK × 32(DLL) = 170 MHz × 32 = 5.44 GHz
1 틱 = 1 / 5.44 GHz ≈ 184 ps
```

**주파수별 Period 및 듀티 분해능 (7채널 구성):**

| 주파수        | Period (틱) | 듀티 분해능 | 위상 분해능 | 비고                 |
| ------------- | ----------- | ----------- | ----------- | -------------------- |
| **500 kHz**   | **10,880**  | **0.009%**  | **0.033°**  | **Ch0 (최저)**       |
| 750 kHz       | 7,253       | 0.014%      | 0.050°      | Ch1                  |
| 1,000 kHz     | 5,440       | 0.018%      | 0.066°      | Ch2 (1MHz)           |
| 1,250 kHz     | 4,352       | 0.023%      | 0.083°      | Ch3                  |
| 1,500 kHz     | 3,627       | 0.028%      | 0.099°      | Ch4                  |
| 1,750 kHz     | 3,109       | 0.032%      | 0.116°      | Ch5                  |
| **2,000 kHz** | **2,720**   | **0.037%**  | **0.132°**  | **Ch6 (최고, 2MHz)** |

**핵심 설정:**

- **PWM 모드**: Up-counting (Edge-aligned) 또는 Up-Down (Center-aligned)
- **상보 출력**: HRTIM_CHA1 + HRTIM_CHA2 (하프 브리지 구동)
- **데드 타임**: HRTIM DTR(Dead-Time Register)로 설정 — 184ps 분해능
  - GaN FET 사용 시: **5~20 ns** 권장
  - Si MOSFET 사용 시: 50~100 ns 권장
- **주파수 설정**: Period 레지스터 값 변경

```
주파수 = HRTIM_EFF_CLK / Period = 5,440,000,000 / Period [Hz]
Period = 5,440,000,000 / 목표주파수 [틱]
```

- **듀티비 설정**: Compare 레지스터 값 변경
- **출력 활성화**: `HAL_HRTIM_WaveformOutputStart()` 호출 필수

**7채널 주파수 테이블 (500kHz~2MHz, 250kHz 단위):**

| 채널 | 주파수 (kHz) | Period (틱) | 용도               |
| ---- | ------------ | ----------- | ------------------ |
| Ch 0 | 500.0        | 10,880      | 최저 주파수        |
| Ch 1 | 750.0        | 7,253       |                    |
| Ch 2 | 1,000.0      | 5,440       | 1MHz               |
| Ch 3 | 1,250.0      | 4,352       |                    |
| Ch 4 | 1,500.0      | 3,627       |                    |
| Ch 5 | 1,750.0      | 3,109       |                    |
| Ch 6 | 2,000.0      | 2,720       | 최고 주파수 (2MHz) |

> **채널 변경 시**: 발진을 **1초간 정지** 후 새 주파수의 Period 값을 로드하고 발진을 재개한다 (매뉴얼 준수).

**안전 기능:**

| 기능          | 설명                                                          |
| ------------- | ------------------------------------------------------------- |
| 소프트 스타트 | 출력 개시 시 듀티비를 0%에서 목표값까지 점진적 증가 (500 ms)  |
| 비상 정지     | SENSOR OPEN, Modbus 명령 또는 에러 감지 시 즉시 출력 차단     |
| 출력 제한     | 듀티비 상한 클램핑 (하드웨어 보호, 최대 1.00W 초과 방지)      |
| HRTIM Fault   | HRTIM 내장 Fault 입력으로 하드웨어 레벨 비상 정지 (확장 시)   |
| 데드타임      | 하이사이드/로우사이드 관통 전류 방지 (184ps 분해능 정밀 제어) |

**GaN FET 드라이버 연동:**

메가소닉(MHz 대역)에서는 스위칭 속도가 극히 빠르므로, 기존 Si MOSFET + IR2110 구성 대신
GaN FET(EPC2xxx 시리즈 등) + 전용 게이트 드라이버(LMG1210, UCC27611 등)를 권장한다.

- **게이트 전압**: GaN FET는 5V 게이트 (Si의 10~12V 대비 낮음)
- **스위칭 시간**: ~2ns (Si의 ~50ns 대비 25배 빠름)
- **데드타임**: 5~20ns (HRTIM 184ps 분해능으로 정밀 제어 가능)
- **역회복 손실**: GaN은 역회복 전류 없음 → MHz 대역에서 발열 극소화

### 2. LCD1602 캐릭터 디스플레이 (`lcd1602` + `lcd1602_hw`)

HD44780 호환 LCD1602 캐릭터 LCD (2행×16칸럼).
**방법 C(74HCT574 ×2)**: 공유 6비트 DATA 버스(PB10~PB15) + CLK_LCD(PD0) / CLK_LED(PD1) 래치 제어.
`lcd1602`가 상위 API(초기화, 문자열, 커서)를, `lcd1602_hw`가 하위 DATA 버스 + CLK 래치 전송을 담당한다.

**하드웨어 드라이버 인터페이스 (`lcd1602_hw`):**

```c
void LCD_HW_InitDelay(void);                        // DWT 사이클 카운터 초기화
void LCD_HW_DelayUs(uint32_t us);                    // 마이크로초 지연 (DWT 기반)
void LCD_HW_SetDataBus(uint8_t val);                 // PB10~PB15에 6비트 데이터 셋팅
void LCD_HW_LatchLCD(void);                          // CLK_LCD(PD0) 상승엣지 → 74HCT574 #1 래치
void LCD_HW_WriteByte(uint8_t data, uint8_t is_data); // RS/EN/D4~D7 바이트 전송 (2회 클럭)
```

**상위 API 인터페이스 (`lcd1602`):**

```c
void LCD_Init(void);                                // 4비트 모드 초기화 시퀀스
void LCD_Clear(void);                               // 화면 전체 지움
void LCD_Home(void);                                // 커서 홈 위치
void LCD_SetCursor(uint8_t row, uint8_t col);        // 커서 위치 설정 (0–1행, 0–15열)
void LCD_WriteString(const char *str);               // 문자열 출력
void LCD_WriteChar(char ch);                         // 단일 문자 출력
void LCD_CreateChar(uint8_t addr, const uint8_t *pattern); // 사용자 정의 문자 (CGRAM)
void LCD_DisplayControl(uint8_t disp, uint8_t cur, uint8_t blink); // 표시 제어
```

**표시 패턴 (매뉴얼 준수, 2행 16칸럼 활용):**

| 상태           | 1행 표시 예시    | 2행 표시 예시     | 비고                   |
| -------------- | ---------------- | ----------------- | ---------------------- |
| 정지 (설정값)  | `SET:  0.50W   ` | `Ch0  500.0kHz  ` | 설정 출력값 blink      |
| 발진 (실측값)  | `OUT:  0.48W   ` | `Ch0  500.0kHz  ` | 실시간 검출 출력값     |
| 주파수 표시    | `FREQ: 1750kHz ` | `Channel: 5     ` | FREQ 버튼 짧게 누름 시 |
| FREQ SET 모드  | `FREQ SET MODE ` | `Channel: 3     ` | 채널 선택 중           |
| H/L SET (HIGH) | `HIGH: 0.80W   ` | `SET ALARM HIGH ` | HIGH 알람 설정값       |
| H/L SET (LOW)  | `LOW:  0.20W   ` | `SET ALARM LOW  ` | LOW 알람 설정값        |
| 에러           | `** ERROR **   ` | `Err1: LOW ALARM` | 에러 코드 표시         |
| EXT ADDR 설정  | `EXT MODE ADDR ` | `Address: 01    ` | 주소 번호 표시         |
| 8POWER 설정    | `8PWR Step: 3  ` | `OUT:  0.30W    ` | Step 출력값 설정       |

### 3. 버튼 입력 (`button`)

**6개 택트 스위치:**

| 버튼       | 기능 (NORMAL)         | 기능 (EXT)     | 기능 (설정 모드)     |
| ---------- | --------------------- | -------------- | -------------------- |
| START/STOP | 발진 시작/정지 토글   | ADDR 설정 진입 | —                    |
| MODE       | 설정 모드 전환        | —              | 다음 설정 모드       |
| UP         | 출력값 +0.01W         | ADDR +1        | 설정값 +0.01 또는 +1 |
| DOWN       | 출력값 -0.01W         | ADDR -1        | 설정값 -0.01 또는 -1 |
| SET        | 현재 출력값 저장      | ADDR 확정 저장 | 설정값 확정 저장     |
| FREQ       | 짧게: 주파수 5초 표시 | —              | —                    |
|            | 5초 장기: FREQ SET    |                |                      |

**디바운싱 알고리즘:**

- 소프트웨어 디바운싱: 10 ms 주기 폴링, 20 ms 연속 안정 시 확정
- SysTick 인터럽트 기반 주기 호출

**이벤트 유형:**

| 이벤트         | 조건                                      |
| -------------- | ----------------------------------------- |
| BTN_PRESS      | 눌림 → 놓임 (단일 클릭)                   |
| BTN_LONG_PRESS | 2초 이상 누름 유지 (동시 누름 판정 포함)  |
| BTN_REPEAT     | 장기 누름 시 200 ms 간격 반복 이벤트 발생 |

**특수 동시 누름 조합 (매뉴얼 준수):**

| 조합                 | 동작                         |
| -------------------- | ---------------------------- |
| MODE + DOWN          | 에러 즉시 해제 (Error Reset) |
| SET + DOWN + 전원 ON | NORMAL 모드 진입             |
| SET + MODE + 전원 ON | REMOTE 모드 진입             |
| UP + MODE + 전원 ON  | EXT 모드 진입                |

> **⚠️ 동시 누름 시 약 2초간 누르고 있어야 인식됨 (매뉴얼 준수).**

**인터페이스:**

```c
void     Button_Init(void);          // GPIO 초기화 (외부 10kΩ 풀업, GPIO_NOPULL)
void     Button_Process(void);       // 주기적 호출 (10 ms마다)
uint8_t  Button_GetEvent(uint8_t id); // 이벤트 읽기 및 소비 (읽으면 클리어)
uint8_t  Button_IsComboPressed(uint8_t mask); // 동시 누름 조합 확인
```

### 4. LED 표시등 (`led_indicator`)

6개 LED의 점등/소등/깜빡임 상태를 관리한다.

```c
void LED_Init(void);                               // GPIO 초기화
void LED_Set(uint8_t led_id, uint8_t state);        // ON/OFF 설정
void LED_SetBlink(uint8_t led_id, uint16_t period_ms); // 깜빡임 (주기 ms)
void LED_Update(void);                              // 깜빡임 상태 갱신 (메인 루프 호출)
```

**LED 동작 규칙 (매뉴얼 준수):**

| LED        | 점등 조건               | 깜빡임 조건             |
| ---------- | ----------------------- | ----------------------- |
| LED_NORMAL | NORMAL 모드             | —                       |
| LED_HL_SET | H/L SET 모드            | —                       |
| LED_8POWER | 8 POWER 모드            | —                       |
| LED_REMOTE | REMOTE 모드             | REMOTE 모드에서 발진 중 |
| LED_EXT    | EXT 모드                | —                       |
| LED_RX     | RS-485 정상 연결 시 OFF | 데이터 수신 시 점멸     |

> **LED_RX 특이사항** (매뉴얼 준수): RS-485 통신이 정상 연결 시 OFF, 연결 불량 시 상시 점등, 수신 시 점멸.
> LED_W/LED_kHz 기능은 LCD1602 화면에서 직접 표시하므로 별도 LED를 두지 않음.

### 5. ADC 전력 계측 (`adc_control`)

**ADC 설정:**

- ADC1 스캔 모드: IN1 (순방향 전력, PA0), IN2 (역방향 전력, PA1)
- ADC2 스캔 모드: IN3 (출력 전류, PA6), IN4 (출력 전압, PA7)
- 해상도: 12비트 (0–4095)
- 샘플링 타임: 92.5 cycles 이상 권장 (RF 검출기 출력은 고임피던스)
- 변환 방식: Scan 모드 + Continuous 변환 + DMA 전송
- 캘리브레이션: `HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED)` 필수

**계측값 변환:**

| 물리량         | ADC 채널 | 표시 범위        | 분해능 | Modbus 레지스터 |
| -------------- | -------- | ---------------- | ------ | --------------- |
| 출력 전력 (W)  | PA0      | 0.00 ~ 1.00 W    | 0.01 W | 0x0002          |
| 역방향 전력    | PA1      | (내부 VSWR 계산) | —      | —               |
| 출력 전류 (mA) | PA6      | 0 ~ 999 mA       | 1 mA   | 0x0015          |
| 출력 전압 (V)  | PA7      | 0 ~ 9 (9단계)    | 1 step | 0x0014          |
| 임피던스 (Ω)   | 계산     | 50 ~ 999 Ω       | 1 Ω    | 0x0016          |

> **임피던스 계산**: Z = V / I (전압과 전류 ADC 값으로 산출)

**노이즈 필터링:**

- 이동 평균 필터(16 샘플) + 히스테리시스(±2 카운트)
- DMA 연속 변환으로 메인 루프 부하 최소화

### 6. 알람 시스템 (`alarm`)

기존 매뉴얼의 에러 코드 체계를 그대로 재현한다.

**에러 코드 정의:**

| 에러 코드 | 명칭               | 내용                                          | 발진 상태 |
| --------- | ------------------ | --------------------------------------------- | --------- |
| Err1      | LOW ALARM          | 출력이 LOW 알람 설정값 이하로 **5초** 유지    | **정지**  |
| Err2      | HI ALARM           | 출력이 HIGH 알람 설정값 이상으로 **5초** 유지 | **정지**  |
| Err5      | TRANSDUCER ALARM   | 진동자 과부하 에러가 **5초** 유지             | **정지**  |
| Err6      | SETTING ERROR      | 설정 불가능한 값 입력 시도                    | 발진 지속 |
| Err7      | SENSOR ALARM (OFF) | SENSOR 입력 단자 OPEN (발진 정지 중)          | **정지**  |
| Err8      | SENSOR ALARM (RUN) | SENSOR 입력 단자 OPEN (발진 동작 중)          | **정지**  |

**에러 해제:**

- MODE + DOWN 동시 누름: 즉시 에러 해제 (Err7 제외)
- Err7: 전원 OFF → SENSOR SHORT 확인 → 전원 ON으로만 해제
- 전원 OFF 후 ON: 모든 에러 자동 해제 (Err7은 SENSOR SHORT 필요)
- Err1, Err2 발생 후: 전원 ON/OFF 1회, 또는 에러 해제 후 출력값/알람값 재설정 필요

**알람 릴레이 동작:**

| 상태              | RELAY_GO | RELAY_LOW | RELAY_HIGH | RELAY_TRANSDUCER | RELAY_OPERATE |
| ----------------- | -------- | --------- | ---------- | ---------------- | ------------- |
| 정상 출력 범위    | SHORT    | OPEN      | OPEN       | OPEN             | SHORT         |
| Err1 (LOW)        | OPEN     | **SHORT** | OPEN       | OPEN             | **OPEN**      |
| Err2 (HIGH)       | OPEN     | OPEN      | **SHORT**  | OPEN             | **OPEN**      |
| Err5 (TRANSDUCER) | OPEN     | OPEN      | OPEN       | **SHORT**        | **OPEN**      |
| Err7/8 (SENSOR)   | OPEN     | OPEN      | OPEN       | OPEN             | **OPEN**      |
| 전원 OFF          | OPEN     | OPEN      | OPEN       | OPEN             | OPEN          |

> **RELAY_OPERATE**: 통합 알람. 정상 동작 시 SHORT, 어떤 에러든 발생하거나 전원 미공급 시 OPEN.
> 24V 0.1A 이상 접점 용량 사용 권장.

### 7. 외부 제어 (`ext_control`)

D-SUB 25핀 커넥터를 통한 외부 제어 인터페이스.

```c
void ExtCtrl_Init(void);              // GPIO 초기화 (내부 풀업)
void ExtCtrl_Process(void);           // 주기적 상태 읽기 (메인 루프)
uint8_t ExtCtrl_GetRemoteState(void); // REMOTE 입력 상태 (0=OPEN, 1=SHORT)
uint8_t ExtCtrl_Get8PowerStep(void);  // 현재 BCD 입력에 해당하는 Step 번호 (0~7)
```

> SENSOR 기능은 알람 릴레이 중 하나(RELAY_ADDR)로 구현 예정. GPIO 입력 대신 릴레이 접점 방식.

### 8. Modbus RTU 통신 (`modbus_rtu` + `modbus_crc` + `modbus_regs`)

기존 매뉴얼의 RS-422 ASCII/BCC 프로토콜을 **RS-485 Modbus RTU**로 현대화.
매뉴얼의 모든 명령(W, STA, VER, C)에 대응하는 레지스터를 제공한다.

**물리 계층:** RS-485 반이중 통신 (MAX3485 / SP3485, 3.3V)

**USART 설정:**

- USART2: 9600 bps 기본 (9600/19200/38400/115200 선택 가능)
- 데이터 포맷: 8N1(기본) / 8E1 / 8O1
- 슬레이브 주소: 1~247 (기본 1, 매뉴얼의 0~E 범위를 Modbus 표준으로 확장)

**RS-485 방향 제어:**

```text
대기 / 수신 시: DE = LOW, /RE = LOW → RXNE 인터럽트로 바이트 수신
데이터 송신 시: DE = HIGH, /RE = HIGH → 데이터 송신 → TC 플래그 대기 → DE=LOW, /RE=LOW 복귀
```

**프레임 감지:**

- 프레임 간 묵음 구간: 3.5 캐릭터 시간 (9600 bps 기준 약 4 ms)
- USART IDLE 인터럽트 또는 타이머(TIM4)로 프레임 종료 감지

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

**매뉴얼 ASCII 명령 → Modbus 레지스터 대응표:**

| 매뉴얼 명령 | 기능             | Modbus 대응                          |
| ----------- | ---------------- | ------------------------------------ |
| W command   | 출력 전력 설정   | FC06 → 0x0001 (전력값 쓰기)          |
| STA command | 상태 읽기        | FC03 → 0x0000~0x0009 (10개 레지스터) |
| VER command | 펌웨어 버전 읽기 | FC03 → 0x0017                        |
| C command   | 주파수 채널 설정 | FC06 → 0x0003 (채널 번호 쓰기)       |

> **접근 제한**: NORMAL/REMOTE 모드에서는 Modbus 읽기(FC03)만 허용, 쓰기(FC06/10) 시 예외 응답.
> EXT 모드에서만 읽기/쓰기 모두 허용 (매뉴얼 준수).

**Modbus 레지스터 맵 (그룹별 0x10 블록 정렬):**

_그룹 0x00: 운전 제어 — 10개 연속 읽기 가능 (STA 명령 대응)_

| 주소   | R/W | 내용                | 단위/범위                            |
| ------ | --- | ------------------- | ------------------------------------ |
| 0x0000 | R/W | 동작 ON/OFF         | 0=OFF, 1=ON                          |
| 0x0001 | R/W | 출력 전력 설정값    | ×0.01 W (20~100 → 0.20~1.00W)        |
| 0x0002 | R   | 현재 실측 출력 전력 | ×0.01 W                              |
| 0x0003 | R/W | 주파수 채널 번호    | 0~6 (Ch0~Ch6)                        |
| 0x0004 | R   | 현재 주파수         | ×0.1 kHz                             |
| 0x0005 | R   | 동작 상태 플래그    | 비트 필드 (아래 참조)                |
| 0x0006 | R/W | 동작 모드           | 0=NORMAL, 1=REMOTE, 2=EXT            |
| 0x0007 | R/W | 슬레이브 주소       | 1~247 (매뉴얼: 0~14를 1~15로 매핑)   |
| 0x0008 | R/W | 로컬 입력 금지      | 0=허용, 1=금지                       |
| 0x0009 | R   | 외부 입력 상태      | bit0=REMOTE, bit1=SENSOR, bit2~4=BCD |

_그룹 0x10: 알람 및 진단 — 8개 연속 읽기 가능_

| 주소   | R/W | 내용             | 단위/범위                                 |
| ------ | --- | ---------------- | ----------------------------------------- |
| 0x0010 | R/W | HIGH 알람 설정값 | ×0.01 W (25~150 → 0.25~1.50W)             |
| 0x0011 | R/W | LOW 알람 설정값  | ×0.01 W (0~95 → 0.00~0.95W)               |
| 0x0012 | R   | 에러 상태 코드   | 비트 필드 (Err1~Err8)                     |
| 0x0013 | W   | 에러 리셋        | 0x0001 기록 시 에러 클리어                |
| 0x0014 | R   | 출력 전압        | 0~9 (9단계)                               |
| 0x0015 | R   | 출력 전류        | ×1 mA (0~999)                             |
| 0x0016 | R   | 임피던스         | ×1 Ω (50~999)                             |
| 0x0017 | R   | 펌웨어 버전      | 상위=Major, 하위=Minor (예: 0x0100=v1.00) |

_그룹 0x20: 8 POWER Step 0~7 출력 설정값_

| 주소          | R/W | 내용                    | 단위/범위        |
| ------------- | --- | ----------------------- | ---------------- |
| 0x0020~0x0027 | R/W | Step 0~7 출력 전력 설정 | ×0.01 W (20~100) |

_그룹 0x30: 8 POWER Step 0~7 HIGH 알람 설정값_

| 주소          | R/W | 내용               | 단위/범위        |
| ------------- | --- | ------------------ | ---------------- |
| 0x0030~0x0037 | R/W | Step 0~7 HIGH 알람 | ×0.01 W (25~150) |

_그룹 0x40: 8 POWER Step 0~7 LOW 알람 설정값_

| 주소          | R/W | 내용              | 단위/범위      |
| ------------- | --- | ----------------- | -------------- |
| 0x0040~0x0047 | R/W | Step 0~7 LOW 알람 | ×0.01 W (0~95) |

_그룹 0x50: 주파수 채널 테이블 (7개)_

| 주소          | R/W | 내용           | 단위/범위                 |
| ------------- | --- | -------------- | ------------------------- |
| 0x0050~0x0056 | R/W | Ch0~Ch6 주파수 | ×0.1 kHz (기본값 위 참조) |

_그룹 0x60: 통신 설정_

| 주소   | R/W | 내용                 | 단위/범위                          |
| ------ | --- | -------------------- | ---------------------------------- |
| 0x0060 | R/W | Modbus 슬레이브 주소 | 1~247                              |
| 0x0061 | R/W | 통신 속도 인덱스     | 0=9600, 1=19200, 2=38400, 3=115200 |
| 0x0062 | R/W | 패리티 설정          | 0=None, 1=Even, 2=Odd              |

_시스템 명령_

| 주소   | R/W | 내용        | 단위/범위                  |
| ------ | --- | ----------- | -------------------------- |
| 0x00FF | W   | 시스템 리셋 | 0x1234 기록 시 소프트 리셋 |

**상태 플래그 (0x0005) 비트 정의:**

| 비트  | 명칭         | 의미                   |
| ----- | ------------ | ---------------------- |
| 0     | RUN          | 발진 동작 중           |
| 1     | REMOTE       | REMOTE 입력 SHORT 상태 |
| 2     | EXT_MODE     | EXT 모드 활성          |
| 3     | SENSOR_OK    | SENSOR 정상 (SHORT)    |
| 4     | HIGH_ALARM   | 출력 HIGH 알람 상태    |
| 5     | LOW_ALARM    | 출력 LOW 알람 상태     |
| 6     | TRANS_ALARM  | 진동자 알람 상태       |
| 7     | POWER_8STEP  | 8POWER 모드 활성       |
| 8     | FREQ_DISPLAY | 주파수 표시 중         |
| 9     | COMM_ACTIVE  | RS-485 통신 활성       |
| 10    | SOFT_START   | 소프트 스타트 진행 중  |
| 11~15 | (예약)       | 향후 확장용 (0 고정)   |

**에러 상태 코드 (0x0012) 비트 정의:**

| 비트 | 에러 코드 | 명칭               |
| ---- | --------- | ------------------ |
| 0    | Err1      | LOW ALARM          |
| 1    | Err2      | HI ALARM           |
| 2    | Err5      | TRANSDUCER ALARM   |
| 3    | Err6      | SETTING ERROR      |
| 4    | Err7      | SENSOR ALARM (OFF) |
| 5    | Err8      | SENSOR ALARM (RUN) |

**CRC-16 (Modbus):** 다항식 0xA001, LSB-first. 테이블 룩업 방식 (256 엔트리).

---

## 메인 루프 구조

```c
int main(void)
{
    HAL_Init();                    // HAL 라이브러리 초기화 (SysTick 1 ms)
    SystemClock_Config();          // HSE 8MHz → PLL → 170 MHz
    GPIO_Init_All();               // 전체 GPIO 초기화
    HRTIM_PWM_Init();              // HRTIM Timer A + DLL 캘리브레이션
    MegasonicCtrl_Init();          // 메가소닉 제어 초기화 (채널 테이블 로드)
    ADC_Control_Init();            // ADC 다중 채널 및 DMA 초기화
    LCD_Init();                    // LCD1602 4비트 초기화
    LED_Init();                    // LED 표시등 초기화
    Button_Init();                 // 택트 스위치 초기화
    Alarm_Init();                  // 알람 시스템 초기화
    ExtCtrl_Init();                // 외부 제어 입력 초기화
    Menu_Init();                   // 모드 FSM 초기 상태 (부팅 시 동시누름 판정)
    Modbus_Init();                 // Modbus USART2 + 타이머 초기화

    while (1) {
        // 버튼 입력은 SysTick ISR에서 10 ms마다 Button_Process() 호출
        ADC_Control_Process();     // 전력/전류/전압 값 읽기 및 필터링
        ExtCtrl_Process();         // REMOTE/8POWER 외부 입력 상태 갱신
        Alarm_Process();           // 알람 조건 판정 (5초 유지 감시) + 릴레이 구동
        Menu_Update();             // 모드 FSM 갱신 (버튼 이벤트 소비)
        MegasonicCtrl_Update();    // 소프트스타트, 채널 변경, 출력 제어
        LCD_Update();              // LCD1602 표시 갱신 (변경 시에만)
        LED_Update();              // LED 깜빡임 갱신
        Modbus_Process();          // Modbus 수신 프레임 처리
    }
}
```

### 부팅 시 모드 결정 로직

```c
void Menu_Init(void)
{
    // 전원 ON 직후 버튼 조합 읽기 (2초 유지 필요)
    // SET + DOWN → NORMAL 모드 (또는 단독 전원 ON)
    // SET + MODE → REMOTE 모드
    // UP + MODE  → EXT 모드
    // 아무것도 → NORMAL 모드 (기본)
}
```

### 인터럽트 우선순위

| 우선순위 (숫자 낮을수록 높음) | 인터럽트         | 용도                               |
| ----------------------------- | ---------------- | ---------------------------------- |
| 0 (최고)                      | HRTIM Fault      | 메가소닉 하드웨어 비상 정지 (확장) |
| 1                             | USART2 RXNE/IDLE | Modbus 바이트 수신 및 프레임 감지  |
| 2                             | TIM4             | Modbus 3.5T 타임아웃               |
| 3                             | SysTick          | 버튼 폴링, HAL 틱 (1 ms)           |
| 4                             | ADC1/ADC2        | ADC 변환 완료 (DMA 사용 시 불필요) |

---

## 코딩 규칙

- **한글 주석**이 표준이며, 코드 수정 시에도 한글 주석을 유지할 것.
- HAL 함수 사용을 기본으로 하되, 타이밍 임계 경로(ISR 내부 등)에서는 LL 드라이버 또는 레지스터 직접 접근 허용.
- **HRTIM 레지스터 직접 접근 권장**: HAL_HRTIM API가 방대하고 오버헤드가 있으므로, 주파수/듀티 실시간 변경 시 레지스터 직접 접근을 적극 활용.
- 인터럽트 핸들러에서 **플래그만 설정**하고, 무거운 처리는 메인 루프에서 수행 (ISR 최소화 원칙).
- 함수 명명: `모듈명_동작()` 형식 (예: `LCD_SetCursor()`, `Modbus_ProcessFrame()`).
- 파일 명명: 소문자 + 언더스코어 (예: `modbus_rtu.c`, `hrtim_pwm.h`).
- 상수: `#define` 또는 `enum` 사용; 매직 넘버 금지.
- 전역 변수 최소화; 모듈 간 데이터 교환은 getter/setter 함수 권장.
- 공유 변수(ISR ↔ main): `volatile` 선언 필수, 필요 시 `__disable_irq()`/`__enable_irq()` 사용하되 임계 구간 최소화.
- **동적 메모리 할당 금지** (`malloc`/`free`/`calloc` 사용하지 않음).
- 각 모듈은 `.c`/`.h` 쌍으로 분리; 헤더에는 include guard (`#ifndef ... #endif`) 사용.
- 변수 선언은 함수 상단 또는 모듈 스코프 `static` — C99 이상 허용.

---

## 핵심 제약사항

- STM32G474MET6: **Flash 512 KB, SRAM 128 KB** — HRTIM 6유닛, COMP 7개, OpAmp 4개 내장.
- **HRTIM은 일반 타이머(TIM1~TIM4)와 완전히 다른 구조**:
  - DLL 캘리브레이션이 완료되어야 고해상도(×32) 모드가 활성화됨.
  - Period/Compare 값은 HRTIM 유효 틱 단위 (184ps).
  - 출력 활성화: `HAL_HRTIM_WaveformOutputStart(&hhrtim, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2)`.
  - 데드타임: HRTIM DTR 레지스터로 Rising/Falling 개별 설정 가능 (184ps 분해능).
  - Fault 입력으로 하드웨어 비상 정지 가능 (소프트웨어 개입 없이 즉시 출력 차단).
- **HRTIM DLL 캘리브레이션**: 반드시 HRTIM 사용 전 `HAL_HRTIM_DLLCalibrationStart()` 호출.
  주기적 자동 캘리브레이션 활성화 권장 (온도 변화에 따른 DLL 드리프트 보상).
- 타이머 클럭: G4 시리즈 APB1/APB2 모두 170MHz.
- RS-485 반이중 특성: 송신 완료(TC) 확인 후 DE → LOW 전환 필수.
- G4 USART 레지스터: `USART->ISR`(상태), `USART->ICR`(플래그 클리어), `USART->TDR`/`USART->RDR`(송수신 분리).
- G4 GPIO 속도: HRTIM PWM 출력 핀은 `GPIO_SPEED_FREQ_VERY_HIGH` 설정 필수.
- **LCD1602 (74HCT574 방법 C)**: HD44780 초기화 시퀀스 필수 (8비트×3회 → 4비트 전환).
  DATA 버스(PB10~PB15)에 {RS,EN,D4~D7} 셋팅 → CLK_LCD(PD0) 상승엣지로 74HCT574 #1 래치.
  DWT 사이클 카운터 초기화(`CoreDebug->DEMCR` + `DWT->CTRL`) 후 마이크로초 지연 사용.
  R/W 핀은 GND 고정 (쓰기 전용), 74HCT574 VCC=5V 필수.
- ADC 캘리브레이션: G4 ADC는 `HAL_ADCEx_Calibration_Start()` 없이는 정확한 값 불가.
- VDDA = 3.3 V; RF 전력 검출기 DC 출력이 3.3V 이내인지 확인 필요.
- **SWD 핀 예약**: PA13(SWDIO), PA14(SWCLK)은 GPIO로 사용 금지.
- Modbus CRC-16: 테이블 룩업 방식 (속도 최적화).
- **동시 누름 판정**: 전원 ON 시 2초간 버튼 조합을 읽어 모드를 결정.

---

## 흔한 실수

- **HRTIM PWM이 출력 안 됨** → DLL 캘리브레이션 호출 확인. `HAL_HRTIM_WaveformOutputStart()` 호출 확인. 출력 핀 AF13 설정 확인.
- **HRTIM 주파수가 부정확함** → DLL 캘리브레이션 미완료 시 ×32 배율 미적용. Period 값이 유효 범위(0x0003~0xFFFD) 내인지 확인.
- **RS-485 수신 불가** → DE/RE 핀이 아이들 시 LOW(수신 모드)인지 확인. TC 플래그 확인 후 DE→LOW.
- **LCD1602 표시 안 됨** → 74HCT574 #1 래치 확인: DATA 버스(PB10~PB15) 셋팅 후 CLK_LCD(PD0) 상승엣지 발생 여부 확인. 74HCT574 VCC=5V인지 확인. LCD VDD=5V 인가 확인. 4비트 초기화 시퀀스(8비트×3 → 4비트 전환) 확인. 대비 조절용 V0 핀 가변저항 확인.
- **에러가 해제 안 됨** → Err7은 MODE+DOWN으로 해제 불가. SENSOR 단자 SHORT 확인 후 전원 재투입만 가능.
- **8POWER 외부 선택이 안 됨** → REMOTE(4-COM) 단자가 SHORT인지 확인. BCD 입력이 제대로 연결되었는지 확인.
- **ADC 값 불안정** → 샘플링 타임 증가 (92.5 cycles 이상), 이동 평균 필터 적용, VDDA 바이패스 커패시터 확인.
- **`HAL_Delay()` 동작 안 함** → SysTick 인터럽트 우선순위 확인. ISR 블록 여부 확인.
- **170 MHz 클럭 설정 실패** → HSE(8 MHz) 크리스탈 발진 확인 + PLL 구성(PLLM/PLLN/PLLR) 확인.
- **채널 변경 시 이상 동작** → 채널 변경 전 반드시 1초간 발진 정지 후 새 Period 로드 (매뉴얼 준수).
- **HardFault 발생** → 스택 오버플로 확인. SRAM 128KB이므로 여유 있지만 큰 로컬 배열 주의.

---

## 파라미터 저장

사용자 설정값은 STM32G474ME 내장 Flash에 저장한다.

- 내장 Flash: 페이지 단위 소거 (**4 KB/page**, G474 기준), 쓰기 단위 **더블워드(64비트/8바이트)**, 수명 10,000회
  - G4 Flash 쓰기 절차: 언락(`HAL_FLASH_Unlock()`) → 페이지 소거(`HAL_FLASHEx_Erase()`) → 더블워드 프로그램(`HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, ...)`) → 락(`HAL_FLASH_Lock()`)
  - 512KB Flash에서 마지막 2~4페이지를 파라미터 영역으로 예약 권장
- 외부 EEPROM (AT24C02 등): I2C1 (PB8/PB9, AF4) 사용, 바이트 단위 쓰기, 100만회 수명 (확장 옵션)

**저장 대상:**

- 운전 모드 (NORMAL/REMOTE/EXT)
- 출력 전력 설정값 (Step 0)
- HIGH/LOW 알람 설정값
- 8 POWER Step 1~7 출력값 및 HIGH/LOW 알람값
- 현재 주파수 채널 번호
- 주파수 채널 테이블 (Ch0~Ch6, 각 주파수값)
- Modbus 슬레이브 주소, 통신 속도, 패리티

---

## 초기 설정값 (매뉴얼 준수)

| 항목                       | 초기값                 |
| -------------------------- | ---------------------- |
| 운전 모드                  | EXT MODE               |
| 슬레이브 주소              | 1 (매뉴얼의 ADDRESS 1) |
| 주파수 채널                | Ch 0                   |
| 통신 속도                  | 9600 bps (고정 기본)   |
| 출력 설정값                | 0.50 W                 |
| LOW 알람                   | 0.20 W                 |
| HIGH 알람                  | 0.80 W                 |
| 8 POWER Step 1~7 출력      | 0.05 W                 |
| 8 POWER Step 1~7 HIGH 알람 | 1.50 W                 |
| 8 POWER Step 1~7 LOW 알람  | 0.00 W                 |

---

## 양산용 펌웨어 다운로드

### 추천 1. ST-Link + STM32CubeProgrammer (가장 간편)

- SWD(PA13, PA14) + GND + NRST 연결
- BOOT0 핀 조작 불필요
- ST-Link V2 (개당 3,000~5,000원) + USB 케이블

### 추천 2. UART(RS-485) 부트로더

- BOOT0 핀을 HIGH로 올린 채 전원 ON → 내장 부트로더 활성화
- USB-to-RS485 변환기로 STM32CubeProgrammer 통해 펌웨어 업로드
- BOOT0 점퍼/버튼 하드웨어 설계에 포함 권장

### 추천 3. Custom 부트로더 (OTA)

- Flash 0x0800_0000 ~ 0x0800_4000에 통신 부트로더 배치
- Modbus 특수 명령으로 부트로더 진입 → RS-485를 통한 펌웨어 업데이트
- YMODEM 프로토콜 또는 Modbus 커스텀 FC로 바이너리 전송

---

## 문서 파일 (한글)

| 파일                                      | 내용                                  |
| ----------------------------------------- | ------------------------------------- |
| `docs/MANUAL- HS AUP Multi Generator.pdf` | 기존 메가소닉 제품 매뉴얼 (참조 원본) |
| `docs/Modbus_레지스터맵.md`               | Modbus 레지스터 상세 사양             |
| `docs/코드_설명서.md`                     | 펌웨어 전체 사양서                    |
| `docs/회로도_설명.md`                     | 회로 설계 및 부품 목록                |
| `docs/메가소닉_설계.md`                   | HRTIM 발진 회로 설계 상세             |
