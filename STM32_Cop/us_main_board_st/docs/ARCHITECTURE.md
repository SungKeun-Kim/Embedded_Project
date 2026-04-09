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
│     └→ ADC DMA 값 필터링 → s_duty_mapped (0~1000) 업데이트       │
│                                                                  │
│  2. Menu_Update()                                                │
│     ├→ Button_GetEvent() 로 버튼 이벤트 소비                     │
│     ├→ 메뉴 FSM 상태 전이                                       │
│     └→ UltrasonicCtrl_SetFrequency() / SetDuty() 호출           │
│                                                                  │
│  3. MenuScreen_Refresh()                                         │
│     ├→ Menu_GetState() 로 현재 메뉴 확인                         │
│     ├→ g_us_state에서 실시간 값 읽기                             │
│     └→ LCD_SetCursor() + LCD_WriteString() 호출                  │
│                                                                  │
│  4. UltrasonicCtrl_Update()                                      │
│     ├→ 소프트 스타트: current_duty 점진 증가                     │
│     ├→ 펄스 모드: ON/OFF 타이밍 관리                             │
│     ├→ 스윕 모드: 주파수 증감 관리                               │
│     └→ UltrasonicPWM_SetFrequency() / SetDuty() 호출            │
│                                                                  │
│  5. Modbus_Process()                                             │
│     ├→ 프레임 수신 완료 시 파싱                                  │
│     ├→ ModbusRegs_Read/Write() 호출                              │
│     └→ 레지스터 쓰기 → UltrasonicCtrl_SetXxx() 연동             │
└──────────────────────────────────────────────────────────────────┘
```

---

## 모듈 인터페이스 계약 (Interface Contracts)

### 단위 규약 (모든 모듈 공통)

| 물리량 | 코드 단위           | 예시           | 변환                           |
| ------ | ------------------- | -------------- | ------------------------------ |
| 주파수 | ×0.1 kHz (uint16_t) | 280 = 28.0 kHz | `freq_hz = freq_01khz * 100`   |
| 듀티비 | ×0.1 % (uint16_t)   | 450 = 45.0%    | `duty_pct = duty_01pct / 10.0` |
| 전류   | ×0.01 A (uint16_t)  | 400 = 4.00 A   | `current_a = val * 0.01`       |
| 온도   | ×0.1 °C (uint16_t)  | 253 = 25.3°C   | `temp_c = val * 0.1`           |
| 시간   | ms (uint16_t)       | 1000 = 1초     | 직접 사용                      |

> **이 단위 규약은 절대 변경 금지.** Modbus 레지스터, 메뉴 표시, 내부 변수 모두 동일 단위를 사용한다.

### ultrasonic_pwm ↔ ultrasonic_ctrl 계약

```
호출 방향: ultrasonic_ctrl → ultrasonic_pwm (단방향)
역방향 호출 금지: ultrasonic_pwm은 ultrasonic_ctrl을 #include하지 않는다

ultrasonic_ctrl이 호출하는 함수:
  UltrasonicPWM_SetFrequency(uint16_t freq_01khz)
    - 내부에서 ARR 계산: arr = TIM1_CLOCK_HZ / (2 * freq_01khz * 100) - 1
    - 범위 검증: FREQ_MIN ≤ freq_01khz ≤ FREQ_MAX (위반 시 클램핑)
    - 반환값 없음, 즉시 TIM1->ARR 반영

  UltrasonicPWM_SetDuty(uint16_t duty_01pct)
    - 내부에서 CCR 계산: ccr = (arr * duty_01pct) / 1000
    - 안전 클램핑: ccr ≤ (arr * DUTY_CLAMP_MAX) / 1000
    - 반환값 없음, 즉시 TIM1->CCR1 반영

  UltrasonicPWM_Start() / Stop()
    - Start: HAL_TIM_PWM_Start + HAL_TIMEx_PWMN_Start (MOE 자동 SET)
    - Stop: __HAL_TIM_MOE_DISABLE → CCR1=0

  UltrasonicPWM_GetARR()
    - 현재 TIM1->ARR 값 반환 (검증용)
