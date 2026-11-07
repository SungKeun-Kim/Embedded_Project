#include <Arduino.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <util/atomic.h>

// AC 조광기 v4.7 현장시험: v4.5 입력 곡선 유지, 최대출력 점화만 140기준틱으로 제한.
// v4.6(168) 시험에서 최대 DC전압 약25V 감소 보고 -> 사용자 요청140으로 제한 완화.
// 50/60Hz 모두 실측 반주기 비례 적용. 분석/시험 범위는 v4_7_140틱_50Hz_검토.md 참고.
// ATtiny85, 내부 RC 8MHz, 저항 + EL354N 제로크로스 검출
// PB0(5번): MOC3021, PB1(6번): 디버그, PB2(7번): ZC, PB3(2번): ADC3
// 퓨즈: E2/DF/FF. DF는 BOD 비활성화이며 2.7V BOD 설정이 아니다.
// Timer1 25us 폴링 유지. INT0 미사용. ZC 신호는 MCU 입력에서 충분한 HIGH 폭 필요.
// 50/60Hz를 고정 경계로 분류하지 않고 측정 반주기 대비 같은 위상을 사용한다.
// RC 클럭 오차와 실제 전원 주파수를 단일 RC 시간축만으로 분리할 수는 없다.

const uint8_t TRIAC_PIN = 0;
const uint8_t DEBUG_PIN = 1;
const uint8_t ZC_PIN = 2;
const uint8_t POT_PIN = 3;

// 25us는 명목값이다. 예: RC ±15%, 전원 47~63Hz이면 약 270~489틱.
// 이 범위는 동작 허용 창이며 MCU 오실레이터의 정확도 보증을 뜻하지 않는다.
const uint16_t MIN_ZC_PERIOD = 250;
const uint16_t MAX_ZC_PERIOD = 500;
const uint16_t ZC_AGE_LIMIT = MAX_ZC_PERIOD + MAX_ZC_PERIOD / 32 + 3;
const uint8_t LOCK_SAMPLES = 8;             // 연속으로 일관된 반주기 8회 확인
const uint8_t ZC_WIDTH_DIVISOR = 4;         // ZC HIGH 허용 상한: 실측 반주기의 1/4

// 기존 60Hz 실측 설정에서 최대/최소 출력 경계에 추가 여유를 둔다. 작은 지연 = 큰 출력.
// offset, 게이트 폭, 경계 여유까지 모두 측정 반주기에 비례한다.
const uint16_t ZC_PERIOD_REF = 332;
const uint16_t MIN_DIM_BASE = 140;          // v4.6보다28기준틱 빠름, v4.5보다7기준틱 늦게 점화
const uint16_t INPUT_MIN_DIM_BASE = 133;    // v4.5 입력 곡선 유지. 계산 후140보다 이른 점화만 제한
const uint8_t ZC_OFFSET_BASE = 2;           // 기존 50us 시험 보정에 해당
const uint8_t TRIGGER_PULSE_BASE = 20;      // 기본 폭: 60Hz 약500us, 50Hz 약600us. 낮은 출력에서는 단축
const uint8_t END_GUARD_BASE = 16;          // 종료 여유: 60Hz 약400us / 50Hz 약480us (최초2보다 확보)
const uint16_t MAX_DIM_BASE = 308;          // 기존 최저출력 위상 복원. 기본 게이트 폭과 분리
const uint8_t MIN_PULSE_BASE = ZC_PERIOD_REF - ZC_OFFSET_BASE
                             - MAX_DIM_BASE - END_GUARD_BASE; // 최저출력 펄스 여유: 6기준틱
const uint16_t DIM_OFF = 0xffff;            // 시간 지연값으로 사용하지 않는 OFF 표식
const uint16_t MAX_DIM_STEP = 10;           // 유효 반주기마다 위상 기준값 최대 10씩 출력 증가
const uint16_t INPUT_INTERVAL = 400;        // 명목 10ms, Arduino delay() 미사용
const uint16_t POWER_SETTLE_TICKS = 40000;  // 부팅 후 명목1초 동안 주기 보정/출력 금지
const int ADC_MIN = 61;                     // 0.3V 이하: 최대 출력 요구
const int ADC_MAX = 880;                    // 약 4.3V: 최소 출력 요구
const int ADC_OFF = 900;                    // 약 4.4V 이상: OFF
const int ADC_ON = ADC_OFF - 1;             // 기존처럼900 미만에서 ON. 추가한890 진입 문턱 제거

static_assert(MIN_DIM_BASE < MAX_DIM_BASE, "Invalid phase limits");
static_assert(INPUT_MIN_DIM_BASE <= MIN_DIM_BASE, "Output cap must not advance firing");
static_assert(MAX_DIM_BASE + ZC_OFFSET_BASE + END_GUARD_BASE < ZC_PERIOD_REF,
              "No room for low-output gate pulse");
