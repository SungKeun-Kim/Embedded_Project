# LM565 PLL 펌웨어 개발 환경

이 폴더에는 두 개의 기존 IAR AVR 프로젝트가 있습니다.

최신 사용자 지정본은 [ATmega8 초기값 220 / 범위 185~255](./programmer_files/20261108_ATmega8_CENTER220_RANGE185_255/사용방법.md)입니다.
GANG용 Flash HEX와 초기 EEPROM을 생성·검증했으며, 이 확대 범위 버전의 실기 기록·시험은 아직 하지 않았습니다.

외부 프로그래머용 파일은 [PRO ISP 4 기록용 패키지](./programmer_files/20261108_START3S_RESET/사용방법.md)에 있습니다.
검증된 두 MCU Flash HEX와 선택적 EEPROM 백업을 포함합니다.

2026-11-08 [START 3초로 튜닝 시간 초과 해제](./PLL_TIMEOUT_RESET_2026-11-08.md)를 추가했습니다.
ATmega8과 ATmega16A를 모두 갱신해야 하며, 과전류·통신 오류는 버튼으로 해제하지 않습니다.

2026-11-08 [자동 튜닝 1분 제한·업로드 표시·ATmega8 스택 검증](./PLL_TUNE_TIMEOUT_2026-11-08.md)을 반영했습니다.
1분 제한 수정본의 ATmega8 기록과 실제 시간 초과·완료 표시 확인을 마쳤습니다.

ATmega8 실제 기록 결과와 백업은 [2026-11-07 업로드 기록](./ATMEGA8_UPLOAD_2026-11-07.md)에 있습니다.
ATmega16A도 갱신했습니다. [ATmega16A 업로드 결과](./ATMEGA16A_UPLOAD_2026-11-07.md)를 참고하세요.
이후 화면·키 이상에 대응하여 스택을 수정하고 재기록했습니다. [최신 패널 수정 기록](./PANEL_STACK_FIX_2026-11-07.md)을 참고하세요.

현재 운전 중 작업 분류·3초 위상 보정은 [운전 중 보정 사양](./PLL_RUNTIME_TUNING_SPEC.md)을 참고하세요.
ATmega8을 먼저 기록한 뒤 ATmega16A도 갱신해야 새 보정 기능이 동작합니다.
전류 변화 0.2A 기준은 ACS712와 회로의 명목 환산이며 실측 보정이 필요합니다.
초기 자동 튜닝 동작은 [초기 자동 튜닝 사양과 검증](./PLL_AUTOTUNE_SPEC.md)을 참고하세요.
3초 길게 누르기로 시작하여 PWM 약 50%에서 최소점을 반복 확인하고,
최소점보다 저항이 낮은 쪽 한 칸을 저장한 뒤 다음 튜닝까지 유지합니다.
초기 코드·회로·보호 분석은 [PLL 튜닝 개선 검토](./PLL_TUNING_REVIEW_2026-11-07.md)에 있습니다.
두 MCU를 함께 갱신하는 후보이며, 실물 검증과 전류/주파수 한계 확정 전에는
출고용으로 사용할 수 없습니다. 운전 중 미세 튜닝은 새 16A의 출력 안정 허가가 있을 때만 동작합니다.
정상 정지 중의 미세 출력은 유지하고 오류 차단을 별도로 처리합니다.

수정 전 동작 기록은 [펌웨어 기능 및 개선 검토](./FIRMWARE_FUNCTION_ANALYSIS.md)에 있습니다.

| 보드 | IAR 프로젝트 | Release 타깃 |
| --- | --- | --- |
| ATmega16A | `Atmega16A_LM565_BD/sonic.ewp` | ATmega16 |
| ATmega8 | `Atmega8_LM565_BD/tso8.ewp` | ATmega8 |

기존 코드는 IAR 전용 헤더와 문법을 사용하므로 AVR-GCC로 변환하지 않았습니다.
빌드에는 IAR Embedded Workbench for AVR 5.0 Evaluation의 빌드 도구와
AVR C/C++ Compiler 5.10A가 필요합니다. 설치 및 라이선스 상태에 따라 빌드가
제한될 수 있습니다.

## VS Code에서 빌드

1. VS Code에서 **이 폴더**를 엽니다.
2. Microsoft의 **C/C++** 확장을 설치합니다.
3. `Ctrl+Shift+B`를 누르고 원하는 `IAR: Build ... Release` 작업을 선택합니다.
4. 경고와 오류는 터미널 및 Problems 패널에서 확인합니다.

빌드 스크립트는 원본 IAR 프로젝트의 Release 설정과 소스 목록을 사용하며,
빌드용 프로젝트 복사본과 결과물을 `.build/<보드>/Release/`에 생성합니다.
기존 보드 폴더의 `Release` 디렉터리에 있는 HEX와 중간 산출물은 덮어쓰지
않습니다. 소스 파일에서 다른 `.c` 파일을 직접 포함하는 기존 구조도 유지합니다.
빌드할 때마다 해당 `.build`의 Release 산출물은 다시 생성됩니다.

기본 IAR 설치 경로는 다음과 같습니다.

```text
C:\Program Files (x86)\IAR Systems\Embedded Workbench 5.0 Evaluation
```

다른 경로에 설치했다면 VS Code를 실행하기 전에 환경 변수 `IAR_AVR_ROOT`를
IAR 설치 폴더로 설정하고, `.vscode/c_cpp_properties.json`의 IAR 헤더 경로도
수정합니다.

## 편집과 주의사항