```

### menu ↔ ultrasonic_ctrl 계약

```
호출 방향: menu → ultrasonic_ctrl (단방향)
menu_screen → g_us_state 읽기 전용 (쓰기 금지)

menu가 호출하는 함수:
  UltrasonicCtrl_SetFrequency(freq)  — 메뉴에서 주파수 변경 확정 시
  UltrasonicCtrl_SetDuty(duty)       — 메뉴에서 듀티 변경 확정 시
  UltrasonicCtrl_Start() / Stop()    — BTN_OK 토글 시
  UltrasonicCtrl_SetMode(mode)       — 모드 메뉴에서 선택 확정 시

menu_screen이 읽는 데이터:
  g_us_state.target_freq    — 화면 표시용 주파수
  g_us_state.current_duty   — 화면 표시용 듀티비
  g_us_state.running        — RUN/STOP 표시
  g_us_state.mode           — 모드 표시
```

### modbus_regs ↔ ultrasonic_ctrl 계약

```
호출 방향: modbus_regs → ultrasonic_ctrl (단방향)

ModbusRegs_Write() 내부에서:
  REG 0x0000 쓰기 → value==1 ? UltrasonicCtrl_Start() : Stop()
  REG 0x0001 쓰기 → UltrasonicCtrl_SetFrequency(value)
  REG 0x0002 쓰기 → UltrasonicCtrl_SetDuty(value)
  REG 0x0006 쓰기 → UltrasonicCtrl_SetMode(value)

ModbusRegs_Read() 내부에서:
  REG 0x0003 읽기 → g_us_state.target_freq 반환
  REG 0x0004 읽기 → g_us_state.current_duty 반환
  REG 0x0005 읽기 → UltrasonicCtrl_GetStatusFlags() 반환
```

### adc_control → ultrasonic_ctrl 계약

```
간접 연동: adc_control은 ultrasonic_ctrl을 직접 호출하지 않는다
중개자: menu.c 또는 main.c가 ADC 값을 읽어 ultrasonic_ctrl에 전달

데이터 흐름:
  ADC_Control_GetDutyMapped()  →  menu.c 또는 main.c  →  UltrasonicCtrl_SetDuty()
```

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
**근거**: uint16_t 하나로 20.0~168.0 kHz, 0.0~100.0% 표현 가능. Modbus 16비트 레지스터와 1:1 대응. 부동소수점 연산 회피.
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

---

## 검증 포인트 (Verification Points)

아래 항목은 코드 변경 후 반드시 확인해야 하는 모듈 간 일관성 체크:

| #   | 검증 항목                                | 관련 모듈     | 자동화 가능        |
| --- | ---------------------------------------- | ------------- | ------------------ |
| V1  | 주파수 설정값 = TIM1->ARR 역산값         | pwm ↔ ctrl    | ✅ 런타임 자기검증 |
| V2  | 듀티 설정값 = TIM1->CCR1 역산값          | pwm ↔ ctrl    | ✅ 런타임 자기검증 |
| V3  | LCD 표시 주파수 = g_us_state.target_freq | screen ↔ ctrl | ✅ 런타임 자기검증 |
| V4  | Modbus 0x0003 = g_us_state.target_freq   | regs ↔ ctrl   | ✅ 호스트 테스트   |
| V5  | 듀티 ≤ DUTY_CLAMP_MAX 항상 성립          | pwm + regs    | ✅ 단위 테스트     |
| V6  | FREQ_MIN ≤ 주파수 ≤ FREQ_MAX             | pwm + regs    | ✅ 단위 테스트     |
| V7  | 소프트 스타트 완료 시간 ≈ 500ms          | ctrl          | ✅ 시뮬레이션      |
| V8  | CRC-16 알려진 벡터 일치                  | crc           | ✅ 단위 테스트     |
| V9  | 비상 정지 후 MOE=0, CCR1=0               | pwm + ctrl    | ✅ 런타임 자기검증 |
