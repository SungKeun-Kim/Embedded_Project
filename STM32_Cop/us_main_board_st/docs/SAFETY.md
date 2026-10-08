# 안전 및 보호 기준 (Safety & Protection)

> 초음파 발진기는 **수백 볼트 고전압 + 수십 kHz 고주파** 출력 장비다.
> 소프트웨어 결함이 곧 하드웨어 파손 및 안전 사고로 직결된다.
> 아래 기준은 **모든 코드 작성 및 리뷰 시 반드시 준수**해야 한다.

---

## 1. 출력 보호 (PWM 안전)

### 1.1 듀티비 상한 클램핑

```
규칙: 출력에 적용되는 듀티비는 DUTY_CLAMP_MAX(900=90.0%) 초과 불가
적용: ultrasonic_pwm.c의 CCR 설정 함수에서 항상 클램핑
위반 시: FET 관통 전류 → 소자 파손, 화재 위험
```

- TIM1 CCR1 값 설정 전 반드시 `if (ccr > ccr_max) ccr = ccr_max;` 적용
- params.h의 `DUTY_CLAMP_MAX`를 단일 안전 상한으로 사용한다.
- Modbus는 900 초과 값을 Clamp하지 않고 Exception 0x03으로 거부한다.

### 1.2 주파수 범위 제한

```
규칙: 주파수는 FREQ_MIN(15.0kHz) ~ FREQ_MAX(128.0kHz) 범위만 허용
위반 시: 공진 이탈 → 임피던스 불일치 → FET 과열/소손
```

- ARR 값 계산 후 범위 검증 필수
- 스윕 모드에서도 시작/끝 주파수 모두 범위 내 확인

### 1.3 데드타임 최소값 보장

```
규칙: 데드타임은 최소 150ns 이상 유지 (BDTR.DTG)
위반 시: 하이사이드/로우사이드 FET 동시 ON → 관통 전류 → 즉시 파손
```

- 데드타임 설정 함수에서 최소값 미만 입력 시 최소값으로 강제 보정

---

## 2. 소프트 스타트 (Soft Start)

```
규칙: 출력 시작 시 반드시 소프트 스타트 적용
      듀티비 0% → 목표값까지 최소 500ms 이상 램프업
위반 시: 갑작스러운 전력 인가 → 과도 전류 → 퓨즈 트립 또는 전원부 손상
```

- BTN_OK 눌러 Start 시, Modbus 원격 ON 시, 비상 정지 해제 후 재시작 시 모두 적용
- 소프트 스타트 진행 중 주파수 변경 금지 (완료 후 허용)

---

## 3. 출력 정지와 Emergency Stop

### 3.1 현재 구현된 운전 정지

| 조건 | 검출 소스 | Firmware 동작 |
| --- | --- | --- |
| Local 모드 전환/긴급 정지 | `PA11/RUN_SW` 상태 토글 | 약 30ms 디바운스 후 출력 정지, Lock 해제, Local 전환 |
| 통신 정지 | `RUN_STATUS_CONTROL(0x0005)=0` | Frame 처리 즉시 출력 정지 및 Lock 해제 |
| 수동 Remote 정지 | `PA10/REMOTE=OFF` | 수동 운전 출력 정지 |

`PB9/START_STOP` 장기 누름은 Emergency Stop으로 사용하지 않는다. 이 입력은
Auto-Tuning 화면에서 3초 유지할 때 탐색을 시작하는 용도로 예약한다.

### 3.2 Error latch와 Hardware Interlock

`UltrasonicCtrl_EmergencyStop()`과 `ERROR_STATUS_RESET(0x0008)` 경로에는 PA6
입력전류 보호가 연결되어 있다. LCD와 동일한 1초 평균 RMS 전류가 7.00 A 이상인
상태가 약 5초 지속되면 PWM과 Signal 출력을 정지하고 `OVER CURRENT` Error latch를
설정한다. 단일 ADC peak와 짧은 스위칭 spike는 과전류 유지시간에 포함하지 않는다.
실제 설비의 비상정지는 Firmware 입력만 의존하지 말고 별도의 Hardware Interlock으로
전력단을 차단해야 한다. Error latch가 설정된 경우 출력 정지와 원인 제거 후
`0x0008`에 `0xFF00`을 써서 Reset하며, 재시작에는 Soft Start를 적용한다.

---

## 4. ADC 안전

```
규칙: ADC 캘리브레이션 없이 ADC 값을 제어 판단에 사용 금지
      G4 ADC는 캘리브레이션 없이 수백 카운트 오차 발생 가능
```

- `HAL_ADCEx_Calibration_Start()` 호출 전 ADC 값 기반 출력 제어 시작 금지
- 전류 센서(PA6/ADC2_IN3) 값은 반드시 필터링 후 판단 (단일 샘플 기반 판단 금지)
- ADC 100% 풀스케일(4095)이 지속되면 센서 단선 의심 → 경고

---

## 5. 통신 안전 (Modbus)

### 5.1 입력값 검증

```
규칙: Modbus로 수신된 모든 쓰기 값은 유효 범위 검증 필수
위반 시: 원격에서 위험한 파라미터 설정 가능
```

