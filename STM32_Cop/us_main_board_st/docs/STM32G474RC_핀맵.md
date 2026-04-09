# STM32G474RCT6 핀맵 (LQFP64)

**적용 장치**: 초음파 발진기 메인보드  
**패키지**: LQFP64 (10×10 mm)  
**문서 버전**: 2.0  
**최종 수정**: 2026-03-30

> 본 문서는 copilot-instructions.md의 핀맵 섹션과 동기화됩니다.

---

## 1. 핀 할당 전체 요약 (물리 핀 번호 순)

### Left Side (Pin 1 ~ 16)

| 물리핀# | 포트 | AF/모드    | 기능 할당                               | 기능 그룹     |
| ------- | ---- | ---------- | --------------------------------------- | ------------- |
| 1       | VBAT | Power      | 배터리 백업 (미사용 시 VDD 연결)        | 전원          |
| 2       | PC13 | —          | (미사용)                                | —             |
| 3       | PC14 | —          | (미사용, OSC32_IN)                      | —             |
| 4       | PC15 | —          | (미사용, OSC32_OUT)                     | —             |
| 5       | PH0  | —          | OSC_IN (HSE 사용 시) 또는 미사용        | 클럭          |
| 6       | PH1  | —          | OSC_OUT (HSE 사용 시) 또는 미사용       | 클럭          |
| 7       | NRST | RESET      | MCU 리셋 (ST-Link 연결 강력 권장)       | 디버그        |
| 8       | PC0  | GPIO_Input | **BTN_MENU** (메뉴 진입/복귀)           | 택트 스위치   |
| 9       | PC1  | GPIO_Input | **BTN_UP** (메뉴 위로/값 증가)          | 택트 스위치   |
| 10      | PC2  | GPIO_Input | **BTN_DOWN** (메뉴 아래로/값 감소)      | 택트 스위치   |
| 11      | PC3  | GPIO_Input | **BTN_OK** (선택/확인, Start/Stop 겸용) | 택트 스위치   |
| 12      | PA0  | ADC1_IN1   | **보드 내장 가변저항** (출력/위상 레벨) | ADC           |
| 13      | PA1  | COMP1_INP  | **전압(V) 위상 검출** 비교기 (+)입력    | 아날로그 COMP |
| 14      | PA2  | USART2_TX  | **RS485 TXD**                           | Modbus RS485  |
| 15      | PA3  | USART2_RX  | **RS485 RXD**                           | Modbus RS485  |
| 16      | PA4  | ADC2_IN17  | **AC 전류 센서** (ACS722/TMCS1100)      | ADC           |

### Bottom Side (Pin 17 ~ 32)

| 물리핀# | 포트 | AF/모드     | 기능 할당                                 | 기능 그룹     |
| ------- | ---- | ----------- | ----------------------------------------- | ------------- |
| 17      | PA5  | GPIO_Output | **RS485 DE** (Driver Enable, HIGH=송신)   | Modbus RS485  |
| 18      | PA6  | GPIO_Output | **RS485 /RE** (Receiver Enable, LOW=수신) | Modbus RS485  |
| 19      | PA7  | COMP2_INP   | **전류(I) 위상 검출** 비교기 (+)입력 (CT) | 아날로그 COMP |
| 20      | PC4  | GPIO_Output | **부저 출력** (조작음 + 에러 알람 겸용)   | 외부 제어     |
| 21      | PC5  | GPIO_Output | **End Signal 릴레이** (사이클 종료 0.5s)  | 외부 제어     |
| 22      | PB0  | TIM3_CH3    | **ATtiny85 3kHz PWM** (위상제어 지령)     | 위상제어 출력 |
| 23      | PB1  | GPIO_Input  | **Remote ON/OFF** (외부 리모트 제어 입력) | 외부 제어     |
| 24      | PB2  | GPIO_Input  | **Sweep 스위치** (수동 스윕 푸쉬락)       | 외부 제어     |
| 25      | PB10 | GPIO_Input  | **Timer 스위치** (내장 타이머 푸쉬락)     | 외부 제어     |
| 26      | PB11 | ADC1_IN14   | **외부 판넬 가변저항** (외부 출력 레벨)   | ADC           |
| 27      | PB12 | GPIO_Output | **LCD RS** (Register Select)              | LCD1602       |
| 28      | PB13 | TIM1_CH1N   | **초음파 PWM 상보 출력** (로우사이드 FET) | 초음파 발진   |
| 29      | PB14 | GPIO_Output | **LCD EN** (Enable)                       | LCD1602       |
| 30      | PB15 | GPIO_Output | **LCD D4**                                | LCD1602       |
| 31      | PC6  | GPIO_Output | **LCD D5**                                | LCD1602       |
| 32      | PC7  | GPIO_Output | **LCD D6**                                | LCD1602       |