static_assert(MIN_PULSE_BASE >= 6, "Low-output gate pulse is too short");
static_assert(MAX_ZC_PERIOD < 0x8000, "Period must fit wrap-safe arithmetic");
static_assert(POWER_SETTLE_TICKS > 0 && POWER_SETTLE_TICKS < 0xffff,
              "Power settling delay must fit the boot timer");
// 기준 여유의 하한 확인. 정수 반올림 경계의 추가 여유는 endGuardTicks()에서 보장한다.
static_assert(MIN_ZC_PERIOD * END_GUARD_BASE / ZC_PERIOD_REF
              > MIN_ZC_PERIOD / 32 + 4, "Insufficient end guard for period jitter");
static_assert(MAX_ZC_PERIOD / ZC_WIDTH_DIVISOR < 256, "Pulse width must fit uint8_t");
static_assert(MIN_DIM_BASE > ZC_PERIOD_REF / ZC_WIDTH_DIVISOR,
              "Trigger must follow pulse validation");

#define S_IDLE 0
#define S_DELAY 1
#define S_TRIGGER 2
#define P_NONE 0
#define P_HIGH 1
#define P_VALID 2
#define P_BAD 3

// ISR 공유 상태. 복잡한 산술은 loop()에서만 수행한다.
volatile uint16_t timerTicks = 0;           // 1.6384초마다 순환, 차이로만 시간 계산
volatile uint16_t zcAge = ZC_AGE_LIMIT;     // 최대 주기의 타임아웃보다 크게 포화
volatile uint16_t edgePeriod = 0;
volatile uint8_t edgeSequence = 0;
volatile uint16_t edgeBlanking = MIN_ZC_PERIOD;
volatile uint16_t fireAt = 0;
volatile uint16_t stopAt = 0;
volatile uint8_t state = S_IDLE;
volatile uint8_t zcPulseState = P_NONE;
volatile uint8_t zcPulseWidth = 0;
volatile uint8_t zcPulseLimit = MAX_ZC_PERIOD / ZC_WIDTH_DIVISOR;
uint8_t lastPin = 0;                        // ISR 전용

// 아래 변수들은 loop() 전용: 인터럽트와 나눗셈/평균 계산을 공유하지 않는다.
bool synchronized = false;
uint8_t lockCount = 0;
uint16_t periodSum = 0;
uint16_t trackedPeriod = 0;
uint16_t periodQ3 = 0;                      // 1/8틱 고정소수점 IIR
uint8_t consumedSequence = 0;
uint16_t outputPhase = DIM_OFF;
uint16_t targetPhase = DIM_OFF;             // ADC 요구값. 실제 점화 위상은 반주기 경계에서 갱신
bool inputOff = true;
uint16_t lastInputTick = 0;
bool powerSettled = false;                  // 고정 대기 완료. 실제 VCC 측정 결과는 아님
bool haveSettledEdge = false;               // 대기 종료 이후 첫 엣지는 기준점으로만 사용

static uint16_t periodTolerance(uint16_t period) {
  return period / 32 + 2;                  // 약 ±3.1% + 폴링 양자화 2틱
}

static uint16_t scalePhase(uint16_t period, uint16_t phase) {
  return ((uint32_t)period * phase + ZC_PERIOD_REF / 2) / ZC_PERIOD_REF;
}

static uint16_t endGuardTicks(uint16_t period) {
  uint16_t guard = scalePhase(period, END_GUARD_BASE);
  // 허용 주기 단축 + 폴링 지터2틱 뒤에도 최소1틱 여유. 반올림 경계도 포함한다.
  uint16_t minimum = periodTolerance(period) + 3;
  return guard < minimum ? minimum : guard;
}

static void stopOutput() {
  outputPhase = DIM_OFF;
  targetPhase = DIM_OFF;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    state = S_IDLE;
    PORTB &= ~(1 << TRIAC_PIN);
  }
}

static void loseSync() {
  synchronized = false;
  lockCount = 0;
  periodSum = 0;
  trackedPeriod = 0;
  periodQ3 = 0;
  stopOutput();
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    edgeBlanking = MIN_ZC_PERIOD;
    zcPulseLimit = MAX_ZC_PERIOD / ZC_WIDTH_DIVISOR;
    PORTB |= (1 << DEBUG_PIN);              // 상시 ON: 신호 없음/동기화 대기
  }
}