- Frequency와 Duty limit은 보드에서만 설정하며 PLC Write 주소를 제공하지 않는다.
- PLC 출력값: 0~500 범위 외 → 예외 응답 0x03
- Boolean: 0x0000/0xFF00 이외 → 예외 응답 0x03
- Output/Run/Sweep 시작은 Local Lock=False이면 예외 응답 0x03
- Local Lock ON은 RS485 보드 감지, RUN_SW=OFF, 메인화면 조건에서만 허용한다.
- RS485 보드가 Lock/Run 중 빠지면 출력 정지와 Lock 해제를 수행한다.
- Stop은 Local Lock 상태와 관계없이 항상 허용한다.
- Modbus Stop은 Lock도 함께 해제하고 최신 PB1 Local 출력값으로 복귀한다.
- 운전 중 Lock OFF도 허용하며, 즉시 최신 PB1 출력과 Local 접점 상태를 적용한다.
- 상태 전용 주소 0x0009~0x000B 쓰기 → 예외 응답 0x02
- Slave ID/Baud/Parity(0x0010~0x0012)는 PLC Read-only → 쓰기 시 예외 응답 0x02
- FC10은 모든 주소와 값을 먼저 검사하고, 하나라도 잘못되면 전체를 적용하지 않는다.

### 5.2 통신 설정 변경 제한

- Slave ID, Baud와 RTERM은 전원 투입 시 Supervisor에서만 변경하고 Parity는 8-E-1로 고정한다.
- PLC는 0x0010~0x0012를 읽어 현재값을 확인할 수 있지만 쓸 수 없다.
- 통신 설정은 CRC를 포함해 Flash에 저장하고, 저장 성공 후에만 UART에 적용한다.
- Local Lock 중에는 외부 PA10/REMOTE ON/OFF만 무시한다. PB1과 설정 버튼은
  유지하고 PA11/RUN_SW 상태 변경은 Local 제어권 인계로 처리한다.
- PB9/START_STOP 장기 누름은 Auto-Tuning 3초 입력 전용이며 일반 운전
  비상정지로 사용하지 않는다.
- PA11/RUN_SW 토글은 출력 정지와 함께 Lock을 해제하고 Local로 전환한다.
- 통신 Watchdog은 Firmware v1.2.0에서 아직 지원하지 않는다.

---

## 6. 타이머/워치독

### 6.1 Independent Watchdog (IWDG)

```
규칙: 양산 펌웨어에서 IWDG 필수 활성화
      메인 루프가 정상 동작하지 않으면 MCU 자동 리셋
```

- 타임아웃: 500ms~1000ms 권장
- 메인 루프 매 주기마다 `HAL_IWDG_Refresh()` 호출
- ISR에서 워치독 리프레시 금지 (메인 루프 교착 감지 불가)

### 6.2 동작 타이머 (사이클 타이머)

- 타이머 만료 시 출력 정상 종료 (비상 정지 아님, 소프트 스톱)
- `PC14/END_BZ`를 2초간 ON → 외부 장비에 종료 알림
- 타이머 카운트다운 중 버튼으로 시간을 바꾸면 새 설정 시간부터 다시 카운트다운
- Modbus Run과 외부 REMOTE 수동 운전은 이 타이머를 사용하지 않는다. 통신 운전은
  PLC의 Run OFF, 비상 정지 또는 전원 차단 전까지 유지된다.

---

## 7. 전원/하드웨어 보호

### 7.1 출력 핀 초기 상태

```
규칙: 부팅 시 모든 출력 핀은 안전 상태(비활성)로 초기화
      TIM1 PWM은 GPIO 초기화 후에도 MOE=0 상태 유지
      릴레이/부저 핀은 LOW(비활성)로 시작
```

- `GPIO_Init_All()`에서 출력 핀 기본값 = 비활성(LOW)
- TIM1 초기화 후에도 Start 명령 전까지 출력 없음

### 7.2 SWD 핀 보호

```
규칙: PA13(SWDIO), PA14(SWCLK)은 절대 GPIO로 설정 금지
      이 핀을 GPIO로 전환하면 디버거 연결 불가 → 벽돌화
```

---

## 8. 코드 리뷰 체크리스트

모든 코드 변경 시 아래 항목 확인:

- [ ] 듀티비 설정 코드에 상한 클램핑이 있는가?
- [ ] 주파수 설정 코드에 범위 검증이 있는가?
- [ ] 새로운 출력 시작 경로에 소프트 스타트가 적용되는가?
- [ ] ISR에서 무거운 처리(LCD 갱신, 문자열 조작 등)를 하고 있지 않은가?
- [ ] volatile 선언이 필요한 공유 변수에 volatile이 있는가?
- [ ] Modbus 쓰기 값에 범위 검증이 있는가?
- [ ] 비상 정지 조건을 우회하는 코드 경로가 없는가?
- [ ] 동적 메모리 할당(malloc/free)을 사용하고 있지 않은가?
- [ ] 큰 로컬 배열(>256 바이트)로 스택을 과다 사용하고 있지 않은가?
