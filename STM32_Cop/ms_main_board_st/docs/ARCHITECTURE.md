# 아키텍처 명세서 (Architecture Specification) — 메가소닉 발진기

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
┌──────────── 메인 루프 (while 1) ──────────────────────────────────────┐
│                                                                        │
│  1. ADC_Control_Process()                                              │
│     └→ ADC DMA 값 필터링 → 순/역방향 전력(W), 전류(mA), 전압(V) 갱신 │
│                                                                        │
│  2. ExtCtrl_Process()                                                  │
│     ├→ REMOTE 입력 상태 → ON/OFF 연동                                 │
│     ├→ SENSOR 입력 상태 → Err7/Err8 감지                              │
│     └→ 8POWER BCD(3비트) → Step 0~7 출력값 선택                       │
│                                                                        │
│  3. Alarm_Process()                                                    │
│     ├→ 출력(W) vs HIGH/LOW 알람 임계값 비교 (5초 유지 감시)           │
│     ├→ 에러 코드 설정 (Err1~Err8)                                     │
│     └→ 릴레이 5개 구동 (GO, OPERATE, LOW, HIGH, TRANSDUCER)           │
│                                                                        │
│  4. Menu_Update()                                                      │
│     ├→ Button_GetEvent() 로 6개 버튼 이벤트 소비                      │
│     ├→ 모드 FSM 상태 전이 (NORMAL/H-L SET/8POWER/FREQ SET)           │
│     └→ MegasonicCtrl_SetOutput() / ChangeChannel() 호출               │
│                                                                        │
│  5. MegasonicCtrl_Update()                                             │
│     ├→ 소프트 스타트: current_duty 점진 증가                           │
│     ├→ 채널 변경: 1초 정지 후 새 Period 로드                          │
│     └→ HRTIM_PWM_SetPeriod() / SetDuty() 호출                        │
│                                                                        │
│  6. LCD_Update()                                                       │
│     ├→ 현재 모드/상태에 따른 표시 (W값, kHz값, 에러 등)               │
│     └→ 값 변경 시에만 LCD 갱신 (깜빡임 방지)                         │
│                                                                        │
│  7. LED_Update()                                                       │
│     └→ 모드별 LED 점등/깜빡임 상태 갱신 (8개)                        │
│                                                                        │
│  8. Modbus_Process()                                                   │
│     ├→ 프레임 수신 완료 시 파싱                                       │
│     ├→ ModbusRegs_Read/Write() 호출                                   │
│     └→ 모드별 접근 제한 적용 (NORMAL/REMOTE: 읽기만, EXT: 읽기/쓰기) │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 모듈 인터페이스 계약 (Interface Contracts)

### 단위 규약 (모든 모듈 공통)

| 물리량    | 코드 단위           | 예시             | 변환                         |
| --------- | ------------------- | ---------------- | ---------------------------- |
| 출력 전력 | ×0.01 W (uint16_t)  | 50 = 0.50 W      | `power_w = val * 0.01`       |
| 주파수    | ×0.1 kHz (uint16_t) | 5000 = 500.0 kHz | `freq_hz = freq_01khz * 100` |
| 전류      | ×1 mA (uint16_t)    | 500 = 500 mA     | 직접 사용                    |
| 전압      | 0~9 단계 (uint8_t)  | 5 = 5단계        | 9단계 환산                   |
| 임피던스  | ×1 Ω (uint16_t)     | 50 = 50 Ω        | 직접 사용                    |
| 채널      | 0~6 (uint8_t)       | 0 = Ch0          | 직접 사용                    |

> **이 단위 규약은 절대 변경 금지.** Modbus 레지스터, 메뉴 표시, 내부 변수 모두 동일 단위를 사용한다.

### hrtim_pwm ↔ megasonic_ctrl 계약

```
호출 방향: megasonic_ctrl → hrtim_pwm (단방향)
역방향 호출 금지: hrtim_pwm은 megasonic_ctrl을 #include하지 않는다

megasonic_ctrl이 호출하는 함수:
  HRTIM_PWM_SetPeriod(uint32_t period)
    - Period = 5,440,000,000 / freq_hz (5.44GHz 유효 클럭)
    - 범위 검증: 0x0003 ≤ period ≤ 0xFFFD
    - 즉시 HRTIM1->sTimerxRegs[A].PERxR 반영

  HRTIM_PWM_SetDuty(uint16_t duty_permille)
    - 내부에서 CMP 계산: cmp = (period * duty_permille) / 1000
    - 안전 클램핑: duty ≤ DUTY_MAX_PERMILLE (params.h 기준)
    - 즉시 HRTIM1->sTimerxRegs[A].CMP1xR 반영

  HRTIM_PWM_Start() / Stop()
    - Start: HAL_HRTIM_WaveformOutputStart() + CountStart()
    - Stop: HAL_HRTIM_WaveformOutputStop() + CountStop() + CMP1xR=0
```

