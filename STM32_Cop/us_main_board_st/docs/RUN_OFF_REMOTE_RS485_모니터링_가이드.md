# RUN_SW OFF + REMOTE 운전 + RS485 모니터링 가이드

## 1. 목적

이 문서는 다음과 같은 운전 방법을 설명합니다.

- 보드의 `RUN_SW`는 `OFF` 위치로 둡니다.
- 외부 `REMOTE` 접점 신호로 초음파 출력을 `ON/OFF` 합니다.
- PLC 또는 PC는 RS485 Modbus RTU 통신으로 운전 상태를 읽습니다.
- RS485에서는 상태만 읽고, 출력 운전 명령은 보내지 않습니다.

결론적으로 이 운전 방법은 현재 펌웨어에서 사용할 수 있습니다.

## 2. 필요한 설정

| 항목 | 설정 | 설명 |
|---|---:|---|
| `RUN_SW` | `OFF` | 외부 `REMOTE` 입력을 사용하는 조건입니다. |
| `LOCAL_INPUT_LOCK` (`0x0004`) | `0x0000` | 반드시 Local Lock을 해제한 상태로 사용합니다. |
| `REMOTE` 입력 (`PA10`) | 접점 입력 | Active Low 방식입니다. |
| RS485 | FC03 읽기 | 상태 확인 용도로만 사용합니다. |

> **중요:** `LOCAL_INPUT_LOCK`에 `0xFF00`을 쓰면 기존 Local/REMOTE 출력이 즉시 정지하고, 그 이후에는 `PA10 REMOTE` 입력이 무시됩니다. 물리 REMOTE 접점으로 운전할 때는 `LOCAL_INPUT_LOCK`을 설정하지 마십시오.

## 3. REMOTE 입력 동작

`REMOTE` 입력은 Active Low 방식입니다.

| REMOTE 입력 상태 | 보드 동작 |
|---|---|
| 접점 ON / 입력 Low | 출력 운전 ON |
| 접점 OFF / 입력 High | 출력 운전 OFF |

입력 노이즈에 의한 오동작을 줄이기 위해 약 30 ms의 디바운스 처리가 적용되어 있습니다.

에러 또는 보호 조건이 발생한 경우에는 REMOTE 입력이 ON이어도 실제 출력이 정지할 수 있습니다. 이때는 아래의 운전 상태와 에러 레지스터를 함께 확인해야 합니다.

## 4. RS485로 확인할 레지스터

PLC 또는 PC에서는 Modbus RTU `FC03 (Read Holding Registers)`으로 다음 값을 읽습니다.

| 주소 | 4xxxx 표기 | 이름 | 확인 내용 |
|---:|---:|---|---|
| `0x0000` | 40001 | `CURRENT_FREQUENCY` | 현재 주파수 설정값, 단위 0.1 kHz |
| `0x0001` | 40002 | `OUTPUT_VALUE` | 출력 지령값, 범위 0~500 |
| `0x0004` | 40005 | `LOCAL_INPUT_LOCK` | `0x0000`인지 확인 |
| `0x0005` | 40006 | `RUN_STATUS_CONTROL` | 실제 논리 운전 상태: OFF=`0x0000`, ON=`0xFF00` |
| `0x0006` | 40007 | `EXTERNAL_INPUT_STATUS` | 물리 REMOTE 입력 상태: OFF=`0x0000`, ON=`0xFF00` |
| `0x0008` | 40009 | `ERROR_STATUS` | 정상=`0x0000`, 에러=`0xFF00` |
| `0x0009` | 40010 | `ERROR_CODE` | 발생한 에러 코드 |
| `0x000A` | 40011 | `RUN_SWITCH_STATUS` | RUN 스위치 상태: OFF=`0x0000`, ON=`0xFF00` |
| `0x000B` | 40012 | `COMM_CONTROL_READY` | 통신 제어 준비 상태 |

