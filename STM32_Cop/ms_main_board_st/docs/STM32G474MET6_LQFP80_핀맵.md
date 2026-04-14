# STM32G474MET6 LQFP80 핀맵 — 메가소닉 발진기 메인보드

> `copilot-instructions.md` 핀맵 섹션의 마스터 데이터.
> 회로도 및 CubeIDE .ioc 설정 시 본 문서를 기준으로 한다.

---

## 핀 할당 총괄표

### 메가소닉 발진 (HRTIM Timer A)

| 핀  | 포트        | AF   | 기능                                | 비고                                         |
| --- | ----------- | ---- | ----------------------------------- | -------------------------------------------- |
| PA8 | HRTIM1_CHA1 | AF13 | 메가소닉 PWM 출력 (하이사이드)      | GPIO_SPEED_FREQ_VERY_HIGH, UCC21520 INA 입력 |
| PA9 | HRTIM1_CHA2 | AF13 | 메가소닉 PWM 상보 출력 (로우사이드) | GPIO_SPEED_FREQ_VERY_HIGH, UCC21520 INB 입력 |

> **게이트 드라이버**: UCC21520 (절연, 5.7kVrms) + 절연 DC-DC (R1215S).
> **FET**: IPB200N15N3 (D2PAK, 150V) 또는 EPC2036 (GaN, 100V).
> **회로 상세**: `docs/circuit_ref_64pin.md` 참조.

### LCD1602 + LED 제어 (방법 C: 74HCT574 ×2, 20핀 커넥터)