void setup() {
  cli();
  // 분주만 해제한다. 내부 RC 자체를 정확한 8MHz로 보정하는 기능은 아니다.
  clock_prescale_set(clock_div_1);
  PORTB &= ~((1 << TRIAC_PIN) | (1 << DEBUG_PIN) | (1 << ZC_PIN));
  DDRB |= (1 << TRIAC_PIN) | (1 << DEBUG_PIN);
  DDRB &= ~((1 << ZC_PIN) | (1 << POT_PIN));
  DIDR0 |= (1 << ADC3D);
  PORTB |= (1 << DEBUG_PIN);
  lastPin = PINB & (1 << ZC_PIN);            // 부팅 시 HIGH를 가짜 상승엣지로 취급하지 않음

  // Arduino Timer0 인터럽트는 사용하지 않는다. millis/micros/delay 사용 금지.
  // 25us ISR 지연을 줄이고, 모든 제어 시각은 같은 Timer1 시간축에서 계산한다.
  TIMSK = 0;
  TCCR0B = 0;
  TCCR1 = 0;
  GTCCR = 0;
  PLLCSR &= ~(1 << PCKE);                  // Timer1도 CPU RC 클럭 사용
  TCNT1 = 0;
  OCR1C = 24;
  OCR1A = 24;
  TIFR = (1 << OCF1A) | (1 << TOV1);        // 이전 플래그는 1을 써서 삭제
  TCCR1 = (1 << CTC1) | (1 << CS12);        // CPU/8, 25카운트 = 명목 25us
  TIMSK = (1 << OCIE1A);
  sei();
  // 부팅 시 ZC를 무한 대기하지 않는다. loop()에서 안정화 대기 후 OFF로 동기화한다.
}

ISR(TIMER1_COMPA_vect) {
  ++timerTicks;
  uint8_t pin = PINB & (1 << ZC_PIN);
  uint8_t rising = pin && !lastPin;
  lastPin = pin;
  uint16_t age = zcAge;
  if (age < ZC_AGE_LIMIT) ++age;
  zcAge = age;

  // OFF/지연/게이트 출력 중에도 항상 ZC를 검출한다.
  // 예상 시점보다 이른 노이즈는 무시하고, 수용한 새 엣지는 이전 펄스를 취소한다.
  if (rising && age >= edgeBlanking) {
    PORTB &= ~(1 << TRIAC_PIN);
    state = S_IDLE;
    edgePeriod = age;
    zcAge = 0;
    zcPulseState = P_HIGH;                   // 상승엣지는 후보. LOW 복귀 확인 전에는 점화 금지
    ++edgeSequence;
    return;
  }

  if (zcPulseState == P_HIGH) {
    if (age > zcPulseLimit) {
      zcPulseState = P_BAD;                 // 폭 과다/HIGH 고착: 이전 예약도 즉시 취소
      state = S_IDLE;
      PORTB &= ~(1 << TRIAC_PIN);
    } else if (!pin) {
      zcPulseWidth = (uint8_t)age;
      zcPulseState = P_VALID;
    }
  }

  // 한 엣지당 한 번만 점화. 신호가 끊겨도 예측 주기로 반복 점화하지 않는다.
  // loop()가 멈춰도 이미 예약한 펄스는 stopAt에서 반드시 끝난다.
  if (state == S_DELAY) {
    if (age >= fireAt) {
      if (zcPulseState == P_VALID && !pin) {
        PORTB |= (1 << TRIAC_PIN);
        state = S_TRIGGER;
      } else {
        state = S_IDLE;                    // 점화 순간에도 HIGH이면 이 반주기는 비점화
      }
    }
  } else if (state == S_TRIGGER && age >= stopAt) {
    PORTB &= ~(1 << TRIAC_PIN);
    state = S_IDLE;
  }
}

