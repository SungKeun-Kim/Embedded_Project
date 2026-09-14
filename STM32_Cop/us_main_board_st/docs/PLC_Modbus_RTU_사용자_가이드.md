# STM32G474CBT6 초음파 제어 보드
# Modbus RTU PLC 사용자 가이드

| 문서 항목 | 내용 |
| --- | --- |
| 대상 | PLC/HMI/SCADA 프로그램 작성자 |
| 통신 | RS-485 2-wire, Modbus RTU |
| Firmware | v1.2.0 |
| 문서 버전 | 1.2 |
| 작성일 | 2026-09-14 |

## 1. 문서 범위

이 문서는 기존 ATmega16A 보드의 `통신 Protocol.pdf`에 정의된 명령 항목을
Modbus RTU Holding Register로 옮긴 PLC용 통신 사양서다.

PLC에 공개하는 항목은 다음으로 제한한다.

- 발진 주파수 읽기
- 출력값 읽기/지정
- 표시 출력 범위 읽기
- Sweep 주파수 읽기
- Local 입력 금지 상태 읽기/지정
- 출력 동작 상태 읽기/지정
- 외부 입력 동작 상태 읽기
- Sweep 동작 상태 읽기/지정
- Error 상태 읽기/Reset
- Modbus 연결 확인에 필요한 통신 설정 읽기

Frequency와 Duty의 직접 설정값은 보드에서만 변경한다. PLC에는
`FREQ_SET`, `DUTY_SET` Register를 제공하지 않는다.

## 2. 빠른 시작

| 항목 | 출하 기본값 |
| --- | --- |
| Mode | Modbus RTU Client/Master |
| Slave ID | 1 |
| Baud rate | 9600 bps |
| Data bit | 8 |
| Parity | Even |
| Stop bit | 1 (`8-E-1`) |
| Response Timeout | 500 ms 권장 시작값 |
| Retry | 2회 권장 |
| Poll interval | 100 ms 이상 권장 |

보드에서 통신 설정을 변경한 경우 LCD에 표시되는 Slave ID와 Baud 및 고정
8-E-1 조건에 맞춰 PLC Port를 설정한다. 첫 연결은 FC03으로 PDU 주소 `0x0010`, 수량 3을
읽어 통신 설정을 확인한다.

## 3. RS-485 연결

- PLC와 보드의 RS-485 `A`, `B`, 신호 기준 `GND`를 연결한다.
- A/B 표기는 제조사마다 반대인 경우가 있으므로 양쪽 설명서를 확인한다.
- Bus의 양 끝에만 120 ohm 종단저항을 사용한다.
- 같은 Bus의 장치는 Baud/Parity/Stop bit를 동일하게 맞춘다.
- Slave ID는 장치마다 다르게 설정한다.
- 요청의 응답 또는 Timeout 처리가 끝난 후 다음 요청을 보낸다.

## 4. 보드에서만 설정하는 항목

### 4.1 Frequency와 Duty

Frequency는 보드 설정을 사용하고, Duty 상한은 `PB1/PWM_VR` 가변저항으로
설정한다. PLC는 둘을 직접 변경할 수 없다.

| 보드 설정 | 의미 | PLC에서 확인하는 방법 |
| --- | --- | --- |
| Frequency | 발진 주파수, ×0.1 kHz | `FREQUENCY_VALUE(0x0000)` 읽기 |
| Duty | PB1/PWM_VR이 정하는 최대 출력 Duty, ×0.1% | 직접 Register 없음 |

PLC의 `OUTPUT_VALUE(0x0001)`는 Duty를 직접 쓰는 Register가 아니다. 기존
Protocol과 같은 `0~500` 정규화 출력 명령이며, PB1에서 설정한 Duty를 최대
한도로 사용한다.

```text
실제 적용 목표 Duty = 보드 설정 Duty × OUTPUT_VALUE / 500
```

예를 들어 보드 Duty가 60.0%일 때 OUTPUT_VALUE가 250이면 목표 Duty는
약 30.0%다. PLC는 보드가 정한 안전 상한보다 높은 Duty를 만들 수 없다.

### 4.2 Supervisor 전력 표시와 RS485 설정