한 번에 상태를 읽으려면 다음과 같이 요청할 수 있습니다.

- Function: `FC03`
- Start Address: `0x0000`
- Quantity: `12` (`0x0000`~`0x000B`)
- 권장 Polling 주기: 100 ms 이상

## 5. 상태 판정 방법

### REMOTE 접점이 OFF인 경우

- `EXTERNAL_INPUT_STATUS (0x0006)` = `0x0000`
- `RUN_STATUS_CONTROL (0x0005)` = `0x0000`

### REMOTE 접점이 ON이고 정상 운전 중인 경우

- `EXTERNAL_INPUT_STATUS (0x0006)` = `0xFF00`
- `RUN_STATUS_CONTROL (0x0005)` = `0xFF00`

### REMOTE 접점은 ON인데 출력이 정지한 경우

- `EXTERNAL_INPUT_STATUS (0x0006)` = `0xFF00`
- `RUN_STATUS_CONTROL (0x0005)` = `0x0000`
- `ERROR_STATUS (0x0008)`와 `ERROR_CODE (0x0009)`를 확인합니다.

## 6. OUTPUT_VALUE 해석 시 주의사항

`OUTPUT_VALUE (0x0001)`는 실제 측정 전력이나 실제 측정 전류가 아닙니다. 보드에 설정된 **출력 지령값**입니다.

또한 출력이 정지해도 마지막 출력 지령값은 유지될 수 있습니다. 따라서 출력의 실제 ON/OFF 여부를 `OUTPUT_VALUE`만 보고 판단하면 안 됩니다.

- 출력 ON/OFF 판단: `RUN_STATUS_CONTROL (0x0005)` 사용
- REMOTE 접점 상태 판단: `EXTERNAL_INPUT_STATUS (0x0006)` 사용
- 출력 설정값 확인: `OUTPUT_VALUE (0x0001)` 사용

2026-10-16 변경본부터 40004(`0x0003`, FC03, 수량 1)에서 PA6 CT 실측 입력 소비전류를 A ×100으로 읽습니다(523 = 5.23 A). LCD와 동일한 1초 평균 RMS이며 200 ms마다 갱신합니다. Lock 설정 없이 읽을 수 있고 정지 중에도 최근 측정값을 반환합니다. 실제 출력 전력 측정값은 제공하지 않습니다.

## 7. PLC 프로그램 작성 시 주의사항

이 운전 방식에서는 PLC가 다음 쓰기 명령을 보내지 않도록 설정하는 것이 안전합니다.

- `LOCAL_INPUT_LOCK (0x0004)`에 `0xFF00` 쓰기 금지
- `RUN_STATUS_CONTROL (0x0005)`에 운전 명령 쓰기 금지

즉, PLC는 `FC03` 읽기만 수행하고, 출력 ON/OFF는 현장의 물리 `REMOTE` 접점이 담당하도록 역할을 분리합니다.

RS485 통신이 끊기더라도 물리 REMOTE 접점 운전은 계속될 수 있습니다. 통신 두절 시 반드시 출력을 정지해야 하는 설비라면 PLC 통신 상태만으로 처리하지 말고 별도의 하드웨어 인터록 또는 안전 회로를 구성해야 합니다.

## 8. 현장 문의에 대한 간단한 답변 예시

> 가능합니다. RUN 스위치를 OFF로 두고 Local Input Lock을 사용하지 않으면, 외부 REMOTE 접점으로 출력을 ON/OFF할 수 있습니다. RS485에서는 FC03으로 REMOTE 입력 상태(`0x0006`)와 실제 운전 상태(`0x0005`)를 읽어 확인하면 됩니다. `OUTPUT_VALUE(0x0001)`는 설정된 출력 지령값이며, 실측 전류는 `MEASURED_CURRENT(0x0003, 40004)`에서 읽습니다. 전류는 PA6 CT 입력 소비전류(A ×100)이고 실제 전력 측정값은 제공하지 않습니다.

