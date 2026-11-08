# ATmega8 운전 중 보정 버전 업로드 결과

## 재업로드

2026-11-07 23:31(KST) 사용자 요청으로 같은 수정본을 ATmega8에 다시 기록했다.
Flash 5,839바이트의 쓰기 검증·재검증, EEPROM 512바이트 복원·검증,
퓨즈/잠금값 보존 확인, 업로드 알림 8바이트 기록·검증이 모두 통과했다.
HEX의 SHA-256은 아래 최초 기록값과 동일하다. ATmega16A는 기록하지 않았다.
재업로드 백업: [ATmega8_upload_20261107_233147_94d7b7a5](./.build/backups/ATmega8_upload_20261107_233147_94d7b7a5/).

## 최초 업로드

2026-11-07 사용자 요청에 따라 USBasp로 **ATmega8만 실제 기록**했다. ATmega16A는 수정·빌드만 완료했으며 기록하지 않았다.

## 결과

- IAR ATmega8 빌드: 오류 0, 경고 0.
- 기능 시험: C 테스트 12개 통과. 업로드 스크립트 모의 시험 통과.
- Flash: 새 이미지 **5,839바이트** 기록, 쓰기 후 검증 및 별도 재검증 통과.
- EEPROM: 기존 **512바이트** 백업·복원·검증 통과. 기존 초기 코드 **233** 보존.
- 퓨즈/잠금: lfuse **0xFF**, hfuse **0xC9**, lock **0xFF**로 업로드 전후 동일.
- 업로드 완료 알림: 예약 EEPROM `0x1F8~0x1FF`의 **8바이트** 별도 기록·검증 통과. 따라서 전체 EEPROM의 최종 내용은 이 알림 영역에서만 백업과 달라질 수 있다. ATmega16A가 알림을 확인하면 ATmega8이 해당 영역을 지운다.
- Flash 사용량 5,839/8,192바이트, SRAM 배치 440/1,024바이트(스택 예약 포함).

Flash 이미지 파일 SHA-256:

```text
E707DF22E2CAA5197F614D0DB2E419465A57943C25E28934232EABB1486D7DDC
```

## 백업과 기록 파일

- [업로드 직전 장비 백업 폴더](./.build/backups/ATmega8_upload_20261107_232522_ab08c1ce/)
- [실제 기록한 ATmega8 HEX 사본](./.build/backups/ATmega8_upload_20261107_232522_ab08c1ce/programmed-atmega8.hex)
- [ATmega8 메모리 배치](./.build/backups/ATmega8_upload_20261107_232522_ab08c1ce/programmed-atmega8.map)
- [아직 기록하지 않은 ATmega16A HEX 사본](./.build/backups/ATmega8_upload_20261107_232522_ab08c1ce/pending-atmega16a.hex)
- [실제 업로드 로그](./.build/runtime_upload_20261107.log)
- 수정 전 소스와 최초 장비 읽기 백업: `.build/before_runtime_20261107_231225/`

## 현재 장비에서 가능한 동작과 다음 단계

새 ATmega8은 부팅 시 기존 EEPROM 코드 233을 AD5280에 적용하고 읽기 확인을 수행하도록 작성되어 있다. 프로그래머의 Flash/EEPROM 검증은 완료했지만 실제 RDAC 응답이나 초음파 출력 파형은 이번 업로드 도구로 관측하지 않았다.

**ATmega16A도 갱신해야 새 초기 튜닝과 운전 중 보정의 전체 절차가 동작한다.** 구형 16A는 출력 안정 허가 `0xB0`를 보내지 않으므로 새 운전 중 보정이 활성화되지 않는다. 초기 튜닝도 새 준비 명령 `0x1C` 없이 시작하면 오류로 처리한다.

명목 0.2A 기준은 ACS712와 회로 저항값에서 환산한 값이며 패널 전류계와 실측 보정하지 않았다. 실제 주파수·전류·출력과 고장 방지 효과는 별도 장비 시험이 필요하다. 구체적인 알고리즘과 제한은 [운전 중 보정 사양](./PLL_RUNTIME_TUNING_SPEC.md)을 참고한다.
