// 실제 src/main.cpp를 그대로 실행하여 회귀 검증한다. 별도 알고리즘 복제본이 아니다.
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#define main firmware_main
#include "../src/main.cpp"
#undef main

int simulatedAdc = 1023;
struct Pulse { unsigned start, end; };
std::vector<Pulse> pulses;
unsigned ticks;
bool gate;

void reset() {
  PORTB = DDRB = PINB = 0;
  timerTicks = 0;
  zcAge = ZC_AGE_LIMIT;
  edgePeriod = edgeSequence = consumedSequence = 0;
  edgeBlanking = MIN_ZC_PERIOD;
  fireAt = stopAt = state = lastPin = 0;
  zcPulseState = P_NONE;
  zcPulseWidth = 0;
  inputOff = true;
  lastInputTick = 0;
  powerSettled = haveSettledEdge = false;
  loseSync();
  simulatedAdc = 1023;
  ticks = 0;
  gate = false;
  pulses.clear();
  setup();
}

void tick(bool high, bool runLoop = true) {
  PINB = high ? (1 << ZC_PIN) : 0;
  TIMER1_COMPA_vect();
  if (runLoop) loop();
  bool nextGate = PORTB & (1 << TRIAC_PIN);
  if (nextGate && !gate) pulses.push_back({ticks, 0});
  if (!nextGate && gate) pulses.back().end = ticks;
  gate = nextGate;
  ++ticks;
}

void cycles(unsigned period, unsigned count, bool noise = false, unsigned loopInterval = 1,
            unsigned width = 3) {
  for (unsigned n = 0; n < count; ++n)
    for (unsigned p = 0; p < period; ++p)
      tick(p < width || (noise && (p == 100 || p == 200 || p == period / 2)),
           ticks % loopInterval == 0);
}

void settle(unsigned period, int adc) {
  simulatedAdc = adc;
  cycles(period, 230000 / period + 1);
  assert(synchronized);
}

void waitPower() {
  for (unsigned n = 0; n < POWER_SETTLE_TICKS; ++n) tick(false);
  assert(powerSettled && !synchronized && lockCount == 0);
}

void powerSettling() {
  reset();
  simulatedAdc = ADC_MIN;
  // 전원 투입 중 정상으로 보이는 신호도 보정에 반영하지 않는다.
  for (unsigned n = 0; n < POWER_SETTLE_TICKS - 1; ++n) {
    tick(n % 333 < 3);
    assert(!powerSettled && !synchronized && lockCount == 0);
    assert(trackedPeriod == 0 && outputPhase == DIM_OFF && pulses.empty());
  }
  tick(false);
  assert(powerSettled && !haveSettledEdge);
  for (unsigned n = 0; n < MIN_ZC_PERIOD; ++n) tick(false);
  cycles(400, 1);                          // 경계를 걸친 측정값은 버리고 첫 엣지로만 사용
  assert(haveSettledEdge && lockCount == 0);
  cycles(400, LOCK_SAMPLES - 1);
  assert(!synchronized && lockCount == LOCK_SAMPLES - 1);
  cycles(400, 1);
  assert(synchronized && trackedPeriod == 400 && outputPhase == DIM_OFF);
  assert(pulses.empty());
  std::puts("PASS: boot settling excludes all early/cross-boundary samples, then 8 full periods");
}

void matrix() {
  unsigned cases = 0;
  for (double hz : {47.0, 50.0, 60.0, 63.0}) {
    for (double clock : {0.85, 0.90, 1.0, 1.10, 1.15}) {
      unsigned period = std::lround(20000 * clock / hz);
      for (int adc : {ADC_MIN, (ADC_MIN + ADC_MAX) / 2, ADC_MAX}) {
        reset();
        settle(period, adc);
        auto initial = pulses.size();
        unsigned base = ticks;
        cycles(period, 100, true, 8);       // loop 처리가 200us 간격이어도 점화 가능
        assert(pulses.size() == initial + 100);
        for (unsigned n = 0; n < 100; ++n) {
          const auto &pulse = pulses[initial + n];
          unsigned delay = pulse.start - (base + n * period);
          assert(pulse.end > pulse.start);
          assert(pulse.end < base + (n + 1) * period);
          assert(std::abs(double(delay) / period
                          - double(outputPhase + ZC_OFFSET_BASE) / ZC_PERIOD_REF) < 0.006);
          assert(pulse.end - pulse.start >= unsigned(scalePhase(period, TRIGGER_PULSE_BASE) - 1));
        }
        ++cases;
      }
    }
  }
  std::printf("PASS: %u frequency/clock/ADC combinations, 100 pulses each\n", cases);
}

void boundaries() {
  // 지원 범위 전체와 모든 출력 위상에 대해 펄스 종료 여유 확인.
  for (unsigned period = MIN_ZC_PERIOD; period <= MAX_ZC_PERIOD; ++period) {
    reset();
    waitPower();
    cycles(period, 10);
    assert(synchronized);
    for (unsigned phase = MIN_DIM_BASE; phase <= MAX_DIM_BASE; ++phase) {
      outputPhase = phase;
      zcAge = 0;
      processEdge(edgeSequence, period, 3);
      assert(state == S_DELAY);
      assert(fireAt < stopAt);
      assert(stopAt <= period - scalePhase(period, END_GUARD_BASE));
      // 허용 범위의 최대 단축 + 2틱 지터가 생겨도 다음 엣지 이전 종료.
      assert(unsigned(stopAt) + 2 < period - periodTolerance(period));
    }
    for (unsigned i = 0; i < ZC_AGE_LIMIT + 1; ++i) tick(false);
    assert(!synchronized && state == S_IDLE && !gate);
  }
  std::printf("PASS: all 251 periods x %u phase settings, shortened-period guard, timeout\n",
              unsigned(MAX_DIM_BASE - MIN_DIM_BASE + 1));
}

