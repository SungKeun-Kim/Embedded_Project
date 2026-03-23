# Copilot 지침 — ATtiny85 AC 조광기

## 프로젝트 개요

**ATtiny85** (DIP-8) 기반 AC 위상 제어 조광기 펌웨어. **PlatformIO + Arduino 프레임워크**로 빌드. Timer1 ISR 폴링(25 µs / 40 kHz)을 통한 제로크로스 검출로 TRIAC을 제어하며, 50 Hz / 60 Hz 자동 감지를 지원한다.

- 소스 파일: `src/main.cpp` (단일 파일)
- 타겟: ATtiny85 @ 8 MHz 내부 RC 오실레이터
- 언어: C/C++ (Arduino + AVR 레지스터 직접 접근)

## 빌드 및 업로드

```bash
# 빌드
pio run

# USBasp로 업로드 (platformio.ini 기본 설정)
pio run -t upload

# 퓨즈 비트 설정 (최초 1회)
pio run -t fuses

# 빌드 결과 hex 파일 (SmartProg2 등 외부 프로그래머용)
# .pio/build/attiny85/firmware.hex
```

**퓨즈 비트:** LFUSE=0xE2, HFUSE=0xDF, EFUSE=0xFF (8 MHz 내부 클럭, CKDIV8 해제, ISP 활성화).

## 아키텍처

### 핀 맵 (DIP-8)

| 핀  | 포트       | 기능                                                |
| --- | ---------- | --------------------------------------------------- |
| 2   | PB3 (ADC3) | 아날로그 조광 입력 (0.3 V–4.3 V 동작, ≥4.4 V = OFF) |
| 5   | PB0        | TRIAC 게이트 출력 (MOC3021 경유)                    |
| 6   | PB1        | 디버그 LED                                          |
| 7   | PB2        | 제로크로스 입력 (폴링, INT0 미사용)                 |

### 펌웨어 상태 머신 (ISR)

```
S_IDLE → S_ZC_OFFSET (0.4 ms) → S_DELAY (위상 지연) → S_TRIGGER (500 µs 펄스) → S_IDLE
```

- 제로크로스 검출: Timer1 ISR이 25 µs마다 PB2를 폴링; 최소 주기 가드(7 ms)로 EMI 노이즈 필터링.
- 위상 지연 (`dimValue`): 값이 작을수록 출력 증가, 클수록 출력 감소.
- `dimValue = DIM_OFF (500)`이면 트리거 완전 비활성화.

### 캘리브레이션 흐름

1. **부팅 캘리브레이션** (setup): ZC 주기 8회 측정 → `minDim`/`maxDim` 산출.
2. **런타임 재보정** (loop): 16회 이동 평균으로 `minDim`/`maxDim` 갱신 → RC 클럭 드리프트 실시간 추적.

### 안전 계층 (적용 순서)

1. ISR `localDim` 스냅샷 — 반주기 도중 값 변경 방지
2. 조용한 시작 — OFF→ON 전환 후 2초간 TRIAC 비점화
3. 소프트 스타트 — 500 ms 동안 최소 출력(maxDim) 고정
4. 레이트 리미터 — 반주기당 최대 10 틱 증가 (loop 및 ISR 양쪽 적용)
5. 안전 타임아웃 — ZC 미검출 시 10.5 ms 후 자동 차단

## 코딩 규칙

- **한글 주석**이 표준이며, 코드 수정 시에도 한글 주석을 유지할 것.
- 타이밍 임계 경로에서는 Arduino 추상화 대신 AVR 레지스터 직접 조작 사용.
- 공유 volatile 변수 접근 시 반드시 `cli()`/`sei()` 쌍으로 감싸되, 원자 구간은 최소화.
- 모든 타이밍 상수는 **25 µs 틱 단위** (밀리초가 아님).
- 상수는 `const` 전역 변수 사용 (값에 `#define` 사용 금지); 상태 정의만 `#define` 사용.
- 동적 메모리 할당 금지; 모든 변수는 static 또는 전역.
- **LTO 비활성화** (`-fno-lto`, platformio.ini) — ISR 관련 링커 문제 방지.

## 핵심 제약사항

- ATtiny85는 Flash 8 KB, RAM 512 B만 보유 — 불필요한 추상화 금지.
- Serial/UART 없음; 디버그는 PB1 LED 토글로만 수행.
- INT0은 **의도적으로 미사용** — 외부 인터럽트 설정을 추가하지 말 것.
- ADC 입력 범위는 하드웨어 제약 (NPN 포화 전압으로 인해 0.3 V–4.3 V); `ADC_MIN`/`ADC_MAX`로 보정.
- ATtiny85의 Timer1은 8비트이며 OCR1C를 TOP으로 사용 (ATmega의 16비트 Timer1과 다름).

## 문서 파일 (한글)

| 파일                           | 내용                                         |
| ------------------------------ | -------------------------------------------- |
| `코드_설명서.txt`              | 펌웨어 전체 사양서                           |
| `ADC_보정_수정사항.txt`        | ADC 선형화 보정 상세                         |
| `PWM평활_증폭회로_설계.txt`    | PWM→DC 아날로그 회로 설계 (STM32 → ATtiny85) |
| `SmartProg2_업로드_가이드.txt` | SmartProg2 프로그래밍 가이드                 |
| `SmartProg2_퓨즈비트_설정.txt` | 퓨즈 비트 빠른 참조                          |
| `USBasp_업로드_가이드.txt`     | USBasp 배선 및 업로드 가이드                 |

## 흔한 실수

- Timer1 프리스케일러나 OCR1C를 변경하면 모든 타이밍 상수가 깨짐 (25 µs 틱 기준으로 설계됨).
- `Serial` 호출을 추가하면 실패함 — ATtiny85에는 하드웨어 UART가 없음.
- ISR은 25 µs 이내에 완료되어야 함; 내부에서 부동소수점 연산이나 나눗셈 금지.
- `dimValue`는 `loop()`와 ISR 간 공유 변수 — 반드시 `cli()`/`sei()` 안에서 접근할 것.
- PB2에 INT0을 활성화하지 말 것; 폴링 방식은 인터럽트 충돌 방지를 위한 의도적 설계.
