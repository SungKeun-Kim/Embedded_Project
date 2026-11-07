# STM32CubeProgrammer 펌웨어 파일

**2026-10-14 GDT 일반 정지 시험본:** 이 폴더 바로 아래의
`US_MAIN_BOARD_UPDATE_V1.2.0.hex`에 적용했다. 기존 PA7 차단에 더해 일반 STOP 시
주기 경계 검출과 GDT 1차 0 V 유지 200 us를 적용한다. 보호 정지는 즉시 차단한다.
상세 변경과 검증 범위는 [`GDT_일반정지_시험본_20261014.md`](GDT_일반정지_시험본_20261014.md)를 확인한다.
기존 `FIELD_UPDATE_*` 하위 폴더와 `US_MAIN_BOARD_FIELD_UPDATE_V1.2.0.zip`은 이전
배포본이므로 이번 수정 검증에는 사용하지 않는다. 이번 시험본의 Writing/Verify/
Reset/Run, 설정 보존 및 대기 상태 PA7/SD LOW 읽기 확인을 완료했다.
출력 ON/OFF와 정지 과도 파형 실측은 아직 수행하지 않았다.

2026-10-14 트랜스듀서 감지 2.5초 설정 반영본(200ms 주기로 약 2.6초 감지)의 GUI/BAT 설정은
`FIELD_UPDATE_V1.2.0_20261014_2500ms/ST_프로그래머_2.5초감지_업데이트_20261014.docx` 또는 같은 이름의 `.md`를 확인한다.
`US_MAIN_BOARD_FIELD_UPDATE_V1.2.0.zip`에는 업데이트 HEX, 업로드 BAT, 안내서와 SHA-256 목록이 포함되어 있다.
과전류 조건은 1초 평균 RMS 전류 7.00A 이상이 약 5초 지속될 때이며 기존 조건을 유지한다.

## 업데이트용

`US_MAIN_BOARD_UPDATE_V1.2.0.hex`

- 기존에 사용 중인 보드의 펌웨어 업데이트에 사용한다.
- Application 영역만 포함한다.
- 마지막 2 KB 설정 Page를 포함하지 않으므로 주파수, Sweep 폭/속도, 운전시간,
  Slave ID, Baud rate, PL 범위와 종단저항 설정을 유지한다.

## 최초 양산용

`US_MAIN_BOARD_FACTORY_V1.2.0.hex`

- 새 MCU의 최초 생산 Writing에 사용한다.
- Application과 출하 기본 설정을 함께 포함한다.
- 기존 설정을 출하 기본값으로 초기화해야 할 때에도 사용한다.

## CubeProgrammer 업데이트 순서

1. ST-LINK와 보드 전원을 연결한다.
2. STM32CubeProgrammer에서 `ST-LINK`, `SWD`로 Connect한다.
3. `Open file`에서 `US_MAIN_BOARD_UPDATE_V1.2.0.hex`를 선택한다.
4. `Download`를 실행하고 `Verify programming` 성공을 확인한다.
5. 보드를 Reset한 뒤 LCD와 저장 설정을 확인한다.

> 업데이트 작업에서는 `Full chip erase`를 실행하지 않는다. 실행하면 저장된 사용자
> 설정이 삭제된다.

여러 보드를 연속으로 작업할 때에는 안내서의 `Automatic Mode` 절차를 사용한다.
첫 번째 보드에서 한 번 Connect한 뒤 완료된 보드를 교체하면 다음 보드를 자동으로 감지하여
Writing, Verify와 Reset/Run을 반복한다.
