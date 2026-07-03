# Manual Update Log

목적:

- 펌웨어/동작 변경 사항을 매뉴얼 관점에서 누적 기록한다.
- 최종 사용자 매뉴얼 편집 시 본 문서를 기준으로 반영 누락을 방지한다.

기록 규칙:

1. 기능 코드 변경 후 문서 반영 내용을 같은 날짜로 기록한다.
2. 코드 기준 동작과 매뉴얼 문구가 다른 경우, 코드 동작을 우선 반영한다.
3. 항목은 영향을 받은 코드 파일, 사용자 영향, 반영 문서를 함께 적는다.

---

## 변경 이력

| 날짜       | 기능 변경 요약                                                                                                                                                                 | 사용자 영향                              | 코드 파일                                                                                                             | 반영 문서                                                                                                                        |
| ---------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------------------------- | --------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| 2026-05-18 | (1) 문서-코드 정합화: 패널/통합 매뉴얼의 구 UI 설명 제거 및 실제 버튼/알람 해제/출력범위 반영, (2) LCD 가독성 개선: 선택모드 POWER 라인 16자 최적화, (3) 1페이지 퀵가이드 신설 | 현장 조작 혼선 감소, 화면 가독성 향상    | src/menu_screen.c                                                                                                     | docs/MANUAL_CSF_JET_LCD_Panel_v1.md, docs/manual_csf_jet_multi_megasonic_full_draft.md, docs/MANUAL_CSF_JET_LCD_QuickGuide_v1.md |
| 2026-05-18 | UI/FSM를 선택모드/설정모드 2계층으로 개편, FREQ(CH/FREQ/L) 및 POWER(CH/LOW/HIGH/DEF) 편집 추가, START/STOP 3초 Auto Tuning, 운전 중 LOW/HIGH 알람 정지 표시 반영               | 패널 조작 절차 및 알람 표시 방식 변경    | src/menu.c, src/menu_screen.c, src/button.c, src/megasonic_ctrl.c, include/menu.h, include/button.h, include/params.h | docs/MANUAL_CSF_JET_LCD_Panel_v1.md, docs/manual_csf_jet_multi_megasonic_full_draft.md                                           |
| 2026-05-18 | FREQENCY 편집을 10채널 프리셋 선택 방식으로 변경 (1CH 400kHz ~ 10CH 2200kHz, UP/DOWN 선택, SET 저장, 편집 필드 블링킹)                                                         | 패널에서 주파수 설정 절차/표시 형식 변경 | src/menu.c, src/menu_screen.c, include/params.h                                                                       | docs/MANUAL_CSF_JET_LCD_Panel_v1.md, docs/manual_csf_jet_multi_megasonic_full_draft.md                                           |
