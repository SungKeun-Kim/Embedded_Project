# 실전 워크플로우 가이드 (Practical Workflow) — 메가소닉 발진기

> AI 에이전트와 협업하여 펌웨어를 개발하는 **단계별 실전 절차서**.
> 한 번에 전부 시키지 말고, Phase별로 빌드 성공 + 하드웨어 검증을 확인하며 진행한다.

---

## Phase 의존성 다이어그램

```
Phase 0 (config.h 골격)
    │
    ▼
Phase 1 (클럭 + GPIO + SysTick)
    │
    ├──────────────┐
    ▼              ▼
Phase 2          Phase 3
(LCD+버튼+LED)   (HRTIM PWM 코어)
    │              │
    └──────┬───────┘
           ▼
       Phase 4 (ADC 전력 계측)
           │
           ├──────────────┐
           ▼              ▼
       Phase 5          Phase 6
       (알람+외부제어) (Modbus RTU)
           │              │
           └──────┬───────┘
                  ▼
              Phase 7 (통합 연동)
                  │
                  ▼
              Phase 8 (양산 준비)
```

> **Phase 2와 3은 독립 → 병렬 진행 가능.** UI가 먼저 되면 HRTIM 디버깅이 수월해지므로 Phase 2를 약간 앞서 진행하는 것을 권장.

---

## AI 에이전트 활용 핵심 원칙

### ① 컨텍스트 관리

```
에이전트가 정확한 코드를 생성하려면, 아래 파일을 매 요청에 포함해야 한다.

[필수 — 매 요청마다]
✅ .github/copilot-instructions.md     (프로젝트 전체 사양)
✅ include/config.h                     (핀 정의, 상수 정의 — 모든 코드의 기준)

[참조 — 단계별 선택]
📖 docs/ARCHITECTURE.md    → 모듈 간 호출 계약 확인 시
📖 docs/SAFETY.md          → 출력 제어, 비상 정지 관련 코드 시
📖 docs/PLANS.md           → 현재 진행 단계 확인 시
📖 docs/Modbus_레지스터맵.md → Modbus 관련 코드 시

[의존 파일 — 해당 모듈 구현 시]
📂 이전에 생성한 .h 헤더 파일  → 에이전트가 API를 정확히 참조하도록
```

### ② 요청 단위 원칙

| 규칙                   | 설명                                                     |
| ---------------------- | -------------------------------------------------------- |
| **1회 = 1~2 파일**     | .c + .h 페어 단위로 요청. 3개 이상이면 분할.             |
| **헤더 먼저**          | 공개 API(.h)를 먼저 확정 후, 구현(.c)을 요청하면 정합성↑ |
| **빌드 후 진행**       | 빌드 에러 수정 완료 전까지 다음 파일 요청 금지           |
| **하드웨어 검증 포함** | 단순 빌드 성공 ≠ 동작 확인. 오실로·LCD·LED로 실물 확인   |

### ③ 프롬프트 작성 패턴

```
[최적 프롬프트 구조]

1. 참조 문서 지시    → "copilot-instructions.md와 docs/SAFETY.md를 읽어라."
2. 에이전트 역할     → "@hw-driver 에이전트로 작업해줘." (선택, 명시하면 정확도↑)
3. 구현 대상         → "hrtim_pwm.h와 hrtim_pwm.c를 구현해줘."
4. 핵심 제약사항     → "HRTIM DLL 캘리브레이션, 데드타임 5~20ns, Period 범위 검증 필수."
5. 연동 정보         → "megasonic_ctrl.c에서 호출할 API: SetPeriod(), SetDuty(), Start(), Stop()"
6. 기존 파일 참조    → "include/config.h의 핀 정의, ARCHITECTURE.md의 hrtim_pwm↔megasonic_ctrl 계약을 따를 것."
```

### ④ Git 체크포인트

```
각 Phase 완료 후 반드시 커밋:

git add -A
git commit -m "Phase N: [완료 내용 요약]"

예시:
  git commit -m "Phase 0: config.h + project skeleton"
  git commit -m "Phase 1: SystemClock 170MHz + GPIO init + SysTick"
  git commit -m "Phase 2: LCD1602 + 5-button + 6-LED + menu FSM"
  git commit -m "Phase 3: HRTIM PWM + megasonic_ctrl + soft-start"
```

