# STM32G474CBT6 초음파 제어 보드
# Modbus RTU PLC 사용자 가이드

| 문서 항목 | 내용 |
| --- | --- |
| 대상 | PLC/HMI/SCADA 프로그램 작성자 |
| 통신 | RS-485 2-wire, Modbus RTU |
| Firmware | v1.2.0 |
| 문서 버전 | 1.3 |
| 작성일 | 2026-10-08 |

## 1. 문서 범위

이 문서는 기존 ATmega16A 보드의 `통신 Protocol.pdf`에 정의된 명령 항목을
Modbus RTU Holding Register로 옮긴 PLC용 통신 사양서다.

PLC에 공개하는 항목은 다음으로 제한한다.

- 발진 주파수 읽기
- 출력값 읽기/지정
- 표시 출력 범위 읽기
- Sweep 주파수 읽기
- PLC 제어 허가 및 외부 REMOTE 입력 차단 상태 읽기/지정
- 출력 동작 상태 읽기/지정
- 외부 입력 동작 상태 읽기
- 본체 Sweep 스위치 상태 읽기
- 설정 중심주파수, Sweep 폭, Sweep 속도 읽기
- Error 상태 읽기/Reset
- Error 원인(Transducer/Over current) 읽기
- RUN 스위치 상태와 통신 제어 가능 상태 읽기
- Modbus 연결 확인에 필요한 통신 설정 읽기

Frequency와 Duty의 직접 설정값은 보드에서만 변경한다. PLC에는
`FREQ_SET`, `DUTY_SET` Register를 제공하지 않는다.

## 2. 빠른 시작

| 항목 | 출하 기본값 |
| --- | --- |
| PLC/QModMaster Mode | Modbus RTU Client/Master |
| 보드 Mode | Modbus RTU Server/Slave |
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

### 2.1 핵심 기능 빠른 표

아래 주소는 `Base Address=0` 기준이다. QModMaster의 FC06 쓰기 시험에서는
`Scan`을 중지한 상태에서 값을 입력하고 `Commands > Write`를 한 번 실행한다.
통신 제어를 시작하려면 `PB3/485BD_DETECT=LOW`로 옵션보드가 감지되고,
`PA11/RUN_SW=OFF`이며 LCD가 메인화면인 조건을 먼저 만족해야 한다.

| 중요 기능 | Function | Start Address | 값 | 결과 |
| --- | --- | ---: | ---: | --- |
| 통신 준비(Local Lock ON) | FC06 | 4 | 65280 (`0xFF00`) | 기존 출력 정지, `485 COMM` 표시 |
| 출력 조정 | FC06 | 1 | 0~500 | PA7 출력 지령 5~100% 조정 |
| RUN ON | FC06 | 5 | 65280 (`0xFF00`) | 타이머 없이 계속 운전 |
| RUN OFF | FC06 | 5 | 0 | 즉시 정지, Lock과 `485 COMM` 준비 상태 유지 |
| Lock 해제 | FC06 | 4 | 0 | RUN 전 취소 시 Local 조작 복구 |
| 전체 상태 읽기 | FC03 | 0 | 수량 15 | 주파수, 출력, 운전, Error, 제어 준비와 Sweep 설정 반환 |

`Start Address=4`에 `65280`을 쓰는 동작은 RUN ON이 아니다. Local Lock만
설정되며, 이 요청으로 표시되는 `485 COMM`은 통신 표시일 뿐 출력 동작 표시가
아니다. 실제 출력 시작은 반드시 `Start Address=5`, Value `65280`으로 쓴다.
Lock을 해제하면 최근 통신 Frame 표시를 거치지 않고 즉시 Local 메인화면으로
복귀한다. 운전 중 `485 ON`과 둘째 줄의 `xx.xkHz`는 LCD 오른쪽 끝에 맞춰 표시한다.

## 3. RS-485 연결

- PLC와 보드의 RS-485 `A`, `B`, 신호 기준 `GND`를 연결한다.
- USB-RS485 변환기는 `2-Wire/Half-Duplex`로 설정한다. `ECHO/NON-ECHO`
  선택형 변환기는 `NON-ECHO`를 사용한다.
