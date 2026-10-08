# STM32G474CBT6 Modbus RTU 내부 레지스터 사양

| 항목      | 내용                                     |
| --------- | ---------------------------------------- |
| Firmware  | v1.2.0                                   |
| 기준 문서 | 기존 `통신 Protocol.pdf`의 3번 명령 항목 |
| 공개 문서 | `PLC_Modbus_RTU_사용자_가이드.md`        |
| 갱신일    | 2026-10-08                               |

## 1. 설계 원칙

- Frequency와 Local Duty limit은 보드에서만 설정한다.
- PLC에는 직접적인 `FREQ_SET`, `DUTY_SET`을 노출하지 않는다.
- 기존 출력값 `0~500`은 `OUTPUT_VALUE`로 유지한다.
- Local 제어에서는 PB1 `PWM_VR`가 PA7 출력을 정한다.
- Local Lock ON 순간 현재 PB1 비율을 `OUTPUT_VALUE`로 인계한다. 이후 PLC와
  PB1 중 마지막으로 조작한 값이 실제 출력을 정한다.
- 통신 제어에서 `OUTPUT_VALUE=0/250/500`은 PA7에서 각각
  `5/52.5/100%` PWM에 대응한다.
- 기존 Boolean 표현 `False=0x0000`, `True=0xFF00`을 유지한다.
- 제품 동작 명령은 `0x0000~0x0008`에 연속 배치한다.
- 상세 오류와 제어 준비 상태는 `0x0009~0x000B`에서 읽는다.
- 통신 설정 `0x0010~0x0012`는 보드 설정, PLC Read-only다.

## 2. Register Map

| PDU 주소(0-based) | 이름                    | 접근 | Source                      | 값/단위                       |
| -------: | ----------------------- | :--: | --------------------------- | ----------------------------- |
| `0x0000` | `FREQUENCY_VALUE`       |  R   | `g_us_state.current_freq`   | ×0.1 kHz                      |
| `0x0001` | `OUTPUT_VALUE`          | R/W  | `g_us_state.output_command` | 0~500                         |
| `0x0002` | `DISPLAY_OUTPUT_RANGE`  |  R   | Supervisor 표시 범위        | 100 또는 300~2500            |
| `0x0003` | `SWEEP_FREQUENCY_VALUE` |  R   | Sweep 중 `current_freq`     | ×0.1 kHz, 미동작=0            |
| `0x0004` | `LOCAL_INPUT_LOCK`      | R/W  | `s_local_input_locked`      | `0000/FF00`                   |
| `0x0005` | `RUN_STATUS_CONTROL`    | R/W  | `g_us_state.running`        | `0000/FF00`                   |
| `0x0006` | `EXTERNAL_INPUT_STATUS` |  R   | `REMOTE` Active LOW         | `0000/FF00`                   |
| `0x0007` | `SWEEP_SWITCH_STATUS`   |  R   | PA12/SWEEP_SW               | `0000`=OFF, `FF00`=ON         |
| `0x0008` | `ERROR_STATUS_RESET`    | R/W  | `error_active`              | Read status, True write reset |
| `0x0009` | `ERROR_CODE`            |  R   | `error_code`                | 0=None, 1=Transducer, 2=Over current |
| `0x000A` | `RUN_SWITCH_STATUS`     |  R   | PA11/RUN_SW                 | `0000`=OFF, `FF00`=ON         |
| `0x000B` | `COMM_CONTROL_READY`    |  R   | 통신 제어 허용 조건         | `0000/FF00`                   |
| `0x000C` | `CENTER_FREQUENCY`      |  R   | 설정 중심주파수             | ×0.1 kHz                      |
| `0x000D` | `SWEEP_WIDTH`           |  R   | 설정 Sweep 폭               | ±Hz                           |
| `0x000E` | `SWEEP_SPEED`           |  R   | 설정 Sweep 속도             | Hz                            |
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
|  `0x0203` | Sweep 스위치 상태   | `0x0007` |
|  `0x0204` | Error 상태/Reset    | `0x0008` |

## 4. Write 정책

|           주소 | 허용 조건                              | 동작                 |
| -------------: | -------------------------------------- | -------------------- |
|       `0x0001` | Local Lock=True, 0~500                 | Output command 변경  |
| `0x0004` True  | 보드 감지, RUN_SW=OFF, 메인화면        | 기존 Stop 후 통신 준비 |
| `0x0004` False | 항상, 운전 중 Unlock 가능              | Lock/출력 제어권 해제 |
|  `0x0005` True | Local Lock=True                        | 타이머 없이 Run 시작 |
| `0x0005` False | 항상                                   | Stop, Lock/485 COMM 유지 |
|  `0x0008` True | 출력 Stop                              | Error latch Reset    |

Local Lock은 PLC 쓰기를 허가하고 외부 `PA10/REMOTE`의 ON/OFF만 차단한다.
Lock ON은 `PB3/485BD_DETECT=LOW`, `PA11/RUN_SW=OFF`, 메인화면 조건에서만
허용하며 최초 진입 시 기존 출력을 정지한다. RS485 보드가 빠지면 출력과 Lock을
자동 해제한다.
상태 읽기는 RUN 스위치와 관계없이 허용한다. `COMM_CONTROL_READY=0xFF00`은
RS485 옵션보드 감지, `RUN_SW=OFF`, 메인화면 조건을 모두 만족해 Lock/RUN 제어
명령을 받을 수 있다는 뜻이다.

