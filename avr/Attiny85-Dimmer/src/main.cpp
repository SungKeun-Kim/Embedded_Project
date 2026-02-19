#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// ========================================
// AC 조광기 (AC Dimmer) 제어 코드
// ATtiny85 전용 - Timer1 폴링 방식 제로크로스 검출
// 50Hz/60Hz 자동 감지 지원 (ISR 내부 기반)
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
//    - ※ INT0 미사용 - Timer1 ISR 내에서 50μs 폴링으로 엣지 검출
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
//    - PB1 (6번 핀) - 디버그 LED (제로크로스마다 토글, 정상시 30Hz 점멸)
//
// 핀 배치 (DIP-8):
//    1: RESET/PB5       - 리셋 (ISP)
//    2: PB3 (ADC3)      - 아날로그 입력 ✅
//    3: PB4 (ADC2)      - 예비
//    4: GND             - 그라운드
//    5: PB0             - 트라이악 제어 출력 ✅
//    6: PB1             - 디버그 LED ✅
//    7: PB2             - 제로크로스 입력 (폴링) ✅
//    8: VCC             - 5V 전원
//
// 동작 원리:
//    - Timer1 ISR이 50μs마다 PB2를 폴링하여 상승 엣지 검출
//    - 최소 주기 가드: 마지막 ZC 후 7ms 이내 엣지는 무시
//      → EMI 노이즈(트라이악 발화시)를 시간 기반으로 원천 차단
//      → 실제 ZC(8.3ms 주기)만 통과, 디바운스 불필요
//    - INT0 미사용으로 인터럽트 설정 관련 문제 완전 제거
//    - dimValue 6 = 최대 밝기 ~96%, 150 = 최소 밝기 ~10%
//    - 트리거 펄스 폭: 500μs (10 × 50μs)
//
// Timer1 설정 (ATtiny85):
//    - CTC 모드, 프리스케일러 8
//    - OCR1C = 49 → 50μs 주기 @ 8MHz
// ========================================

// ---- 핀 정의 ----
const uint8_t TRIAC_PIN = 0;                 // PB0 (5번 핀) - 트라이악 제어
const uint8_t DEBUG_PIN = 1;                 // PB1 (6번 핀) - 디버그 LED
const uint8_t ZC_PIN    = 2;                 // PB2 (7번 핀) - 제로크로스 입력 (폴링)
const uint8_t POT_PIN   = 3;                 // PB3 (2번 핀) - 아날로그 입력 (ADC3)

// ---- 타이밍 상수 ----
// 60Hz 반주기 = 8.33ms = 166틱, 50Hz = 10ms = 200틱 (50μs 단위)
const uint8_t TRIGGER_PULSE_WIDTH = 10;      // 트리거 펄스 폭 (10 × 50μs = 500μs)
const uint8_t ZC_OFFSET = 8;                 // ZC 오프셋 (8 × 50μs = 0.4ms)
const uint8_t SAFETY_TIMEOUT = 210;          // 10.5ms (50Hz 대응, ZC 미검출시 타임아웃)
const uint8_t MIN_ZC_PERIOD = 140;           // 7ms (노이즈 필터)
const uint8_t MIN_DIM_DEFAULT = 62;          // 기본값 (60Hz, 8MHz 기준)
const uint8_t MIN_DIM_BASE = 62;             // 비율 계산 기준 (166틱 대비 62틱 = 37.3%)
const uint8_t MAX_DIM_DEFAULT = 157;         // 기본값 (부팅 시, 보수적)
const uint8_t MAX_DIM_MARGIN = 9;            // maxDim 계산 시 여유 (측정주기 - margin)
                                             // 166 - 9 = 157
const uint8_t MAX_DIM_MIN = 156;             // maxDim 하한 (느린칩 대응)
const uint8_t MAX_DIM_MAX = 195;             // maxDim 상한 (50Hz 대응)
const uint8_t MIN_DIM_MIN = 50;              // minDim 하한 (느린칩 대응)
const uint8_t MIN_DIM_MAX = 80;              // minDim 상한 (50Hz 대응)