- A/B 표기는 제조사마다 반대인 경우가 있으므로 양쪽 설명서를 확인한다.
- Bus의 양 끝에만 120 ohm 종단저항을 사용한다.
- 같은 Bus의 장치는 Baud/Parity/Stop bit를 동일하게 맞춘다.
- Slave ID는 장치마다 다르게 설정한다.
- 요청의 응답 또는 Timeout 처리가 끝난 후 다음 요청을 보낸다.

## 4. 보드에서만 설정하는 항목

### 4.1 Frequency와 Duty

Frequency는 보드 설정을 사용한다. Local 운전의 출력은 `PB1/PWM_VR`
가변저항으로 설정한다. 통신 운전도 기본적으로 Lock 순간 인계된 PB1 값을
사용하고, PLC 출력 지정이 필요할 때만 `OUTPUT_VALUE`를 쓴다.

| 보드 설정 | 의미 | PLC에서 확인하는 방법 |
| --- | --- | --- |
| Frequency | 발진 주파수, ×0.1 kHz | `FREQUENCY_VALUE(0x0000)` 읽기 |
| Local Duty | PB1/PWM_VR이 정하는 출력 Duty, ×0.1% | 통신 제어 전 `OUTPUT_VALUE`에서 확인 |

PLC의 `OUTPUT_VALUE(0x0001)`는 기존 Protocol과 같은 `0~500` 정규화 출력
명령이다. `LOCAL_INPUT_LOCK=0xFF00`을 쓰는 순간 현재 PB1 위치가 같은 비율의
`OUTPUT_VALUE`로 자동 인계되므로 출력이 갑자기 변하지 않는다. 그다음부터는
주소 1 쓰기를 생략하고 RUN하면 인계된 PB1 볼륨값으로 ON/OFF한다. PLC와 PB1 중
마지막으로 조작한 값이 실제 출력 지령이 된다. PLC가 값을 쓴 뒤에도 작업자가
PB1을 약 5% 이상 움직이면 수동 제어로 인계되고, 이후 작은
변화도 연속적으로 출력에 반영된다. PLC가 다시 값을 쓰면 PLC 값이 우선된다.
약 5%의 최초 판정 폭은 ADC 노이즈와 작은 접촉 변화를 사용자 조작으로 잘못
인식하지 않기 위한 값이다.

```text
기본 Local 볼륨 운전: Lock → 주소 1 쓰기 생략 → RUN
PLC 지정 출력 운전 : Lock → 주소 1에 0~500 쓰기 → RUN
```

```text
통신 적용 목표 Duty = 안전 Duty 상한(90.0%) × OUTPUT_VALUE / 500
```

예를 들어 `OUTPUT_VALUE=250`이면 통신 출력은 50%이며 내부 목표 Duty는
안전 상한의 50%인 45.0%다. PLC는 Firmware 안전 상한인 90.0%보다 높은
Duty를 만들 수 없다.

| OUTPUT_VALUE | 정규화 출력 | PA7 PWM 출력 |
| ---: | ---: | ---: |
| 0 | 0% | 5.0% |
| 250 | 50% | 52.5% |
| 500 | 100% | 100.0% |

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

Local 제어 중 LCD의 `PL`은 PB1 가변저항 위치를 표시한다. 통신 제어 중에는
실제 `OUTPUT_VALUE`를 0~100%로 환산해 표시한다. 작업자가 PB1을 약 5% 이상 움직이면
수동으로 인계된 뒤 연속 반영되고 1.5초 동안 `VR: xxx% MANUAL`을 표시한다.
이후 PLC가 출력값을 다시 쓰면 PLC 값이 다시 적용된다. 전력 범위가 W로
설정되면 실제 출력 비율을 설정된 최대 W로 환산한다.

주소와 CRC가 정상인 Modbus 요청만 들어오는 것으로는 화면을 변경하지 않는다.
Local Lock이 ON이면 기존 `TIME:xxM/S` 자리에 `485 COMM`을 계속 표시한다.
따라서 Lock 해제 시 중간 통신 화면을 거치지 않고 즉시 Local 메인화면으로 복귀한다.
통신 RUN 중에는 외부 `RemoteON` 대신 `485 ON`을 LCD 마지막 칸에 맞춰 계속
표시한다. Sweep 운전 중에는
`RUN|SWP → RUN/SWP → RUN-SWP → RUN\\SWP` 순서로 문자가 회전한다.