1. 전원을 끈다.
2. `MODE+DOWN`을 누른 상태로 전원을 넣어 `SUPERVISOR MODE`로 진입한다.
3. 버튼을 놓고 `MODE`를 눌러 `PL SETTING` 화면으로 이동한다.
4. `UP/DOWN`으로 `PERCENT` 또는 `300~2500W`를 100W 단위로 선택한다.
5. `PB3/485BD_DETECT=LOW`이면 `MODE`를 눌렀을 때 `BAUD`, `ADDR`,
   `TERM` 화면이 차례로 열린다.
6. 마지막 `TERM` 화면에서 `MODE`를 누르면 Flash에 저장하고 즉시 적용한다.

| 항목 | 선택값 |
| --- | --- |
| `ADDR` | Supervisor 화면에서 1~16 |
| `BAUD` | 9600 / 19200 / 38400 / 57600 / 115200 bps |
| `TERM` | UNMOUNTED / MOUNTED (`PB2/RTERM` OFF/ON) |
| Frame | 8-E-1 고정 |

설정은 Flash에 저장되어 전원을 꺼도 유지된다. PLC에서 `0x0010~0x0012`를
읽을 수 있지만 쓸 수는 없다. 기존 v1~v3 Flash에 None/Odd가 저장되어 있더라도
현재 Firmware에서는 안전하게 8-E-1로 통일한다.

### 4.3 LCD PL 및 RS485 통신 표시

LCD의 `PL`은 PB1 가변저항 값만 표시하지 않고 실제 출력 명령과 동일하게
`PB1 설정 × OUTPUT_VALUE / 500`으로 계산한다. 전력 범위가 W로 설정되면 이
비율을 설정된 최대 W로 환산한다.

`RUN_SW=OFF` 상태에서 주소와 CRC가 정상인 Modbus 요청을 받으면 첫째 줄에
`485 COMM`을 표시한다. 마지막 정상 요청 후 1.5초 동안 유지되므로 PLC가
100ms 이상 주기로 Polling하면 계속 통신 중으로 표시된다. Sweep 운전 중에는
`RUN|SWP → RUN/SWP → RUN-SWP → RUN\\SWP` 순서로 문자가 회전한다.

## 5. Modbus 주소 입력 방법

Frame은 0부터 시작하는 PDU 주소를 사용한다. PLC Software에 따라 PDU offset
또는 40001 reference를 입력한다.

| PDU 주소 | 40001 방식 | PLC 입력 예 |
| ---: | ---: | --- |
| `0x0000` | 40001 | 0 또는 40001 |
| `0x0001` | 40002 | 1 또는 40002 |
| `0x0010` | 40017 | 16 또는 40017 |

주소가 한 칸씩 어긋나면 PLC의 0-based/1-based 주소 방식을 확인한다.

## 6. 지원 Function Code

| Code | 이름 | 용도 |
| ---: | --- | --- |
| `0x03` | Read Holding Registers | 값과 상태 읽기 |
| `0x06` | Write Single Register | 제어 명령 1개 쓰기 |
| `0x10` | Write Multiple Registers | 연속 Register 다중 쓰기 |

본 제품은 제어 순서가 명확한 FC06 사용을 권장한다. FC10은 모든 주소와 값을
검증한 후 적용하며, R Register나 예비 주소가 포함되면 전체 요청을 거부한다.

## 7. Holding Register Map

모든 Register는 unsigned 16-bit다. `R/W`는 읽기/쓰기, `R`은 읽기 전용이다.