> 에이전트 생성 코드에 문제가 있으면 `git diff` 또는 `git stash`로 빠르게 롤백 가능.

---

## Step 0: 사전 준비

### 0-1. 회로도 & CubeIDE

1. KiCad/Altium 회로도 최종 확인 → `docs/STM32G474MET6_LQFP64_핀맵.md`와 대조
2. STM32CubeIDE에서 **STM32G474RETx** (LQFP64) 선택
3. `.ioc` 핀 할당 (`docs/cubemx_64pin_pin_table.md`, `docs/cubemx_64pin_onepage_checklist.md` 참조):

| 기능 그룹        | 핀                                        | CubeIDE 설정               |
| ---------------- | ----------------------------------------- | -------------------------- |
| HRTIM PWM        | PA8, PA9                                  | HRTIM1_CHA1/CHA2 (AF13)    |
| LCD/LED 공유버스 | PB10~PB15 + PC14(CLK_LCD) + PC15(CLK_LED) | GPIO_Output                |
| 버튼 5ea         | PC0~PC4                                   | GPIO_Input (Pull-up)       |
| ADC              | PA0, PA1, PA6, PA7                        | ADC1_IN1/IN2, ADC2_IN3/IN4 |
| RS-485           | PA2~PA3(USART2) + PB0,PB1(DE/RE)          | USART2 + GPIO              |
| 외부제어         | PA15, PC10~PC12, PD2                      | GPIO_Input (Pull-up)       |
| 알람릴레이 5ea   | PA11, PA12, PA10, PB3, PB4                | GPIO_Output                |
| LC릴레이 4ea     | PC6, PC7, PC8, PC9                        | GPIO_Output                |
| 부저             | PC5                                       | GPIO_Output                |
| 디버그 UART      | PB6, PB7                                  | USART1 (AF7)               |
| SWD              | PA13, PA14                                | **절대 GPIO 금지**         |

4. Project Manager → Toolchain: **CMake**, Generate Code
5. 생성 파일을 `cmake/stm32cubemx/`에 배치
6. 문서 HTML 생성(선택): `cmake --build build --target docs_html`

### 0-2. config.h 생성 (매우 중요)

> **config.h는 모든 모듈이 참조하는 중앙 핀/상수 정의 파일이다.**
> 이 파일이 없으면 에이전트가 매직 넘버를 사용하거나, 모듈 간 핀 정의가 불일치한다.

```
📋 프롬프트:

copilot-instructions.md의 핀맵 섹션과 docs/STM32G474MET6_LQFP64_핀맵.md를 읽어라.
include/config.h를 구현해줘. 아래 내용을 모두 포함:

1) 모든 GPIO 핀 정의 (#define LCD_RS_PORT, LCD_RS_PIN 등)
2) HRTIM 유효 클럭 상수 (5,440,000,000ULL)
3) 7채널 주파수 테이블 기본값 (Hz 단위, 500kHz~2MHz)
4) 출력 범위 상수 (POWER_MIN_001W, POWER_MAX_001W)
5) 알람 임계값 기본값
6) Modbus 기본 슬레이브 주소
7) 버튼 인덱스 enum (BTN_START_STOP=0, BTN_MODE=1, BTN_UP=2, BTN_DOWN=3, BTN_SET=4)
8) LED 인덱스 enum
9) 에러 코드 enum (ERR_NONE, ERR_LOW, ERR_HIGH, ...)
10) 동작 모드 enum (MODE_NORMAL, MODE_REMOTE, MODE_EXT, ...)

⚠️ 모든 상수는 copilot-instructions.md의 사양과 정확히 일치해야 한다.
```

**검증:**

- [ ] 빌드 성공 (config.h 단독으로는 빌드 대상 아니므로, main.c에서 include 후 확인)
- [ ] 핀 정의가 핀맵.md 및 copilot-instructions.md와 100% 일치

**커밋:** `git commit -m "Phase 0: config.h central pin/constant definitions"`

### 0-3. 빌드 환경 확인