static void processEdge(uint8_t sequence, uint16_t period, uint8_t width) {
  if (period < MIN_ZC_PERIOD || period > MAX_ZC_PERIOD
      || !width || width > period / ZC_WIDTH_DIVISOR) {
    loseSync();                             // 첫 엣지/누락/실측 주기에 비해 너무 긴 펄스
    return;
  }
  if (lockCount) {
    uint16_t tolerance = periodTolerance(trackedPeriod);
    if (period + tolerance < trackedPeriod || period > trackedPeriod + tolerance) {
      loseSync();                           // 갑작스러운 주기 변경: OFF 후 다시 8회 검증
    }
  }
  if (!synchronized) {
    periodSum += period;
    ++lockCount;
    trackedPeriod = periodSum / lockCount;
    if (lockCount < LOCK_SAMPLES) return;
    periodQ3 = trackedPeriod * 8;
    synchronized = true;
  } else {
    // 매 반주기를 처리한다. ADC 갱신 간격(10ms) 때문에 주기 샘플을 버리지 않는다.
    periodQ3 = periodQ3 - trackedPeriod + period;
    trackedPeriod = (periodQ3 + 4) / 8;
  }

  // 평균보다 짧아진 반주기는 즉시 반영하여 다음 ZC 침범을 줄인다.
  uint16_t safePeriod = period < trackedPeriod ? period : trackedPeriod;
  uint16_t blanking = trackedPeriod - periodTolerance(trackedPeriod);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    edgeBlanking = blanking;
    zcPulseLimit = safePeriod / ZC_WIDTH_DIVISOR;
    PORTB ^= (1 << DEBUG_PIN);              // 동기화 후 50/60Hz 사각파 (육안 판정용 아님)
  }
  if (targetPhase == DIM_OFF) return;

  // SG3525 대기/최소출력 시간 유지는 없다. 첫 반주기만 최소출력으로 점화한다.
  // 실제 예약이 성공한 반주기마다 상승을 제한하므로 loop 속도에 좌우되지 않는다.
  uint16_t phase = targetPhase;
  if (outputPhase == DIM_OFF) phase = MAX_DIM_BASE;
  else if (phase < outputPhase && outputPhase - phase > MAX_DIM_STEP)
    phase = outputPhase - MAX_DIM_STEP;

  uint16_t start = scalePhase(safePeriod, phase + ZC_OFFSET_BASE);
  uint16_t end = start + scalePhase(safePeriod, TRIGGER_PULSE_BASE);
  uint16_t deadline = safePeriod - endGuardTicks(safePeriod);
  // 점화 시작 위상은 유지하고 낮은 출력에서만 폭을 줄여 다음 ZC와 여유를 둔다.
  // 예: 반주기333틱/위상308이면 start311, end317, 폭6틱. 실제 부하 래치는 시험 필요.
  if (end > deadline) end = deadline;
  if (end <= start) return;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // 계산 중 다음 엣지가 왔거나 예정 시각을 이미 지나쳤으면 이 반주기는 건너뛴다.
    if (edgeSequence == sequence && zcAge < start) {
      fireAt = start;
      stopAt = end;
      state = S_DELAY;
      outputPhase = phase;
    }
  }
}

static void updateInput(int potValue) {
  if (potValue >= ADC_OFF) inputOff = true;
  else if (potValue <= ADC_ON) inputOff = false;
  if (inputOff || !synchronized) {
    stopOutput();
    return;
  }

  if (potValue <= ADC_MIN) targetPhase = INPUT_MIN_DIM_BASE;
  else if (potValue >= ADC_MAX) targetPhase = MAX_DIM_BASE;
  else targetPhase = INPUT_MIN_DIM_BASE + (uint32_t)(potValue - ADC_MIN)
                * (MAX_DIM_BASE - INPUT_MIN_DIM_BASE) / (ADC_MAX - ADC_MIN);
  // 상단만 제한하여 v4.5의 저출력/중출력 입력 관계를 유지한다.
  if (targetPhase < MIN_DIM_BASE) targetPhase = MIN_DIM_BASE;
}

void loop() {
  uint8_t sequence, pulseState, width;
  uint16_t period, bootTick;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    sequence = edgeSequence;
    period = edgePeriod;
    bootTick = timerTicks;
    pulseState = zcPulseState;
    width = zcPulseWidth;
  }
  if (!powerSettled) {
    // ISR의 부팅 중 후보는 보정에 사용하지 않는다. Timer1은 setup에서 0부터 시작한다.
    // loop를 막지 않는 1회성 대기이며, 동기화 손실 때마다 이 부팅 대기를 반복하지 않는다.
    consumedSequence = sequence;
    if (bootTick >= POWER_SETTLE_TICKS) {
      powerSettled = true;
      lastInputTick = bootTick;
    }
    return;
  }
  if (sequence != consumedSequence && (pulseState == P_VALID || pulseState == P_BAD)) {
    if ((uint8_t)(sequence - consumedSequence) != 1) loseSync();
    consumedSequence = sequence;
    if (pulseState == P_BAD) {
      loseSync();
      haveSettledEdge = false;
    } else if (!haveSettledEdge) {
      // 전원 안정화 전후에 걸친 측정값도 제외한다. 다음 엣지부터 온전한 반주기이다.
      haveSettledEdge = true;
    } else {
      processEdge(sequence, period, width);
    }
  }

  uint16_t age, now;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    age = zcAge;
    now = timerTicks;
  }
  if (lockCount && age > trackedPeriod + periodTolerance(trackedPeriod)) loseSync();

  // 무대기 루프: ZC 처리는 계속하고 ADC만 10ms 간격으로 갱신한다.
  // 16비트 타이머가 순환해도 unsigned 차이는 올바르다.
  if ((uint16_t)(now - lastInputTick) >= INPUT_INTERVAL) {
    lastInputTick = now;
    updateInput(analogRead(POT_PIN));
  }
}

// ATtiny 프레임워크용 main()
int main(void) {
  init();
  setup();
  for (;;) loop();
  return 0;
}