통신 준비 또는 OFF 상태에서는 둘째 줄에 출력 Level과 주파수를 표시하지 않고
`K-SONICS`를 표시한다. `RUN_STATUS_CONTROL=0xFF00`으로 실제 ON된 동안에만
둘째 줄에 `PL` 출력 Level과 설정 주파수를 표시한다.

## 5. Modbus 주소 입력 방법

`PDU`는 `Protocol Data Unit`의 약자다. Modbus RTU Frame은 다음처럼 구성된다.

```text
Slave ID | PDU(Function Code + Register 주소/값) | CRC
```

이 문서의 `PDU 주소`는 PDU 안에 실제로 전송되는 **0부터 시작하는 Register
offset**을 뜻한다. Slave ID나 전체 RTU Frame 주소를 뜻하지 않는다. PLC Software에
따라 이 값을 그대로 입력하거나 40001 방식으로 바꾸어 입력한다.

| PDU 주소(0-based) | 40001 방식 | PLC 입력 예 |
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
검증한 후 적용하며, Read-only Register가 포함되면 전체 요청을 거부한다.

## 7. Holding Register Map

모든 Register는 unsigned 16-bit다. `R/W`는 읽기/쓰기, `R`은 읽기 전용이다.

| PDU 주소(0-based) | 4xxxx | 이름 | 접근 | 값/단위 | 설명 |
| ---: | ---: | --- | :---: | --- | --- |
| `0x0000` | 40001 | `FREQUENCY_VALUE` | R | ×0.1 kHz | 현재 적용 발진 주파수 |
| `0x0001` | 40002 | `OUTPUT_VALUE` | R/W | 0~500 | 기존 Protocol 정규화 출력값 |
| `0x0002` | 40003 | `DISPLAY_OUTPUT_RANGE` | R | 100 또는 300~2500 | PERCENT이면 100, W 설정이면 최대 W |
| `0x0003` | 40004 | `SWEEP_FREQUENCY_VALUE` | R | ×0.1 kHz | Sweep 운전 중 현재 주파수, 아니면 0 |
| `0x0004` | 40005 | `LOCAL_INPUT_LOCK` | R/W | Boolean | PLC 제어 허가, PA10 REMOTE 차단 |
| `0x0005` | 40006 | `RUN_STATUS_CONTROL` | R/W | Boolean | 출력 Stop/Run, ON은 OFF 명령까지 유지 |
| `0x0006` | 40007 | `EXTERNAL_INPUT_STATUS` | R | Boolean | 외부 Remote 입력 상태 |
| `0x0007` | 40008 | `SWEEP_SWITCH_STATUS` | R | Boolean | 본체 SWEEP_SW 상태, 정지 중에도 읽기 가능 |
| `0x0008` | 40009 | `ERROR_STATUS_RESET` | R/W | Boolean | 읽기=Error 상태, True 쓰기=Reset |
| `0x0009` | 40010 | `ERROR_CODE` | R | 0~2 | 0=None, 1=Transducer, 2=Over current |
| `0x000A` | 40011 | `RUN_SWITCH_STATUS` | R | Boolean | `0000`=RUN_SW OFF, `FF00`=ON |
| `0x000B` | 40012 | `COMM_CONTROL_READY` | R | Boolean | Lock/RUN 제어 명령 허용 상태 |
| `0x000C` | 40013 | `CENTER_FREQUENCY` | R | ×0.1 kHz | Local 메뉴에 저장된 설정 중심주파수 |
| `0x000D` | 40014 | `SWEEP_WIDTH` | R | Hz | 중심주파수 기준 한쪽 Sweep 폭(±Hz) |
| `0x000E` | 40015 | `SWEEP_SPEED` | R | Hz | 설정 Sweep 속도 |
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

### 7.2 PLC 제어 허가와 REMOTE 입력 잠금 규칙

PLC에서 출력값 변경 또는 Run 시작을 하기 전에 반드시
`LOCAL_INPUT_LOCK=0xFF00`을 먼저 쓴다.