```powershell
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

---

## Step 1: 기반 동작 (Phase 1) — @hw-driver

```
📋 프롬프트:

copilot-instructions.md, docs/SAFETY.md를 읽어라.
include/config.h를 참조하여 구현해줘.

[1회차] system_clock.c + system_clock.h
- HSE 8MHz → PLL → SYSCLK 170MHz
- APB1 = APB2 = AHB = 170MHz
- Flash Latency = 4
- HRTIM DLL 캘리브레이션은 Phase 3에서 하므로, 클럭 활성화만 해줘

[2회차] gpio_init.c + gpio_init.h + stm32g4xx_it.c
- config.h의 핀 정의를 사용하여 모든 GPIO 초기화
- 모든 출력 핀은 안전 상태(LOW)로 시작 (SAFETY.md 9항)
- SysTick 1ms ISR: HAL_IncTick() + 10ms 주기 플래그 설정
```

**검증:**

- [ ] 빌드 성공
- [ ] LED 6개 순차 점등 (HAL_Delay 정상 → 170MHz 클럭 정상)
- [ ] 버튼 5개 GPIO 읽기 (풀업 상태에서 HIGH, 누르면 LOW)

**커밋:** `git commit -m "Phase 1: SystemClock 170MHz + GPIO init + SysTick 1ms"`

---

## Step 2: 사용자 인터페이스 (Phase 2) — @ui

> **Step 3과 병렬 가능**하지만, LCD가 먼저 동작하면 HRTIM 디버깅이 수월하므로 먼저 진행 권장.

```
📋 프롬프트 (4회 분할):

[1회차] lcd1602_hw.h/c + lcd1602.h/c
copilot-instructions.md, include/config.h를 읽어라.
DWT 기반 delay_us() 구현 후, HD44780 4비트 초기화 시퀀스 구현.
⚠️ config.h의 LCD 핀 정의를 반드시 사용: DATA=PB10~PB15, CLK_LCD=PC14, CLK_LED=PC15 (LQFP64 재편성)

[2회차] button.h/c
include/config.h의 BTN_xxx 정의를 사용하여 5개 버튼 디바운싱 구현.
ARCHITECTURE.md의 "10ms: Button_Process()" 패턴을 따를 것.
동시 누름 조합: MODE+DOWN(에러 해제), SET+DOWN/SET+MODE/UP+MODE(부팅 모드 판정)

[3회차] led_indicator.h/c
6개 LED 점등/소등/깜빡임(500ms 주기) 상태 관리.
모드별 LED 상태는 copilot-instructions.md의 "LED 동작 규칙" 참조.

[4회차] menu.h/c
모드 FSM: NORMAL → H/L SET → 8POWER → NORMAL 순환 (MODE 버튼)
FREQ SET: 통신 명령(Modbus) 또는 내부 서비스 루틴으로 진입
부팅 시 동시누름: SET+DOWN=NORMAL, SET+MODE=REMOTE, UP+MODE=EXT
⚠️ 이전에 구현한 lcd1602.h, button.h, led_indicator.h의 API를 참조할 것
⚠️ ARCHITECTURE.md의 menu↔megasonic_ctrl 계약을 따를 것 (g_mega_state 읽기 전용)
```

**검증:**

- [ ] LCD "Hello" 표시 → 대비 조절(V0) 확인
- [ ] 5개 버튼 단일/장기/반복 이벤트 감지
- [ ] MODE 순환: NORMAL→H/L SET→8POWER→NORMAL
- [ ] 6개 LED 모드별 점등/깜빡임

**커밋:** `git commit -m "Phase 2: LCD1602 + 5-button + 6-LED + menu FSM"`

---

## Step 3: HRTIM 메가소닉 PWM (Phase 3) — @hw-driver + @megasonic

```
📋 프롬프트 (3회 분할):

[1회차 — @hw-driver] hrtim_pwm.h/c
copilot-instructions.md, docs/SAFETY.md, docs/ARCHITECTURE.md를 읽어라.
include/config.h의 HRTIM 상수(유효 클럭 5.44GHz, 채널 테이블)를 사용.
skills/stm32g4-dev/references/hal-patterns.md의 "HRTIM Timer A PWM 패턴" 참조.

