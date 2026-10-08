# STM32CubeProgrammer 펌웨어 파일

상세 GUI 작업 순서는 `STM32CubeProgrammer_최초용_업데이트용_사용순서_v1.1.pdf`를
먼저 확인한다.

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
