#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ========================================
// AC 조광기 (AC Dimmer) 제어 코드
// ATtiny85 전용 - Timer1 폴링 방식 제로크로스 검출
// 50Hz/60Hz 자동 감지 지원 (ISR 내부 기반)
// 25μs 고분해능 타이머 (v3.1)
// ========================================
//
// 하드웨어 요구사항:
//
// 1. MCU: ATtiny85 (DIP-8)
//    - 내부 8MHz 클럭 사용 (외부 크리스탈 불필요)
//    - 퓨즈 설정: LFUSE=0xE2, HFUSE=0xDF, EFUSE=0xFF
//
// 2. 제로크로스 검출부:
//    - AC 220V → 9V 다운 트랜스 → H11AA1 제로크로스 옵토커플러
//    - H11AA1 출력 (풀업) → ATtiny85 PB2 (7번 핀)
//    - ※ INT0 미사용 - Timer1 ISR 내에서 25μs 폴링으로 엣지 검출
//
// 3. 위상 제어부:
//    - ATtiny85 PB0 (5번 핀) → 220Ω → MOC3021 → 트라이악
//
// 4. 조광 입력:
//    - PB3 (2번 핀, ADC3) 입력 전압 범위: 0.2V ~ 4.3V
//    - 4.3V → 트라이액 게이트 ~10%, 0.2V → 트라이액 게이트 ~96%
//    - 5V (START 버튼 누르기 전, PWM 없음) → 트라이액 OFF
//
// 5. 디버그:
//    - PB1 (6번 핀) - 디버그 LED
//      캘리브레이션 중: ON → 완료 후: ZC마다 토글 (정상시 ~30Hz 점멸)
//      LED 상시 ON = 캘리브레이션 멈춤 (ZC 미검출)
//
// 핀 배치 (DIP-8):
//    1: RESET/PB5       - 리셋 (ISP)
//    2: PB3 (ADC3)      - 아날로그 입력 ✅
//    3: PB4 (ADC2)      - 미사용 (향후 SG3525 제어용 예약)
//    4: GND             - 그라운드
//    5: PB0             - 트라이악 제어 출력 ✅
//    6: PB1             - 디버그 LED ✅
//    7: PB2             - 제로크로스 입력 (폴링) ✅
//    8: VCC             - 5V 전원
//
// 동작 원리:
//    - Timer1 ISR이 25μs마다 PB2를 폴링하여 상승 엣지 검출
//    - 최소 주기 가드: 마지막 ZC 후 7ms 이내 엣지는 무시
//      → EMI 노이즈(트라이악 발화시)를 시간 기반으로 원천 차단
//      → 실제 ZC(8.3ms 주기)만 통과, 디바운스 불필요
//    - INT0 미사용으로 인터럽트 설정 관련 문제 완전 제거
//    - 트리거 펄스 폭: 500μs (20 × 25μs)
//
// Timer1 설정 (ATtiny85):
//    - CTC 모드, 프리스케일러 8
//    - OCR1C = 24 → 25μs 주기 @ 8MHz (40kHz ISR)
// ========================================

// ---- 핀 정의 ----
const uint8_t TRIAC_PIN = 0;                 // PB0 (5번 핀) - 트라이악 제어
const uint8_t DEBUG_PIN = 1;                 // PB1 (6번 핀) - 디버그 LED
const uint8_t ZC_PIN    = 2;                 // PB2 (7번 핀) - 제로크로스 입력 (폴링)
const uint8_t POT_PIN   = 3;                 // PB3 (2번 핀) - 아날로그 입력 (ADC3)

