# ST 프로그래머 설정과 보드 업데이트 방법

작성일: 2026-10-14 / Firmware: V1.2.0, 트랜스듀서 감지 2.5초 반영본

## 1. 이번 변경 내용

| 항목 | 적용 조건 |
| --- | --- |
| TRANSDUCER ERROR | 운전 중 실제 PA7 출력이 50% 이상이고 PA6 입력전류 신호 진폭이 기준 이하로 지속: 2.5초 설정, 실제 감지 약 2.6초 |
| OVER CURRENT | LCD에 표시하는 1초 평균 RMS 전류가 7.00A 이상으로 약 5초 지속될 때 |
| 오류 발생 시 | 초음파 PWM과 제어 출력을 정지하고 오류 메시지를 유지 |

직전 반영본의 2000ms 감지 설정을 2500ms로 변경했다.
전류 계측 결과는 200ms마다 갱신한다. 2500ms를 계측 주기로 올림하여 무신호가 13회 연속 검출되면 정지한다.
실제 감지 시간은 약 2.6초이며 2.4초에는 정지하지 않는다.
실제 출력이 50% 미만으로 내려가거나 정상 전류 신호가 검출되면 무신호 누적 횟수는 초기화된다.
전류 신호 기준은 DC 중심에서의 peak 진폭 50 ADC count 미만이다.
즉, 일정한 크기의 정상 AC 전류가 흐르는 상태 자체를 오류로 판정하는 기능은 아니다.

START 시 출력은 1.5초 동안 Soft Start로 증가한다. 실제 출력이 50% 이상인 구간부터
무신호 시간을 누적하므로 START 버튼을 누른 순간부터 정확히 2.5초 후에 오류가 뜨는 것은 아니다.
예를 들어 목표 출력이 100%라면 약 0.75초에 50%에 도달하고 이후 약 2.6초를 감시한다.
200ms 계측 주기에 따라 실제 메시지 발생 시점에 차이가 있다. 시작 후뿐 아니라 운전 중에도 감시한다.

과전류의 1초 평균은 200ms RMS 전류 결과 5개의 이동평균이다. 이를 7.00A와 비교하며
25회 연속 이상일 때 OVER CURRENT를 표시한다. 평균값이 7.00A 미만이면 과전류 횟수는 초기화된다.
평균 필터가 차오르는 시간 때문에 실제 전류 상승부터 오류 발생까지는 5초보다 길 수 있다.
전원 직후 평균 이력이 5개보다 적으면 수집된 결과의 평균을 사용한다.

## 2. 사용할 파일

현장 업데이트 파일: **US_MAIN_BOARD_UPDATE_V1.2.0.hex**

배포 ZIP: **US_MAIN_BOARD_FIELD_UPDATE_V1.2.0.zip**

ZIP을 풀면 HEX, program_update.bat, 이 안내서, 변경 및 검증 기록, SHA256SUMS.txt가 있다.
배포 파일명이 동일하므로 이번 ZIP에 포함된 SHA-256 값으로 2.5초 반영본을 구분한다.
프로젝트 루트의 production 폴더에도 같은 HEX가 있다.

UPDATE HEX는 Application만 기록하며 사용자 설정 영역 0x0800F800~0x0800FFFF를 포함하지 않는다.
주파수, Sweep, 운전시간, 통신 설정 등을 유지하는 업데이트 방식이다.
**Full chip erase는 해제하고 Factory HEX를 선택하지 않는다.**
이번 배포는 STM32G474CBT6 기반 기존 프로젝트 보드용이다. MCU나 메모리 구성이 다른 보드는 별도 확인이 필요하다.

## 3. 연결 준비

1. 초음파를 STOP한다. REMOTE 접점과 PLC RUN 명령도 OFF로 둔다.
2. 전력단을 정지 상태로 유지하고 MCU 제어 전원은 공급한다.
3. 보드 전원을 끈 상태에서 ST-LINK와 아래 신호를 연결한 뒤 제어 전원을 켠다.
4. STM32CubeIDE Debug나 ST-LINK를 사용 중인 다른 프로그램은 종료한다.

| ST-LINK 신호 | 보드 연결 |
| --- | --- |
| SWDIO | PA13 / SWDIO |
| SWCLK | PA14 / SWCLK |
| NRST | MCU NRST |
| GND | 보드 GND |
| VTref / Target voltage | 보드 3.3V 전압 기준, 사용하는 ST-LINK 핀 기능 확인 |

VTref는 대상 보드 전압을 감지하는 핀이다. ST-LINK 모델에 따라 전원 출력 핀과 다르므로
커넥터 표시를 확인한다. Under reset 연결에는 NRST 배선이 필요하다.

## 4. STM32CubeProgrammer GUI 설정

오른쪽 연결 설정을 다음과 같이 지정한다.

