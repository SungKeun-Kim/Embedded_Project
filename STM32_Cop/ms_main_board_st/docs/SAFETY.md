# 안전 및 보호 기준 (Safety & Protection) — 메가소닉 발진기

> 메가소닉 발진기는 **500kHz~2MHz 고주파 + GaN FET 고속 스위칭** 출력 장비다.
> 소프트웨어 결함이 곧 하드웨어 파손 및 안전 사고로 직결된다.
> 아래 기준은 **모든 코드 작성 및 리뷰 시 반드시 준수**해야 한다.

---

## 1. 출력 보호 (HRTIM PWM 안전)

### 1.1 출력 전력 상한

```
규칙: 출력 전력은 어떤 경우에도 1.00W (레지스터값 100) 초과 불가
적용: megasonic_ctrl.c의 출력 설정 함수에서 항상 클램핑
위반 시: 트랜스듀서 과열, GaN FET 소손, 화재 위험
```

- 듀티비 → 출력 전력 변환 후 상한 검증
- Modbus 원격 쓰기 시에도 동일 클램핑 적용 (modbus_regs.c)
- 8POWER Step별 설정값도 범위 검증 (0.20~1.00W)

### 1.2 데드타임 최소값 보장

```
규칙: 데드타임은 최소 5ns 이상 유지 (HRTIM DTR)
      GaN FET 사용 시 5~20ns 권장
      Si MOSFET 사용 시 50~100ns 권장
위반 시: 하이사이드/로우사이드 FET 동시 ON → 관통 전류 → 즉시 파손
```

- HRTIM DTR Rising/Falling 설정 함수에서 최소값 미만 입력 시 최소값으로 강제 보정
- 184ps 분해능으로 정밀 제어 가능

### 1.3 HRTIM Period 유효 범위

```
규칙: Period 값은 0x0003 ~ 0xFFFD 범위만 허용
위반 시: HRTIM 카운터 오동작 → 예측 불가능한 출력
```

- 주파수 변환 후 Period 범위 검증 필수
- 채널 테이블의 모든 값이 유효 범위 내인지 부팅 시 확인

---

## 2. 소프트 스타트 (Soft Start)

```
규칙: 출력 시작 시 반드시 소프트 스타트 적용
      듀티비 0% → 목표값까지 최소 500ms 이상 램프업
위반 시: 갑작스러운 전력 인가 → 과도 전류 → GaN FET 파손 또는 전원부 손상
```

- START/STOP 버튼으로 Start 시, REMOTE 외부 ON 시, 에러 해제 후 재시작 시 모두 적용
- Modbus 원격 ON(0x0000=1) 시에도 소프트 스타트 적용
- 채널 변경 후 재시작 시에도 소프트 스타트 적용
- 소프트 스타트 진행 중 채널 변경 금지 (완료 후 허용)

---

## 3. 비상 정지 (Emergency Stop)

### 3.1 즉시 출력 차단 조건

아래 조건 중 하나라도 감지 시 **HRTIM 출력 즉시 차단**:

| 조건 | 검출 소스 | 응답 시간 |
|------|----------|-----------|
| **SENSOR OPEN** | PC7 GPIO (Err7/Err8) | < 10ms (다음 폴링 주기) |
| 출력 LOW 5초 유지 | ADC (순방향 전력) vs LOW 알람값 | 5초 |
| 출력 HIGH 5초 유지 | ADC (순방향 전력) vs HIGH 알람값 | 5초 |
| 진동자 과부하 5초 | ADC (역방향 전력 이상) | 5초 |
| Modbus 비상 정지 명령 | 레지스터 0x0000 = 0 | 즉시 |
| REMOTE OFF | PC6 GPIO | 다음 폴링 주기 |
| HRTIM Fault 입력 | HRTIM 하드웨어 Fault (확장) | **즉시 (하드웨어)** |

### 3.2 출력 차단 절차

