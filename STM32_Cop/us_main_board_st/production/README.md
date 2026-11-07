# STM32CubeProgrammer 펌웨어 파일

**최신 빌드: V1.2.0_LCD40003_CURRENT40004_20261016**

루트 `US_MAIN_BOARD_UPDATE_V1.2.0.hex`는 40003(`0x0002`)을 LCD 현재 출력
숫자(W 또는 %) 읽기로 변경한 빌드다. 40004 CT 실측 전류(A ×100) 읽기와
기존 40002 출력 지령 읽기/쓰기는 유지한다.
전류는 LCD와 동일한 PA6 입력 소비전류의 1초 평균 RMS이며 200 ms마다 갱신한다.
이번 변경본의 보드 업로드 및 PLC 실기 통신 검증은 아직 수행하지 않았다.
PLC 사양은 [사용자 가이드 v1.5](../docs/PLC_Modbus_RTU_사용자_가이드.md)를 참고한다.

배포용 [전용 폴더](FIELD_UPDATE_V1.2.0_LCD40003_CURRENT40004_20261016/README.md)와
[ZIP](US_MAIN_BOARD_FIELD_UPDATE_V1.2.0_LCD40003_CURRENT40004_20261016.zip)에 이번 HEX,
SHA-256 확인 기능이 있는 업로드 BAT, PLC 가이드 v1.5 PDF/DOCX 및 사용 순서를 묶었다.
ZIP을 풀고 출력 STOP 및 REMOTE·PLC RUN OFF를 확인한 뒤 BAT를 실행한다.

이전 `40004_CURRENT` 전용 폴더와 ZIP에는 40003 LCD 표시값 변경이 포함되지 않는다.

**이전 실기 검증 빌드: V1.2.0_GDT200us_OC8A_20261014**
Writing/Verify/Reset/Run 및 설정 보존 확인 완료. [작업 결과](과전류8A_업데이트_20261014.md).
과전류 상한 8.00 A, 기존 약 5초 지속 판정 및 GDT 유지시간 200 us를 적용한다.
전용 BAT와 고정 HEX는 [`FIELD_UPDATE_V1.2.0_GDT200us_OC8A_20261014`](FIELD_UPDATE_V1.2.0_GDT200us_OC8A_20261014/README.md)에 있다.
이 고정 폴더와 기존 ZIP은 이전 배포본이며 40004 실측 전류 변경을 포함하지 않는다.

이전 GDT 구현 내용은 [`GDT_일반정지_시험본_20261014.md`](GDT_일반정지_시험본_20261014.md)를 참고한다.
아래는 이전 7 A 배포본 기록이다. 최신 업데이트에는 루트 UPDATE HEX를 사용한다.

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
