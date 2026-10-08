# 실전 워크플로우 가이드 (Practical Workflow)

> CubeMX 핀 설정 완료 후, AI 에이전트에게 코딩을 요청하는 **단계별 실전 절차서**.
> 한 번에 전부 시키지 말고, Phase별로 빌드 성공을 확인하며 진행한다.

---

## 전체 흐름 요약

```text
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
- `docs/회로설계_참조.md` 및 `.github/copilot-instructions.md`의 CBT6 핀맵과 실제 회로가 일치하는지 대조

### 0-2. CubeIDE 핀 설정

1. STM32CubeIDE에서 **STM32G474CBT6 (LQFP48)** 칩 선택
2. `.ioc` 파일에서 핀 할당 (`copilot-instructions.md` 핀맵 참조):

   | 기능 그룹       | 핀        | CubeIDE 설정           |
   | --------------- | --------- | ---------------------- |
   | 풀브리지 입력   | PA8 / PA9 | HRTIM1_CHA1 / CHA2     |
   | 드라이버 Enable | PA15      | GPIO_Output (SONIC_ON) |
   | LCD (6핀)       | PB10~PB15 | GPIO_Output            |
   | 버튼 (4ea)      | PB6~PB9   | GPIO_Input (Pull-up)   |
   | ADC 검출        | PA0 / PA1 | ADC1_IN1 / IN2         |
   | CT 전류 입력    | PA6       | ADC2_IN3               |
   | RS485 TX/RX     | PA2/PA3   | USART2_TX/RX           |
   | RS485 DE / /RE  | PB4 / PB5 | GPIO_Output            |
   | PWM 조정 VR     | PB1       | ADC1_IN12 (PWM_VR)     |
   | PWM 파형 출력   | PA7       | TIM3_CH2 (PWM_OUTPUT)  |
   | RS485 종단/감지 | PB2 / PB3 | GPIO_Output / Input    |
   | SWD             | PA13/PA14 | SYS_JTMS/SYS_JTCK      |

3. **Project Manager → Toolchain: CMake** 선택
4. **Generate Code** → 생성된 파일을 `cmake/stm32cubemx/`에 복사

### 0-3. 빌드 환경 확인

```powershell
# 프로젝트 루트에서
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

> CubeMX 모드 감지 메시지: `-- [MODE] CubeMX 자동 생성 구성 사용` 확인

### 0-4. VS Code 빌드·디버그·업로드

STM32CubeCLT와 STM32CubeG4 펌웨어 팩이 설치된 Windows 개발 PC에서는 프로젝트 폴더
(`us_main_board_st`)를 VS Code로 연다. `.vscode/`와 `CMakePresets.json`이 Debug/Release
컴파일, Cortex-Debug, ST-LINK 업로드 작업을 제공한다.

- **Debug 빌드**: `Ctrl+Shift+B` 또는 `Build Debug (STM32)` 작업
- **Release 빌드**: `Build Release (STM32)` 작업
- **호스트 시험**: `Run Host Tests` 작업
- **디버그 및 프로그램 쓰기**: 실행 및 디버그에서 `Debug STM32G474 (ST-LINK)` 선택
  (Debug 빌드를 마친 뒤 ST-LINK GDB 서버로 연결하고 `main`에서 멈춤)
- **현장 업데이트**: `Upload UPDATE HEX (ST-LINK; stop output first)` 작업
  (Release 빌드 후 UPDATE HEX 쓰기, Verify, Reset 실행)

디버그/업로드 전에 초음파 출력을 정지하고 ST-LINK의 SWDIO, SWCLK, GND, NRST를
연결한다. 업로드 작업은 설정 보존용 UPDATE HEX를 사용하며 mass erase를 실행하지 않는다.

명령줄에서 직접 실행하려면 다음 preset을 사용한다.

```powershell
cmake --preset vscode-debug
cmake --build --preset vscode-debug

cmake --preset vscode-release
cmake --build --preset vscode-release
cmake --build --preset vscode-release --target test_host
```

---

## Step 1 ~ 8: 에이전트 코딩 요청

### 공통 규칙

1. **매 요청 시작에 반드시 포함할 문장:**

   ```text
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

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
PLANS.md Phase 1을 참조하여 system_clock.c와 gpio_init.c를 구현해줘.
stm32g4xx_it.c에 SysTick_Handler도 포함해줘.
```

**검증:**

- [ ] 빌드 성공
- [ ] SWD로 초기 펌웨어 다운로드 → HSE 8MHz→PLL→170MHz 클럭 정상
- [ ] `HAL_Delay(1000)` 정확히 1초

---

### Step 2: LCD + 버튼 (PLANS.md Phase 2)