```c
/* 비상 정지 시퀀스 (순서 엄수) */
1. HRTIM_PWM_OutputStop();            // ① HRTIM 출력 즉시 비활성화
2. HRTIM1->CMP1xR = 0;               // ② 듀티비 0으로 클리어
3. g_mega_state.error_code = ERR_xxx; // ③ 에러 코드 설정
4. Alarm_SetRelay(err_code);          // ④ 해당 릴레이 출력 (OPERATE OPEN)
5. Buzzer_AlarmBeep();                // ⑤ 청각 알림
6. LCD_ShowError(err_code);           // ⑥ 시각 알림
```

### 3.3 에러별 해제 방법

| 에러 | 해제 방법 |
|------|----------|
| Err1 (LOW ALARM) | MODE+DOWN 동시 누름 또는 전원 OFF→ON |
| Err2 (HIGH ALARM) | MODE+DOWN 동시 누름 또는 전원 OFF→ON |
| Err5 (TRANSDUCER) | MODE+DOWN 동시 누름 또는 전원 OFF→ON |
| Err6 (SETTING) | 자동 해제 (설정값 수정 시) |
| **Err7 (SENSOR OFF)** | **전원 OFF → SENSOR SHORT 확인 → 전원 ON만 가능** |
| Err8 (SENSOR RUN) | 전원 OFF → SENSOR SHORT 확인 → 전원 ON |

> **⚠️ Err7은 MODE+DOWN으로 해제 불가**. SENSOR 단자가 물리적으로 SHORT 상태가 되어야만 해제 가능. 이는 안전 사고 방지를 위한 의도적 설계.

### 3.4 에러 해제 후 재시작

- 에러 상태에서 출력 재시작 불가 (사용자가 명시적으로 에러 리셋 해야 함)
- Err1/Err2 발생 후: 에러 해제 완료 후 출력값/알람값 재확인 필요
- 재시작 시 반드시 소프트 스타트 적용 (2항)

---

## 4. 알람 릴레이 우선순위

```
규칙: 복수 에러 동시 발생 시 모든 해당 릴레이가 독립적으로 동작
      RELAY_OPERATE는 통합 알람으로, 어떤 에러든 발생하면 OPEN
```

| 상태 | RELAY_GO | RELAY_LOW | RELAY_HIGH | RELAY_TRANSDUCER | RELAY_OPERATE |
|------|----------|-----------|------------|------------------|---------------|
| 정상 출력 범위 | SHORT | OPEN | OPEN | OPEN | SHORT |
| Err1 (LOW) | OPEN | **SHORT** | OPEN | OPEN | **OPEN** |
| Err2 (HIGH) | OPEN | OPEN | **SHORT** | OPEN | **OPEN** |
| Err5 (TRANSDUCER) | OPEN | OPEN | OPEN | **SHORT** | **OPEN** |
| Err7/8 (SENSOR) | OPEN | OPEN | OPEN | OPEN | **OPEN** |
| 전원 OFF | OPEN | OPEN | OPEN | OPEN | OPEN |

> **RELAY_OPERATE**: 통합 알람 릴레이. 정상 시 SHORT, 에러·단전 시 OPEN (Fail-safe).
> 릴레이 접점 사양: AC PEAK 100V, DC 100V, 0.3A. OPERATE는 24V 0.1A 이상 접점 권장.

---

## 5. ADC 안전

```
규칙: ADC 캘리브레이션 없이 ADC 값을 제어 판단에 사용 금지
      G4 ADC는 캘리브레이션 없이 수백 카운트 오차 발생 가능
```

- `HAL_ADCEx_Calibration_Start()` 호출 전 ADC 값 기반 출력 제어 시작 금지
- RF 전력 검출기(AD8318, LTC5507) DC 출력이 3.3V 이내인지 확인 필요
- ADC 100% 풀스케일(4095)이 지속되면 센서 단선 의심 → 경고
- 역방향 전력(Reflected Power) 이상 → VSWR 과다 → 진동자 알람(Err5) 판정

---

## 6. 통신 안전 (Modbus)

### 6.1 입력값 검증

```
규칙: Modbus로 수신된 모든 쓰기 값은 유효 범위 검증 필수
위반 시: 원격에서 위험한 파라미터 설정 가능
```