| PDU | 4xxxx | 이름 | 접근 | 값/단위 | 설명 |
| ---: | ---: | --- | :---: | --- | --- |
| `0x0000` | 40001 | `FREQUENCY_VALUE` | R | ×0.1 kHz | 현재 적용 발진 주파수 |
| `0x0001` | 40002 | `OUTPUT_VALUE` | R/W | 0~500 | 기존 Protocol 정규화 출력값 |
| `0x0002` | 40003 | `DISPLAY_OUTPUT_RANGE` | R | 100 또는 300~2500 | PERCENT이면 100, W 설정이면 최대 W |
| `0x0003` | 40004 | `SWEEP_FREQUENCY_VALUE` | R | ×0.1 kHz | Sweep 운전 중 현재 주파수, 아니면 0 |
| `0x0004` | 40005 | `LOCAL_INPUT_LOCK` | R/W | Boolean | Local 입력 금지 |
| `0x0005` | 40006 | `RUN_STATUS_CONTROL` | R/W | Boolean | 출력 Stop/Run |
| `0x0006` | 40007 | `EXTERNAL_INPUT_STATUS` | R | Boolean | 외부 Remote 입력 상태 |
| `0x0007` | 40008 | `SWEEP_STATUS_CONTROL` | R/W | Boolean | Continuous/Sweep 선택 및 상태 |
| `0x0008` | 40009 | `ERROR_STATUS_RESET` | R/W | Boolean | 읽기=Error 상태, True 쓰기=Reset |
| `0x0009` | 40010 | `RESERVED_1` | - | - | 향후 명령용, 현재 접근 금지 |
| `0x000A` | 40011 | `RESERVED_2` | - | - | 향후 명령용, 현재 접근 금지 |
| `0x000B` | 40012 | `RESERVED_3` | - | - | 향후 명령용, 현재 접근 금지 |
| `0x0010` | 40017 | `MODBUS_SLAVE_ID` | R | 1~16 | Supervisor에서 설정 |
| `0x0011` | 40018 | `MODBUS_BAUD_INDEX` | R | 0~4 | 아래 표 참조 |
| `0x0012` | 40019 | `MODBUS_PARITY` | R | 1 | Even, 8-E-1 고정 |

Baud index는 다음과 같다.

| Index | Baud rate |
| ---: | ---: |
| 0 | 9600 |
| 1 | 19200 |
| 2 | 38400 |
| 3 | 57600 |
| 4 | 115200 |

### 7.1 Boolean 표현

기존 Protocol과 호환되도록 Boolean은 다음 값을 사용한다.

| 의미 | Register 값 |
| --- | ---: |
| False / OFF / 허용 | `0x0000` |
| True / ON / 금지 | `0xFF00` |

`1`은 True가 아니며 Exception `0x03`을 반환한다.

### 7.2 Local 입력 잠금 규칙

PLC에서 출력값 변경, Run 시작 또는 Sweep 시작을 하기 전에 반드시
`LOCAL_INPUT_LOCK=0xFF00`을 먼저 쓴다.

- Lock 중에는 보드의 짧은 버튼 입력을 무시한다.
- `START/STOP` 장기 누름 비상 정지는 Lock 중에도 동작한다.
- Stop 명령은 Lock 여부와 관계없이 항상 허용한다.
- Error 상태에서는 Reset하기 전 Run 시작을 거부한다.
- 운전 중 Unlock은 거부한다. Stop 확인 후 `LOCAL_INPUT_LOCK=0x0000`으로 Local 조작을 복구한다.
- PLC 통신이 끊겨 Unlock할 수 없으면 전원을 재인가하면 Lock은 해제된다.

### 7.3 Error 상태

Firmware v1.2.0에서 Error latch는 보드의 비상 정지 동작을 나타낸다.

1. `ERROR_STATUS_RESET`을 읽어 `0xFF00`인지 확인한다.
2. 먼저 `RUN_STATUS_CONTROL=0x0000`으로 정지한다.
3. 원인을 제거한다.
4. `ERROR_STATUS_RESET=0xFF00`을 써서 Reset한다.
5. 다시 읽어 `0x0000`인지 확인한다.

향후 과전류/과온도 같은 Hardware Fault가 추가되더라도 같은 Register를 사용한다.

## 8. 권장 PLC 제어 순서

```text
통신 초기화
  ├─ Serial Port 설정
  ├─ FC03: 0x0010부터 3 Word 읽기
  └─ Slave ID/Baud/Parity 확인

상태 확인
  └─ FC03: 0x0000부터 9 Word 읽기

Remote 운전 준비
  ├─ FC06: 0x0004 = 0xFF00  (Local 입력 잠금)
  ├─ FC06: 0x0001 = 0~500   (출력 지정)
  └─ 필요 시 0x0007 = 0xFF00 (Sweep 선택)

운전
  ├─ FC06: 0x0005 = 0xFF00  (Run)
  └─ FC03: 0x0000부터 9 Word를 100 ms 이상 주기로 읽기

종료
  ├─ FC06: 0x0005 = 0x0000  (Stop)
  ├─ 필요 시 Error 확인/Reset
  └─ FC06: 0x0004 = 0x0000  (Local 입력 허용)
```