필수 구현:
- DLL 캘리브레이션 + 자동 주기 캘리브레이션
- Timer A 시간 기반 + 데드타임(Rising/Falling, 5~20ns)
- SetPeriod(uint32_t period), SetDuty(uint16_t duty_permille)
- OutputStart(), OutputStop()
- 듀티 클램핑: DUTY_MAX_PERMILLE 초과 불가 (SAFETY.md 1항)
- Period 범위 검증: 0x0003 ≤ period ≤ 0xFFFD

[2회차 — @megasonic] megasonic_ctrl.h/c
docs/ARCHITECTURE.md의 hrtim_pwm↔megasonic_ctrl 계약을 따를 것.
docs/SAFETY.md의 소프트 스타트, 비상 정지 규칙 준수.

필수 구현:
- 7채널 관리 (config.h 채널 테이블 참조, 500kHz~2MHz)
- 소프트 스타트: 0%→목표 듀티, 50단계, 500ms
- 채널 변경: 출력 정지 → 1초 대기 → 새 Period → 소프트 스타트 재개
- Start/Stop 토글
- 비상 정지 (EmergencyStop: HRTIM 출력 즉시 차단)
- g_mega_state 전역 상태 구조체 (menu, modbus에서 읽기 전용)

[3회차 — 통합] main.c에 Phase 1~3 연동
system_clock → gpio → hrtim_pwm → megasonic_ctrl 초기화 순서.
메인 루프: Button_Process(10ms) + Menu_Update() + MegasonicCtrl_Update() + LCD_Update()
```

**검증:**

- [ ] 오실로스코프 PA8/PA9 파형: 상보 출력 + 데드타임 확인
- [ ] Ch0(500kHz) → Ch2(1MHz) → Ch6(2MHz) 채널 변경: Period 정확도 확인
- [ ] 채널 변경 시 1초 정지 확인 (오실로 트리거)
- [ ] BTN_START_STOP → Start/Stop 토글 → LCD에 상태 표시
- [ ] 소프트 스타트: 듀티 램프업 500ms (오실로)

**커밋:** `git commit -m "Phase 3: HRTIM PWM + megasonic_ctrl + soft-start + channel mgmt"`

---

## Step 3.5: 자동 공진 탐색 (Phase 3.5) — @megasonic

> **Phase 3 빌드 + 하드웨어 검증 완료 후 진행.**
> ADC(Phase 4)와 병렬 개발 가능하지만, 실 트랜스듀서 테스트는 ADC 완료 후 가능.

```
📋 프롬프트:

copilot-instructions.md (섹션 9 자동 공진 탐색), docs/SAFETY.md,
include/config.h, include/params.h (RESCAN_* 상수),
include/megasonic_ctrl.h (ResonanceScanResult_t, ScanResonance API)를 읽어라.

megasonic_ctrl.c에 아래 기능을 추가 구현해줘 (@megasonic 에이전트):

[1단계] LC 릴레이 제어
- MegasonicCtrl_SetLCRelay(uint8_t combo)
  4비트 combo(0~15)를 LC_RELAY1~4 GPIO 핀에 출력
  고정 인덕턴스: L_BASE=1.79uH(3.3||3.9)
  bit0=LC_RELAY1(L1=0.89uH=1.5||2.2), bit1=LC_RELAY2(L2=2.7uH),
  bit2=LC_RELAY3(L3=4.7uH), bit3=LC_RELAY4(L4=6.8uH)
  L_total = L_BASE + 선택된 릴레이 인덕턴스 합
  핀: PC6/PC7/PC8/PC9 (config.h 참조)

[2단계] VSWR 계산 유틸리티
- static uint16_t CalcVSWR_x100(uint16_t fwd, uint16_t rev)
  반환: (fwd + rev) * 100 / (fwd - rev), fwd<=rev 시 9999 반환