// ---- 타이밍 상수 (25μs 틱 단위) ----
// 60Hz 반주기 = 8.33ms = 333틱, 50Hz = 10ms = 400틱
const uint8_t TRIGGER_PULSE_WIDTH = 20;      // 트리거 펄스 폭 (20 × 25μs = 500μs)
const uint8_t ZC_OFFSET = 16;               // ZC 오프셋 (16 × 25μs = 0.4ms)
const uint16_t SAFETY_TIMEOUT = 420;         // 10.5ms (50Hz 대응, ZC 미검출시 타임아웃)
const uint16_t MIN_ZC_PERIOD = 280;          // 7ms (노이즈 필터)
const uint16_t MAX_ZC_PERIOD = 440;          // ZC 주기 유효 상한 (50Hz + 빠른칩 대응)
const uint16_t ZC_PERIOD_REF = 332;          // 기준 주기 (60Hz @ 8MHz, 25μs 단위)
const uint16_t MIN_DIM_DEFAULT = 126;        // 기본값 (60Hz, 8MHz, +1 old tick 상향)
const uint16_t MIN_DIM_BASE = 126;           // 비율 계산 기준 (332틱 대비 126틱 = 38.0%)
const uint16_t MAX_DIM_DEFAULT = 312;        // 기본값 (부팅 시, 보수적)
const uint8_t MAX_DIM_MARGIN = 20;           // maxDim 계산 시 여유 (측정주기 - 20)
                                             // 332 - 20 = 312 (old 10틱 margin 복원)
const uint16_t MAX_DIM_MIN = 312;            // maxDim 하한 (느린칩 대응)
const uint16_t MAX_DIM_MAX = 391;            // maxDim 상한 (50Hz 대응)
const uint16_t MIN_DIM_MIN = 115;            // minDim 하한 (경험적 안정 래치 한계)
const uint16_t MIN_DIM_MAX = 161;            // minDim 상한 (50Hz 대응)

// ---- ADC 보정 상수 ----
// PB3 입력 전압 범위: 0.3V ~ 4.3V (동작 범위), 5V = OFF
const int ADC_MIN = 61;                      // 입력 최소값 (0.3V: 0.3/5.0 * 1023 ≈ 61)
const int ADC_MAX = 880;                     // 입력 최대값 (4.3V)
const int ADC_OFF = 900;                     // OFF 임계값 (4.4V 이상 → OFF)
const uint16_t DIM_OFF = 500;               // OFF 값 (> SAFETY_TIMEOUT → 타임아웃으로 트리거 안됨)
const uint16_t MAX_DIM_STEP = 10;           // 출력 증가 최대 속도 (10틱/50ms, 노이즈 과출력 방지)
const uint8_t QUIET_START_CYCLES = 50;      // 조용한 시작 (50 × 50ms = 2.5s, SG3525 VCO 안정화 대기)
const uint8_t SOFT_START_CYCLES = 10;       // 소프트 스타트 (10 × 50ms = 500ms, maxDim 고정)

// ---- 상태 머신 정의 ----
#define S_IDLE      0                         // 대기: 다음 제로크로스 대기 중
#define S_ZC_OFFSET 1                         // ZC 오프셋: 실제 ZC까지 0.4ms 대기
#define S_DELAY     2                         // 지연: 위상 지연 카운트 중
#define S_TRIGGER   3                         // 트리거: 게이트 펄스 출력 중 (500μs)

// ---- 전역 변수 (ISR용) ----
volatile uint8_t state = S_IDLE;             // 상태 머신 현재 상태
volatile uint16_t counter = 0;               // 범용 카운터 (DELAY: 위상지연, TRIGGER: 펄스폭)
volatile uint16_t dimValue = DIM_OFF;        // 조광 값 - OFF 상태로 시작 (START 전 5V)
volatile bool calibMode = true;              // 캘리브레이션 모드 플래그
volatile uint16_t calibCounter = 0;          // 캘리브레이션용 틱 카운터
volatile uint16_t minDim = MIN_DIM_DEFAULT;  // 동적 최소 지연값 (자동 보정)
volatile uint16_t maxDim = MAX_DIM_DEFAULT;  // 동적 최대 지연값 (자동 보정)
volatile uint16_t lastZcPeriod = 0;          // 마지막 ZC 주기 틱 수 (ISR→loop 전달)
volatile bool newZcReady = false;            // 새 ZC 주기 측정 완료 플래그
int potValue;                                // ADC 읽기 값