Modbus 상태 읽기는 `RUN_SW`가 ON이어도 허용한다. 출력 제어권을 얻는 Lock ON과
RUN ON 명령은 `COMM_CONTROL_READY=0xFF00`일 때만 실행한다. 이 값은 RS485
옵션보드 감지, `RUN_SW=OFF`, LCD 메인화면 조건을 모두 만족할 때 `0xFF00`이다.
RUN ON 전에는 `ERROR_STATUS_RESET=0x0000`도 함께 확인한다. Error가 있으면
`COMM_CONTROL_READY`가 `0xFF00`이어도 RUN ON 명령은 거부된다.

- RS485 옵션보드가 감지되고 `RUN_SW=OFF`, 메인화면일 때만 Lock ON을 허용한다.
- 최초 Lock ON 시 기존 Local/REMOTE 출력은 정지하고 통신 준비 상태가 된다.
- Lock 중에는 외부 `PA10/REMOTE`의 ON/OFF만 무시한다.
- PB1 가변저항은 계속 출력 조정에 사용할 수 있다.
- 출력 정지 중 MODE/UP/DOWN 설정은 그대로 사용할 수 있다.
- `PA11/RUN_SW` 반고정 스위치 상태를 바꾸면 Local 제어권을 우선하고 Lock을
  해제한다. `PB9/START_STOP`은 Lock 해제 버튼이 아니다.
- `PB9/START_STOP` 장기 누름은 Auto-Tuning 3초 입력에 사용하므로 일반 운전
  비상정지로 사용하지 않는다.
- 긴급 출력 정지와 Local 전환은 `PA11/RUN_SW` 반고정 스위치를 토글한다.
- Stop 명령은 Lock 여부와 관계없이 항상 허용한다.
- Error 상태에서는 Reset하기 전 Run 시작을 거부한다.
- 운전 중에도 `LOCAL_INPUT_LOCK=0x0000`을 써서 Unlock할 수 있다. Unlock 즉시
  최신 PB1 값을 출력에 적용하고 Local 접점과 버튼 입력을 다시 허용한다.
- 통신 프로그램 없이 보드에서 해제하려면 `PA11/RUN_SW` 반고정 스위치 상태를
  바꾼다.
- `RUN_STATUS_CONTROL=0x0000`을 쓰면 출력만 정지하고 Lock과 `485 COMM` 통신 준비
  상태를 유지한다. 따라서 주소 5에 `0xFF00`을 다시 쓰면 바로 재운전할 수 있다.
- 통신 제어를 끝내고 Local 가변저항·타이머·REMOTE·MODE 버튼 제어로 복귀하려면
  `LOCAL_INPUT_LOCK=0x0000`을 쓰거나 `PA11/RUN_SW` 상태를 바꾼다.
- RUN을 시작하기 전에 통신 제어를 취소하려면 Stop 상태에서
  `LOCAL_INPUT_LOCK=0x0000`을 쓴다.
- PLC 통신이 끊겨 Unlock할 수 없으면 전원을 재인가하면 Lock은 해제된다.
- 운전 중 RS485 옵션보드가 빠지면 출력을 정지하고 Lock을 해제한다.

#### RUN_SW를 Local ON에서 OFF로 되돌렸을 때

PLC는 주소 10 `RUN_SWITCH_STATUS`와 주소 11 `COMM_CONTROL_READY`를 계속 Scan한다.

1. 작업자가 `RUN_SW`를 ON하면 주소 10은 `65280`, 주소 11은 `0`이 되고 기존
   `LOCAL_INPUT_LOCK`은 자동 해제된다. Local 운전이 우선한다.
2. 작업자가 `RUN_SW`를 OFF하면 주소 10이 `0`으로 돌아온다.
3. RS485 보드 감지와 메인화면 조건까지 만족하면 주소 11이 `65280`이 된다.
4. PLC는 이 두 값을 확인한 뒤 주소 4에 `65280`을 **다시 써야 한다.**
5. 주소 4가 `65280`로 읽히는 것을 확인한 후, 필요하면 Output을 쓰고 RUN한다.

Firmware는 RUN_SW OFF만으로 이전 Lock이나 RUN을 자동 복원하지 않는다. 따라서
PLC Scan Logic에 `주소 10=0 AND 주소 11=65280 → 주소 4 재설정` 절차를 넣어야
한다. RUN_SW ON 중에도 상태 읽기는 가능하지만 Lock ON, Output 변경, RUN ON은
허용되지 않는다.