// ---- ADC 보정 상수 ----
// PB3 입력 전압 범위: 0.3V ~ 4.3V (동작 범위), 5V = OFF
// PWM 전체 범위(0~100%) 사용
// 4.3V (PWM 0%)   → dimValue = MAX_DIM (172) → 트라이액 거의 OFF
// 0.3V (PWM 100%) → dimValue = MIN_DIM (73)  → 트라이액 ~56% (최고 출력)
// 4.5V 이상 → dimValue = 255 → OFF (START 버튼 누르기 전 5V 상태)
const int ADC_MIN = 61;                      // 입력 최소값 (0.3V: 0.3/5.0 * 1023 ≈ 61)
const int ADC_MAX = 880;                     // 입력 최대값 (4.3V)
const int ADC_OFF = 920;                     // OFF 임계값 (4.5V: 4.5/5.0 * 1023 ≈ 920)
                                             // 이 이상 = PWM 미입력 → 트라이액 OFF
const uint8_t DIM_OFF = 255;                 // OFF 값 (> SAFETY_TIMEOUT → 타임아웃으로 트리거 안됨)

// ---- 상태 머신 정의 ----
#define S_IDLE      0                         // 대기: 다음 제로크로스 대기 중
#define S_ZC_OFFSET 1                         // ZC 오프셋: 실제 ZC까지 0.4ms 대기
#define S_DELAY     2                         // 지연: 위상 지연 카운트 중
#define S_TRIGGER   3                         // 트리거: 게이트 펄스 출력 중 (500μs)

// ---- 전역 변수 (ISR용) ----
volatile uint8_t state = S_IDLE;             // 상태 머신 현재 상태
volatile uint8_t counter = 0;                // 범용 카운터 (DELAY: 위상지연, TRIGGER: 펄스폭)
volatile uint8_t dimValue = DIM_OFF;         // 조광 값 - OFF 상태로 시작 (START 전 5V)
volatile bool calibMode = true;              // 캘리브레이션 모드 플래그
volatile uint16_t calibCounter = 0;          // 캘리브레이션용 틱 카운터
volatile uint8_t minDim = MIN_DIM_DEFAULT;   // 동적 최소 지연값 (자동 보정)
volatile uint8_t maxDim = MAX_DIM_DEFAULT;   // 동적 최대 지연값 (자동 보정)
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

  // ==========================================
  // 2단계: Timer1 설정 (50μs 주기) - 캘리브레이션 모드로 시작
  // ==========================================
  cli();
  calibMode = true;                            // 캘리브레이션 모드 활성화
  calibCounter = 0;

  GTCCR = 0;                                   // 타이머 동기화 해제
  TCCR1 = 0;                                   // Timer1 정지
  TCNT1 = 0;                                   // 카운터 클리어
  TCCR1 = (1 << CTC1) | (1 << CS12);           // CTC 모드 + 프리스케일러 8
  OCR1C = 49;                                  // TOP = 49 (50μs @ 8MHz)
  OCR1A = 49;                                  // 비교 매치 A

  TIMSK = (1 << TOIE0) | (1 << OCIE1A);        // Timer0 + Timer1 인터럽트 활성화
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
      
      // 유효한 주기만 합산 (140~220 범위: 60Hz±10% + 여유)
      if (ticks >= 140 && ticks <= 220) {
        periodSum += ticks;
        samples++;
      }
    }
    
    // avgPeriod 계산
    uint8_t avgPeriod = periodSum / 8;
    
    // maxDim 계산 (avgPeriod - margin)
    int16_t calcMax = (int16_t)avgPeriod - MAX_DIM_MARGIN;
    if (calcMax < MAX_DIM_MIN) calcMax = MAX_DIM_MIN;
    if (calcMax > MAX_DIM_MAX) calcMax = MAX_DIM_MAX;
    maxDim = (uint8_t)calcMax;
    
    // minDim 계산 (비율 기반: avgPeriod × 62 / 166 - offset)
    int16_t calcMin = (int16_t)avgPeriod * MIN_DIM_BASE / 166 - 1;
    if (calcMin < MIN_DIM_MIN) calcMin = MIN_DIM_MIN;
    if (calcMin > MIN_DIM_MAX) calcMin = MIN_DIM_MAX;
    minDim = (uint8_t)calcMin;
  }

  // ==========================================
  // 4단계: 캘리브레이션 완료 → 정상 모드 전환
  // ==========================================
  cli();
  calibMode = false;                           // 정상 모드로 전환
  sei();
}