### Right Side (Pin 33 ~ 48)

| 물리핀# | 포트 | AF/모드     | 기능 할당                                | 기능 그룹    |
| ------- | ---- | ----------- | ---------------------------------------- | ------------ |
| 33      | PC8  | GPIO_Output | **LCD D7**                               | LCD1602      |
| 34      | PC9  | —           | **(Spare)** GPIO 자유 사용               | 여유         |
| 35      | PA8  | TIM1_CH1    | **초음파 PWM 출력** (하이사이드 FET)     | 초음파 발진  |
| 36      | PA9  | USART1_TX   | **디버그 UART TX** (115200bps, 선택사항) | 디버그       |
| 37      | PA10 | USART1_RX   | **디버그 UART RX** (선택사항)            | 디버그       |
| 38      | PA11 | —           | **(Spare)** TIM1_CH4(AF11) 또는 GPIO     | 여유         |
| 39      | PA12 | GPIO_Output | **Fault 릴레이** (에러 상태 지시)        | 외부 제어    |
| 40      | PA13 | SWDIO       | **SWD 데이터** (GPIO 사용 금지)          | 디버그       |
| 41      | VSS  | Power       | GND                                      | 전원         |
| 42      | VDD  | Power       | 3.3V                                     | 전원         |
| 43      | PA14 | SWCLK       | **SWD 클럭** (GPIO 사용 금지)            | 디버그       |
| 44      | PA15 | GPIO_Output | **Running 릴레이** (정상 동작 지시)      | 외부 제어    |
| 45      | PC10 | GPIO_Output | **상태 LED** (Run/Error 토글)            | 디버그       |
| 46      | PC11 | GPIO_Output | **RS485 RTERM** (종단 저항 ON/OFF)       | Modbus RS485 |
| 47      | PC12 | GPIO_Input  | **Board_Detect** (RS485 옵션보드 인식)   | Modbus RS485 |
| 48      | PD2  | —           | **(Spare)** TIM3_ETR(AF2) 또는 GPIO      | 여유         |

### Top Side (Pin 49 ~ 64)

| 물리핀# | 포트  | AF/모드       | 기능 할당                                 | 기능 그룹 |
| ------- | ----- | ------------- | ----------------------------------------- | --------- |
| 49      | PB3   | SWO           | **SWO** (Serial Wire Output, 선택사항)    | 디버그    |
| 50      | PB4   | —             | **(Spare)** GPIO 또는 TIM16_CH1(AF1)      | 여유      |
| 51      | PB5   | —             | **(Spare)** GPIO 또는 SPI1_MOSI(AF5)      | 여유      |
| 52      | PB6   | —             | **(Spare)** I2C1_SCL(AF4) 또는 GPIO       | 여유      |
| 53      | PB7   | —             | **(Spare)** I2C1_SDA(AF4) 또는 GPIO       | 여유      |
| 54      | BOOT0 | —             | 부트 모드 선택 (HIGH=부트로더, 점퍼 권장) | 시스템    |
| 55      | PB8   | I2C1_SCL(AF4) | (확장) I2C EEPROM SCL                     | 확장      |
| 56      | PB9   | I2C1_SDA(AF4) | (확장) I2C EEPROM SDA                     | 확장      |
| 57      | VSS   | Power         | GND                                       | 전원      |
| 58      | VDD   | Power         | 3.3V                                      | 전원      |
| 59~64   | —     | Power         | VSS/VDD/VDDA/VSSA 전원 핀들               | 전원      |

---

## 2. 기능 그룹별 핀 요약

### 2.1 초음파 발진 (TIM1)

| 핀   | 물리핀# | 포트      | 기능                             |
| ---- | ------- | --------- | -------------------------------- |
| PA8  | 35      | TIM1_CH1  | 초음파 PWM (하이사이드 FET)      |
| PB13 | 28      | TIM1_CH1N | 초음파 PWM 상보 (로우사이드 FET) |

### 2.2 위상제어 출력 (TIM3)

| 핀  | 물리핀# | 포트     | 기능                   |
| --- | ------- | -------- | ---------------------- |
| PB0 | 22      | TIM3_CH3 | ATtiny85 3kHz PWM 지령 |

### 2.3 내장 아날로그 블록 (COMP + DAC + TIM2 캡처)