`RUN_STATUS_CONTROL=0xFF00`으로 시작한 통신 운전은 보드의 M/S/CONT 운전시간
설정을 사용하지 않는다. 외부 `REMOTE` 수동 운전과 마찬가지로 PLC에서
`RUN_STATUS_CONTROL=0x0000`을 쓰거나 비상 정지가 발생해 OFF될 때까지 계속
운전한다.

### 통신 단절 안전 주의

현재 Firmware에는 통신 단절 Watchdog이 없다. RUN ON 후 PLC 전원이나 RS485
케이블이 끊겨도 보드는 계속 운전하므로 PLC는 정상 통신 복구 후 반드시 RUN
상태를 다시 읽어야 한다. 설비에서는 `PA11/RUN_SW` 토글 정지와 별도의
Hardware Interlock을 함께 사용한다.

### 7.3 주파수 읽기와 출력 조정

- `FREQUENCY_VALUE(0x0000)`는 Firmware가 현재 HRTIM에 적용한 주파수를
  `×0.1 kHz` 단위로 반환한다. 예를 들어 `280`은 28.0 kHz다. Sweep 운전
  중에는 변하는 순간 주파수를 반환하며 PLC에서 이 Register를 쓸 수는 없다.
- Sweep가 선택된 정지 상태에서는 다음 운전을 위해 `중심주파수-Sweep 폭`을
  HRTIM에 미리 적용한다. 예를 들어 중심 39.1 kHz, 폭 500 Hz이면 정지 상태의
  `FREQUENCY_VALUE`가 `386`(38.6 kHz)으로 읽힐 수 있으며 정상 동작이다.
- `SWEEP_FREQUENCY_VALUE(0x0003)`도 Sweep 운전 중 순간 주파수를 반환하고,
  Sweep 정지 또는 Continuous 운전 중에는 `0`을 반환한다.
- `CENTER_FREQUENCY(0x000C)`는 Local 보호 메뉴에 저장된 중심주파수다. 단위는
  `×0.1 kHz`이며 `391`은 39.1 kHz다.
- `SWEEP_WIDTH(0x000D)`는 중심주파수 기준 한쪽 폭을 Hz로 반환한다. `500`이면
  중심주파수에서 `±500 Hz` 범위로 Sweep한다.
- `SWEEP_SPEED(0x000E)`는 설정된 Sweep 속도를 Hz로 반환한다.
- 주소 `0x0000`, `0x0003`, `0x000C~0x000E`는 모두 읽기 전용이다. 중심주파수,
  Sweep 폭과 속도는 본체 Local 보호 메뉴에서만 변경할 수 있으며 통신 쓰기는
  Exception `0x02`로 거부한다.
- `OUTPUT_VALUE(0x0001)`는 측정 전력이 아니라 `0~500` 절대 출력 명령이다.
  쓰기 전에 `LOCAL_INPUT_LOCK(0x0004)=0xFF00`이 필요하다.
- Lock ON 순간에는 현재 PB1 출력 비율을 `OUTPUT_VALUE`로 자동 인계한다.
  주소 1 쓰기를 생략하면 이 Local 볼륨값으로 통신 RUN/STOP한다.
- PLC에서 별도 출력이 필요할 때만 주소 1에 `0~500`을 쓴다. 이후 PLC와 PB1 중
  마지막으로 조작한 값이 실제 출력에 적용된다.
- PLC 지정 출력으로 운전 중 PB1이 기준점에서 약 5% 이상 움직이면 Local 값으로
  수동 인계되고, 이후의 작은 PB1 변화도 연속 반영된다. PLC가 주소 1을 다시
  쓰면 PLC 값이 다시 우선된다.
- 출력 명령은 정지 중에는 다음 Start 값에 즉시 반영되고, 운전 중에는 다음
  제어 주기에 반영된다. 실제 적용 목표 Duty는 다음 제한식을 따른다.

```text
통신 적용 목표 Duty = 안전 Duty 상한(90.0%) × OUTPUT_VALUE / 500
```