```text
📋 프롬프트 예시 (2회 분할):

[1회차]
copilot-instructions.md, docs/ARCHITECTURE.md를 먼저 읽어라.
lcd1602_hw.c와 lcd1602.c를 구현해줘.
클럭 독립 bounded NOP 마이크로초 지연과 HD44780 4비트 초기화 시퀀스를 정확히 구현할 것.

[2회차]
이전에 구현한 lcd1602.h의 API를 참조하여
button.c, menu.c, menu_screen.c를 구현해줘.
메뉴 구조는 copilot-instructions.md의 "메뉴 시스템" 섹션을 따를 것.
```

**검증:**

- [ ] LCD "Hello World" 표시
- [ ] 버튼 4개 각각 이벤트 감지
- [ ] MODE→UP→DOWN→START_STOP 네비게이션 동작

---

### Step 3: 초음파 PWM (PLANS.md Phase 3)

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
ultrasonic_pwm.c를 구현해줘.
PA8/PA9 HRTIM Timer A를 50% 듀티, 180도 역위상으로 설정하고 PA15/SONIC_ON과 함께 출력 제어할 것.
IR2104 내부 deadtime은 앞단 IRF7351 레그를 보호한다. TR1 뒤 IRFP460용으로
HRTIM TA1/TA2 dead-time은 중심주파수 72.0 kHz 미만에서 1.4 us, 72.0 kHz
이상에서 0.8 us를 적용한다. Start 전에 한 번만 선택하며 sweep 순간주파수에는
바꾸지 않는다. TR1 47:42 및 IRFP460 Gate-Source 부하 상태에서 확인할 것.

그 다음 ultrasonic_ctrl.c를 구현해줘.
소프트 스타트(1500ms), Start/Stop 토글, 비상 정지를 포함할 것.
```

**검증:**

- [ ] 오실로스코프로 PA8/PA9 파형 확인 (50%, 180도 역위상)
- [ ] PA15/SONIC_ON 정지 시 IR2104 SD가 LOW인지 확인
- [ ] 메뉴에서 주파수 변경 → 실시간 파형 변화
- [ ] START_STOP → Timer Start/Stop, Auto-Tuning 화면에서 3초 유지 → 탐색 시작
- [ ] PA11/RUN_SW 토글 → 출력 정지, Local Lock 해제 및 모드 전환
- [ ] `python tests/test_logic.py` — HRTIM PER/CT 정규화 계산 32개 테스트 통과

---

### Step 4: CT ADC 전류 검출 (PLANS.md Phase 4)

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md를 먼저 읽어라.
adc_control.c를 구현해줘.
ADC2 PA6/IN3 단일엔드 캘리브레이션 → 이동 평균 필터 → 0~1000 정규화값을 구현할 것.
CT 값은 전류 측정/보호용이며 LCD PL 또는 PA7 PWM 듀티를 직접 변경하지 않을 것.
ADC1 PB1/IN12 PWM_VR은 별도 필터링하여 Local LCD PL 0~100% 표시와
0~90% Local Duty에 사용할 것. PLC Lock 중에도 PB1 이동이 감지되면
last-command-wins 방식으로 실제 출력에 반영할 것.
```

**검증:**

- [ ] CT 입력 변화에 따라 정규화 전류값 변화
- [ ] ADC 값 안정 (노이즈 ±2 이내)

---

### Step 5: Modbus RTU (PLANS.md Phase 6)

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/Modbus_레지스터맵.md, docs/ARCHITECTURE.md를 먼저 읽어라.
modbus_rtu.c, modbus_crc.c, modbus_regs.c를 구현해줘.
USART2 + TIM4 t1.5/t3.5 타임아웃, RS485 DE/RE 제어, FC 03/06/10 지원.
레지스터 맵은 Modbus_레지스터맵.md를 정확히 따를 것.
Slave ID/Baud/RTERM/PL 범위는 Supervisor에서 저장하고 Parity는 8-E-1로 고정한다. PLC에서는 통신 설정을 읽기 전용으로 유지할 것.
```

**검증:**

- [ ] PC에서 Modbus Poll 툴로 레지스터 읽기(FC03) 성공
- [ ] Frequency/Duty 직접 Write가 0x02로 거부되는지 확인
- [ ] 보드 감지 + RUN_SW OFF + MAIN → Local Lock/485 COMM → Output → Run 성공
- [ ] 통신 OFF 화면에서 PL/주파수 숨김, ON 화면에서만 표시
- [ ] RUN OFF 후 Lock/485 COMM 유지, 주소 5 재 ON으로 즉시 재운전
- [ ] RS485 보드 분리 → 출력 Stop 및 Lock 자동 해제
- [ ] Boolean 0x0000/0xFF00 이외 값이 0x03으로 거부되는지 확인
- [ ] 다중 쓰기(FC10) 성공 및 중간 값 오류 시 전체 미적용
- [ ] 잘못된 주소 → 예외 응답(0x02) 확인
- [ ] 잘못된 값 → 예외 응답(0x03) 확인
- [ ] Supervisor에서 ID/Baud/RTERM/PL 변경 → 전원 재인가 후 유지
- [ ] 9600/19200/38400/57600/115200 RTU 타이밍 확인

---

### Step 6: PLL 위상 검출 (핀 재배선 후, PLANS.md Phase 5)

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/ARCHITECTURE.md, docs/SAFETY.md를 먼저 읽어라.
COMP1/COMP2 + DAC3 + TIM2 듀얼 입력 캡처를 구현해줘.
현재 PA7은 TIM3_CH2/PWM_OUTPUT으로 예약되어 COMP2_INP와 동시 사용 불가다.
COMP2 전류 zero-cross 입력 핀이 별도로 재배선된 뒤에만 다음을 구현할 것.
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

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/SAFETY.md를 먼저 읽어라.
ultrasonic_ctrl.c에 정전류 제어 로직을 추가해줘.
PB1 PWM_VR 로컬 상한과 ADC2(PA6) CT 정규화값을 구분하고, 정전류 보정 결과를 TIM3_CH2(PA7) 듀티에 반영.
상태 출력(PC13 GOING, PC14 END_BZ, PA5 BZ_OUT)과 외부 스위치(PA10 REMOTE, PA11 RUN_SW, PA12 SWEEP_SW)도 포함. PC15는 SPARE 입력으로 유지.
```