| 블록  | 단자    | 연결 대상       | 핀/경로       | 용도                     |
| ----- | ------- | --------------- | ------------- | ------------------------ |
| COMP1 | INP(+)  | PA1 (외부)      | 물리핀 13     | 전압(V) 제로크로싱 검출  |
| COMP1 | INM(−)  | DAC3_CH1 (내부) | 내부 크로스바 | 기준전압 ~1.65V (VDDA/2) |
| COMP1 | OUT     | TIM2_IC1 (내부) | 내부 라우팅   | 전압 타임스탬프 캡처     |
| COMP2 | INP(+)  | PA7 (외부)      | 물리핀 19     | 전류(I) 제로크로싱 검출  |
| COMP2 | INM(−)  | DAC3_CH2 (내부) | 내부 크로스바 | 기준전압 ~1.65V (VDDA/2) |
| COMP2 | OUT     | TIM2_IC2 (내부) | 내부 라우팅   | 전류 타임스탬프 캡처     |
| DAC3  | CH1     | → COMP1_INM     | 내부 전용     | ✅ 사용                  |
| DAC3  | CH2     | → COMP2_INM     | 내부 전용     | ✅ 사용                  |
| DAC4  | CH1/CH2 | (예비)          | 내부 전용     | 🔵 예비                  |

> **위상차 측정**: TIM2(32비트) 듀얼 입력 캡처. 170MHz 기준 5.88ns/tick, 40kHz↔0.085°/tick 정밀도.

### 2.4 DAC 핀 충돌 상태

| DAC  | 채널 | 출력핀 | 충돌 대상             | 상태        |
| ---- | ---- | ------ | --------------------- | ----------- |
| DAC3 | CH1  | 없음   | 내부 전용 → COMP1_INM | ✅ **사용** |
| DAC3 | CH2  | 없음   | 내부 전용 → COMP2_INM | ✅ **사용** |
| DAC4 | ALL  | 없음   | 내부 전용 (예비)      | 🔵 예비     |
| DAC1 | CH1  | PA4    | ADC2_IN17 (전류 센서) | 🔴 충돌     |
| DAC1 | CH2  | PA5    | RS485 DE              | 🔴 충돌     |
| DAC2 | CH1  | PA6    | RS485 /RE             | 🔴 충돌     |

### 2.5 OPAMP 핀 충돌 상태 (LQFP64 전부 사용 불가)

| OPAMP  | VINP           | VOUT | 충돌 대상                   | 상태 |
| ------ | -------------- | ---- | --------------------------- | ---- |
| OPAMP1 | PA1, PA3, PA7  | PA2  | COMP1, USART2_RX/TX         | 🔴   |
| OPAMP2 | PA7            | PA6  | COMP2, RS485 /RE            | 🔴   |
| OPAMP3 | PB0, PB13, PA1 | PB1  | TIM3_CH3, TIM1_CH1N, Remote | 🔴   |
| OPAMP4 | PB13, PB11     | PB12 | TIM1_CH1N, 외부VR, LCD RS   | 🔴   |
| OPAMP5 | PB14, PC3      | PA8  | LCD EN, BTN_OK, TIM1_CH1    | 🔴   |
| OPAMP6 | PB12           | PB11 | LCD RS, 외부VR ADC          | 🔴   |

> LQFP100 이상 마이그레이션 시 일부 OPAMP 활용 여지 있음.

### 2.6 ADC 입력

| 핀   | 물리핀# | ADC 채널  | 기능                     |
| ---- | ------- | --------- | ------------------------ |
| PA0  | 12      | ADC1_IN1  | 보드 내장 가변저항       |
| PB11 | 26      | ADC1_IN14 | 외부 판넬 가변저항       |
| PA4  | 16      | ADC2_IN17 | AC 전류 센서 (ACS722 등) |

### 2.7 LCD1602 (4비트 병렬)

| 핀   | 물리핀# | 기능   | 비고            |
| ---- | ------- | ------ | --------------- |
| PB12 | 27      | LCD RS | Register Select |
| PB14 | 29      | LCD EN | Enable          |
| PB15 | 30      | LCD D4 | 데이터 비트 4   |
| PC6  | 31      | LCD D5 | 데이터 비트 5   |
| PC7  | 32      | LCD D6 | 데이터 비트 6   |
| PC8  | 33      | LCD D7 | 데이터 비트 7   |

> RW = GND 고정 (쓰기 전용). V0 = 10kΩ 가변저항 (대비 조절).

