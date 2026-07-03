# CSF JET Multi Megasonic - 1페이지 퀵가이드

- 문서 버전: v1.0 (2026-05-18)
- 대상: LCD 패널 현장 운전
- 기준: 현재 펌웨어 코드 동작

---

## 1. 버튼 한눈에 보기

| 버튼       | 짧게 누름            | 길게 누름                                            |
| ---------- | -------------------- | ---------------------------------------------------- |
| START/STOP | RUN/STOP             | FREQ 편집 화면에서 Auto Tuning(약 3초)               |
| MODE       | 항목/필드 순환, 복귀 | 선택모드에서 설정 진입(2초), 편집에서 상위 복귀(2초) |
| UP         | 값 증가              | 연속 증가                                            |
| DOWN       | 값 감소              | 연속 감소                                            |
| SET        | 저장/확정            | -                                                    |

알람 표시 중에는 SET으로 에러 해제.

---

## 2. 화면/메뉴 흐름도

```text
[선택모드]
  MODE(짧게): 항목순환  MODE -> FREQ CH -> 8PWR
  MODE(2초): 설정모드 진입
  START/STOP: RUN/STOP

        MODE(2초)
           |
           v
[설정모드 상위]
  1.FREQ SET / 2.POWER SET
  UP/DOWN: 항목 선택
  SET: 편집 진입
  MODE(짧게/2초): 선택모드 복귀

     SET on 1.FREQ SET            SET on 2.POWER SET
           |                               |
           v                               v
 [FREQ 편집]                        [POWER 편집]
  MODE(짧게): CH->FREQ->L            MODE(짧게): CH->LOW->HIGH->DEF
  UP/DOWN: 값 변경                   UP/DOWN: 값 변경
  SET: 저장                          SET: 저장
  START/STOP(약3초): Auto Tuning     MODE(2초): 설정 상위
  MODE(2초): 설정 상위
```

---

## 3. 현장 기본 절차 (30초 버전)

1. 전원 ON -> 선택모드 확인
2. MODE 짧게로 8PWR 항목 이동
3. UP/DOWN으로 채널(정지 중) 또는 출력 DEF(운전 중) 조정
4. START/STOP으로 운전 시작
5. 화면의 O값(추정 출력) 확인
6. 필요 시 MODE 2초로 설정 진입 후 채널 파라미터 저장

---

## 4. LCD 예시 문자열

```text
RUN NOR F01 P1
>MODE:NORMAL
```

```text
RUN NOR F01 P1
>F01  400k L01
```

```text
RUN NOR F01 P1
>P1 D0.50 O0.48
```

```text
   ALARM STOP
ERR1 LOW ALARM
```

---

## 5. 자주 막히는 포인트

- 값이 안 바뀌면: 현재 선택 항목이 맞는지 먼저 확인 (MODE로 항목 이동)
- 출력 조정이 안 되면: 8PWR 항목인지 확인
- 알람 후 정지면: 원인 확인 후 SET으로 해제
- FREQ/L 저장 안 했으면: 편집 화면에서 SET 저장 후 복귀

---

상세 설명은 다음 문서 참조:

- docs/MANUAL_CSF_JET_LCD_Panel_v1.md
- docs/manual_csf_jet_multi_megasonic_full_draft.md
