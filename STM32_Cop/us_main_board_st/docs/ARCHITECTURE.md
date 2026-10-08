# 아키텍처 명세서 (Architecture Specification)

> 모듈 간 인터페이스, 데이터 흐름, 설계 결정 근거를 명시한다.
> 어떤 에이전트든 이 문서를 읽으면 **동일한 구조와 호출 패턴**으로 코드를 작성해야 한다.

---

## 데이터 흐름 다이어그램

```
┌──────────── SysTick ISR (1ms) ─────────────┐
│  HAL_IncTick()                              │
│  매 10ms: Button_Process() → 이벤트 큐 적재 │
└─────────────────────────────────────────────┘
                    ↓ (이벤트 큐)
┌──────────── 메인 루프 (while 1) ────────────────────────────────┐
│                                                                  │
│  1. ADC_Control_Process()                                        │
│     ├→ PB1/ADC1_IN12 PWM_VR 이동평균 및 0~1000 정규화           │
│     ├→ PA6/ADC2_IN3 4kSPS DMA, offset 제거 200ms RMS/0.01A      │
│     └→ PB1 값을 Local Duty(0~900)와 PLC 인계값으로 전달          │
│                                                                  │
│  2. ResonanceTuning_Process()                                    │
│     ├→ PA0(COMP3 전압)/PA1(COMP1 전류) HRTIM capture 위상차     │
│     └→ PA6 최소점 러프 scan → V/I 위상차 정밀 scan             │
│                                                                  │
│  3. Menu_Update()                                                │
│     ├→ Button_GetEvent() 로 버튼 이벤트 소비                     │
│     ├→ 메뉴 FSM 상태 전이                                       │
│     └→ UltrasonicCtrl_SetFrequency() / SetDuty() 호출           │
│                                                                  │
│  4. MenuScreen_Refresh()                                         │
│     ├→ Menu_GetState() 로 현재 메뉴 확인                         │
│     ├→ g_us_state에서 실시간 값 읽기                             │
│     └→ LCD_SetCursor() + LCD_WriteString() 호출                  │
│                                                                  │
│  5. UltrasonicCtrl_Update()                                      │
│     ├→ 소프트 스타트: current_duty 점진 증가                     │
│     ├→ 동작 시간: remaining_time_ms 카운트다운 및 만료 정지      │
│     ├→ 펄스 모드: ON/OFF 타이밍 관리                             │
│     ├→ 스윕 외 모드와 듀티 관리                                 │
│     └→ UltrasonicPWM_SetFrequency() / SetDuty() 호출            │
│                                                                  │
│  6. Modbus_Process()                                             │
│     ├→ 프레임 수신 완료 시 파싱                                  │
│     ├→ ModbusRegs_Read/Write() 호출                              │
│     └→ 레지스터 쓰기 → UltrasonicCtrl_SetXxx() 연동             │
│                                                                  │
│  7. Buzzer_Process()                                             │
│     └→ 버튼 요청 시 PA5/TIM2_CH1 2.7 kHz를 70 ms 출력           │
└──────────────────────────────────────────────────────────────────┘

┌──────────── TIM7 ISR (10 kHz, Sweep 운전 중) ───────────────────┐
│  32-bit phase accumulator → Hz 단위 삼각 Sweep                  │
│  Update disable로 PER/CMP1 preload 원자적 기록                  │
│  다음 HRTIM repetition(주기 경계)에서 동시 갱신                 │
│  PA8(TA1)/PA9(TA2): 설정 주파수, 50% 상보(역위상) 출력          │
└──────────────────────────────────────────────────────────────────┘
```

---

## 모듈 인터페이스 계약 (Interface Contracts)

### 단위 규약 (모든 모듈 공통)

| 물리량 | 코드 단위                       | 예시           | 변환                                             |
| ------ | ------------------------------- | -------------- | ------------------------------------------------ |
| 주파수 | ×0.1 kHz (uint16_t)             | 280 = 28.0 kHz | `freq_hz = freq_01khz * 100`                     |
| 듀티비 | ×0.1 % (uint16_t)               | 450 = 45.0%    | `duty_pct = duty_01pct / 10.0`                   |
| 전류   | ×0.01 A (uint16_t)              | 400 = 4.00 A   | `current_a = val * 0.01`                         |
| 온도   | ×0.1 °C (uint16_t)              | 253 = 25.3°C   | `temp_c = val * 0.1`                             |
| 시간   | ms (`uint16_t` 또는 `uint32_t`) | 1000 = 1초     | 펄스/스윕은 `uint16_t`, 동작 타이머는 `uint32_t` |

