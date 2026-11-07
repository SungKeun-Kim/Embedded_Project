# 40004 실측 전류 업데이트 (2026-10-16)

## 포함 파일

- `US_MAIN_BOARD_UPDATE_V1.2.0.hex`: 기존 설정을 보존하는 업데이트 펌웨어.
- `program_update_V1.2.0_40004_CURRENT_20261016.bat`: 이 HEX 전용 ST-LINK 업로드 실행 파일.
- `STM32G474CBT6_Modbus_RTU_PLC_사용자_가이드_v1.4.pdf`: PLC 통신 사양서.
- `SHA256SUMS.txt`: 배포 파일 무결성 확인값.

BAT가 HEX를 대신하는 것은 아니다. BAT가 STM32CubeProgrammer를 실행하여
같은 폴더의 HEX를 보드에 기록한다. 압축을 푼 뒤 폴더 전체를 함께 보관한다.

## 사용 순서

1. PC에 STM32CubeProgrammer를 설치하고 ST-LINK와 보드 전원을 연결한다.
2. 초음파 출력을 STOP하고 REMOTE 및 PLC RUN 명령을 OFF로 둔다.
3. CubeProgrammer GUI가 열려 있으면 종료한다.
4. `program_update_V1.2.0_40004_CURRENT_20261016.bat`을 더블클릭한다.
5. 출력 정지 상태를 확인한 뒤 `Y`를 누른다. 취소하려면 `N`을 누른다.
6. Writing, Verify, Reset/Run 성공 및 마지막 `[PASS]`를 확인한다.
7. 기존 설정과 LCD를 확인하고 PLC에서 40004를 읽어 전류 표시와 비교한다.

BAT는 이 배포본의 HEX SHA-256을 먼저 검사한다. 다른 HEX로 바뀌면 업로드를
거부한다. `Full chip erase` 명령을 사용하지 않으며 Factory HEX는 포함하지 않는다.
UPDATE HEX에는 설정 Page `0x0800F800`이 포함되지 않는다.

Programmer를 찾지 못하면 STM32CubeProgrammer를 설치하거나
`STM32_PROGRAMMER_CLI` 환경변수에 `STM32_Programmer_CLI.exe` 전체 경로를
따옴표 없이 지정한다. 연결 실패 시 전원, SWDIO, SWCLK, NRST, GND를 확인한다.

## 프로토콜 변경

| 항목 | 내용 |
| --- | --- |
| 40002 / `0x0001` | 기존 출력 지령 0~500 읽기/쓰기 유지 |
| 40004 / `0x0003` | Sweep 주파수 대신 CT 실측 전류(A ×100) 읽기 |
| 읽기 방법 | FC03, 시작 주소 3(0-based), 수량 1 |
| 환산 | 523 = 5.23 A, PLC 표시 시 수신값 ÷100.0 |
| 측정값 | LCD와 동일한 PA6 CT 입력 소비전류의 1초 평균 RMS |
| 갱신 주기 | 200 ms |

정지 시에도 최근 측정값을 반환하므로 평균 필터의 이전 값이나 대기 소비전류가
읽힐 수 있다. 운전 여부는 40006에서 확인한다. Sweep 순간 주파수는 40001에서
계속 읽을 수 있다.

펌웨어 Release Build와 `test_host` 전체 시험은 통과했다.
이 배포 작업에서 실제 보드 업로드 및 PLC 통신 시험은 수행하지 않았다.