PLC 시작 시 이전 통신에서 남은 상태를 가정하지 말고 반드시 전체 상태를 읽는다.

## 9. Raw RTU Frame 예제

예제는 Slave ID 1 기준이다. 마지막 두 byte는 CRC Low, CRC High다. 일반적인
PLC Modbus Function Block은 CRC를 자동으로 생성한다.

```text
통신 설정 3 Word 읽기
01 03 00 10 00 03 04 0E

상태/명령 9 Word 읽기
01 03 00 00 00 09 85 CC

Local 입력 잠금
01 06 00 04 FF 00 89 FB

출력값 250 지정
01 06 00 01 00 FA 58 49

Run 시작
01 06 00 05 FF 00 D8 3B

Stop
01 06 00 05 00 00 99 CB

Error Reset
01 06 00 08 FF 00 49 F8

Local 입력 잠금 해제
01 06 00 04 00 00 C8 0B
```

## 10. Exception과 무응답

| Code | 이름 | 본 제품에서의 의미 |
| ---: | --- | --- |
| `0x01` | Illegal Function | 지원하지 않는 Function Code |
| `0x02` | Illegal Data Address | 없는 주소, R Register 쓰기, 예비 주소 접근 |
| `0x03` | Illegal Data Value | 값/수량 오류, Lock 없이 원격 시작·출력 변경 |
| `0x04` | Server Device Failure | 내부 처리 실패 |

CRC 오류, 다른 Slave ID 또는 손상된 Frame에는 응답하지 않는다. PLC는 이를
Response Timeout으로 처리한다. Broadcast ID 0은 FC06/FC10 쓰기만 처리하고
응답하지 않는다. 여러 장치를 동시에 운전할 수 있으므로 Broadcast Run은 사용하지
않는 것을 권장한다.

## 11. 기존 Protocol과 값 대응

| 기존 명령 | 기존 항목 | Modbus PDU | 변경점 |
| ---: | --- | ---: | --- |
| `0x0100` | 발진 주파수 | `0x0000` | 읽기 전용, ×0.1 kHz 유지 |
| `0x0102` | 출력 | `0x0001` | R/W, 0~500 유지 |
| `0x0103` | 표시 출력 범위 | `0x0002` | 읽기 전용, 100% 또는 설정된 300~2500W |
| `0x0104` | Sweep 주파수 | `0x0003` | 운전 중 현재값 제공 |
| `0x0200` | Local 입력 금지 | `0x0004` | R/W, Boolean 유지 |
| `0x0201` | 출력 동작 상태 | `0x0005` | R/W, Boolean 유지 |
| `0x0202` | 외부 입력 상태 | `0x0006` | 읽기 전용 |
| `0x0203` | Sweep 동작 상태 | `0x0007` | R/W, Boolean 유지 |
| `0x0204` | Error 상태/Reset | `0x0008` | 읽기와 Reset 겸용 |

기존 8-byte Custom Frame, Little-endian Word와 CRC-CCITT는 사용하지 않는다.
Modbus Register는 High byte 먼저이며 CRC-16/Modbus는 CRC Low byte를 먼저 보낸다.

## 12. PLC 프로그램 작성 완료 기준

다음 항목을 모두 만족하면 이 문서만으로 제품 공통 통신 Logic을 구성할 수 있다.

- PLC Serial Port Parameter 설정
- 0-based 또는 40001 주소 방식 확인
- FC03/FC06 요청과 응답/Timeout 처리
- `0x0000~0x0008` 상태 읽기
- Local Lock → Output → Run → Stop → Unlock 순서 구현
- Exception `0x01~0x04`와 재시도 처리
- Boolean `0x0000/0xFF00`, Frequency ×0.1 kHz, Output 0~500 변환

단, PLC 제조사마다 Function Block 이름, Busy/Done/Error 신호와 주소 입력 방식이
다르므로 해당 PLC의 Modbus RTU Client 설명서는 별도로 필요하다. 실제 설비에서는
사용 PLC와 보드를 연결한 통신 시험 및 안전 Interlock 시험 후 배포한다.

## 13. 표준 참고

- MODBUS Application Protocol Specification V1.1b3
- MODBUS over Serial Line Specification and Implementation Guide V1.02
- https://www.modbus.org/modbus-specifications

Register Map과 제품 값의 의미는 이 보드 전용 사양이다.