- C/C++ 확장의 IntelliSense 구성은 명령 팔레트의
  `C/C++: Select a Configuration`에서 편집 중인 보드에 맞게 선택합니다.
  IAR 전용 문법은 C/C++ 확장에서 완전히 해석하지 못하므로 편집기 오류 밑줄을
  비활성화했습니다. 실제 오류와 경고는 IAR 빌드 결과를 기준으로 확인합니다.
- 보드와 프로그래머, 클록 및 퓨즈 설정을 확인하기 전에는 HEX를 기록하지
  마세요. 업로드 중 장비가 재시작되므로 출력을 정지하고 안전한 상태로
  만들어야 합니다.
- 소스에는 CP949와 UTF-8 파일이 섞여 있습니다. VS Code가 인코딩을 자동
  추측하도록 설정했지만 완벽하지 않을 수 있으므로 한글이 깨져 보이면 저장하기
  전에 `Reopen with Encoding`에서 해당 파일의 인코딩을 확인하세요.
- ATmega16A LCD는 시작 시 4비트 인터페이스를 다시 동기화한 뒤 두 줄 표시
  모드로 설정합니다. LCD 초기화 절차를 변경하면 실물 화면도 확인해야 합니다.

## ATmega8 업로드 및 표시

업로드 완료 표시를 사용하려면 이 버전의 ATmega16A 펌웨어가 LCD 보드에
먼저 설치되어 있어야 합니다. ATmega8과 ATmega16A 사이의 UART 연결도
필요합니다. ATmega16A를 기록할 때도 대상 칩을 확인하고 기존 플래시와
EEPROM을 백업 및 보존해야 합니다.

USBasp를 대상 ATmega8에 연결하고 장비를 안전하게 정지한 뒤 다음을
실행합니다. IAR 빌드 도구와 `avrdude`가 필요합니다.

```powershell
.\.vscode\upload-atmega8.ps1
```

스크립트는 현재 소스를 빌드하고 기존 플래시, EEPROM, 퓨즈와 잠금 비트를
`.build/backups/`에 백업합니다. EEPROM을 복원하며 플래시를 기록하고,
플래시와 EEPROM의 재검증 및 퓨즈·잠금 비트 확인이 성공한 뒤에만
완료 표시 신호를 기록합니다. 기본 USBasp 클록은 8 kHz이며 필요하면
`-BitClock`으로 AVRDUDE의 비트 클록 주기(마이크로초)를 지정할 수
있습니다. 잠금 비트가 설정되어 있으면 기록을 중단합니다.

완료 신호를 위해 ATmega8 EEPROM의 `0x1F8`~`0x1FF`를 예약합니다.
업로드 전에 이 영역이 모두 `0xFF`가 아니면 기존 데이터를 덮어쓰지 않고
중단합니다. ATmega16A가 표시 후 응답하면 ATmega8이 이 영역을 다시 `0xFF`로
지웁니다. 표시가 아직 전달되지 않았다면 다음 업로드 전에 LCD 보드와
UART 연결을 확인하세요. 쓰기 중 오류가 나면 칩의 내용이 불완전할 수
있으므로 출력된 백업 경로를 확인하고 복구해야 합니다.

스크립트의 검증이 끝나고 두 보드가 통신하면 LCD 두 번째 줄에
`UPLOAD COMPLETE`가 약 2초간 표시됩니다. 운전이나 튜닝 중에는 표시를
미루고, 오류 화면을 우선합니다. 표시 전까지 ATmega8은 알림을 재전송하며,
전원 재인가만으로 업로드 완료 메시지를 표시하지 않습니다. 이 메시지는
플래시 검증 후 실행 및 UART 통신을 확인하지만 PLL·초음파 출력의 정상
동작을 보증하지 않습니다. 다른 방법으로 HEX를 기록하면 완료 신호가
생성되지 않습니다.

### ATmega8 실행 표시

두 보드에 이 버전의 펌웨어가 설치되어 있고 UART 통신이 가능하면,
ATmega8의 시작 신호 또는 ATmega16A의 상태 요청에 대한 응답을 받은 뒤
LCD 두 번째 줄에 `ATMEGA8 ONLINE`이 약 3초간 표시됩니다.
운전 또는 튜닝 중에는 표시를 미루며, 기존 오류 화면이 우선합니다.
전원을 켜거나 재시작할 때마다 표시될 수 있습니다.
ATmega8의 시작 알림은 응답이 없으면 최대 20회 전송하며, 이후에도
ATmega16A의 상태 요청에는 응답합니다.

이 표시는 ATmega8 펌웨어의 실행과 UART 통신을 확인할 뿐, 업로드 검증을
의미하지 않습니다. 메시지가 없으면 UART
연결뿐 아니라 ATmega8 초기화 및 주변 장치 상태도 확인해야 합니다.

상태 표시 로직의 호스트 테스트는 Visual Studio C 컴파일러 환경에서 다음과
같이 실행할 수 있습니다.

```powershell
cl /TC tests\lcd_init_test.c /Fe:lcd-init-test.exe
.\lcd-init-test.exe
cl /TC tests\ready_notice_atmega16_test.c /Fe:ready16-test.exe
.\ready16-test.exe
cl /TC tests\ready_notice_atmega8_test.c /Fe:ready8-test.exe
.\ready8-test.exe
cl /TC tests\ready_notice_integration_test.c /Fe:ready-integration-test.exe
.\ready-integration-test.exe
```

업로드 스크립트의 순서와 실패 처리는 모의 프로그래머로 검증할 수
있습니다. `-BackupRoot`에는 테스트 전용 빈 폴더를 지정합니다.

```powershell
.\tests\upload_atmega8_test.ps1 -BackupRoot C:\Temp\atmega8-upload-test
```