### menu ↔ megasonic_ctrl 계약

```
호출 방향: menu → megasonic_ctrl (단방향)
LCD_Update → 상태 변수 읽기 전용 (쓰기 금지)

menu가 호출하는 함수:
  MegasonicCtrl_SetOutput(uint16_t power_001w)  — 출력 전력 설정 (×0.01W)
  MegasonicCtrl_ChangeChannel(uint8_t ch)       — 채널 변경 (1초 정지 후)
  MegasonicCtrl_Start() / Stop()                — 발진 시작/정지 토글
  MegasonicCtrl_GetState()                      — 현재 상태 읽기

LCD_Update / LED_Update가 읽는 데이터:
  g_mega_state.current_channel        — 현재 채널 번호
  g_mega_state.current_power_001w     — 현재 실측 출력(×0.01W)
  g_mega_state.set_power_001w         — 설정 출력값(×0.01W)
  g_mega_state.running                — 발진 중 여부
  g_mega_state.mode                   — 동작 모드 (NORMAL/REMOTE/EXT)
  g_mega_state.error_code             — 현재 에러 코드
```

### modbus_regs ↔ megasonic_ctrl 계약

```
호출 방향: modbus_regs → megasonic_ctrl (단방향)
접근 제한: 모드별 쓰기 허용/거부 판정 포함

ModbusRegs_Write() 내부에서:
  ① 모드 확인: EXT 모드가 아니면 예외 응답(0x01) 반환
  ② REG 0x0000 쓰기 → value==1 ? MegasonicCtrl_Start() : Stop()
  ③ REG 0x0001 쓰기 → MegasonicCtrl_SetOutput(value)
  ④ REG 0x0003 쓰기 → MegasonicCtrl_ChangeChannel(value)

ModbusRegs_Read() 내부에서:
  REG 0x0002 읽기 → g_mega_state.current_power_001w 반환
  REG 0x0004 읽기 → 현재 채널 주파수(×0.1kHz) 반환
  REG 0x0005 읽기 → MegasonicCtrl_GetStatusFlags() 반환
```

### alarm ↔ megasonic_ctrl 계약

```
호출 방향: alarm → megasonic_ctrl (비상 정지 단방향)
alarm이 호출하는 함수:
  MegasonicCtrl_EmergencyStop()  — Err1/2/5/7/8 발동 시 즉시 출력 차단

alarm이 읽는 데이터:
  g_mega_state.current_power_001w     — 현재 실측 출력 (HIGH/LOW 알람 판정)
  g_mega_state.running                — 발진 중 여부 (Err8 감지용)

ext_control이 alarm에 전달:
  ExtCtrl_GetSensorState()            — SENSOR 입력 (Err7/Err8 판정)
```

---

## 설계 결정 기록 (ADR — Architecture Decision Records)

### ADR-001: HRTIM 선택 (TIM1 아닌 이유)

**결정**: HRTIM Timer A를 메가소닉 PWM 생성에 사용
**대안**: TIM1 고급 타이머
**근거**: 메가소닉(500kHz~2MHz)은 TIM1(170MHz 클럭)으로는 분해능이 부족. 1MHz 출력 시 TIM1 ARR=84(170MHz÷2÷1MHz-1)로 듀티 단계가 84밖에 안 됨. HRTIM은 유효 5.44GHz 클럭으로 1MHz에서 Period=5440, 0.018% 듀티 분해능 제공.
**결과**: DLL 캘리브레이션 필수, AF13 핀 설정, 별도의 HAL API 세트 사용

### ADR-002: 출력 단위 ×0.01W 규약

**결정**: 모든 내부 변수와 Modbus 레지스터에서 출력 전력은 ×0.01W 단위 사용
**대안**: mW 단위, 부동소수점
**근거**: uint16_t 하나로 0.00~6.55W 표현 가능. Modbus 16비트 레지스터와 1:1 대응. 부동소수점 연산 회피. 매뉴얼의 0.01W 분해능과 일치.
**결과**: 표시 시 100으로 나누어 소수점 2자리 표현 (예: 50 → "0.50W")