> **이 단위 규약은 절대 변경 금지.** Modbus 레지스터, 메뉴 표시, 내부 변수 모두 동일 단위를 사용한다.

### ultrasonic_pwm ↔ ultrasonic_ctrl 계약

```
호출 방향: ultrasonic_ctrl → ultrasonic_pwm (단방향)
역방향 호출 금지: ultrasonic_pwm은 ultrasonic_ctrl을 #include하지 않는다

ultrasonic_ctrl이 호출하는 함수:
  UltrasonicPWM_SetFrequency(uint16_t freq_01khz)
    - Timer A MUL4 counter=680 MHz
    - 내부에서 period 계산: period = HRTIM_COUNTER_CLOCK_HZ / (freq_01khz * 100)
    - 범위 검증: FREQ_MIN ≤ freq_01khz ≤ FREQ_MAX (위반 시 클램핑)
    - HRTIM Timer A PER/CMP1을 갱신하여 PA8/PA9에 50% 역위상 반영

  UltrasonicPWM_SetFrequencyHzFast(uint32_t frequency_hz)
    - TIM7 10 kHz Sweep ISR 전용 Hz 단위 경로
    - PER/CMP1 preload 기록 중 update를 잠근 뒤 주기 경계에서 동시 반영
    - 출력 중 software update 금지: 진행 중인 PWM 주기를 자르지 않음

  UltrasonicPWM_SetDuty(uint16_t duty_01pct)
    - 내부에서 CCR 계산: ccr = (arr * duty_01pct) / 1000
    - 안전 클램핑: ccr ≤ (arr * DUTY_CLAMP_MAX) / 1000
    - 반환값 없음, 즉시 TIM1->CCR1 반영

  UltrasonicPWM_Start() / Stop()
    - Start: HRTIM Timer A count 및 TA1/TA2 output enable
    - Stop: TA1/TA2 output disable 후 Timer A count 정지

  UltrasonicPWM_GetARR()
    - 현재 HRTIM Timer A period 값 반환 (검증용)

Dead-time 계약:
  PA8/PA9는 MOSFET gate 상보쌍이 아니라 두 IR2104의 IN 입력이다.
  각 IR2104 내부 고정 dead-time(약 0.5 us급)은 앞단 IRF7351 레그를 보호한다.
  TR1 뒤 IRFP460 보호를 위해 HRTIM TA1/TA2에는 중심주파수 대역별
  positive dead-time을 적용한다. 72.0 kHz 미만은 1.4 us(238 count),
  72.0 kHz 이상은 0.8 us(136 count)이며, 이 구간에는 두 출력이 모두
  LOW가 된다. Start 전에 중심주파수로 선택하고 sweep 중에는 바꾸지 않는다.
  TA2는 TA1의 hardware complementary 출력이며 독립 SET/RESET source를 쓰지 않는다.
```

### menu ↔ ultrasonic_ctrl 계약