// ==========================================
// Timer1 비교 매치 A 인터럽트 (50μs마다 호출)
// - 캘리브레이션 모드: calibCounter++ 만 수행
// - 정상 모드: PB2 폴링으로 제로크로스 검출 + 트라이악 제어
//
// ★★★ 핵심 설계 원칙 ★★★
// 1. ZC 엣지 검출은 IDLE 상태에서만 수행
// 2. ZC_OFFSET: 상승엣지 후 0.4ms 대기 → 실제 ZC 시점 보정
// 3. 최소 주기 가드: 마지막 ZC 후 7ms(140틱) 이내는 무시
//    → 트라이악 EMI 노이즈 완전 차단
// ==========================================
ISR(TIMER1_COMPA_vect) {
  // ---- 캘리브레이션 모드: 틱 카운트만 수행 ----
  if (calibMode) {
    calibCounter++;
    return;
  }

  // ---- PB2 상승 엣지 검출 (단순 2샘플 비교) ----
  static uint8_t lastPin = 0;
  uint8_t curPin = (PINB >> ZC_PIN) & 1;     // PB2: 0 또는 1
  uint8_t rising = curPin & ~lastPin;         // LOW→HIGH = 1
  lastPin = curPin;

  // ---- 최소 주기 타이머 (포화 카운터) ----
  // 마지막 ZC 이후 경과 틱 수. 255에서 포화(12.75ms).
  static uint8_t zcTimer = 255;               // 초기값 255 → 첫 ZC 즉시 검출
  if (zcTimer < 255) zcTimer++;

  // ---- 4단계 상태 머신 ----
  switch (state) {

    case S_IDLE:
      // 제로크로스 상승 엣지 + 최소 주기 경과 확인
      if (rising && zcTimer > MIN_ZC_PERIOD) {
        zcTimer = 0;                           // 주기 타이머 리셋
        counter = 0;
        state = S_ZC_OFFSET;                   // 오프셋 대기로 전환
        PORTB ^= (1 << DEBUG_PIN);            // 디버그 LED 토글 (ZC 확인)
      }
      break;

    case S_ZC_OFFSET:
      // ZC 오프셋 대기 (0.4ms = 8틱)
      // 상승엣지 → 실제 ZC까지 보정
      counter++;
      if (counter >= ZC_OFFSET) {
        counter = 0;
        state = S_DELAY;
      }
      break;

    case S_DELAY:
      // 위상 지연 카운트 (dimValue × 50μs, 실제 ZC 기준)
      counter++;
      if (counter >= dimValue) {
        // 지연 완료 → 트라이악 게이트 ON!
        PORTB |= (1 << TRIAC_PIN);
        counter = 0;
        state = S_TRIGGER;
      }
      else if (counter >= SAFETY_TIMEOUT) {
        // 안전 타임아웃: 반주기 초과 또는 OFF 상태
        PORTB &= ~(1 << TRIAC_PIN);
        state = S_IDLE;
      }
      break;

    case S_TRIGGER:
      // 트리거 펄스 출력 (TRIGGER_PULSE_WIDTH × 50μs = 500μs)
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
  // ADC 읽기 (PB3/ADC3)
  potValue = analogRead(POT_PIN);

  uint8_t tempDimValue;

  // 5V 상태 (START 버튼 안 누름, PWM 미입력) → OFF
  if (potValue >= ADC_OFF) {
    tempDimValue = DIM_OFF;                                // OFF (트라이액 비활성화)
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
    // minDim, maxDim은 setup()에서 자동 계산됨 (volatile)
    uint8_t currentMinDim = minDim;
    uint8_t currentMaxDim = maxDim;
    tempDimValue = (uint8_t)map(potScaled, 0, 1023, currentMinDim, currentMaxDim);
  }

  // 원자성 보장 (uint8_t는 1바이트이므로 안전하지만 명시적으로)
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