- 출력값: 20~100 (×0.01W, 0.20~1.00W) 범위 외 → 예외 응답 0x03
- 채널 번호: 0~6 범위 외 → 예외 응답 0x03
- HIGH 알람: 25~150 범위 외 → 예외 응답 0x03
- LOW 알람: 0~95 범위 외 → 예외 응답 0x03
- 슬레이브 주소: 1~247 범위 외 → 예외 응답 0x03
- 시스템 리셋(0x00FF): 0x1234 외 값은 무시

### 6.2 모드별 접근 제한

- **NORMAL/REMOTE 모드**: FC03(읽기)만 허용. FC06/10(쓰기) 요청 시 예외 응답 0x01 반환.
- **EXT 모드**: FC03/06/10 모두 허용.
- **비상 정지 조건은 어떤 모드에서도 항상 동작** (안전 우선)

---

## 7. 채널 변경 안전

```
규칙: 채널(주파수) 변경 시 반드시 1초간 발진 정지 후 전환
위반 시: HRTIM Period 급변 → 과도 스위칭 → GaN FET 과응력에 의한 파손
```

- 채널 변경 요청 → 출력 정지 → 1000ms 대기 → 새 Period 로드 → 소프트 스타트 발진 재개
- 1초 대기 중 추가 채널 변경 요청 무시

---

## 8. 타이머/워치독

### 8.1 Independent Watchdog (IWDG)

```
규칙: 양산 펌웨어에서 IWDG 필수 활성화
      메인 루프가 정상 동작하지 않으면 MCU 자동 리셋
```

- 타임아웃: 500ms~1000ms 권장
- 메인 루프 매 주기마다 `HAL_IWDG_Refresh()` 호출
- ISR에서 워치독 리프레시 금지 (메인 루프 교착 감지 불가)

---

## 9. 전원/하드웨어 보호

### 9.1 출력 핀 초기 상태

```
규칙: 부팅 시 모든 출력 핀은 안전 상태(비활성)로 초기화
      HRTIM PWM은 DLL 캘리브레이션 후에도 Start 명령 전까지 출력 없음
      릴레이/부저 핀은 LOW(비활성)로 시작
      LED 핀은 LOW로 시작
```

### 9.2 SWD 핀 보호

```
규칙: PA13(SWDIO), PA14(SWCLK)은 절대 GPIO로 설정 금지
      이 핀을 GPIO로 전환하면 디버거 연결 불가 → 벽돌화
```

### 9.3 SENSOR 단자 미사용 시

```
규칙: SENSOR 단자 미사용 시 D-SUB 25P 커넥터 내부에서 SHORT 처리하여 출고
      OPEN 상태로 출하하면 전원 ON 즉시 Err7 발생
```

---

## 10. 코드 리뷰 체크리스트

모든 코드 변경 시 아래 항목 확인:

- [ ] 출력 설정 코드에 1.00W 상한 클램핑이 있는가?
- [ ] HRTIM Period 설정 코드에 유효 범위(0x0003~0xFFFD) 검증이 있는가?
- [ ] 데드타임 설정에 최소값 보장이 있는가?
- [ ] 새로운 출력 시작 경로에 소프트 스타트가 적용되는가?
- [ ] 채널 변경 시 1초 정지가 보장되는가?
- [ ] ISR에서 무거운 처리(LCD 갱신, 문자열 조작 등)를 하고 있지 않은가?
- [ ] volatile 선언이 필요한 공유 변수에 volatile이 있는가?
- [ ] Modbus 쓰기 값에 범위 검증이 있는가?
- [ ] Modbus 쓰기 시 모드별 접근 제한이 적용되는가?
- [ ] 비상 정지 조건을 우회하는 코드 경로가 없는가?
- [ ] 알람 5초 유지 로직이 정확한가?
- [ ] 동적 메모리 할당(malloc/free)을 사용하고 있지 않은가?
- [ ] 큰 로컬 배열(>256 바이트)로 스택을 과다 사용하고 있지 않은가?
- [ ] HRTIM DLL 캘리브레이션이 HRTIM 사용 전에 완료되는가?