```
호출 방향: menu → ultrasonic_ctrl (단방향)
menu_screen → g_us_state 읽기 전용 (쓰기 금지)

menu가 호출하는 함수:
  UltrasonicCtrl_SetFrequency(freq)  — 28/40/68/80k+ 및 Manual 100Hz 변경
  UltrasonicCtrl_SetDuty(duty)       — 메뉴에서 듀티 변경 확정 시
  UltrasonicCtrl_Start() / Stop()    — 운전 및 Manual Tune 시험출력
  UltrasonicCtrl_SetMode(mode)       — 모드 메뉴에서 선택 확정 시
  UltrasonicCtrl_SetSweepParameters(center, width, rate)
                                     — ±100~1000Hz, 50~200Hz
  ResonanceTuning_Start(center)      — AUTO 화면 START 3초 후 시작
  UltrasonicCtrl_SetRunDurationMinutes(minutes)
                                     — 동작 시간(1~180분) 설정 시

menu_screen이 읽는 데이터:
  g_us_state.target_freq    — 화면 표시용 주파수
  g_us_state.current_duty   — 화면 표시용 듀티비
  g_us_state.running        — RUN/STOP 표시
  g_us_state.mode           — 모드 표시
  g_us_state.remaining_time_ms — 운전 중 남은 시간 표시

일반 MODE 화면:
  MAIN → TIME → MAIN

RUN_SW OFF + MODE+UP 2초 보호 설정 화면:
  FREQUENCY → SWEEP WIDTH → SWEEP SPEED → TUNE SELECT → 선택 TUNE →
  OUTPUT MODE → [CURRENT SET] → FREQUENCY 순환

보호 설정은 MODE+UP을 다시 2초 유지할 때만 MAIN으로 복귀한다.

Flash settings v3는 기존 통신/타이머 설정과 함께 선택 Band 및 네 Band별
중심주파수를 저장한다. v1/v2 Record도 읽으며 새 필드는 기본값으로 migration한다.
Manual Tune에서 MODE로 다음 화면을 이동하거나 시험출력을 STOP하면 저장한다.
시험출력에는 보호 메뉴에서 선택한 Sweep 폭과 속도를 강제로 적용하고,
Auto Tune 완료 결과도 현재 Band 중심주파수로 저장한다.
```

### modbus_regs ↔ ultrasonic_ctrl 계약

```
호출 방향: modbus_regs → ultrasonic_ctrl (단방향)

ModbusRegs_Write() 내부에서:
  REG 0x0001 쓰기 → 통신 절대 Output 0~500 설정
  REG 0x0004 True  → 보드 감지+RUN_SW OFF+MAIN 검증, 기존 출력 Stop 후 통신 준비
  REG 0x0004 False → PLC 쓰기 허가 및 PA10 REMOTE 선택 차단 해제
  REG 0x0005 쓰기 → Remote와 같은 무시간 Run, Stop 시 Local 자동 복귀
  REG 0x0007 읽기 → 물리 SWEEP_SW 선택 상태(정지 중에도 확인)
  REG 0x0008 True 쓰기 → Stop 상태에서 Error Reset

ModbusRegs_Read() 내부에서:
  REG 0x0000 읽기 → g_us_state.current_freq 반환
  REG 0x0001 읽기 → Output command 0~500 반환
  REG 0x0002 읽기 → 표시 출력 범위 100 반환
  REG 0x0003~0x0008 읽기 → Sweep/Local/Run/외부입력/Error 상태
  REG 0x000C~0x000E 읽기 → 설정 중심주파수/Sweep 폭/Sweep 속도
  REG 0x0010~0x0012 읽기 → 현재 Slave ID/Baud index/고정 Even 반환

Frequency는 Local 메뉴에서만 설정하며 PLC는 직접 쓰지 않는다. Local 출력은
PB1 Duty를 사용한다. Lock ON 순간 PB1 비율을 PLC Output에 인계하고 이후
PLC Write 또는 PB1 이동 중 마지막 입력값을 실제 출력으로 적용한다.
통신 설정 Register 0x0010~0x0012는 PLC Read-only다. 원격 Write는
Illegal Data Address(0x02)이며 변경은 보드 메뉴에서만 수행한다.
```

### modbus_rtu ↔ settings_storage 계약

```
호출 방향: modbus_rtu → settings_storage (단방향)

초기화:
  SettingsStorage_Load() 성공 → 저장된 ID/Baud index/전력범위/RTERM 적용
  실패 또는 유효 Record 없음 → 1 / 9600 / Even 기본값 적용

보드 메뉴 저장:
  SettingsStorage_Save() 성공 → RAM 설정 갱신 → USART2/TIM4 재설정
  저장 실패 → 기존 통신 설정 유지, LCD에 STOP/SAVE ERR 표시

Flash 배치:
  Application FLASH = 0x08000000~0x0800F7FF (Bank 1, 62 KB)
  SETTINGS          = 0x0800F800~0x0800FFFF (Bank 1 마지막 2 KB page)
  128 KB dual-bank의 Bank 2는 0x08040000부터 시작하므로 연속 영역으로 보지 않는다.
  v3 32-byte CRC Record를 순차 기록하고 page가 찬 경우에만 erase
  (기존 v1 16-byte, v2 24-byte Record 읽기 호환)
```

### adc_control → ultrasonic_ctrl 계약