void setup() {
  // ==========================================
  // 0단계: 클럭 8MHz 강제 설정
  // 퓨즈 CKDIV8 상태와 무관하게 8MHz 보장
  // ==========================================
  cli();
  CLKPR = (1 << CLKPCE);                       // 클럭 프리스케일러 변경 허용
  CLKPR = 0;                                   // 프리스케일러 1 (8MHz)
  sei();

  // ==========================================
  // 1단계: 핀 설정
  // ==========================================
  DDRB |= (1 << TRIAC_PIN) | (1 << DEBUG_PIN); // PB0, PB1 출력
  DDRB &= ~(1 << ZC_PIN);                      // PB2 입력
  PORTB &= ~((1 << TRIAC_PIN) | (1 << DEBUG_PIN) | (1 << ZC_PIN)); // 모두 LOW, 풀업 없음
  DIDR0 |= (1 << ADC3D);                        // PB3 디지털 입력 비활성화 (ADC 노이즈 감소)

  // 디버그 LED ON → 캘리브레이션 진행 중 표시
  PORTB |= (1 << DEBUG_PIN);

  // ==========================================
  // 2단계: Timer1 설정 (25μs 주기) - 캘리브레이션 모드로 시작
  // ==========================================
  cli();
  calibMode = true;                            // 캘리브레이션 모드 활성화
  calibCounter = 0;

  GTCCR = 0;                                   // 타이머 동기화 해제
  TCCR1 = 0;                                   // Timer1 정지
  TCNT1 = 0;                                   // 카운터 클리어
  TCCR1 = (1 << CTC1) | (1 << CS12);           // CTC 모드 + 프리스케일러 8
  OCR1C = 24;                                  // TOP = 24 (25μs @ 8MHz)
  OCR1A = 24;                                  // 비교 매치 A

  TIMSK = (1 << OCIE1A);        // Timer1 비교 매치 A 인터럽트만 활성화 (25μs 주기)
  sei();

  // ==========================================
  // 3단계: ZC 주기 측정 (Timer1 틱 기반, 트라이악 OFF 상태)
  // 8회 측정하여 avgPeriod 계산 → minDim/maxDim 설정
  // ==========================================
  {
    uint16_t periodSum = 0;
    uint8_t samples = 0;
    
    // 첫 상승 엣지 대기 (동기화)
    while ((PINB >> ZC_PIN) & 1);              // HIGH→LOW 대기
    while (!((PINB >> ZC_PIN) & 1));           // LOW→HIGH 대기 (상승 엣지)
    
    // calibCounter 리셋
    cli();
    calibCounter = 0;
    sei();
    
    // 8회 측정 (상승 엣지 → 상승 엣지)
    while (samples < 8) {
      // 다음 상승 엣지 대기
      while ((PINB >> ZC_PIN) & 1);            // HIGH→LOW 대기
      while (!((PINB >> ZC_PIN) & 1));         // LOW→HIGH 대기 (상승 엣지)
      
      // 틱 수 읽기
      cli();
      uint16_t ticks = calibCounter;
      calibCounter = 0;
      sei();
      
      // 유효한 주기만 합산 (280~440 범위)
      if (ticks >= MIN_ZC_PERIOD && ticks <= MAX_ZC_PERIOD) {
        periodSum += ticks;
        samples++;
      }
    }
    
    // avgPeriod 계산
    uint16_t avgPeriod = periodSum / 8;
    
    // maxDim 계산 (avgPeriod - margin)
    int16_t calcMax = (int16_t)avgPeriod - MAX_DIM_MARGIN;
    if (calcMax < (int16_t)MAX_DIM_MIN) calcMax = MAX_DIM_MIN;
    if (calcMax > (int16_t)MAX_DIM_MAX) calcMax = MAX_DIM_MAX;
    maxDim = (uint16_t)calcMax;
    
    // minDim 계산 (비율 기반: avgPeriod × 125 / 332)
    int16_t calcMin = (int16_t)((int32_t)avgPeriod * MIN_DIM_BASE / ZC_PERIOD_REF);
    if (calcMin < (int16_t)MIN_DIM_MIN) calcMin = MIN_DIM_MIN;
    if (calcMin > (int16_t)MIN_DIM_MAX) calcMin = MIN_DIM_MAX;
    minDim = (uint16_t)calcMin;
  }

  // ==========================================
  // 4단계: 캘리브레이션 완료 → 정상 모드 전환
  // ==========================================
  cli();
  calibMode = false;                           // 정상 모드로 전환
  sei();

  // 디버그 LED OFF → 캘리브레이션 완료 표시 (이후 ISR에서 ZC마다 토글)
  PORTB &= ~(1 << DEBUG_PIN);
}

