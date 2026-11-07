# V1.2.0_GDT200us_OC8A_20261014

- 과전류: LCD와 같은 1초 평균 RMS 전류 8.00 A 이상이 약 5초 지속되면 보호 정지.
- GDT 일반 정지 유지시간: 200 us 유지. 보호 정지는 즉시 차단.
- 기존 계측 환산과 정상 계측범위 7 A 기준은 유지. 8 A 근처 실제 계측 정확도/보호 동작은 장비에서 별도 확인 필요.
- 펌웨어 기본 버전은 V1.2.0이며 파일명 뒤 표기는 이번 빌드 구분명이다.

## 업데이트

1. 이 폴더를 통째로 보관한다. BAT와 HEX를 같은 폴더에 둔다.
2. 출력 STOP 및 REMOTE·PLC RUN OFF 상태로 둔다.
3. `program_update_V1.2.0_GDT200us_OC8A_20261014.bat`을 실행한다. 이 빌드의 HEX SHA-256 일치를 먼저 확인한다.
4. Writing/Verify/Reset/Run 성공 표시를 확인한다.

사용 HEX: `US_MAIN_BOARD_UPDATE_V1.2.0.hex`. 사용자 설정 페이지를 포함하지 않는다.

SHA-256: `cd70595a25baa4034a75e4480e73b62cc55eb818c9800a080355ac43c7ba3d8b`