[3단계] 자동 공진 탐색 메인 함수
- ResonanceScanResult_t MegasonicCtrl_ScanResonance(void)
  7채널 × 16 L조합 = 112포인트 스윕
  1차 탐색: RESCAN_POWER_01W(0.15W) 전류 기반
  2차 VSWR: RESCAN_POWER_VSWR(0.3W) 또는 RESCAN_POWER_VSWR_LF(0.5W, 500~700kHz)
  각 포인트마다 ADC_Control_GetFwdRaw()/RevRaw() 읽기 + CalcVSWR_x100()
  LCD 진행률 표시: "SCANNING...  3/7 ch" (상단) / "L=0110 VSWR:1.20" (하단)
  VSWR ≤ RESCAN_VSWR_OK_X100 발견 시 즉시 탐색 완료
  완료 후 best 포인트 적용 (채널 + L조합 설정) + 발진 정지

[4단계] Flash 저장/로드
- MegasonicCtrl_SaveScanResult() / LoadScanResult()
  params.h 의 Flash 영역 사용 (기존 파라미터 저장 메커니즘과 동일)

[5단계] 결과 적용
- MegasonicCtrl_ApplyScanResult()
  HRTIM SetChannel(best.ch) + SetLCRelay(best.l_combo) → 소프트스타트

⚠️ 안전 규칙 (SAFETY.md 준수):
- 탐색 실패(valid=false) 시 발진 정지, LCD "TUNE FAIL" → Ch0/L=0으로 폴백
- 탐색 중 BTN_START_STOP 누르면 즉시 탐색 중단 → 발진 정지 상태 복귀
- 릴레이 전환 중(20ms 이내) 전류 측정 금지
```

**LCD 표시 패턴 (탐색 중)**:

| 상태      | 1행                | 2행                 |
| --------- | ------------------ | ------------------- |
| 탐색 중   | `SCANNING... 3/7`  | `L:0110 VSWR:1.42`  |
| 탐색 성공 | `TUNE OK  Ch3    ` | `L=0110 VSWR:1.12`  |
| 탐색 실패 | `TUNE FAIL       ` | `Ch0 L=0000 (Dflt)` |

**검증:**

- [ ] 더미 부하(저항) 연결 후 탐색 실행 → 탐색 포인트별 VSWR 값 디버그 UART 출력
- [ ] 실 PZT 트랜스듀서 연결 → 탐색 완료 → LCD "TUNE OK" 표시
- [ ] 탐색 완료 후 Flash 저장 → 전원 재투입 → 재탐색 없이 바로 동작 확인
- [ ] BTN_START_STOP 누름으로 탐색 중단 동작 확인

**커밋:** `git commit -m "Phase 3.5: auto resonance scan (112-point VSWR sweep + LC relay control)"`

---

## Step 4: ADC 전력 계측 (Phase 4) — @hw-driver

```
📋 프롬프트:

copilot-instructions.md, docs/ARCHITECTURE.md를 읽어라.
include/config.h의 ADC 핀 정의를 사용.
skills/stm32g4-dev/references/hal-patterns.md의 "ADC 패턴" 참조.

adc_control.h/c 구현:
- ADC1: IN1(PA0 순방향전력) + IN2(PA1 역방향전력) 스캔, DMA
- ADC2: IN3(PA6 전류) + IN4(PA7 전압) 스캔, DMA
- 캘리브레이션 필수 (SAFETY.md 5항)
- 이동 평균 필터 (16샘플)
- RF 검출기 DC → W 변환 (교정 테이블/수식, 초기에는 선형 근사)
- 전류(mA), 전압(단계), 임피던스(Ω) 계산
- 간접 연동: ARCHITECTURE.md "adc → menu/alarm" 계약 (getter 함수 제공)
```

**검증:**

- [ ] 디버그 UART로 ADC 원시값 4채널 출력 → 노이즈 ±2 이내
- [ ] RF 검출기 DC 입력 → LCD에 전력(W) 표시 (0.00~1.00W)
- [ ] 필터링 후 안정적인 값 유지

**커밋:** `git commit -m "Phase 4: ADC1/ADC2 dual power measurement + filter"`

---

## Step 5: 알람 + 외부 제어 (Phase 5~6) — @megasonic

```
📋 프롬프트 (2회 분할):

[1회차] ext_control.h/c
copilot-instructions.md, include/config.h를 읽어라.
- REMOTE(PC6): SHORT=발진 허용, OPEN=발진 정지
- 8POWER BCD(PC10~12): 3비트 → Step 0~7 선택
- getter 함수 제공: ExtCtrl_GetRemoteState(), Get8PowerStep()