`OUTPUT_VALUE`를 다시 읽으면 측정값이 아니라 현재 제어 지령을 반환한다.
RUN OFF 후에도 통신 제어 지령을 유지하고, Local Lock을 해제할 때 PB1 비율로
다시 갱신된다. 실제 출력 표시는 보드의 `PL`을 사용하며, PA6 CT 입력은 별도의
전류 측정 및 보호 입력이다.

### 7.4 Error 상태

Firmware v1.2.0에서 Error latch는 보드의 비상 정지 동작을 나타낸다.

| ERROR_CODE | LCD 표시 | 발생 조건 | 동작 |
| ---: | --- | --- | --- |
| 0 | 없음 | 정상 | 운전 허용 |
| 1 | `TRANSDUCER ERROR` | PA7 출력 50% 이상에서 PA6 CT 신호가 약 1초간 기준 이하 | PWM과 Signal 출력 정지, Error latch |
| 2 | `OVER CURRENT` | LCD와 동일한 1초 평균 RMS 전류가 7.00A 이상인 상태가 약 5초 지속 | PWM과 Signal 출력 정지, Error latch |

두 보호 기능은 Local 운전과 PLC 운전 모두에 적용된다. `TRANSDUCER ERROR`는
시작 후뿐 아니라 운전 중에도 감시한다. 실제 전류 환산은 CT와 부담저항 오차의
영향을 받으므로 출하 시험에서 외부 전류계와 함께 확인한다.

1. `ERROR_STATUS_RESET`을 읽어 `0xFF00`인지 확인한다.
2. `ERROR_CODE`를 읽어 원인을 확인한다.
   - `0`: 오류 없음
   - `1`: `TRANSDUCER ERROR`
   - `2`: `OVER CURRENT`
3. 먼저 `RUN_STATUS_CONTROL=0x0000`으로 정지한다.
4. 원인을 제거한다.
5. `ERROR_STATUS_RESET=0xFF00`을 써서 Reset한다.
6. 주소 8과 9를 다시 읽어 모두 `0`인지 확인한다.

향후 과전류/과온도 같은 Hardware Fault가 추가되더라도 같은 Register를 사용한다.

## 8. 권장 PLC 제어 순서

```text
통신 초기화
  ├─ Serial Port 설정
  ├─ FC03: 0x0010부터 3 Word 읽기
  └─ Slave ID/Baud/Parity 확인

상태 확인
  └─ FC03: 0x0000부터 15 Word 읽기

Remote 운전 준비
  ├─ FC03: 0x000A부터 2 Word 읽기
  │        RUN_SWITCH_STATUS=0, COMM_CONTROL_READY=0xFF00 확인
  ├─ FC03: 0x0008부터 2 Word 읽기
  │        ERROR_STATUS_RESET=0, ERROR_CODE=0 확인
  ├─ FC06: 0x0004 = 0xFF00  (기존 출력 정지 + 485 COMM 준비)
  └─ FC06: 0x0001 = 0~500   (필요한 경우 통신 출력 지정)

운전
  ├─ FC06: 0x0005 = 0xFF00  (Run)
  └─ FC03: 0x0000부터 15 Word를 100 ms 이상 주기로 읽기

  ※ 통신 Run은 보드 운전시간 타이머와 무관하게 OFF 명령까지 유지

정지/재운전
  ├─ FC06: 0x0005 = 0x0000  (Stop, Lock과 485 COMM 유지)
  └─ FC06: 0x0005 = 0xFF00  (필요할 때 다시 Run)

통신 제어 종료
  ├─ FC06: 0x0004 = 0x0000  (Local 복귀)
  └─ 필요 시 Error 확인/Reset
```

PLC 시작 시 이전 통신에서 남은 상태를 가정하지 말고 반드시 전체 상태를 읽는다.

### 8.1 PLC 운전 Sequence

PLC 프로그램은 한 번에 여러 명령을 보내지 않고, 각 요청의 정상 응답을 확인한
뒤 다음 단계로 진행한다.