### 2.8 택트 스위치 (내부 풀업, Active LOW)

| 핀  | 물리핀# | 기능     | 동작                  |
| --- | ------- | -------- | --------------------- |
| PC0 | 8       | BTN_MENU | 메뉴 진입/상위 복귀   |
| PC1 | 9       | BTN_UP   | 위로/값 증가          |
| PC2 | 10      | BTN_DOWN | 아래로/값 감소        |
| PC3 | 11      | BTN_OK   | 선택/확인, Start/Stop |

### 2.9 Modbus RS485 (USART2)

| 핀   | 물리핀# | 포트/모드   | 기능                          |
| ---- | ------- | ----------- | ----------------------------- |
| PA2  | 14      | USART2_TX   | RS485 TXD                     |
| PA3  | 15      | USART2_RX   | RS485 RXD                     |
| PA5  | 17      | GPIO_Output | DE (HIGH=송신) — USART2 인접  |
| PA6  | 18      | GPIO_Output | /RE (LOW=수신) — USART2 인접  |
| PC11 | 46      | GPIO_Output | RTERM (종단 저항 ON/OFF)      |
| PC12 | 47      | GPIO_Input  | Board_Detect (옵션 보드 인식) |

### 2.10 외부 제어 인터페이스

| 핀   | 물리핀# | I/O    | 기능                                 |
| ---- | ------- | ------ | ------------------------------------ |
| PC4  | 20      | Output | 부저 (조작음 + 에러 알람 겸용)       |
| PC5  | 21      | Output | End Signal 릴레이 (사이클 종료 0.5s) |
| PA12 | 39      | Output | Fault 릴레이 (에러 상태 지시)        |
| PA15 | 44      | Output | Running 릴레이 (정상 동작 지시)      |
| PB1  | 23      | Input  | Remote ON/OFF (외부 리모트 제어)     |
| PB2  | 24      | Input  | Sweep 스위치 (수동 스윕 푸쉬락)      |
| PB10 | 25      | Input  | Timer 스위치 (내장 타이머 푸쉬락)    |

### 2.11 디버그 / 시스템

| 핀    | 물리핀# | 포트/모드   | 기능                          |
| ----- | ------- | ----------- | ----------------------------- |
| PA9   | 36      | USART1_TX   | 디버그 UART TX (115200bps)    |
| PA10  | 37      | USART1_RX   | 디버그 UART RX                |
| PA13  | 40      | SWDIO       | SWD 데이터 (GPIO 사용 금지)   |
| PA14  | 43      | SWCLK       | SWD 클럭 (GPIO 사용 금지)     |
| PB3   | 49      | SWO         | Serial Wire Output (선택사항) |
| PC10  | 45      | GPIO_Output | 상태 LED (Run/Error 토글)     |
| NRST  | 7       | RESET       | MCU 리셋 (ST-Link 연결 권장)  |
| BOOT0 | 54      | —           | 부트 모드 선택 (점퍼 권장)    |

### 2.12 여유(Spare) 핀

| 핀   | 물리핀# | 가능한 AF                    |
| ---- | ------- | ---------------------------- |
| PC9  | 34      | GPIO 자유 사용               |
| PA11 | 38      | TIM1_CH4(AF11) 또는 GPIO     |
| PB4  | 50      | GPIO 또는 TIM16_CH1(AF1)     |
| PB5  | 51      | GPIO 또는 SPI1_MOSI(AF5)     |
| PB6  | 52      | I2C1_SCL(AF4) 확장 또는 GPIO |
| PB7  | 53      | I2C1_SDA(AF4) 확장 또는 GPIO |
| PD2  | 48      | TIM3_ETR(AF2) 또는 GPIO      |

---

## 3. 핀 사용률 통계

| 항목                  | 수량       |
| --------------------- | ---------- |
| 전체 GPIO 핀          | 51         |
| 사용 중 GPIO 핀       | 37         |
| 여유(Spare) 핀        | 7          |
| 전원/GND 핀           | ~10        |
| 디버그 전용 (SWD)     | 2          |
| 기타 (BOOT0, NRST 등) | 3          |
| **사용률**            | **약 73%** |

---

## 4. PCB 아트웍 배선 가이드 (LQFP64 4면 기준)