void faultsAndRecovery() {
  reset();
  simulatedAdc = ADC_MIN;
  for (unsigned i = 0; i < 200000; ++i) tick(false);
  assert(pulses.empty() && !synchronized);
  for (unsigned i = 0; i < 200000; ++i) tick(true);
  assert(pulses.empty() && !synchronized);

  reset();
  settle(333, ADC_MIN);
  cycles(400, 20);
  assert(synchronized && trackedPeriod == 400 && outputPhase == DIM_OFF);
  settle(400, ADC_MIN);
  cycles(333, 20);
  assert(synchronized && trackedPeriod == 333 && outputPhase == DIM_OFF);
  settle(333, ADC_MAX);
  auto before = pulses.size();
  for (unsigned i = 0; i < 4000; ++i) tick(false);
  assert(!synchronized && !gate && pulses.size() == before);
  cycles(333, 20);
  assert(synchronized && outputPhase == DIM_OFF);

  reset();
  settle(400, ADC_MAX);
  before = pulses.size();
  for (unsigned i = 0; i < 100; ++i) cycles(i % 2 ? 399 : 401, 1);
  assert(synchronized && pulses.size() == before + 100);

  // foreground 정지: ISR은 펄스를 끝내며 새로운 반주기를 임의 점화하지 않는다.
  reset();
  settle(333, ADC_MIN);
  for (unsigned i = 0; i < 4; ++i) tick(i < 3);
  assert(state == S_DELAY);
  for (unsigned i = 4; i < 1000; ++i) tick(false, false);
  assert(!gate && state == S_IDLE);
  loop();
  assert(!synchronized);

  // stale 계산 결과 또는 이미 늦은 예약은 버린다.
  reset();
  waitPower();
  cycles(333, 10);
  outputPhase = MIN_DIM_BASE;
  zcAge = 0;
  state = S_IDLE;
  processEdge(edgeSequence - 1, 333, 3);
  assert(state == S_IDLE);
  zcAge = 200;
  processEdge(edgeSequence, 333, 3);
  assert(state == S_IDLE);
  std::puts("PASS: missing/stuck ZC, 50<->60 transition, jitter, stalled/late foreground");
}

void startupAndOff() {
  reset();
  waitPower();
  cycles(333, 9);
  assert(synchronized);
  for (unsigned n = 0; n < QUIET_START_CYCLES; ++n) {
    updateInput(ADC_MIN);
    assert(outputPhase == DIM_OFF);
  }
  for (unsigned n = 0; n < SOFT_START_CYCLES; ++n) {
    updateInput(ADC_MIN);
    assert(outputPhase == MAX_DIM_BASE);
  }
  updateInput(ADC_MIN);
  assert(outputPhase == MAX_DIM_BASE - MAX_DIM_STEP);
  updateInput(ADC_OFF);
  assert(outputPhase == DIM_OFF && state == S_IDLE);
  updateInput(ADC_OFF - 1);
  assert(inputOff && outputPhase == DIM_OFF);
  updateInput(ADC_ON);
  assert(!inputOff && quietRemain == QUIET_START_CYCLES - 1);

  // 타이머/시퀀스 순환을 여러 번 통과하며 OFF에서도 모든 반주기 추적.
  reset();
  waitPower();
  cycles(333, 2000);
  assert(synchronized && pulses.empty());
  assert(edgeSequence == uint8_t(2000));
  assert(consumedSequence == edgeSequence);
  std::puts("PASS: quiet/soft start, slew limit, OFF hysteresis, timer/sequence wrap");
}

void pulseWidths() {
  unsigned cases = 0;
  for (unsigned period : {250u, 283u, 333u, 400u, 460u, 500u}) {
    for (unsigned width : {1u, 2u, 4u, period / 10, period / 4}) {
      for (int adc : {ADC_MIN, ADC_MAX}) {
        reset();
        simulatedAdc = adc;
        cycles(period, 230000 / period + 1, false, 1, width);
        assert(synchronized);
        auto before = pulses.size();
        auto base = ticks;
        cycles(period, 30, false, 8, width);
        assert(synchronized && pulses.size() == before + 30);
        for (unsigned n = 0; n < 30; ++n) {
          unsigned delay = pulses[before + n].start - base - n * period;
          assert(delay > width);
          assert(std::abs(double(delay) / period
                         - double(outputPhase + ZC_OFFSET_BASE) / ZC_PERIOD_REF) < 0.006);
        }
        ++cases;
      }
    }
    reset();
    settle(period, ADC_MIN);
    auto before = pulses.size();
    cycles(period, 20, false, 1, period / 4 + 1);
    assert(!synchronized && !gate && pulses.size() == before);
    cycles(period, 12);
    assert(synchronized && outputPhase == DIM_OFF);

    reset();
    settle(period, ADC_MIN);
    before = pulses.size();
    for (unsigned n = 0; n < period * 3; ++n) tick(true);
    assert(!synchronized && !gate && pulses.size() == before);
  }
  // 허용 폭을 넘긴 펄스는 foreground가 멈췄어도 ISR에서 차단한다.
  reset();
  settle(333, ADC_MIN);
  auto before = pulses.size();
  for (unsigned n = 0; n < 1000; ++n) tick(true, false);
  assert(zcPulseState == P_BAD && state == S_IDLE && !gate && pulses.size() == before);
  loop();
  assert(!synchronized);
  std::printf("PASS: %u width/period/output cases, overwide/stuck pulses blocked, recovery\n", cases);
}

int main() {
  powerSettling();
  matrix();
  boundaries();
  faultsAndRecovery();
  startupAndOff();
  pulseWidths();
  std::puts("All timing regression tests passed.");
}