| 단계 | 요청 | 정상 확인 | 실패 시 처리 |
| ---: | --- | --- | --- |
| 1 | FC03, 주소 16, 수량 3 | ID/Baud/Parity 일치 | Serial 설정과 Slave ID 확인 |
| 2 | FC03, 주소 0, 수량 15 | 주소 8=0, 10=0, 11=65280 | Error 및 RUN_SW/메인화면 확인 |
| 3 | FC06, 주소 4, 값 65280 | 같은 값 Echo, 주소 4=65280 | 조건 확인 후 재시도 |
| 4 | 선택: FC06, 주소 1, 값 0~500 | 같은 값 Echo | PLC 지정 출력이 필요할 때만 수행 |
| 5 | FC06, 주소 5, 값 65280 | 같은 값 Echo, 주소 5=65280 | Error와 Lock 상태 확인 |
| 6 | FC03, 주소 0, 수량 15 주기 읽기 | 운전·주파수·Sweep 설정·스위치·Error 감시 | Timeout 재시도 후 상태 재확인 |
| 7 | FC06, 주소 5, 값 0 | 주소 5=0 | 즉시 재시도, 현장 정지 수단 사용 |
| 8 | FC06, 주소 4, 값 0 | 주소 4=0 | Local 복귀 확인 |

PLC 재부팅 또는 통신 복구 직후에는 RUN ON 명령을 바로 보내지 않는다. 전체 상태를
다시 읽고, 기존 운전 상태와 Error 상태를 확인한 다음 운전 Sequence를 처음부터
수행한다.

Local RUN_SW가 ON되면 기존 Lock이 해제된다. PLC는 Scan 중 주소 10이 다시 `0`,
주소 11이 `65280`이 된 것을 확인한 후 단계 3부터 다시 수행해야 한다.

### 8.2 전원 재인가와 설정 복원

발진기 전원을 껐다 켜도 다음 보드 설정은 내장 Flash에서 자동 복원된다.

- Slave ID, Baud rate, Parity
- 선택 주파수 대역과 대역별 설정 주파수
- Sweep 폭과 속도
- Local 운전 시간
- PL 표시 범위와 RS485 종단저항 설정

다음 운전 명령은 안전을 위해 저장하지 않으며 전원 투입 시 OFF 또는 해제된다.

- `LOCAL_INPUT_LOCK`: 해제
- `RUN_STATUS_CONTROL`: OFF
- PLC `OUTPUT_VALUE`: 이전 PLC 명령을 자동 복원하지 않음
- Error latch: 초기화

따라서 전원 복구만으로 발진기가 자동 재운전하지 않는다. PLC가 발진기 부팅을 위해
약 3초 기다린 다음 주소 16의 통신 설정과 주소 0부터 15 Word의 전체 상태를 읽고,
PLC 또는 HMI에서 `Lock → [필요 시 Output] → RUN` Sequence를 자동
실행해야 한다. 이 Logic을 PLC 시작 프로그램에 넣으면 작업자가 발진기 메뉴를 다시
설정하지 않아도 이전 운전 조건으로 복귀할 수 있다.

Sweep 사용 여부는 발진기 본체의 물리 `SWEEP_SW`로만 선택한다. PLC는 주소 7을
읽어 `0=OFF`, `65280(0xFF00)=ON` 상태를 확인할 수 있으며, 출력 정지 중에도 실제
스위치 상태가 반환된다. 주소 7에 쓰기를 시도하면 Exception `0x02`가 응답된다.

## 9. Raw RTU Frame 예제

예제는 Slave ID 1 기준이다. 마지막 두 byte는 CRC Low, CRC High다. 일반적인
PLC Modbus Function Block은 CRC를 자동으로 생성한다.