`RUN_SW`가 ON으로 바뀌면 Local 제어가 우선되어 기존 Lock이 해제된다. PLC는
주소 `0x000A`와 `0x000B`를 주기적으로 읽는다. 이후 `RUN_SW`가 OFF되어 주소
`0x000A=0x0000`, `0x000B=0xFF00`이 되면 주소 `0x0004=0xFF00`을 다시 써서
통신 제어권을 얻어야 한다. 자동으로 Lock 또는 RUN을 복원하지 않는다.
PB1 가변저항과 정지 중 설정 버튼은 계속 사용할 수 있다. `PA11/RUN_SW`
반고정 스위치 상태 변경은 Local 제어권 인계로 처리하여 Lock을 해제한다.
`PB9/START_STOP` 장기 누름은 Auto-Tuning 3초 입력 전용이며 Emergency Stop에
사용하지 않는다. 긴급 출력 정지와 Local 전환은 `PA11/RUN_SW`를 토글한다.
정상적인 Modbus RUN OFF는 출력만 정지하고 Lock과 `485 COMM` 준비 상태를 유지한다.
다시 `0x0005=True`를 쓰면 별도의 재진입 명령 없이 운전을 시작할 수 있다.
Local 제어로 복귀하려면 `0x0004=False`를 쓰거나 `PA11/RUN_SW`를 토글한다.
Lock은 RAM에만 유지되어 전원 재인가 시에도 False로 복귀한다.

통신 운전의 기본 출력은 Lock ON 순간 인계한 Local PB1 볼륨값이다. PLC가 별도의
출력 설정이 필요하지 않으면 주소 `0x0001` 쓰기를 생략하고 주소 `0x0005=True`로
바로 운전한다. PLC 지정 출력이 필요할 때만 주소 `0x0001=0~500`을 쓴 뒤 RUN한다.
통신 운전 중 PB1이 마지막 기준점에서 약 5% 이상 움직이면 Local 볼륨값으로
수동 인계되며, 이후 작은 변화도 연속 반영한다. PLC가 주소 `0x0001`을 다시 쓰면
PLC 지정 출력이 다시 우선되고 새로운 PB1 기준점이 설정된다.

Sweep 사용 여부는 본체의 `PA12/SWEEP_SW` 물리 스위치로만 선택한다. 주소
`0x0007`은 해당 스위치의 디바운스된 상태를 운전 여부와 관계없이 반환하는 읽기
전용 Register다. 통신으로 이 주소에 쓰면 Exception `0x02`가 반환된다.

설정 중심주파수, Sweep 폭, Sweep 속도는 각각 주소 `0x000C~0x000E`에서 읽을
수 있다. 세 값은 Local 보호 메뉴에서만 변경하며 통신 쓰기는 Exception `0x02`로
거부한다.

`ERROR_STATUS_RESET`은 오류 유무와 Reset을 담당한다. 원인은 `ERROR_CODE`에서
`0=None`, `1=TRANSDUCER ERROR`, `2=OVER CURRENT`로 구분한다. Reset은 출력 정지
상태에서 주소 `0x0008`에 `0xFF00`을 쓴다.

통신 Run은 `UltrasonicCtrl_StartUntimed()`를 사용한다. 따라서 보드에 저장된
M/S/CONT 운전시간과 관계없이 `0x0005=False`, 비상 정지 또는 전원 차단 전까지
유지된다. Local 자동 운전만 설정된 운전시간 타이머를 사용한다.

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
| USB 변환기 시험 | 2-Wire/Half-Duplex, NON-ECHO          |
| 19200 이하      | 11-bit character 기준 t1.5/t3.5 계산 |
| 19200 초과      | t1.5=750 us, t3.5=1.750 ms           |

설정은 Flash 마지막 2 KB page에 CRC/sequence record로 저장한다.

## 7. 현재 제한과 검증

- `FREQUENCY_VALUE`는 Firmware가 적용한 값이며 별도 주파수 Sensor 측정값은 아니다.
- Continuous에서는 설정 주파수, Sweep 운전 중에는 HRTIM에 적용한 순간 주파수를
  `×0.1 kHz`로 반환한다. PLC의 주파수 직접 쓰기는 허용하지 않는다.
- `OUTPUT_VALUE` Read는 현재 `0~500` 제어 지령을 반환한다. Local Lock ON 시
  현재 PB1 위치를 먼저 인계한다. 이후 PB1을 약 5% 움직이면 수동으로 인계되어
  작은 변화도 연속 반영하고, PLC가 다시 쓰면 PLC 값이 우선된다. RUN OFF 뒤에도 최신 명령을
  유지하며, Lock 해제 시 최신 PB1 위치의 Local 출력으로 복귀한다.
- `DISPLAY_OUTPUT_RANGE`는 PERCENT 설정이면 100, W 설정이면 300~2500을 반환한다.
- 통신 준비/OFF 화면에서는 출력 Level과 주파수를 숨기고, Modbus RUN ON 중에만
  `PL`과 주파수를 표시한다.
- Transducer 신호 손실과 Over current 보호는 같은 Error latch를 사용하고,
  `ERROR_CODE`로 원인을 구분한다.
- `0x0009~0x000B`는 Read-only이며 FC06/FC10 쓰기는 Exception 0x02로 처리한다.

출하 전 실제 PLC에서 FC03/FC06, Local Lock 순서, Error Reset, 모든 Baud와
전원 재인가 후 통신 설정 복원을 검증한다.