```
간접 연동: adc_control은 ultrasonic_ctrl을 직접 호출하지 않는다
중개자: menu.c 또는 main.c가 ADC 값을 읽어 ultrasonic_ctrl에 전달

데이터 흐름:
  PB1/ADC1_IN12 PWM_VR
    → ADC_Control_GetPwmVrNormalized()
    → menu_screen.c의 실제 PA7 기준 PL 5~100% 표시

  PB1/ADC1_IN12 PWM_VR
    → ADC_Control_GetPwmVrDutyLimit()
    → main.c
    → UltrasonicCtrl_SetDuty()

  실제 내부 지령
    Local = PB1 Duty
    PLC   = 안전 Duty 상한 × Modbus OUTPUT_VALUE / 500

  제어권 전환
    사전 조건 → PB3 보드 감지, PA11 RUN_SW OFF, MAIN 화면
    Lock ON  → 기존 출력 Stop, 현재 PB1 비율 인계, 485 COMM 표시
    기본 RUN → 주소 1 쓰기를 생략하면 인계한 PB1 볼륨값으로 ON/OFF
    PLC 출력 → 필요할 때만 주소 1을 쓰고 RUN, PB1 5% 이동 시 Local 수동 인계
    통신 중  → PLC Write와 PB1 이동 중 마지막 입력값을 출력에 적용
    입력 정책 → PA10 REMOTE만 무시, PB1/설정 버튼은 유지
    Local 조작 → PA11 RUN_SW 상태 변경 시 Lock 해제 및 Local 전환
    Lock OFF → 운전 중에도 허용, 최신 PB1 Duty와 전체 Local 입력으로 전환
    RUN OFF  → 출력만 Stop, Lock과 485 COMM 준비 상태 유지
    보드 분리 → 출력 Stop 및 Lock 자동 해제

  PA6/ADC2_IN3 CT
    → TIM6 TRGO 4kSPS + DMA1_Channel1 circular
    → 200ms/800 sample 평균 offset 제거 + RMS
    → ADC_Control_GetCurrentCentiAmp() (500 = 5.00A)
    → 공진 러프 탐색, 전류 측정 및 향후 보호/정전류 제어

  PA0/COMP3 COMP_V + PA1/COMP1 COMP_I
    → 실측 ADC 중심값을 DAC3_CH1/DAC1_CH1 threshold로 설정
    → HRTIM EEV5/EEV4, Timer A CPT1/CPT2 hardware timestamp
    → ResonanceTuning_GetPhaseDifference() (-180.0~+180.0도)

  Auto-Tuning
    → 중심 ±1kHz에서 PA6 입력 소비전류 국부 최저점 러프 탐색
    → 후보 ±300Hz에서 PA0/PA1 위상차 0점 정밀 탐색
```

AUTO TUNE 화면에서 START 3초를 유지하면 메뉴가 제한된 시험출력을 시작하고,
소프트 스타트 완료 후 `ResonanceTuning_Start()`를 호출한다. 완료/오류/취소 시
시험출력은 정지한다. 완료 결과는 현재 Band의 중심주파수로 Flash v3에 저장한다.

---

## 설계 결정 기록 (ADR — Architecture Decision Records)

### ADR-001: Center-aligned PWM 선택

**결정**: TIM1을 Center-aligned mode 1로 사용
**대안**: Edge-aligned PWM
**근거**: 대칭 PWM은 EMI 고조파가 짝수 배에서 상쇄되어 방사 노이즈 감소. 초음파 출력의 스펙트럼 순도 향상.
**결과**: ARR 계산식이 `TIM_CLK / (2 × freq)` (÷2 추가)

### ADR-002: 주파수/듀티 단위 ×0.1 규약

**결정**: 모든 내부 변수와 Modbus 레지스터에서 주파수는 ×0.1kHz, 듀티는 ×0.1% 단위 사용
**대안**: Hz 단위, 퍼밀(‰) 단위
**근거**: uint16_t 하나로 15.0~128.0 kHz, 0.0~90.0% 명령 범위를 표현한다. Modbus 16비트 레지스터와 1:1 대응하며 부동소수점 연산을 피한다.
**결과**: 표시 시 10으로 나누어 소수점 1자리 표현

### ADR-003: 모듈 간 단방향 의존성

