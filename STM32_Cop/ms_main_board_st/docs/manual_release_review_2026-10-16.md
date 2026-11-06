# 사용자 매뉴얼 v5.0-KR 작성·검토 메모

- 작성일: 2026-10-16
- 요청한 `manual_csf_fet_lcd_panel_v1.docx`와 정확히 같은 이름은 없었음.
- 가장 가까운 `docs/MANUAL_CSF_JET_LCD_Panel_v1.docx`(v4.1-KR)를 기준으로 작성. 원본은 보존.
- 사용자 배포 파일은 `docs/distribution/CSF_JET_LCD_User_Manual_KR_v5.0.docx` 및 `.pdf`.
- 소스 변경·장비 다운로드·하드웨어 시험은 수행하지 않음.

## 원본에서 정정한 주요 내용

| 항목 | 현재 소스 근거 | 문서 반영 |
| --- | --- | --- |
| CH10 기본 주파수 | include/params.h, FREQ_CH9_DEFAULT=23000 | 2300 kHz |
| SET 길게 복귀 | BTN_LONG_PRESS_MS=1500 + SET_EXIT_HOLD_EXTRA_MS=1500 | 약 3초 |
| Auto Tuning | board_runtime_config.h, AUTO_TUNE_ENABLED=0 | 비활성. 남아 있는 LCD 안내 문구 설명 |
| SENSOR 인터록 | params.h, SENSOR_RUN_INTERLOCK_ENABLED=0; IsSensorInputOk | 현재 자동 차단 없음 |
| LOW/HIGH 알람 | menu.c, MENU_LOW_ALARM_ENABLED=0 / MENU_HIGH_ALARM_ENABLED=0 | 설정값·전압 환산과 보호 정지를 구분 |
| 알람 릴레이 | gpio_init.c에 초기화, src/에서 운전 상태 연동 쓰기 미발견 | 핀 명칭만 참고, 접점 동작 보장 제외 |
| TX RX LED | menu_screen.c, UpdateLEDs에서 RX 비트 설정 없음 | 통신 판단 수단으로 사용하지 않음 |
| RUN 표시 | menu_screen.c, RenderSelect | STOP의 0000kHz, RUN I/V·추정 W, 주파수 조정 표시 |
| RUN 중 SET | StoreRunTunePointBySet, SaveStoreNow | 조정 지점 저장 및 일시 출력 정지 명시 |
| POWER 값 | MapPowerToSelectedVoltage01V | 실제 출력 보증과 구분. 프로파일 범위가 목표 전압에 영향 |
| REMOTE 시작 | ServiceRemoteRunRequest | OFF를 먼저 확인한 뒤 ON, 외부 ON 유지 시 재시작 주의 |
| 보호 표시 | ErrorText, RenderSelect | 과전류·전류센서·시작전압·Buck 과전압 반영 |
| TERM | SaveRs485EditBuffer | 설정 보관만 수행, 저항 전환 없음 |

## 장비 공급자가 확인해야 할 실제 미검증 사항

1. 현재 SENSOR/LOW/HIGH 보호가 비활성인 시운전 설정이다. 일반 출고 적용 가능 여부는 펌웨어·장비 책임자가 판단해야 한다. 문서 작성으로 보호가 활성화되지는 않는다.
2. `docs/SAFETY.md`의 1.00 W 제한과 현재 프로파일 기본값·상한(최대 2.50 W 수준)이 다르다. 매뉴얼에는 소프트웨어 설정 범위와 장비 승인 정격을 구분했다. 승인 출력 조건은 별도 확정해야 한다.
3. 25pin D-SUB 핀 번호는 참고 Word의 표에서 가져왔다. 현재 MCU 핀 정의만으로 외부 하네스 번호·접점 정격·극성·핀 관찰 방향은 검증할 수 없다. 출고 하네스와 대조 필요.
4. 알람/GO/OPERATE 접점의 실제 하드웨어 상태·Fail-safe 동작은 미검증이며 소프트웨어 연동도 현재 없다. 정상 시 SHORT라는 과거 보증 설명은 제거했다.
5. POWER 편집 경계는 LOW/HIGH 상호 보정에 의해 0.01 W 차이가 생길 수 있다. 일반 0.05 W 변화 단위와 경계 보정을 구분했다.
6. RS-485 타이머 분주값과 시스템 클럭의 불일치는 별도 통신 문서에 기재되어 있다. 실장비 통신 호환성 시험 필요.
7. 실제 출력·센서 계측·전원 재투입·알람 해제·REMOTE 재시작 및 RUN 중 SET의 출력 중단은 실제 보드에서 확인해야 한다.

## 문서 검증

- Word에서 직접 PDF로 변환하여 같은 페이지 배치 유지.
- 전체 페이지의 본문과 표 셀을 Word XML 및 해당 PDF 페이지의 추출 텍스트와 비교.
- 패널 이미지는 참고 Word에서 원본 바이트 그대로 재사용.
- 소스·참고 Word·생성 파일 SHA-256은 사용자 매뉴얼 전용 manifest JSON에 기록.