| 핀   | 포트        | 기능                                    | 비고            |
| ---- | ----------- | --------------------------------------- | --------------- |
| PB10 | GPIO_Output | DATA[0] — 공유 버스 (74HCT574 #1/#2 D0) | 6비트 DATA 버스 |
| PB11 | GPIO_Output | DATA[1] — 공유 버스 (74HCT574 #1/#2 D1) | 6비트 DATA 버스 |
| PB12 | GPIO_Output | DATA[2] — 공유 버스 (74HCT574 #1/#2 D2) | 6비트 DATA 버스 |
| PB13 | GPIO_Output | DATA[3] — 공유 버스 (74HCT574 #1/#2 D3) | 6비트 DATA 버스 |
| PB14 | GPIO_Output | DATA[4] — 공유 버스 (74HCT574 #1/#2 D4) | 6비트 DATA 버스 |
| PB15 | GPIO_Output | DATA[5] — 공유 버스 (74HCT574 #1/#2 D5) | 6비트 DATA 버스 |
| PD8  | GPIO_Output | CLK_LCD — 74HCT574 #1 래치 클럭 (LCD용) | 디스플레이 클럭 |
| PD9  | GPIO_Output | CLK_LED — 74HCT574 #2 래치 클럭 (LED용) | 디스플레이 클럭 |

> PB10~PB15: 6비트 공유 DATA 버스. CLK_LCD(PD8) 상승엣지로 #1에 {RS,EN,D4~D7} 래치 → LCD1602.
> CLK_LED(PD9) 상승엣지로 #2에 {LED1~6} 래치. 74HCT574 VCC=5V, /OE=GND 고정.
> 디스플레이 보드 분리 구조, 20핀 커넥터로 연결. R/W 핀 GND 고정 (쓰기 전용).
> PD0는 SENSOR 입력으로 사용, PB0/PB1은 RS-485 DE/RE로 사용.

### 택트 스위치 (5개, 외부 10kΩ 풀업, Active LOW)

| 핀  | 포트       | 기능                                                   |
| --- | ---------- | ------------------------------------------------------ |
| PC0 | GPIO_Input | BTN_START_STOP (발진 시작/정지, EXT모드에서 ADDR 설정) |
| PC1 | GPIO_Input | BTN_MODE (모드 변경: NORMAL→H/L SET→8POWER→NORMAL)     |
| PC2 | GPIO_Input | BTN_UP (값 증가, 0.01W 단위)                           |
| PC3 | GPIO_Input | BTN_DOWN (값 감소, 0.01W 단위)                         |
| PC4 | GPIO_Input | BTN_SET (설정 확인/저장)                               |

> 버튼은 디스플레이 보드 내 외부 10kΩ 풀업 저항 사용 (3.3V ← R_PU ← 핀). gpio_init.c: GPIO_NOPULL.

### LED 표시등 (6개, 74HCT574 #2 Q 출력, 560Ω 저항)

LED는 74HCT574 #2 Q0~Q5에 연결. DATA 버스(PB10~PB15)에 마스크 설정 후 CLK_LED(PD9) 상승엣지로 래치.

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

### RS-485 Modbus RTU 통신

| 핀  | 포트        | AF  | 기능                                   |
| --- | ----------- | --- | -------------------------------------- |
| PA2 | USART2_TX   | AF7 | RS-485 TXD                             |
| PA3 | USART2_RX   | AF7 | RS-485 RXD                             |
| PB0 | GPIO_Output | —   | RS-485 DE (Driver Enable, HIGH=송신)   |
| PB1 | GPIO_Output | —   | RS-485 /RE (Receiver Enable, LOW=수신) |

> **핀 재배치 (v5)**: RS-485 DE/RE를 PB0/PB1로 이동.
> PA4는 DAC1_OUT1 (BUCK 전압 제어용), PA5는 여유 핀으로 전환.
> **v3**: HSE 8MHz 크리스탈 사용 (PF0/PF1). 클럭 정밀도 ±20ppm.

### BUCK 전력 컨버터 제어

| 핀  | 포트      | AF  | 기능                                         |
| --- | --------- | --- | -------------------------------------------- |
| PA4 | DAC1_OUT1 | —   | BUCK FB 전압 조절 (LM5005, 0~3.3V → 2.5~25V) |
| PA5 | ADC2_IN13 | —   | BUCK 출력 전압 ADC (옵션, 피드백 보조)       |

> **전력 제어**: DAC 출력으로 LM5005 FB 핀에 전류 주입, 출력 전력 0.1W~10W 가변.
> 회로 상세: `docs/circuit_ref_64pin.md` 섹션 9 참조.

### ADC 계측 입력 (전력, 전류, 전압)

| 핀  | 포트     | 기능                                                             |
| --- | -------- | ---------------------------------------------------------------- |
| PA0 | ADC1_IN1 | 순방향 전력 검출 (Forward Power, 방향성 커플러 → RF 검출기 DC)   |
| PA1 | ADC1_IN2 | 역방향 전력 검출 (Reflected Power, 방향성 커플러 → RF 검출기 DC) |
| PA6 | ADC2_IN3 | BUCK 출력 전류 측정 (전류 센스 저항 0.1Ω → 10mV/100mA)           |
| PA7 | ADC2_IN4 | BUCK 출력 전압 측정 (저항 분압 1:10 → 0~2.5V)                    |

### 외부 제어 입력 (REMOTE, 8POWER BCD + SENSOR — 5개, 광커플러)

| 핀   | 포트       | 기능                                                 |
| ---- | ---------- | ---------------------------------------------------- |
| PA15 | GPIO_Input | REMOTE 입력 (SHORT=발진ON, OPEN=발진OFF, 4-COM 겸용) |
| PC10 | GPIO_Input | 8POWER BCD BIT1                                      |
| PC11 | GPIO_Input | 8POWER BCD BIT2                                      |
| PC12 | GPIO_Input | 8POWER BCD BIT3                                      |
| PD0  | GPIO_Input | SENSOR 입력 (OPEN=Err7/Err8, SHORT=정상)             |

> REMOTE/BIT1~3은 TLP281-4, SENSOR는 TLP181(1채널) 사용.

### 알람 릴레이 출력 (5개, ULN2003A #1로 구동)

| 핀   | 포트        | 기능                                                         |
| ---- | ----------- | ------------------------------------------------------------ |
| PA11 | GPIO_Output | RELAY_GO (출력 정상 범위: 알람 시 OPEN)                      |
| PA12 | GPIO_Output | RELAY_OPERATE (통합 알람, 정상 시 SHORT / 알람·단전 시 OPEN) |
| PC15 | GPIO_Output | RELAY_LOW (Err1 LOW ALARM 시 SHORT)                          |
| PB3  | GPIO_Output | RELAY_HIGH (Err2 HIGH ALARM 시 SHORT)                        |
| PB4  | GPIO_Output | RELAY_TRANSDUCER (Err5 진동자 알람 시 SHORT)                 |

> 릴레이 접점 사양: AC PEAK 100V, DC 100V, 0.3A. OPERATE는 24V 0.1A 이상 접점 권장.
> ULN2003A #1: 알람 릴레이 5채널 사용 (IN6/IN7 여유 2채널).
> **부저(PB5)**: ULN2003A #1에서 제거 → KEC105S(NPN, 내장 베이스 저항) 직접 구동 (디스플레이 보드 탑재).

### LC 탱크 인덕턴스 조합 릴레이 (4개, ULN2003A #2로 구동)

| 핀  | 포트        | 기능                                           |
| --- | ----------- | ---------------------------------------------- |
| PC6 | GPIO_Output | LC_RELAY1 (인덕턴스 L1 스위칭, PB8→PC6 재배치) |
| PC7 | GPIO_Output | LC_RELAY2 (인덕턴스 L2 스위칭, PB9→PC7 재배치) |
| PC8 | GPIO_Output | LC_RELAY3 (인덕턴스 L3 스위칭)                 |
| PC9 | GPIO_Output | LC_RELAY4 (인덕턴스 L4 스위칭)                 |

> 릴레이 ON/OFF 조합으로 LC 탱크 인덕턴스 값 변경 → 공진주파수 미세 조정.
> ULN2003A #2: LC 릴레이 4개 사용 (3채널 여유).
> **v5 변경**: LC 릴레이 4개를 **GPIOC (PC6/7/8/9)로 통합**.
> GPIOB DATA 버스(PB10~PB15)와 포트 완전 분리 → BSRR 충돌 상황 제거.

### 부저

| 핀  | 포트        | 기능                                                                          |
| --- | ----------- | ----------------------------------------------------------------------------- |
| PB5 | GPIO_Output | KEC105S Base 구동 (디스플레이 보드 내 부저, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL) |

### 디버그 / 기타

| 핀   | 포트      | AF  | 기능                                 |
| ---- | --------- | --- | ------------------------------------ |
| PB6  | USART1_TX | AF7 | 디버그 메시지용 UART TX (115200 bps) |
| PB7  | USART1_RX | AF7 | 디버그 메시지 수신용 UART RX         |
| PA13 | SWDIO     | —   | SWD 데이터 (GPIO 사용 금지)          |
| PA14 | SWCLK     | —   | SWD 클럭 (GPIO 사용 금지)            |
| NRST | RESET     | —   | MCU 리셋 (ST-Link 연결 권장)         |

### HSE 크리스탈 (PF0/PF1)

| 핀  | 기능    | 비고                |
| --- | ------- | ------------------- |
| PF0 | OSC_IN  | 8 MHz 크리스탈 입력 |
| PF1 | OSC_OUT | 8 MHz 크리스탈 출력 |

> **v3 변경**: HSI(±1%) → HSE(±20ppm) 전환. PF0/PF1을 HSE 전용으로 할당.
> LC_RELAY1~4는 PC6~PC9로 재배치.

### 여유(Spare) 핀

config.h 기준 현재 할당되지 않은 여유 핀. **8개**.

| 핀   | 비고                                             |
| ---- | ------------------------------------------------ |
| PA5  | ADC2_IN13 또는 GPIO (BUCK 보조 ADC 옵션)         |
| PA10 | GPIO (향후 SENSOR/기타 입력 확장용)              |
| PB2  | GPIO (구 LED_8POWER, 74HCT574 #2로 전환 후 해제) |
| PB8  | GPIO (구 LC_RELAY1, PC6로 재배치 후 해제)        |
| PB9  | I2C1_SDA(AF4) 또는 GPIO (EEPROM 확장 옵션)       |
| PC13 | GPIO (구 RS-485 DE)                              |
| PC14 | GPIO (구 RS-485 /RE)                             |
| PC5  | GPIO (구 BTN_FREQ, 하드웨어 삭제로 해제)         |

> **v5 변경**: RS-485 DE/RE를 PC13/PC14에서 PB0/PB1로 재배치하여 PC13/PC14를 spare로 전환.

---

## 핀 사용 통계

| 포트     | 사용 핀 수  | 할당 목록                                                               |
| -------- | ----------- | ----------------------------------------------------------------------- |
| **PA**   | 14          | PA0~PA4, PA6~PA9, PA11~PA15 (PA5/PA10 여유)                             |
| **PB**   | 13          | PB0,PB1,PB3~PB7,PB10~PB15 (PB2/PB8/PB9 여유)                            |
| **PC**   | 13          | PC0~PC4, PC6~PC12, PC15 (PC5 여유, LC릴레이 4개 모두 GPIOC로 통합)      |
| **PD**   | 3           | PD8(CLK_LCD), PD9(CLK_LED), PD0(SENSOR_INPUT)                           |
| **PF**   | 2           | PF0(OSC_IN), PF1(OSC_OUT) — **HSE 크리스탈 전용**                       |
| **총계** | **41 GPIO** | FREQ 버튼 삭제 기준 GPIO 41개 사용, **여유 GPIO 19개**(총 60 GPIO 기준) |

> 비전원 핀 기준 집계: GPIO 41 + SWD 2 + HSE 2 + BOOT0 1 + NRST 1 = **47핀 사용**, 여유 **19핀**.

> PD 포트는 보드 핀맵 기준으로 실제 배치된 핀만 사용한다.

---

## PCB 아트웍 배선 라우팅 가이드

```text
              ┌──── Top (61~80) ───────────────────────┐
              │ PB3~PB5(릴레이H/T+부저) PB6~PB9(I2C+여유)│
              │ BOOT0                                    │
   Left       │                                         │  Right
  (1~20)      │        STM32G474MET6                     │  (41~60)
  버튼 5ea    │          LQFP80                          │  LC_REL1~4(PC6~PC9)
  (PC0~PC4)   │                                         │  HRTIM PWM (PA8,PA9)
              │                                         │  SWD (PA13,PA14)
  ADC 4ch     │                                         │  릴레이 GO/OPER
  (PA0,PA1,   │                                         │  (PA11,PA12,PA15)
   PA6,PA7)   │                                         │
  RS485       │                                         │
  (PA2~PA5)   │                                         │
              └──── Bottom (21~40) ─────────────────────┘
                DATA버스(PB10~PB15) CLK_LCD(PD8) CLK_LED(PD9)
                20핀 커넥터→디스플레이 보드(LCD+LED+BTN+BUZZER)
                외부제어(PA15,PC10~PC12,PD0)
```

### 부품 배치 권장

| PCB 영역      | 배치 부품                                                      | MCU 핀                                   |
| ------------- | -------------------------------------------------------------- | ---------------------------------------- |
| **좌측 상단** | 택트 스위치 5개                                                | PC0~PC4                                  |
| **좌측 하단** | MAX3485 + RS-485 커넥터                                        | PA2~PA5                                  |
| **좌측**      | RF 전력 검출기(ADC)                                            | PA0, PA1, PA6, PA7                       |
| **하측 중앙** | 20핀 커넥터 → 디스플레이 보드 (74HCT574×2, LCD+LED+BTN+BUZZER) | PB10~PB15, PD8, PD9, PC0~PC4, PB5        |
| **하측 우**   | 외부 제어 커넥터 (D-SUB 25P)                                   | PA15, PC10~PC12, PD0                     |
| **우측**      | HRTIM PWM → UCC21520 + FET                                     | PA8, PA9                                 |
| **우측**      | 알람 릴레이 5개 + LC 릴레이 4개                                | PA11,PA12,PC15,PB3,PB4 / PC6,PC7,PC8,PC9 |
| **우측**      | ST-Link SWD 커넥터                                             | PA13, PA14                               |
| **상측**      | BOOT0 점퍼                                                     | BOOT0                                    |

### LQFP64 검토 시 여유핀 후보

LQFP64 전환 시 본 프로젝트는 PD0/PD1/PD2 사용 제약을 먼저 검토해야 한다.
현재 매핑 기준에서 PD0(SENSOR)은 대체 핀(예: PA10)으로 이동이 필요하다.

| 분류                | 핀   | 비고                                    |
| ------------------- | ---- | --------------------------------------- |
| 즉시 여유 후보      | PA5  | ADC2_IN13 겸용                          |
| 즉시 여유 후보      | PB2  | 일반 GPIO                               |
| 즉시 여유 후보      | PB8  | I2C1_SCL 겸용                           |
| 즉시 여유 후보      | PB9  | I2C1_SDA 겸용                           |
| 즉시 여유 후보      | PC13 | 일반 GPIO                               |
| 즉시 여유 후보      | PC14 | 일반 GPIO                               |
| FREQ 삭제 반영 여유 | PC5  | 기존 BTN_FREQ 자리                      |
| 조건부 여유         | PA10 | SENSOR를 다른 핀으로 재배치 시에만 여유 |

### PCB 핵심 포인트

- **HRTIM PWM 트레이스(PA8, PA9)**: UCC21520 게이트 드라이버까지 최단 거리 배선. 그라운드 플레인 바로 위/아래 레이어에 배치. 디지털 신호와의 크로스토크 방지를 위해 별도 레이어 또는 가드 트레이스 삽입 필수.
- **UCC21520 절연 거리**: 1차측(MCU)과 2차측(FET) 사이 최소 8mm 이격 (5.7kV 절연 확보).
- **절연 DC-DC (R1215S)**: VDDA용 절연 전원. 하이사이드 FET 소스(SW 노드) 근처 배치.
- **RF 전력 검출기 ADC 입력(PA0, PA1)**: 아날로그 배선은 디지털 신호와 분리. GND 플레인 연속 확보.
- **LCD 데이터/클럭 핀 혼재**: PD8/PD9(클럭), PB12/PB13/PB15(DATA), PD0(SENSOR)가 분산되므로 커넥터 근접 배치로 배선 경로 최적화.

---

## 8POWER BCD 선택 테이블 (매뉴얼 준수)

| REMOTE(4) | BIT3(3) | BIT2(2) | BIT1(1) | 동작                   |
| --------- | ------- | ------- | ------- | ---------------------- |
| OPEN      | -       | -       | -       | 발진 정지              |
| SHORT     | OPEN    | OPEN    | OPEN    | Step 0 (NORMAL 설정값) |
| SHORT     | OPEN    | OPEN    | SHORT   | Step 1 설정값 출력     |
| SHORT     | OPEN    | SHORT   | OPEN    | Step 2 설정값 출력     |
| SHORT     | OPEN    | SHORT   | SHORT   | Step 3 설정값 출력     |
| SHORT     | SHORT   | OPEN    | OPEN    | Step 4 설정값 출력     |
| SHORT     | SHORT   | OPEN    | SHORT   | Step 5 설정값 출력     |
| SHORT     | SHORT   | SHORT   | OPEN    | Step 6 설정값 출력     |
| SHORT     | SHORT   | SHORT   | SHORT   | Step 7 설정값 출력     |

> ⚠️ 2대 이상 발진기 동시 제어 시 반드시 **독립 접점(4-COM)** 사용.
> ⚠️ REMOTE ON/OFF 간 최소 **3초 이상** 간격 유지.