**검증:**

- [ ] 부하 변동 시 전류 자동 보정 (오실로스코프 확인)
- [ ] 상태 출력 및 부저 드라이버 입력 상태 정상
- [ ] Remote(PA10) 스위치로 외부 ON/OFF

---

### Step 8: 통합 + 양산 준비 (PLANS.md Phase 8)

```text
📋 프롬프트 예시:

copilot-instructions.md, docs/SAFETY.md를 먼저 읽어라.
1) 통신 외 파라미터 Flash 저장/로드 확장 (통신 설정은 settings_storage.c에 구현됨)
2) IWDG 워치독 설정 (타임아웃 2초)
3) selftest.c의 SelfTest_RunAll()을 메인 루프에 통합 (이미 되어 있으면 확인만)
4) 전체 인터럽트 우선순위 표를 stm32g4xx_it.c 주석에 정리
```

**검증:**

- [ ] 전원 OFF→ON 후 설정값 유지
- [ ] Bank 1 Application 62 KB와 Settings 마지막 2 KB page가 Linker map에서 중첩되지 않음
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
| `region FLASH overflowed`        | 코드 크기 초과 (128KB)       | 최적화 -Os 적용, 미사용 HAL 모듈 제거             |
| `LOAD segment with RWX permissions` | Flash 초기화 배열의 쓰기 플래그 | 링커 스크립트의 Flash 초기화 배열을 `READONLY`로 선언 |
| `hard fault at startup`          | 스택 오버플로                | 링커 스크립트 \_Min_Stack_Size 증가 (0x800 이상)  |

### 런타임 에러

| 증상                   | 원인                              | 해결                                                              |
| ---------------------- | --------------------------------- | ----------------------------------------------------------------- |
| HRTIM 출력 안 됨       | Timer A 또는 출력 Enable 미설정   | PA8/PA9 AF13, Timer A count, TA1/TA2 output, PA15를 순서대로 확인 |
| LCD 아무것도 안 보임   | V0 대비 미조절 또는 초기화 타이밍 | 가변저항 조절, 15ms→4.1ms→100µs 준수                              |
| Modbus 무응답          | 배선/설정/DE·RE 상태              | A/B, ID/Baud/Parity, 아이들 시 DE=LOW와 /RE=LOW 확인              |
| 설정 변경 후 무응답    | PLC가 이전 통신값 사용            | LCD 값에 맞춰 PLC Port를 갱신(None은 8N2)                         |
| ADC 값 이상            | 캘리브레이션 누락                 | `HAL_ADCEx_Calibration_Start()` 호출 필수                         |
| SelfTest 주파수 불일치 | HRTIM PER 정수 양자화 오차        | 15~128kHz 범위에서 실제 주파수 오차 100Hz 이내 확인               |

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
- [ ] Modbus Poll로 구현 레지스터 `0x0000~0x000E`, `0x0010~0x0012` 검증
- [ ] 상태 전용 주소 `0x0009~0x000B` 쓰기가 Exception 0x02인지 확인
- [ ] 오실로스코프 파형 캡처 (주파수별 3점: 15kHz, 28kHz, 128kHz)
- [ ] 24시간 연속 동작 → Fault 0건
- [ ] SWD(ST-Link) 플래싱 및 NRST 연결 확인
- [ ] Flash 파라미터 저장 → 전원 사이클 후 복원 확인
- [ ] IWDG 동작 확인 (의도적 행 → 자동 리셋)
- [ ] 진동자 부하 연결 → 공진 추적(PLL) 정상 동작
