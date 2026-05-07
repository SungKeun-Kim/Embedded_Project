# CSF JET Multi Megasonic LCD 조작패널 매뉴얼 (초안)

- 문서 목적: 기존 HS AUP 조작 매뉴얼을 LCD 패널 구조에 맞게 개정하기 위한 초안
- 기준 이미지: docs/LCD_Display.png
- 모델명: CSF JET Multi Megasonic
- 적용 대상: STM32G474RE 기반 메가소닉 메인보드 (LCD1602 + 5키 + 상태 LED)

![LCD 조작패널](./LCD_Display.png)

## 0. 중요 변경 스펙

- 작동 주파수: 10CH (CH0~CH9), 500kHz~2.2MHz
- 최대 출력: 10W
- 출력 설정 범위: 0.10W~10.00W
- 외부 인터페이스: RS-485 Modbus RTU

## 1. 조작패널 구성

### 1.1 버튼(5개)

- START/STOP: 발진 시작/정지, EXT 모드에서 ADDR 설정 진입
- MODE: 모드/설정 화면 순환, NORMAL 복귀
- UP: 값 증가, 항목 상향 이동
- DOWN: 값 감소, 항목 하향 이동
- SET: 선택/저장/확정

기존 매뉴얼 조작 원칙:

- 기존 조작 흐름을 우선 유지한다.
- 신규 길게 누름 조합은 도입하지 않는다.
- 조작자는 기존과 동일한 키 조합으로 모드 진입/해제를 수행한다.

### 1.2 상태 LED

- NORMAL: 일반 운전 모드
- H/L SET: 알람 상/하한 설정 모드
- 8 POWER: 8단계 출력 설정 모드
- REMOTE: 외부 REMOTE 제어 모드
- EXT: 통신(EXT) 제어 모드
- TX RX: 통신 수신/송신 표시

### 1.3 LCD 상시 표시 (권장)

LCD는 숫자 7세그(FND)와 달리 문자열을 동시에 표시할 수 있으므로, 아래 4개 정보를 상시 표시한다.

- 상단 좌측: 출력 전력 (설정값/실측값)
- 상단 우측: 주파수 (채널 + kHz)
- 하단 좌측: 운전 모드 + RUN/STOP 상태
- 하단 우측: REMOTE/통신 상태 (주소, RX 표시, 경보 요약)

표시 예시:

- 1행: OUT 0.48W F 1750k
- 2행: EXT RUN ADR01 RX

## 2. 운전 모드와 키 동작

### 2.1 NORMAL 모드

- START/STOP: 발진 시작/정지
- UP/DOWN: 출력 설정값 0.01W 단위 변경
- SET: 현재 설정값 저장
- MODE: H/L SET 모드로 이동 (반복 누름 시 NORMAL 복귀)

### 2.2 H/L SET 모드

- MODE: 8 POWER 모드로 이동
- SET: HIGH/LOW 설정 항목 전환 및 저장
- UP/DOWN: 알람 임계값 조정
- START/STOP: 동작 없음(실수 방지)

### 2.3 8 POWER 모드

- MODE: NORMAL 모드로 복귀
- SET: Step 선택/확정
- UP/DOWN: Step별 출력/알람값 조정
- START/STOP: 동작 없음(실수 방지)

### 2.4 REMOTE 모드

- 외부 입력이 우선이며, 로컬 키는 제한 동작
- START/STOP, UP/DOWN, SET은 기본 무시 또는 안내 메시지 표시
- MODE는 정보 화면 전환만 허용 가능
- REMOTE 진입: 전원 ON 시 SET+MODE 동시 누름 유지
- NORMAL 복귀: 전원 ON 시 SET+DOWN 동시 누름 유지

### 2.5 EXT 모드

- 통신 설정/주소 관리 중심
- START/STOP: ADDR 설정 진입
- UP/DOWN: ADDR 또는 BPS 값 증감
- SET: 항목 전환/확정 저장
- MODE: 정보 화면 순환
- EXT 진입: 전원 ON 시 UP+MODE 동시 누름 유지
- NORMAL 복귀: 전원 ON 시 SET+DOWN 동시 누름 유지

## 3. 키 조합 (기존 매뉴얼 복원안)

기존 매뉴얼 기준 키 조합을 아래와 같이 유지한다.

- MODE+DOWN(2초): 에러 해제 (Err7 제외)
- 전원 ON + SET+DOWN 유지: NORMAL 모드 진입
- 전원 ON + SET+MODE 유지: REMOTE 모드 진입
- 전원 ON + UP+MODE 유지: EXT 모드 진입

추가 규칙:

- EXT 모드에서 START/STOP으로 ADDR 설정 화면 진입
- ADDR/BPS 값 변경은 UP/DOWN, 저장은 SET

## 4. 안전 및 제약 사항

- Err7(SENSOR OFF)는 키 조합으로 해제 불가
- Err7 해제는 반드시 전원 OFF 후 SENSOR 입력 정상(SHORT) 확인 후 재기동
- 통신 쓰기 명령(설정 변경)은 EXT 모드에서만 허용
- NORMAL/REMOTE 모드에서는 읽기 전용 운용 권장

## 5. LCD 화면 구성 예시

### 5.1 기본 운전 화면

- 1행: 출력/주파수
- 2행: 모드/RUN 상태/통신 상태

예)

- OUT 0.50W F 1000k
- NOR STOP ADR01

### 5.2 알람 화면

예)

- ** ERROR **
- Err1 LOW ALARM

### 5.3 통신 설정 화면

예)

- EXT COMM SET
- ADR:01 BPS:96

## 6. 권장 매뉴얼 작성 원칙

- 조작 조합은 기존 매뉴얼 기준으로 유지
- 사용자가 자주 쓰는 기능은 짧은 조작으로 배치
- 핵심 설정 기능(REMOTE, ADDR/BPS)은 기존 키 순서를 그대로 유지
- NORMAL 복귀는 MODE 반복 또는 전원 ON + SET+DOWN으로 수행
- 서비스 전용 조합은 본문이 아닌 부록으로 분리

## 7. 판넬 인쇄 문구 권장안

판넬 실크 인쇄는 버튼 기본명 + 조합 안내를 함께 표기한다.

- START/STOP
- MODE
- UP
- DOWN (-)
- SET

보조 문구(후면 라벨 또는 매뉴얼 표기 권장):

- NORMAL: PWR ON + SET+DOWN
- REMOTE: PWR ON + SET+MODE
- EXT: PWR ON + UP+MODE

주석:

- EXT 모드 ADDR/BPS: START/STOP -> UP/DOWN -> SET
- 에러 해제(Err7 제외): MODE+DOWN(2초)

---

본 문서는 UI/펌웨어 확정 전 검토용 초안이다. 회로/펌웨어 버전 확정 후 용어와 화면 문구를 최종 고정한다.
