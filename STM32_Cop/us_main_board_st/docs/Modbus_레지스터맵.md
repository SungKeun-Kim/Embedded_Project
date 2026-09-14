# STM32G474CBT6 Modbus RTU 내부 레지스터 사양

| 항목      | 내용                                     |
| --------- | ---------------------------------------- |
| Firmware  | v1.2.0                                   |
| 기준 문서 | 기존 `통신 Protocol.pdf`의 3번 명령 항목 |
| 공개 문서 | `PLC_Modbus_RTU_사용자_가이드.md`        |
| 갱신일    | 2026-09-14                               |

## 1. 설계 원칙

- Frequency와 Local Duty limit은 보드에서만 설정한다.
- PLC에는 직접적인 `FREQ_SET`, `DUTY_SET`을 노출하지 않는다.
- 기존 출력값 `0~500`은 `OUTPUT_VALUE`로 유지한다.
- PB1 `PWM_VR`가 로컬 Duty limit을 정하고, `OUTPUT_VALUE`는 그 범위 안에서
  PA7 `PWM_OUTPUT`을 조정한다.
- PB1이 최대일 때 `OUTPUT_VALUE=0/250/500`은 PA7에서 각각
  `5/52.5/100%` PWM에 대응한다.
- 기존 Boolean 표현 `False=0x0000`, `True=0xFF00`을 유지한다.
- 제품 동작 명령은 `0x0000~0x0008`에 연속 배치한다.
- 향후 명령은 `0x0009~0x000B` 세 주소만 예약한다.
- 통신 설정 `0x0010~0x0012`는 보드 설정, PLC Read-only다.

## 2. Register Map

|      PDU | 이름                    | 접근 | Source                      | 값/단위                       |
| -------: | ----------------------- | :--: | --------------------------- | ----------------------------- |
| `0x0000` | `FREQUENCY_VALUE`       |  R   | `g_us_state.current_freq`   | ×0.1 kHz                      |
| `0x0001` | `OUTPUT_VALUE`          | R/W  | `g_us_state.output_command` | 0~500                         |
| `0x0002` | `DISPLAY_OUTPUT_RANGE`  |  R   | Supervisor 표시 범위        | 100 또는 300~2500            |
| `0x0003` | `SWEEP_FREQUENCY_VALUE` |  R   | Sweep 중 `current_freq`     | ×0.1 kHz, 미동작=0            |
| `0x0004` | `LOCAL_INPUT_LOCK`      | R/W  | `s_local_input_locked`      | `0000/FF00`                   |
| `0x0005` | `RUN_STATUS_CONTROL`    | R/W  | `g_us_state.running`        | `0000/FF00`                   |
| `0x0006` | `EXTERNAL_INPUT_STATUS` |  R   | `REMOTE` Active LOW         | `0000/FF00`                   |
| `0x0007` | `SWEEP_STATUS_CONTROL`  | R/W  | Mode/Running                | `0000/FF00`                   |
| `0x0008` | `ERROR_STATUS_RESET`    | R/W  | `error_active`              | Read status, True write reset |
| `0x0009` | `RESERVED_1`            |  -   | 미구현                      | 접근 시 0x02                  |
| `0x000A` | `RESERVED_2`            |  -   | 미구현                      | 접근 시 0x02                  |
| `0x000B` | `RESERVED_3`            |  -   | 미구현                      | 접근 시 0x02                  |
| `0x0010` | `MODBUS_SLAVE_ID`       |  R   | `g_modbus_cfg.address`      | Supervisor 1~16              |
| `0x0011` | `MODBUS_BAUD_INDEX`     |  R   | Baud table index            | 0~4                           |
| `0x0012` | `MODBUS_PARITY`         |  R   | `g_modbus_cfg.parity`       | 1 (8-E-1)                    |

## 3. 기존 명령 대응

| 기존 명령 | 이름                |   새 PDU |
| --------: | ------------------- | -------: |
|  `0x0100` | 발진 주파수         | `0x0000` |
|  `0x0102` | 출력                | `0x0001` |
|  `0x0103` | 표시 출력 범위      | `0x0002` |
|  `0x0104` | Sweep 주파수        | `0x0003` |
|  `0x0200` | Local 입력 금지     | `0x0004` |
|  `0x0201` | 출력 동작 상태      | `0x0005` |
|  `0x0202` | 외부 입력 동작 상태 | `0x0006` |
|  `0x0203` | Sweep 동작 상태     | `0x0007` |
|  `0x0204` | Error 상태/Reset    | `0x0008` |

## 4. Write 정책

|           주소 | 허용 조건                              | 동작                 |
| -------------: | -------------------------------------- | -------------------- |
|       `0x0001` | Local Lock=True, 0~500                 | Output command 변경  |
|       `0x0004` | `0000` 또는 `FF00`, Unlock은 Stop 상태 | Local Lock 변경      |
|  `0x0005` True | Local Lock=True                        | Run 시작             |
| `0x0005` False | 항상                                   | 즉시 Stop            |
|  `0x0007` True | Local Lock=True                        | Sweep Mode 선택      |
| `0x0007` False | 항상                                   | Continuous Mode 선택 |
|  `0x0008` True | 출력 Stop                              | Error latch Reset    |

Local Lock 중 `menu.c`는 짧은 버튼 입력을 무시한다. START/STOP 장기 누름
Emergency Stop은 계속 허용한다. Lock은 RAM에만 유지되어 전원 재인가 시
False로 복귀한다.

## 5. Function과 Exception

- FC03: Read Holding Registers
- FC06: Write Single Register
- FC10: Write Multiple Registers, 전체 사전 검증
- Broadcast 0: FC06/FC10 처리 후 무응답
- `0x01`: Illegal Function
- `0x02`: Illegal Address 또는 R/Reserved 쓰기
- `0x03`: 값/수량 오류 또는 제어 조건 미충족
- `0x04`: 검증 후 내부 적용 실패

FC10에서 `0x0004=True`와 `0x0005=True`를 한 요청에 쓰면 사전 검증 시점의
Lock이 False이므로 요청을 거부한다. PLC는 Lock을 별도 FC06으로 먼저 설정한다.

## 6. RTU 설정

| 항목            | 값                                   |
| --------------- | ------------------------------------ |
| 기본            | ID 1, 9600, 8-E-1                    |
| Baud            | 9600/19200/38400/57600/115200        |
| Frame           | 8-E-1 고정                           |
| 19200 이하      | 11-bit character 기준 t1.5/t3.5 계산 |
| 19200 초과      | t1.5=750 us, t3.5=1.750 ms           |

설정은 Flash 마지막 2 KB page에 CRC/sequence record로 저장한다.

## 7. 현재 제한과 검증

- `FREQUENCY_VALUE`는 Firmware가 적용한 값이며 별도 주파수 Sensor 측정값은 아니다.
- `DISPLAY_OUTPUT_RANGE`는 PERCENT 설정이면 100, W 설정이면 300~2500을 반환한다.
- Error latch는 현재 Emergency Stop을 나타내며 Hardware Fault 입력은 향후 확장한다.
- 예비 주소 3개는 구현 전까지 FC03/FC06/FC10 모두 Exception 0x02로 처리한다.

출하 전 실제 PLC에서 FC03/FC06, Local Lock 순서, Error Reset, 모든 Baud와
전원 재인가 후 통신 설정 복원을 검증한다.