```text
통신 설정 3 Word 읽기
01 03 00 10 00 03 04 0E

전체 상태/명령 15 Word 읽기
01 03 00 00 00 0F 05 CE

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

### 9.1 QModMaster 확인 절차

QModMaster는 `RTU`, 보드에 설정된 Slave ID, `8-E-1`, `RTS=Disable`,
`Base Address=0`으로 연결한다. USB-RS485 변환기는 `2-Wire/NON-ECHO`로 두고,
`Data Format=Dec`를 사용하면 Boolean True는 `65280(0xFF00)`으로 입력한다.
FC06 쓰기 중에는 `Scan`을 사용하지 않고 아래 명령을 한 번씩 실행한다.

1. FC03, Start Address `0`, Number of Registers `15`로 전체 상태를 읽는다.
   첫 값 `280`은 28.0 kHz이며 주소 9~11에서 오류 원인, RUN 스위치,
   통신 제어 가능 상태를 확인한다. 주소 12~14에서는 설정 중심주파수, Sweep 폭,
   Sweep 속도를 읽을 수 있다.
2. FC06, Start Address `4`, Value `65280`을 써서 Local 입력을 잠근다.
   이때 RS485 보드 감지, RUN_SW OFF, 메인화면 조건이 필요하다.
3. 기본 Local 볼륨 운전이면 주소 1 쓰기를 생략한다. PLC 지정 출력이 필요할 때만
   FC06, Start Address `1`, Value `0~500`을 써서 출력을 조정한다.
4. FC06, Start Address `5`, Value `65280`을 쓰면 현재 선택된 출력으로 타이머 없이 Run한다.
5. FC06, Start Address `5`, Value `0`을 쓰면 즉시 Stop하고 Local 입력 잠금과
   `485 COMM` 준비 상태는 유지한다.
6. 운전 상태를 유지하면서 Local로 넘기거나 RUN 전에 취소할 때는 주소 `4`에
   `0`을 쓴다. 보드에서는 `PA11/RUN_SW` 반고정 스위치 상태를 바꿔도 Lock이
   해제된다.
7. RUN_SW를 ON했다가 OFF하면 Scan에서 주소 10=`0`, 주소 11=`65280`을 확인하고
   주소 4=`65280`을 다시 쓴 다음 Output과 RUN 명령을 진행한다.

FC06 화면에서는 Start Address 아래 흰색 Register 셀을 선택해 값을 입력하고
`Commands > Write`를 실행한다. 통신 중 Bus Monitor에 TX와 RX가 모두 표시되어야
하며 RX가 없으면 USB 변환기의 2-Wire/NON-ECHO 설정을 먼저 확인한다.

## 10. Exception과 무응답

| Code | 이름 | 본 제품에서의 의미 |
| ---: | --- | --- |
| `0x01` | Illegal Function | 지원하지 않는 Function Code |
| `0x02` | Illegal Data Address | 없는 주소 또는 Read-only Register 쓰기 |
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
| `0x0203` | Sweep 스위치 상태 | `0x0007` | 읽기 전용, Boolean 유지 |
| `0x0204` | Error 상태/Reset | `0x0008` | 읽기와 Reset 겸용 |

기존 8-byte Custom Frame, Little-endian Word와 CRC-CCITT는 사용하지 않는다.
Modbus Register는 High byte 먼저이며 CRC-16/Modbus는 CRC Low byte를 먼저 보낸다.

## 12. PLC 프로그램 작성 완료 기준

다음 항목을 모두 만족하면 이 문서만으로 제품 공통 통신 Logic을 구성할 수 있다.

- PLC Serial Port Parameter 설정
- 0-based 또는 40001 주소 방식 확인
- FC03/FC06 요청과 응답/Timeout 처리
- `0x0000~0x000E` 상태 읽기
- Local Lock(PB1 초기값 인계) → 선택적 Output → Run → Stop → Lock 해제 순서 구현
- Exception `0x01~0x04`와 재시도 처리
- Boolean `0x0000/0xFF00`, Frequency ×0.1 kHz, Output 0~500 변환

단, PLC 제조사마다 Function Block 이름, Busy/Done/Error 신호와 주소 입력 방식이
다르므로 해당 PLC의 Modbus RTU Client 설명서는 별도로 필요하다. 실제 설비에서는
사용 PLC와 보드를 연결한 통신 시험 및 안전 Interlock 시험 후 배포한다.

## 13. 문서 개정 이력

| 문서 버전 | 날짜 | 변경 내용 |
| --- | --- | --- |
| 1.2 | 2026-09-14 | Modbus RTU 기본 운전과 레지스터맵 정리 |
| 1.3 | 2026-10-08 | Error/RUN_SW 복귀, 보호 절차, Sweep 설정 읽기, Local 볼륨 기본 통신 출력과 PB1 5% 수동 인계 추가 |

## 14. 표준 참고

- MODBUS Application Protocol Specification V1.1b3
- MODBUS over Serial Line Specification and Implementation Guide V1.02
- https://www.modbus.org/modbus-specifications

Register Map과 제품 값의 의미는 이 보드 전용 사양이다.