### ADR-003: 7채널 이산 주파수 방식

**결정**: 연속 가변 주파수 대신 7채널 이산 주파수 테이블 사용 (500kHz~2MHz, 250kHz 단위)
**대안**: 연속 가변 (초음파 방식)
**근거**: 메가소닉 트랜스듀서는 특정 주파수에서만 효율적으로 동작. 채널 변경 시 1초 정지 규칙으로 안전성 확보.
**결과**: Flash에 7개 채널 주파수 저장, FREQ SET 모드로 채널 선택, 공진주파수 탐색 알고리즘 지원

### ADR-004: 모듈 간 단방향 의존성

**결정**: 의존성은 항상 상위→하위 단방향. 하위 모듈이 상위 모듈을 #include하지 않음.
**대안**: 콜백 함수 등록
**근거**: 순환 의존 방지, 테스트 용이성, 빌드 순서 단순화.
**결과**:

- megasonic_ctrl → hrtim_pwm (O) / 역방향 (X)
- menu → megasonic_ctrl (O) / 역방향 (X)
- modbus_regs → megasonic_ctrl (O) / 역방향 (X)
- alarm → megasonic_ctrl (O) / 역방향 (X)

### ADR-005: ISR 최소화 원칙

**결정**: ISR에서는 플래그 설정과 버퍼 저장만 수행. 모든 로직은 메인 루프.
**근거**: ISR 실행 시간 최소화 → 다른 인터럽트 지연 방지. 디버깅 용이성.
**결과**: `volatile` 공유 변수 + 메인 루프 폴링 패턴

### ADR-006: LCD 조건부 갱신

**결정**: LCD는 값이 실제로 변경되었을 때만 갱신.
**근거**: HD44780은 쓰기에 37μs. 매 루프 전체 갱신 시 메인 루프 주기 지연.
**결과**: 이전 표시값 캐시 변수 유지

### ADR-007: 알람 5초 유지 규칙

**결정**: Err1(LOW)/Err2(HIGH)/Err5(TRANSDUCER)는 조건이 **5초간 연속 유지** 시에만 발동
**대안**: 즉시 발동
**근거**: 매뉴얼 준수. 순간적인 노이즈나 과도 상태에 의한 오동작 방지. 5초는 충분한 안정화 시간.
**결과**: `AlarmTimer_t` 구조체로 시작 시각 기록 + 경과 판정

---

## 검증 포인트 (Verification Points)

아래 항목은 코드 변경 후 반드시 확인해야 하는 모듈 간 일관성 체크:

| #   | 검증 항목                                         | 관련 모듈   | 자동화 가능        |
| --- | ------------------------------------------------- | ----------- | ------------------ |
| V1  | 주파수 채널 = HRTIM Period 역산값                 | pwm ↔ ctrl  | ✅ 런타임 자기검증 |
| V2  | 듀티 설정값 ≤ DUTY_MAX_PERMILLE                   | pwm ↔ ctrl  | ✅ 런타임 클램핑   |
| V3  | LCD 표시 출력(W) = g_mega_state.set/current_power | LCD ↔ ctrl  | ✅ 런타임 자기검증 |
| V4  | Modbus 0x0002 = g_mega_state.current_power_001w   | regs ↔ ctrl | ✅ 호스트 테스트   |
| V5  | 출력 ≤ 1.00W (100) 항상 성립                      | pwm + regs  | ✅ 범위 검증       |
| V6  | 채널 변경 시 1초 정지 보장                        | ctrl        | ✅ 시뮬레이션      |
| V7  | 소프트 스타트 완료 시간 ≈ 500ms                   | ctrl        | ✅ 시뮬레이션      |
| V8  | CRC-16 알려진 벡터 일치                           | crc         | ✅ 단위 테스트     |
| V9  | 비상 정지 후 HRTIM 출력 OFF, CMP=0                | pwm + ctrl  | ✅ 런타임 자기검증 |
| V10 | Err1/Err2 5초 유지 후 발동                        | alarm       | ✅ 시뮬레이션      |
| V11 | EXT 외 모드에서 Modbus 쓰기 거부                  | regs + menu | ✅ 호스트 테스트   |