```text
              ┌──── Top (49~64) ──────────────────┐
              │ PB3(SWO)  PB4~7(여유/I2C확장)     │
              │ BOOT0  PB8(SCL) PB9(SDA)          │
   Left       │                                    │  Right
  (1~16)      │        STM32G474RCT6               │  (33~48)
  버튼 4ea    │          LQFP64                     │  LCD D7
  ADC(내장VR) │                                    │  PWM CH1
  COMP1(V위상)│                                    │  USART1(디버그)
  USART2 TX/RX│                                    │  SWD(ST-Link)
  ADC(전류)   │                                    │  릴레이 2ea + LED
              └──── Bottom (17~32) ────────────────┘
                RS485 DE/RE   COMP2(I위상)   부저
                ATtiny PWM  외부스위치 3ea  외부VR(ADC)
                LCD RS~D6(6핀연속)   TIM1_CH1N
```

### PCB 부품 배치 권장

| PCB 영역      | 배치 부품                         | MCU 핀 (물리 핀#)                           |
| ------------- | --------------------------------- | ------------------------------------------- |
| **좌측 상단** | 택트 스위치 4개 + 내장 가변저항   | PC0~PC3(8–11), PA0(12)                      |
| **좌측 하단** | MAX3485 + RS485 커넥터            | PA2(14)TX, PA3(15)RX, PA5(17)DE, PA6(18)/RE |
| **하측 좌**   | COMP 아날로그 회로 (CT, 분압기)   | PA1(13)COMP1, PA7(19)COMP2                  |
| **하측 중앙** | 외부 스위치 커넥터, 외부 가변저항 | PB1~PB2(23–24), PB10~PB11(25–26)            |
| **하측 우**   | LCD1602 FPC/핀헤더 커넥터         | PB12~PB15(27–30), PC6~PC7(31–32)            |
| **우하 코너** | FET 게이트 드라이버 (IR2110 등)   | PB13(28)CH1N ↔ PA8(35)CH1                   |
| **우측 중앙** | ST-Link SWD 커넥터                | PA13(40)SWDIO, PA14(43)SWCLK                |
| **우측**      | 릴레이 출력 터미널, LED           | PA12(39)Fault, PA15(44)Running, PC10(45)LED |
| **상측**      | BOOT0 점퍼, I2C EEPROM (확장)     | BOOT0(54), PB8(55)SCL, PB9(56)SDA           |

### ⚠️ 아트웍 주의사항

1. **PB13(pin28) PWM ↔ LCD 크로스토크**: PB13(TIM1_CH1N)이 LCD 핀 블록(PB12~PC7) 사이에 끼여 있으므로, PWM 트레이스는 **별도 레이어**로 분리하거나 GND 가드 트레이스를 삽입할 것.

2. **PA7(pin19) 아날로그 ↔ PA5/PA6 디지털 노이즈**: PA7(COMP2, 고주파 아날로그)이 PA5(RS485 DE)·PA6(RS485 /RE) 디지털 핀과 1핀 간격으로 인접함.
   - PA6(pin18) ↔ PA7(pin19) 사이에 **GND 가드 트레이스** 또는 **GND 비아 펜스** 삽입
   - COMP2 아날로그 배선과 RS485 디지털 배선은 **별도 레이어**로 분리
   - PA7 트레이스 하부에 연속 GND 플레인 확보 (분할 금지)
   - CT 센서 → PA7 아날로그 경로는 최단 거리 라우팅, 디지털 교차 금지

---

## 5. 핀 변경 이력

| 변경 내용         | 이전 핀 | 현재 핀  | 변경 사유                                      |
| ----------------- | ------- | -------- | ---------------------------------------------- |
| 외부 가변저항 ADC | PA1     | PB11     | PA1을 COMP1_INP 전용 확보 (ADC 스캔 간섭 해소) |
| RS485 DE          | PC9     | PA5      | USART2(PA2/PA3) 인접 배치 → MAX3485 배선 단축  |
| RS485 /RE         | PC10    | PA6      | USART2(PA2/PA3) 인접 배치 → MAX3485 배선 단축  |
| Fault 릴레이      | PA6     | PA12     | PA6을 RS485 /RE로 재할당                       |
| Running 릴레이    | PA7     | PA15     | PA7을 COMP2_INP 전용 확보 (핀 충돌 해소)       |
| LED               | PA5     | PC10     | PA5를 RS485 DE로 재할당                        |
| COMP1 INM 추가    | —       | DAC3_CH1 | 내부 전용 DAC로 기준전압 공급 (v2.0 신규)      |
| COMP2 INM 추가    | —       | DAC3_CH2 | 내부 전용 DAC로 기준전압 공급 (v2.0 신규)      |
| COMP→TIM2 추가    | —       | 내부     | TIM2 듀얼 입력 캡처 위상차 측정 (v2.0 신규)    |