**결정**: 의존성은 항상 상위→하위 단방향. 하위 모듈이 상위 모듈을 #include하지 않음.
**대안**: 콜백 함수 등록
**근거**: 순환 의존 방지, 테스트 용이성, 빌드 순서 단순화.
**결과**:

- ultrasonic_ctrl → ultrasonic_pwm (O)
- ultrasonic_pwm → ultrasonic_ctrl (X)
- menu → ultrasonic_ctrl (O)
- ultrasonic_ctrl → menu (X)
- modbus_regs → ultrasonic_ctrl (O)

### ADR-004: ISR 최소화 원칙

**결정**: ISR에서는 플래그 설정과 버퍼 저장만 수행. 모든 로직은 메인 루프.
**대안**: ISR에서 직접 처리
**근거**: ISR 실행 시간 최소화 → 다른 인터럽트 지연 방지. 디버깅 용이성.
**결과**: `volatile` 공유 변수 + 메인 루프 폴링 패턴

### ADR-005: LCD 조건부 갱신

**결정**: LCD는 값이 실제로 변경되었을 때만 갱신. MenuScreen_Refresh()는 이전 값과 비교.
**대안**: 주기적 전체 갱신
**근거**: HD44780은 쓰기에 37μs가 소요. 매 루프 전체 갱신 시 메인 루프 주기 지연.
**결과**: menu_screen.c에 이전 표시값 캐시 변수 유지

### ADR-006: Modbus 통신 설정은 보드 Local 메뉴에서만 변경

**결정**: Slave ID와 Baud는 Supervisor에서 설정하고 Parity는 Even으로 고정한다. 통신 설정 Register는 PLC Read-only다.
**근거**: 원격 Write 응답 직후 통신 조건이 바뀌어 PLC가 장치를 잃는 상황과 현장 오설정을 방지
**결과**: 설정을 마지막 Flash page에 CRC/sequence record로 저장하고 저장 성공 후 즉시 UART에 적용

---

## 검증 포인트 (Verification Points)

아래 항목은 코드 변경 후 반드시 확인해야 하는 모듈 간 일관성 체크:

| #   | 검증 항목                                | 관련 모듈     | 자동화 가능         |
| --- | ---------------------------------------- | ------------- | ------------------- |
| V1  | 주파수 설정값 = HRTIM PER 역산값         | pwm ↔ ctrl    | ✅ 런타임 자기검증  |
| V2  | 듀티 설정값 = TIM3 CCR2 역산값           | pwm ↔ ctrl    | ✅ 런타임 자기검증  |
| V3  | LCD 표시 주파수 = g_us_state.target_freq | screen ↔ ctrl | ✅ 런타임 자기검증  |
| V4  | Modbus 0x0000 = g_us_state.current_freq  | regs ↔ ctrl   | ✅ 호스트 테스트    |
| V5  | 듀티 ≤ DUTY_CLAMP_MAX 항상 성립          | pwm + regs    | ✅ 단위 테스트      |
| V6  | FREQ_MIN ≤ 주파수 ≤ FREQ_MAX             | pwm + regs    | ✅ 단위 테스트      |
| V7  | 소프트 스타트 완료 시간 ≈ 1500ms         | ctrl          | ✅ 시뮬레이션       |
| V8  | CRC-16 알려진 벡터 일치                  | crc           | ✅ 단위 테스트      |
| V9  | 비상 정지 후 MOE=0, CCR1=0               | pwm + ctrl    | ✅ 런타임 자기검증  |
| V10 | 기본 통신값=ID 1/9600/Even               | rtu + params  | ✅ 정적 계약 테스트 |
| V11 | 0x0010~0x0012 PLC 쓰기 거부              | regs          | ✅ 정적 계약 테스트 |
| V12 | Application과 Settings Flash 미중첩      | linker        | ✅ Linker map 검사  |
| V13 | FREQ/DUTY 직접 PLC Write 주소 없음       | regs + docs   | ✅ 정적 계약 테스트 |
| V14 | 예비 Register가 정확히 3개               | regs + docs   | ✅ 정적 계약 테스트 |
| V15 | PB1/PWM_VR=ADC1_IN12, PA7/PWM_OUTPUT=TIM3_CH2 | ADC + PWM + ioc | ✅ 정적 계약 테스트 |