| 항목 | 설정 |
| --- | --- |
| 연결 장치 | ST-LINK |
| Serial number | 지금 작업할 ST-LINK 선택 |
| Port | SWD |
| Frequency | 100 kHz부터 시작 |
| Mode | Under reset |
| Reset mode | Hardware reset |

1. Connect를 누르고 MCU 인식 및 Target voltage가 정상인지 확인한다.
2. Erasing & Programming 화면에서 US_MAIN_BOARD_UPDATE_V1.2.0.hex를 선택한다.
3. Full chip erase를 **해제**한다. Erase all 버튼도 누르지 않는다.
4. Skip flash memory erase before programming은 **해제**한다. 기록 대상 Application page의 정상 지우기는 필요하다.
5. Verify programming을 **체크**한다.
6. Run after programming을 **체크**한다.
7. HEX는 주소를 포함하므로 주소를 임의로 바꾸지 않는다. Application 시작 주소는 0x08000000이다.
8. Start Programming 또는 Download를 누른다.
9. 하단 Log에서 Programming 완료와 Verify 성공을 확인한다.
10. Reset 후 Run 결과를 확인하고 LCD가 정상 표시되는지, 기존 설정이 유지되는지 확인한다.

Reset/Run은 MCU 프로그램을 실행하는 단계이다. 초음파 운전 시작은 별도의 START 입력으로 수행한다.
외부 RUN 입력이 ON인 경우 재부팅 후 운전할 수 있으므로 작업 중 OFF를 유지한다.
Verify 실패 또는 Reset/Run 실패가 있으면 완료 보드로 처리하지 않는다.

## 5. BAT 파일로 업로드

STM32CubeProgrammer를 설치한 PC에서 ZIP을 폴더에 풀고 program_update.bat를 실행한다.
출력 정지와 외부 RUN OFF를 확인한 뒤 Y를 누른다. N을 누르면 기록하지 않고 종료한다.
완료 화면의 Update, Verify, Reset and Run completed를 확인한다.
BAT와 HEX는 같은 폴더에 두며, 여러 ST-LINK가 연결된 PC에서는 사용할 장치 하나만 남긴다.

BAT가 사용하는 명령은 다음과 같다. 명령 프롬프트에서 실행할 경우 Programmer의 전체 경로를 사용한다.

```bat
STM32_Programmer_CLI.exe -c port=SWD mode=UR reset=HWrst freq=100 -d "US_MAIN_BOARD_UPDATE_V1.2.0.hex" -v -rst -run
```

- -d: HEX 기록 및 필요한 Application page 지우기
- -v: 기록 직후 내용 Verify
- -rst: MCU Reset
- -run: MCU 프로그램 실행

## 6. 다른 보드에 반복 업로드

1. 첫 보드의 Verify와 Reset/Run 성공을 확인한다.
2. Disconnect 후 보드 전원을 끄고 SWD 커넥터를 분리한다.
3. 다음 보드도 초음파 STOP 및 외부 RUN OFF 상태로 준비한다.
4. SWD 커넥터와 제어 전원을 연결하고 같은 HEX로 위 절차를 반복한다.
5. 보드마다 LCD와 기존 설정을 확인한다.

Automatic Mode를 사용할 경우에도 Full chip erase는 해제하고 Download file, Verify programming,
Run after programming을 체크한다. 첫 보드 Connect 후 Start automatic mode를 실행하고,
각 보드의 성공 Log와 Waiting for device를 확인한 뒤 전원을 끄고 다음 보드로 교체한다.

## 7. 업로드 후 동작 확인

- 전원을 다시 켰을 때 LCD 및 저장 설정을 확인한다.
- 정상 부하 운전에서 외부 전류계와 LCD 평균 전류를 비교한다.
- 트랜스듀서 감지는 출력 50% 이상에서 약 2.6초간 신호가 기준 이하일 때 발생한다(설정값 2.5초).
- 과전류는 1초 평균 RMS 전류 7.00A 이상이 약 5초 지속될 때 발생한다.
- 오류 원인을 제거하고 오류 Reset 후 재운전한다.

이번 PC 시험은 논리와 컴파일 검증이다. 실제 부하에서의 시간 및 전류 정확도는 계측으로 확인한다.
운전 중 트랜스듀서를 분리하거나 실제 전력단에 고의로 과전류를 만드는 방법으로 시험하지 않는다.

## 8. 공식 참고 자료

- [STMicroelectronics STM32CubeProgrammer UM2237](https://www.st.com/resource/en/user_manual/dm00403500-stm32cubeprogrammer-stmicroelectronics.pdf)
- [STMicroelectronics Memory programming and erasing](https://dev.st.com/stm32cube-docs/prog/2.23.0/en/docs/markup/CubeProg_UserManual/Memory_programming_and_erasing.html)

GUI 항목명은 설치 버전에 따라 조금 다를 수 있다. 이번 PC의 CLI 버전은 2.23.0이다.