[2회차] alarm.h/c
copilot-instructions.md, docs/SAFETY.md를 읽어라.
adc_control.h(4단계 완료)와 ext_control.h의 API를 참조.
- Err1(LOW): 실측출력 < LOW알람값, 5초 유지 시 발동
- Err2(HIGH): 실측출력 > HIGH알람값, 5초 유지 시 발동
- Err5(TRANSDUCER): 역방향 전력 이상, 5초 유지 시 발동
- Err7(SENSOR OFF): 부팅 시 SENSOR OPEN → 즉시 (전원 재투입만 해제)
- Err8(SENSOR RUN): 운전 중 SENSOR OPEN → 즉시
- 릴레이 6개 구동: config.h의 RELAY_xxx 핀 사용
- Alarm_Process(): 메인 루프에서 주기 호출
- 에러 해제: MODE+DOWN 동시누름(Err1/2/5) 또는 전원사이클(Err7/8)
```

**검증:**

- [ ] SENSOR 단자 OPEN → Err7 즉시 발동 → LCD "Err7" → RELAY_OPERATE OPEN
- [ ] SENSOR OPEN 시 MODE+DOWN 해제 **불가** 확인 (전원 재투입만 가능)
- [ ] 출력 LOW 상태 5초 유지 → Err1 발동 → RELAY_LOW SHORT
- [ ] MODE+DOWN 동시누름 → Err1 해제 확인
- [ ] 8POWER BCD 입력 → Step 0~7 출력값 전환

**커밋:** `git commit -m "Phase 5: alarm Err1-Err8 + ext_control REMOTE/SENSOR/8POWER"`

---

## Step 6: Modbus RTU (Phase 7) — @protocol

```
📋 프롬프트 (3회 분할):

[1회차] modbus_crc.h/c
modbus_crc.c: CRC-16 다항식 0xA001, 256엔트리 테이블 룩업.
   알려진 벡터로 검증: {0x01, 0x04, 0x00, 0x01, 0x00, 0x01} → CRC=0xA060

[2회차] modbus_rtu.h/c
copilot-instructions.md, include/config.h를 읽어라.
skills/stm32g4-dev/references/hal-patterns.md의 "USART + RS-485" 패턴 참조.
- USART2 초기화 (9600/19200/38400 가변)
- RS-485 DE/RE(PC13/PC14) 제어: 아이들=수신, 송신 시 DE=HIGH
- RXNE 인터럽트 + IDLE 프레임 종료 감지
- TIM4 3.5T 타임아웃 (보조)
- 프레임 파싱: 주소 → FC → 데이터 → CRC 검증

[3회차] modbus_regs.h/c
docs/Modbus_레지스터맵.md를 정확히 따를 것.
docs/ARCHITECTURE.md의 modbus_regs↔megasonic_ctrl 계약 준수.
- FC 0x03: Read Holding Registers
- FC 0x06: Write Single Register
- FC 0x10: Write Multiple Registers
- 입력값 범위 검증 (SAFETY.md 6항)
- ⚠️ 모드별 접근 제한: NORMAL/REMOTE 모드에서 FC06/10 시 예외 응답 0x01
```

**검증:**

- [ ] PC → USB-RS485 → Modbus Poll: 레지스터 읽기(FC03) 성공
- [ ] EXT 모드: 원격 출력 변경(FC06) → LCD 값 반영 확인
- [ ] NORMAL 모드: 쓰기 시도 → 예외 응답 0x01 반환 확인
- [ ] 잘못된 레지스터 주소 → 예외 응답 0x02 확인
- [ ] CRC 오류 프레임 → 무응답(자동 버림) 확인

**커밋:** `git commit -m "Phase 6: Modbus RTU FC03/06/10 + mode access control"`

---

## Step 7: 통합 연동 (Phase 7)

```
📋 프롬프트:

copilot-instructions.md, docs/ARCHITECTURE.md를 읽어라.
모든 모듈의 .h 파일을 참조하여 main.c를 최종 통합해줘.