// ==========================================
// Timer1 비교 매치 A 인터럽트 (25μs마다 호출, 40kHz)
// - 캘리브레이션 모드: calibCounter++ 만 수행
// - 정상 모드: PB2 폴링으로 제로크로스 검출 + 트라이악 제어
//
// ★★★ 핵심 설계 원칙 ★★★
// 1. ZC 엣지 검출은 IDLE 상태에서만 수행
// 2. ZC_OFFSET: 상승엣지 후 0.4ms 대기 → 실제 ZC 시점 보정
// 3. 최소 주기 가드: 마지막 ZC 후 7ms(280틱) 이내는 무시
//    → 트라이악 EMI 노이즈 완전 차단
// ==========================================
ISR(TIMER1_COMPA_vect) {
  // ---- 캘리브레이션 모드: 틱 카운트만 수행 ----
  if (calibMode) {
    calibCounter++;
    return;
  }

  // ---- PB2 상승 엣지 검출 (단순 2샘플 비교) ----
  static uint16_t localDim = DIM_OFF;        // S_DELAY용 dimValue 스냅샷 (레이스 방지)
  static uint8_t lastPin = 0;
  uint8_t curPin = (PINB >> ZC_PIN) & 1;     // PB2: 0 또는 1
  uint8_t rising = curPin & ~lastPin;         // LOW→HIGH = 1
  lastPin = curPin;

  // ---- 최소 주기 타이머 (포화 카운터) ----
  // 마지막 ZC 이후 경과 틱 수. 500에서 포화(12.5ms).
  static uint16_t zcTimer = 500;              // 초기값 500 → 첫 ZC 즉시 검출
  if (zcTimer < 500) zcTimer++;

  // ---- 4단계 상태 머신 ----
  switch (state) {

    case S_IDLE:
      // 제로크로스 상승 엣지 + 최소 주기 경과 확인
      if (rising && zcTimer > MIN_ZC_PERIOD) {
        lastZcPeriod = zcTimer;                // 주기 캡처 (런타임 보정용)
        newZcReady = true;
        zcTimer = 0;                           // 주기 타이머 리셋
        counter = 0;
        state = S_ZC_OFFSET;                   // 오프셋 대기로 전환
        PORTB ^= (1 << DEBUG_PIN);            // 디버그 LED 토글 (ZC 확인)
      }
      break;

    case S_ZC_OFFSET: {
      counter++;
      if (counter >= ZC_OFFSET) {
        counter = 0;
        uint16_t newDim = dimValue;
        // ★ ISR-level 레이트 리미터 (최종 방어선)
        // loop()와 독립적으로 동작 → loop 타이밍 무관하게 과출력 원천 차단
        if (newDim != DIM_OFF) {
          if (localDim == DIM_OFF || localDim >= SAFETY_TIMEOUT) {
            newDim = maxDim;             // OFF→ON: maxDim(최소 출력) 강제
          } else if (newDim < localDim) {
            if ((localDim - newDim) > MAX_DIM_STEP) {
              newDim = localDim - MAX_DIM_STEP;  // 반주기당 증가폭 제한
            }
          }
        }
        localDim = newDim;
        state = S_DELAY;
      }
      break;
    }

    case S_DELAY:
      // 위상 지연 카운트 (localDim × 25μs, 실제 ZC 기준)
      // ★ 스냅샷 사용 → loop()가 dimValue를 변경해도 이 반주기는 안전
      counter++;
      if (counter >= localDim) {
        PORTB |= (1 << TRIAC_PIN);
        counter = 0;
        state = S_TRIGGER;
      }
      else if (counter >= SAFETY_TIMEOUT) {
        PORTB &= ~(1 << TRIAC_PIN);
        state = S_IDLE;
      }
      break;

    case S_TRIGGER:
      // 트리거 펄스 출력 (TRIGGER_PULSE_WIDTH × 25μs = 500μs)
      counter++;
      if (counter >= TRIGGER_PULSE_WIDTH) {
        // 펄스 완료 → 게이트 OFF, 다음 ZC 대기
        PORTB &= ~(1 << TRIAC_PIN);
        state = S_IDLE;
      }
      break;

    default:
      // 비정상 상태 복구
      PORTB &= ~(1 << TRIAC_PIN);
      state = S_IDLE;
      break;
  }
}

