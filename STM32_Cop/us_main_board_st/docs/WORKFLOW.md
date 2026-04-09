# 실전 워크플로우 가이드 (Practical Workflow)

> CubeMX 핀 설정 완료 후, AI 에이전트에게 코딩을 요청하는 **단계별 실전 절차서**.
> 한 번에 전부 시키지 말고, Phase별로 빌드 성공을 확인하며 진행한다.

---

## 전체 흐름 요약

```
[사용자 작업]                    [에이전트 작업]
    │                                │
    ▼                                │
1. 회로도 완성 (KiCad/Altium)       │
    │                                │
    ▼                                │
2. CubeIDE에서 핀 할당              │
   (.ioc → CMake 프로젝트 생성)     │
    │                                │
    ▼                                │
3. cmake/stm32cubemx/ 에 배치       │
    │                                │
    ▼                                ▼
4. ─────── Phase별 코딩 요청 ────────►
    │          빌드 확인 ◄────────────
    │                                │
    ▼                                ▼
5. ─────── 다음 Phase 요청 ──────────►
    │          테스트 실행 ◄──────────
    │                                │
    ▼                                │
6. 보드 플래싱 → 하드웨어 검증       │
```

---

## Step 0: 사전 준비 (사용자 작업)

### 0-1. 회로도 완성

- KiCad 또는 Altium에서 회로도 최종 확인
- `docs/STM32G474RC_핀맵.md` 및 `copilot-instructions.md`의 핀맵과 실제 회로가 일치하는지 대조

### 0-2. CubeIDE 핀 설정

1. STM32CubeIDE에서 **STM32G474RCT6** 칩 선택
2. `.ioc` 파일에서 핀 할당 (`copilot-instructions.md` 핀맵 참조):

   | 기능 그룹      | 핀                     | CubeIDE 설정         |
   | -------------- | ---------------------- | -------------------- |
   | 초음파 PWM     | PA8                    | TIM1_CH1             |
   | 상보 출력      | PB13                   | TIM1_CH1N            |
   | LCD (6핀)      | PB12,PB14,PB15,PC6~PC8 | GPIO_Output          |
   | 버튼 (4ea)     | PC0~PC3                | GPIO_Input (Pull-up) |
   | ADC 내장VR     | PA0                    | ADC1_IN1             |
   | ADC 외부VR     | PB11                   | ADC1_IN14            |
   | COMP1 전압위상 | PA1                    | COMP1_INP            |
   | COMP2 전류위상 | PA7                    | COMP2_INP            |
   | RS485 TX/RX    | PA2/PA3                | USART2_TX/RX         |
   | RS485 DE/RE    | PA5/PA6                | GPIO_Output          |
   | 위상제어 PWM   | PB0                    | TIM3_CH3             |
   | 디버그 UART    | PA9/PA10               | USART1_TX/RX         |
   | SWD            | PA13/PA14              | SYS_JTMS/SYS_JTCK    |

3. **Project Manager → Toolchain: CMake** 선택
4. **Generate Code** → 생성된 파일을 `cmake/stm32cubemx/`에 복사

### 0-3. 빌드 환경 확인

```powershell
# 프로젝트 루트에서
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

> CubeMX 모드 감지 메시지: `-- [MODE] CubeMX 자동 생성 구성 사용` 확인

---

## Step 1 ~ 8: 에이전트 코딩 요청

### 공통 규칙

1. **매 요청 시작에 반드시 포함할 문장:**

   ```
   copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
   ```

2. **한 번에 1~3개 파일만 요청** (컨텍스트 초과 방지)

3. **빌드 성공 확인 후** 다음 단계로 진행:

   ```powershell
   cmake --build build
   ```

4. **호스트 테스트 수시 실행** (로직 검증):
   ```powershell
   python tests/test_logic.py
   ```

---

### Step 1: 기반 동작 (PLANS.md Phase 1)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
PLANS.md Phase 1을 참조하여 system_clock.c와 gpio_init.c를 구현해줘.
stm32g4xx_it.c에 SysTick_Handler도 포함해줘.
```

**검증:**

- [ ] 빌드 성공
- [ ] LED(PC10) 500ms 토글 → HSI→PLL→170MHz 클럭 정상
- [ ] `HAL_Delay(1000)` 정확히 1초

---

### Step 2: LCD + 버튼 (PLANS.md Phase 2)