main.c 메인 루프 통합 순서 (ARCHITECTURE.md 데이터 흐름도 준수):
1. ADC_Control_Process()      — 전력/전류/전압 갱신
2. ExtCtrl_Process()          — REMOTE/SENSOR/8POWER 입력 처리
3. Alarm_Process()            — 알람 판정 + 릴레이 구동
4. Menu_Update()              — 버튼 이벤트 소비 + FSM 전이
5. MegasonicCtrl_Update()     — 소프트스타트/채널변경/정상운전 상태머신
6. LCD_Update()               — 값 변경 시에만 LCD 갱신
7. LED_Update()               — 모드별 LED 상태 갱신
8. Modbus_Process()           — 프레임 수신 시 처리

초기화 순서:
HAL_Init → SystemClock → GPIO → DWT → LCD → HRTIM DLL → ADC Cal →
MegasonicCtrl_Init → Alarm_Init → ExtCtrl_Init → Modbus_Init → 메인 루프

⚠️ 모든 모듈 간 데이터 교환은 getter/setter 함수로. 직접 extern 접근 최소화.
```

**검증 (통합 테스트):**

- [ ] 전원 ON → SENSOR SHORT → NORMAL 모드 진입 → LCD 정상 표시
- [ ] START → 소프트 스타트 → 발진 → LCD에 출력(W) 실시간 표시
- [ ] 모드 전환(MODE) → LED 변경 + LCD 화면 변경 정상
- [ ] Modbus 읽기 → 현재 출력값/채널/에러 정확히 반영
- [ ] SENSOR OPEN → 즉시 Err7 → 발진 정지 → 릴레이 동작 → LCD "Err7"
- [ ] 전원 재투입 → SENSOR SHORT → 정상 복귀

**커밋:** `git commit -m "Phase 7: full integration main.c + all module linkage"`

---

## Step 8: 양산 준비 (Phase 8)

```
📋 프롬프트:

copilot-instructions.md, docs/SAFETY.md를 읽어라.

[1회차] flash_params.h/c
파라미터 Flash 저장/로드: 내장 Flash 마지막 페이지 (4KB)
더블워드(8바이트) 단위 쓰기. 매직 넘버 검증으로 초기화 시 기본값 로드.
저장 항목: 채널별 주파수, 출력설정값, HIGH/LOW 알람값, 8POWER Step값, Modbus 주소/보레이트

[2회차] 워치독 + 인터럽트 정리
- IWDG 타임아웃 1초, 메인 루프에서만 리프레시
- stm32g4xx_it.c에 전체 인터럽트 우선순위 표 주석 정리:
  P0: HRTIM Fault (최우선, 하드웨어 비상 정지)
  P1: USART2 RX (Modbus 바이트 수신)
  P2: ADC DMA (전력 계측)
  P3: SysTick (1ms, 버튼 디바운스)