void loop() {
  // ---- 런타임 ZC 주기 추적 → minDim/maxDim 자동 재보정 ----
  // 매 ZC마다 ISR이 캡처한 주기를 16회 누적 평균하여 재계산.
  // 내부 RC 클럭 드리프트(온도/전압 변화)에 실시간 대응.
  {
    static uint16_t periodAccum = 0;
    static uint8_t  periodCount = 0;

    cli();
    bool     ready  = newZcReady;
    uint16_t period = lastZcPeriod;
    if (ready) newZcReady = false;
    sei();

    if (ready && period >= MIN_ZC_PERIOD && period <= MAX_ZC_PERIOD) {
      periodAccum += period;
      periodCount++;

      if (periodCount >= 16) {
        uint16_t avg = periodAccum / 16;

        int16_t calcMax = (int16_t)avg - MAX_DIM_MARGIN;
        if (calcMax < (int16_t)MAX_DIM_MIN) calcMax = MAX_DIM_MIN;
        if (calcMax > (int16_t)MAX_DIM_MAX) calcMax = MAX_DIM_MAX;

        int16_t calcMin = (int16_t)((int32_t)avg * MIN_DIM_BASE / ZC_PERIOD_REF);
        if (calcMin < (int16_t)MIN_DIM_MIN) calcMin = MIN_DIM_MIN;
        if (calcMin > (int16_t)MIN_DIM_MAX) calcMin = MIN_DIM_MAX;

        cli();
        maxDim = (uint16_t)calcMax;
        minDim = (uint16_t)calcMin;
        sei();

        periodAccum = 0;
        periodCount = 0;
      }
    }
  }

  // ADC 읽기 (PB3/ADC3)
  potValue = analogRead(POT_PIN);

  uint16_t tempDimValue;

  // 5V 상태 (START 버튼 안 누름, PWM 미입력) → OFF
  if (potValue >= ADC_OFF) {
    tempDimValue = DIM_OFF;
  }
  // 정상 동작 범위 (0.2V ~ 4.3V) → 위상 제어
  else {
    int potScaled;
    if (potValue <= ADC_MIN) {
      potScaled = 0;
    } else if (potValue >= ADC_MAX) {
      potScaled = 1023;
    } else {
      potScaled = (long)(potValue - ADC_MIN) * 1023 / (ADC_MAX - ADC_MIN);
    }
    cli();
    uint16_t currentMinDim = minDim;
    uint16_t currentMaxDim = maxDim;
    sei();
    tempDimValue = (uint16_t)map(potScaled, 0, 1023, currentMinDim, currentMaxDim);
  }

  // ---- 4단계 보호: 조용한 시작 + 소프트 스타트 + 레이트 리미터 ----
  // 1단계: ISR localDim 스냅샷 (반주기 중간 변경 방지)
  // 2단계: 조용한 시작 (250ms TRIAC 완전 비점화, 초기 과도 안정화)
  // 3단계: 소프트 스타트 (500ms maxDim 고정)
  // 4단계: 레이트 리미터 (노이즈/EMI 과출력 방지)
  static bool wasOff = true;
  static uint8_t quietRemain = 0;
  static uint8_t softStartRemain = 0;

  cli();
  uint16_t currentDim = dimValue;
  sei();

  if (tempDimValue == DIM_OFF) {
    wasOff = true;
    quietRemain = 0;
    softStartRemain = 0;
  } else {
    if (wasOff) {
      quietRemain = QUIET_START_CYCLES;
      softStartRemain = SOFT_START_CYCLES;
      wasOff = false;
    }

    if (quietRemain > 0) {
      tempDimValue = DIM_OFF;
      quietRemain--;
    } else if (softStartRemain > 0) {
      cli();
      tempDimValue = maxDim;
      sei();
      softStartRemain--;
    } else if (currentDim == DIM_OFF) {
      cli();
      tempDimValue = maxDim;
      sei();
    } else if (tempDimValue < currentDim) {
      if ((currentDim - tempDimValue) > MAX_DIM_STEP) {
        tempDimValue = currentDim - MAX_DIM_STEP;
      }
    }
  }

  cli();
  dimValue = tempDimValue;
  sei();

  delay(50);
}

// ATtiny 프레임워크용 main()
int main(void) {
  init();
  setup();
  for (;;) loop();
  return 0;
}