```
📋 프롬프트 예시 (2회 분할):

[1회차]
copilot-instructions.md, docs/ARCHITECTURE.md를 먼저 읽어라.
lcd1602_hw.c와 lcd1602.c를 구현해줘.
DWT 기반 마이크로초 지연과 HD44780 4비트 초기화 시퀀스를 정확히 구현할 것.

[2회차]
이전에 구현한 lcd1602.h의 API를 참조하여
button.c, menu.c, menu_screen.c를 구현해줘.
메뉴 구조는 copilot-instructions.md의 "메뉴 시스템" 섹션을 따를 것.
```

**검증:**

- [ ] LCD "Hello World" 표시
- [ ] 버튼 4개 각각 이벤트 감지
- [ ] MENU→UP→DOWN→OK 네비게이션 동작

---

### Step 3: 초음파 PWM (PLANS.md Phase 3)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
ultrasonic_pwm.c를 구현해줘.
반드시 SAFETY.md의 듀티 클램핑(90%), 데드타임 최소값, MOE 제어 규칙을 지킬 것.

그 다음 ultrasonic_ctrl.c를 구현해줘.
소프트 스타트(500ms), Start/Stop 토글, 비상 정지를 포함할 것.
```

**검증:**

- [ ] 오실로스코프로 PA8/PB13 파형 확인 (센터-얼라인드, 상보)
- [ ] 데드타임 300ns 확인
- [ ] 메뉴에서 주파수 변경 → 실시간 파형 변화
- [ ] BTN_OK → Start/Stop 토글
- [ ] `python tests/test_logic.py` — ARR/CCR 계산 32개 테스트 통과

---

### Step 4: ADC 가변저항 (PLANS.md Phase 4)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md를 먼저 읽어라.
adc_control.c를 구현해줘.
ADC1 캘리브레이션 → 스캔 모드(IN1 + IN14) → DMA → 이동 평균 필터.
가변저항 값을 0~1000 (×0.1%) 듀티로 매핑하고 데드존/히스테리시스 적용할 것.
```

**검증:**

- [ ] 가변저항 돌리면 LCD에 듀티비 변화 표시
- [ ] ADC 값 안정 (노이즈 ±2 이내)

---

### Step 5: Modbus RTU (PLANS.md Phase 6)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/Modbus_레지스터맵.md, docs/ARCHITECTURE.md를 먼저 읽어라.
modbus_rtu.c, modbus_crc.c, modbus_regs.c를 구현해줘.
USART2 + TIM4 3.5T 타임아웃, RS485 DE/RE 제어, FC 03/06/10 지원.
레지스터 맵은 Modbus_레지스터맵.md를 정확히 따를 것.
```

**검증:**

- [ ] PC에서 Modbus Poll 툴로 레지스터 읽기(FC03) 성공
- [ ] 원격 주파수/듀티 변경(FC06) 성공
- [ ] 잘못된 주소 → 예외 응답(0x02) 확인

---

### Step 6: PLL 위상 검출 (PLANS.md Phase 5)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
COMP1/COMP2 + DAC3 + TIM2 듀얼 입력 캡처를 구현해줘.
1) DAC3 CH1/CH2 → COMP1/COMP2 INM 기준전압 1.65V
2) COMP1 출력 → TIM2_IC1, COMP2 출력 → TIM2_IC2 (내부 라우팅)
3) 위상차(ns) 계산 → ultrasonic_ctrl.c의 Auto-Tuning 연동
```

> ⚠️ 이 단계는 하드웨어(CT 센서, 분압기)가 연결되어야 실질 검증 가능

**검증:**

- [ ] DAC3 출력값 확인 (내부 전용이므로 COMP 동작으로 간접 확인)
- [ ] COMP 출력 토글 확인 (테스트 신호 인가 시)
- [ ] 디버그 UART로 위상차(°) 출력

---

### Step 7: 정전류 + 외부 I/O (PLANS.md Phase 7)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/SAFETY.md를 먼저 읽어라.
ultrasonic_ctrl.c에 정전류 제어 로직을 추가해줘.
ADC2(PA4) 전류 센서값 → 목표 전류 비교 → TIM3_CH3(PB0) 듀티 조절.
릴레이(PA12 Fault, PA15 Running, PC5 End) 및 외부 스위치(PB1/PB2/PB10) 처리도 포함.
```

**검증:**

- [ ] 부하 변동 시 전류 자동 보정 (오실로스코프 확인)
- [ ] 릴레이 출력 상태 정상
- [ ] Remote(PB1) 스위치로 외부 ON/OFF

---

### Step 8: 통합 + 양산 준비 (PLANS.md Phase 8)

```
📋 프롬프트 예시:

copilot-instructions.md, docs/SAFETY.md를 먼저 읽어라.
1) 파라미터 Flash 저장/로드 (내장 Flash 페이지, 더블워드 쓰기)
2) IWDG 워치독 설정 (타임아웃 2초)
3) selftest.c의 SelfTest_RunAll()을 메인 루프에 통합 (이미 되어 있으면 확인만)
4) 전체 인터럽트 우선순위 표를 stm32g4xx_it.c 주석에 정리
```

**검증:**

- [ ] 전원 OFF→ON 후 설정값 유지
- [ ] 의도적 무한루프 → IWDG 리셋 확인
- [ ] `python tests/test_logic.py` — 전체 테스트 통과
- [ ] 24시간 연속 동작 이상 무

---

## 트러블슈팅 체크리스트

### 빌드 에러

| 증상                             | 원인                         | 해결                                              |
| -------------------------------- | ---------------------------- | ------------------------------------------------- |
| `undefined reference to HAL_xxx` | CubeMX 모드 미감지           | `cmake/stm32cubemx/CMakeLists.txt` 존재 여부 확인 |
| `multiple definition of`         | CubeMX 생성 파일과 src/ 중복 | CubeMX의 main.c 제거, src/main.c만 사용           |
| `region FLASH overflowed`        | 코드 크기 초과 (256KB)       | 최적화 -Os 적용, 미사용 HAL 모듈 제거             |
| `hard fault at startup`          | 스택 오버플로                | 링커 스크립트 \_Min_Stack_Size 증가 (0x800 이상)  |

### 런타임 에러

| 증상                   | 원인                              | 해결                                                       |
| ---------------------- | --------------------------------- | ---------------------------------------------------------- |
| TIM1 PWM 출력 안 됨    | MOE 비트 미설정                   | `HAL_TIM_PWM_Start()` + `HAL_TIMEx_PWMN_Start()` 호출 확인 |
| LCD 아무것도 안 보임   | V0 대비 미조절 또는 초기화 타이밍 | 가변저항 조절, 15ms→4.1ms→100µs 준수                       |
| Modbus 무응답          | DE/RE 핀 상태                     | 아이들 시 DE=LOW, /RE=LOW 확인                             |
| ADC 값 이상            | 캘리브레이션 누락                 | `HAL_ADCEx_Calibration_Start()` 호출 필수                  |
| SelfTest 주파수 불일치 | ARR 역산 절사 오차                | ±0.1kHz (±1 틱) 이내 → 정상                                |

---

## 에이전트별 역할 매핑

요청 내용에 따라 적합한 에이전트를 지정하면 더 정확한 결과를 얻는다.

| 요청 내용                            | 추천 에이전트   | 근거                    |
| ------------------------------------ | --------------- | ----------------------- |
| TIM, ADC, COMP, DAC, GPIO, 클럭 설정 | **hw-driver**   | 레지스터 레벨 지식 특화 |
| Modbus RTU, CRC, 레지스터 맵, RS485  | **protocol**    | 프로토콜 규격 준수 특화 |
| LCD, 메뉴, 버튼, 화면 렌더링         | **ui**          | FSM + HD44780 특화      |
| 소프트 스타트, PLL, 스윕, 정전류     | **ultrasonic**  | 초음파 제어 로직 특화   |
| 전체 구조 파악, 정합성 검토          | (기본 에이전트) | 크로스 모듈 리뷰        |

---

## 최종 체크리스트 (양산 전)

- [ ] 모든 Phase 상태 ✅ (PLANS.md 확인)
- [ ] `python tests/test_logic.py` — 32개 테스트 전부 통과
- [ ] SelfTest_RunAll() 200ms 주기 동작 → 불일치 0건
- [ ] Modbus Poll로 전체 레지스터 읽기/쓰기 검증
- [ ] 오실로스코프 파형 캡처 (주파수별 3점: 20kHz, 28kHz, 50kHz)
- [ ] 24시간 연속 동작 → Fault 0건
- [ ] BOOT0 점퍼 → UART 플래싱 성공 확인
- [ ] Flash 파라미터 저장 → 전원 사이클 후 복원 확인
- [ ] IWDG 동작 확인 (의도적 행 → 자동 리셋)
- [ ] 진동자 부하 연결 → 공진 추적(PLL) 정상 동작