```

**검증:**

- [ ] 파라미터 변경 → 전원 OFF → ON → 설정값 유지
- [ ] 의도적 `while(1){}` → IWDG 리셋 → 자동 복귀
- [ ] BOOT0 HIGH → CubeProgrammer UART 플래싱 성공
- [ ] 24시간 연속 동작 → Fault 0건, 메모리 누수 없음

**커밋:** `git commit -m "Phase 8: Flash params + IWDG + interrupt priority final"`

---

## 에이전트별 역할 매핑

| Step | 핵심 에이전트                 | 보조 에이전트 | 담당 파일                             |
| ---- | ----------------------------- | ------------- | ------------------------------------- |
| 0    | (사용자)                      | —             | config.h, CubeIDE .ioc                |
| 1    | **hw-driver**                 | —             | system_clock, gpio_init, stm32g4xx_it |
| 2    | **ui**                        | —             | lcd1602, button, led_indicator, menu  |
| 3    | **hw-driver** + **megasonic** | —             | hrtim_pwm, megasonic_ctrl             |
| 4    | **hw-driver**                 | —             | adc_control                           |
| 5    | **megasonic**                 | hw-driver     | alarm, ext_control                    |
| 6    | **protocol**                  | —             | modbus_rtu, modbus_crc, modbus_regs   |
| 7    | (기본)                        | 전체          | main.c 통합                           |
| 8    | **hw-driver**                 | —             | flash_params, IWDG                    |

---

## 트러블슈팅 체크리스트

### 빌드 에러

| 증상                               | 원인                         | 해결                                              |
| ---------------------------------- | ---------------------------- | ------------------------------------------------- |
| `undefined reference to HAL_xxx`   | CubeMX 모드 미감지           | `cmake/stm32cubemx/CMakeLists.txt` 존재 여부 확인 |
| `multiple definition of`           | CubeMX 생성 파일과 src/ 중복 | CubeMX의 main.c 제거, src/main.c만 사용           |
| `region FLASH overflowed`          | 코드 크기 초과 (512KB)       | 최적화 -Os 적용, 미사용 HAL 모듈 제거             |
| `hard fault at startup`            | 스택 오버플로                | \_Min_Stack_Size 증가 (0x800→0x1000)              |
| `implicit declaration of function` | 헤더 include 누락            | config.h 경로 및 include 구조 확인                |

### 런타임 에러

| 증상                  | 원인                            | 해결                                          |
| --------------------- | ------------------------------- | --------------------------------------------- |
| HRTIM PWM 출력 안 됨  | DLL 캘리브레이션 미완료         | `DLLCalibrationStart()` + `Poll` 호출 확인    |
| HRTIM 주파수 부정확   | DLL 미완료 → ×1 모드            | DLL 교정 상태 + Period 유효 범위 확인         |
| LCD 아무것도 안 보임  | V0 대비/초기화 타이밍/핀 재배치 | 가변저항 + PB12/PB13/PB15 핀 확인             |
| Modbus 무응답         | DE/RE 핀 상태                   | 아이들 시 DE=LOW, /RE=LOW (PC13/PC14)         |
| ADC 값 이상           | 캘리브레이션 누락               | `Calibration_Start()` 호출 필수               |
| Err7이 해제 안 됨     | 의도된 동작                     | SENSOR SHORT 확인 후 전원 재투입만 가능       |
| 8POWER 선택 안 됨     | REMOTE OPEN                     | REMOTE(PC6) SHORT 먼저 확인                   |
| 채널 변경 시 이상     | 1초 정지 미적용                 | 정지 → 1초 대기 → Period 로드 → 소프트 스타트 |
| Flash 저장 후 값 깨짐 | 8바이트 정렬 미준수             | 주소 & 매직넘버 확인, 더블워드 정렬           |

### 에이전트 문제

| 증상                            | 원인                       | 해결                                         |
| ------------------------------- | -------------------------- | -------------------------------------------- |
| config.h 핀 정의와 다른 핀 사용 | 컨텍스트에 config.h 미포함 | 요청에 "include/config.h를 참조"를 명시      |
| 모듈 간 API 불일치              | ARCHITECTURE.md 미참조     | 요청에 "ARCHITECTURE.md 계약을 따를 것" 추가 |
| 안전 규칙 위반 (클램핑 빠짐)    | SAFETY.md 미참조           | 출력 관련 코드 시 반드시 SAFETY.md 포함 지시 |
| 이전 파일과 타입 불일치         | 이전 .h 파일 미전달        | 의존하는 .h 파일을 컨텍스트에 포함           |

---

## 최종 체크리스트 (양산 전)

- [ ] 모든 Phase 상태 ✅ (PLANS.md 기준, git log로 확인)
- [ ] 오실로스코프 파형: Ch0(500kHz), Ch2(1MHz), Ch6(2MHz) 3점 캡처
- [ ] 데드타임 5~20ns 범위 확인
- [ ] 알람 릴레이 6개 동작 확인 (Err1, Err2, Err5 각각)
- [ ] 8POWER BCD 8단계(Step0~7) 전환 확인
- [ ] Modbus Poll: FC03 전체 레지스터 읽기 + FC06 쓰기 + 모드별 접근 제한
- [ ] Flash 파라미터 저장 → 전원 사이클 → 복원 확인
- [ ] IWDG 동작 확인 (의도적 행 → 자동 리셋)
- [ ] BOOT0 점퍼 → UART 플래싱 성공
- [ ] 24시간 연속 동작 → Fault 0건
- [ ] 스택/힙 사용량 Build Analyzer 확인 (SRAM 128KB 내)
