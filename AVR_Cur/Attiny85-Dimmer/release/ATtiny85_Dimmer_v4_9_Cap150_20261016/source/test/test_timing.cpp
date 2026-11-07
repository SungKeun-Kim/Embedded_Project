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
      // 정상 연속 점화 시험의 노이즈는 점화 이전에 배치한다.
      // 점화 순간 HIGH 차단은 noiseAtFiring()에서 따로 검증한다.
      tick(p < width || (noise && (p == period / 8 || p == period / 4 || p == period / 3)),
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
  assert(synchronized && trackedPeriod == 400);
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
        if (pulses.size() != initial + 100)
          std::fprintf(stderr, "matrix mismatch: hz=%.1f clock=%.2f period=%u adc=%d phase=%u fire=%u pulses=%u\n",
                       hz, clock, period, adc, unsigned(outputPhase), unsigned(fireAt),
                       unsigned(pulses.size() - initial));
        assert(pulses.size() == initial + 100);
        for (unsigned n = 0; n < 100; ++n) {
          const auto &pulse = pulses[initial + n];
          unsigned delay = pulse.start - (base + n * period);
          assert(pulse.end > pulse.start);
          assert(pulse.end < base + (n + 1) * period);
          assert(std::abs(double(delay) / period
                          - double(outputPhase + ZC_OFFSET_BASE) / ZC_PERIOD_REF) < 0.006);
          unsigned width = pulse.end - pulse.start;
          assert(width <= unsigned(scalePhase(period, TRIGGER_PULSE_BASE) + 1));
          assert(width * 25.0 / clock >= 110.0); // 검증한47~63Hz/클럭범위에서 지나치게 짧지 않은지
          if (outputPhase + ZC_OFFSET_BASE + TRIGGER_PULSE_BASE + END_GUARD_BASE <= ZC_PERIOD_REF)
            assert(width >= unsigned(scalePhase(period, TRIGGER_PULSE_BASE) - 1));
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
      targetPhase = phase;
      zcAge = 0;
      processEdge(edgeSequence, period, 3);
      assert(state == S_DELAY);
      assert(fireAt < stopAt);
      assert(stopAt - fireAt >= 4);        // 허용 주기 전체에서 최소4개의 폴링 틱 확보
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
  assert(synchronized && trackedPeriod == 400 && outputPhase != DIM_OFF);
  settle(400, ADC_MIN);
  cycles(333, 20);
  assert(synchronized && trackedPeriod == 333 && outputPhase != DIM_OFF);
  settle(333, ADC_MAX);
  auto before = pulses.size();
  for (unsigned i = 0; i < 4000; ++i) tick(false);
  assert(!synchronized && !gate && pulses.size() == before);
  cycles(333, 20);
  assert(synchronized && outputPhase != DIM_OFF);

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
  targetPhase = MIN_DIM_BASE;
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
  simulatedAdc = ADC_MIN;
  updateInput(ADC_MIN);
  assert(targetPhase == MIN_DIM_BASE && outputPhase == DIM_OFF);
  cycles(333, 1);
  assert(outputPhase == MAX_DIM_BASE && pulses.size() == 1);
  // ADC를 반복 갱신해도 상승 제한은 진행하지 않는다. 실제 반주기마다만 진행한다.
  for (unsigned n = 0; n < 100; ++n) updateInput(ADC_MIN);
  assert(outputPhase == MAX_DIM_BASE);
  cycles(333, 1);
  assert(outputPhase == MAX_DIM_BASE - MAX_DIM_STEP);
  updateInput(ADC_OFF);
  assert(outputPhase == DIM_OFF && state == S_IDLE);
  updateInput(ADC_OFF - 1);
  assert(!inputOff && targetPhase == MAX_DIM_BASE); // 기존 입력 경계:900 미만부터 ON
  updateInput(ADC_ON);
  assert(!inputOff && targetPhase == MAX_DIM_BASE && outputPhase == DIM_OFF);

  // 타이머/시퀀스 순환을 여러 번 통과하며 OFF에서도 모든 반주기 추적.
  reset();
  waitPower();
  cycles(333, 2000);
  assert(synchronized && pulses.empty());
  assert(edgeSequence == uint8_t(2000));
  assert(consumedSequence == edgeSequence);
  std::puts("PASS: immediate start, half-cycle slew limit, legacy ON threshold, timer/sequence wrap");
}

void responseLatency() {
  unsigned cases = 0;
  for (unsigned hz : {50u, 60u}) {
    for (double clock : {0.85, 1.0, 1.15}) {
      unsigned period = std::lround(20000 * clock / hz);
      for (unsigned offset : {0u, 1u, 100u, 399u}) {
        reset();
        settle(period, 1023);              // 전원/동기화 완료 후 START
        unsigned origin = ticks;
        auto advance = [&]() { tick((ticks - origin) % period < 3); };
        for (unsigned n = 0; n < offset; ++n) advance();
        unsigned start = ticks;
        simulatedAdc = ADC_MIN;
        while (pulses.empty() || outputPhase != MIN_DIM_BASE || !gate) {
          advance();
          assert(ticks - start < unsigned(0.22 * 40000 * clock));
        }
        assert((pulses.front().start - start) / (40000.0 * clock) < 0.035);
        assert((ticks - start) / (40000.0 * clock) < 0.22);
        // 연속 출력 중 최소로 내리면 다음 입력 갱신/반주기에 반영된다.
        simulatedAdc = ADC_MAX;
        unsigned changed = ticks;
        while (outputPhase != MAX_DIM_BASE || !gate) {
          advance();
          assert(ticks - changed < unsigned(0.035 * 40000 * clock));
        }
        // 다시 올릴 때도 추가 시작 대기 없이 0.22초 안에 최대에 도달한다.
        simulatedAdc = ADC_MIN;
        changed = ticks;
        while (outputPhase != MIN_DIM_BASE || !gate) {
          advance();
          assert(ticks - changed < unsigned(0.22 * 40000 * clock));
        }
        simulatedAdc = ADC_OFF;
        changed = ticks;
        while (targetPhase != DIM_OFF || gate) {
          advance();
          assert(ticks - changed <= INPUT_INTERVAL + 1);
        }
        auto before = pulses.size();
        for (unsigned n = 0; n < 3 * period; ++n) advance();
        assert(pulses.size() == before);
        simulatedAdc = ADC_MIN;            // STOP/START 반복에서도 대기 없음
        changed = ticks;
        while (pulses.size() == before) {
          advance();
          assert(ticks - changed < unsigned(0.035 * 40000 * clock));
        }
        ++cases;
      }
    }
  }
  std::printf("PASS: %u response cases, first gate <35ms, max <220ms, restart/volume/OFF\n", cases);
}

void slowInputSweep() {
  for (unsigned period : {333u, 400u}) {
    reset();
    settle(period, 1023);
    uint16_t previous = MAX_DIM_BASE;
    // 정지 상태에서 볼륨을 아주 천천히 올리는 입력: ADC 한 단계마다 3반주기 유지.
    for (int adc = 1023; adc >= ADC_MIN; --adc) {
      simulatedAdc = adc;
      cycles(period, 3);
      auto before = pulses.size();
      cycles(period, 1);
      if (adc > ADC_ON) {
        assert(inputOff && targetPhase == DIM_OFF && pulses.size() == before);
      } else {
        assert(!inputOff && synchronized && pulses.size() == before + 1);
        assert(outputPhase <= previous && previous - outputPhase <= 1);
        if (adc >= ADC_MAX) assert(outputPhase == MAX_DIM_BASE);
        previous = outputPhase;
      }
    }
    assert(outputPhase == MIN_DIM_BASE);
    // 반대 방향에서도 단조롭게 감소하며 OFF 임계값 직전까지 펄스가 유지된다.
    for (int adc = ADC_MIN; adc <= 1023; ++adc) {
      simulatedAdc = adc;
      cycles(period, 3);
      auto before = pulses.size();
      cycles(period, 1);
      if (adc >= ADC_OFF) {
        assert(inputOff && targetPhase == DIM_OFF && pulses.size() == before);
      } else {
        assert(!inputOff && synchronized && pulses.size() == before + 1);
        assert(outputPhase >= previous && outputPhase - previous <= 1);
        previous = outputPhase;
      }
    }
  }
  std::puts("PASS: slow ADC sweep both directions at 50/60Hz, ON=899 OFF=900; no gate gaps in ON range");
}

void legacyLowRange() {
  reset();
  waitPower();
  cycles(332, 10);
  for (int adc = 899; adc >= 700; --adc) {
    updateInput(adc);
    assert(!inputOff && targetPhase != DIM_OFF);
    // 이전 소스의2단계 정수 map과 같은 입력으로 비교한다. 저출력1/5 구간 위상차는2 이내.
    unsigned scaled = adc >= 880 ? 1023 : (adc - 61) * 1023 / (880 - 61);
    unsigned legacy = 128 + scaled * (308 - 128) / 1023;
    assert(std::abs(int(targetPhase) - int(legacy)) <= 2);
  }
  // 동일 입력의 정지/점화 경계: 이전과 같은899에서 켜지고900에서 꺼짐.
  updateInput(900);
  assert(inputOff && targetPhase == DIM_OFF);
  updateInput(899);
  assert(!inputOff && targetPhase == 308);
  outputPhase = targetPhase;
  zcAge = 0;
  processEdge(edgeSequence, 332, 3);
  assert(fireAt == 310 && stopAt == 316);  // 시작은 기존과 같고 종료는16틱 앞에 둔다.
  std::puts("PASS: same ADC low-range mapping within 2 reference ticks of legacy; 310->316 gate");
}

void maximumOutputCap() {
  reset();
  waitPower();
  cycles(332, 10);
  // 독립적인 v4.5 입력 곡선과 전체 ADC 범위를 비교한다.
  // 하단 곡선은 동일하고, 최대출력만150으로 제한되어야 한다.
  for (int adc = 0; adc <= 1023; ++adc) {
    updateInput(adc);
    if (adc >= 900) {
      assert(inputOff && targetPhase == DIM_OFF && state == S_IDLE);
      continue;
    }
    unsigned oldPhase = adc <= 61 ? 133 : adc >= 880 ? 308
      : 133 + unsigned(adc - 61) * 175 / 819;
    unsigned expected = oldPhase < 150 ? 150 : oldPhase;
    assert(!inputOff && targetPhase == expected);
    assert(targetPhase >= 150 && targetPhase <= 308);
    if (oldPhase >= 150) assert(targetPhase == oldPhase);
  }
  // 실제 출력의 상한, 입력을 계속 높여도 점화가 앞당겨지지 않는지 확인.
  for (unsigned period : {333u, 400u}) {
    reset();
    settle(period, 0);
    assert(outputPhase == 150);
    assert(fireAt == (period == 333 ? 152 : 183));
    auto before = pulses.size();
    cycles(period, 20);
    assert(pulses.size() == before + 20);
    for (unsigned n = before; n < pulses.size(); ++n)
      assert(pulses[n].end - pulses[n].start == (period == 333 ? 20u : 24u));
  }
  std::puts("PASS: v4.9 cap150, all1024 ADC codes, v4.5 lower curve retained, 50/60Hz max gate");
}

void noiseAtFiring() {
  // 과거 시험의 고정200틱 노이즈가140위상의 점화 순간과 겹쳤던468틱도 포함.
  for (unsigned period : {333u, 400u, 468u}) {
    reset();
    settle(period, ADC_MIN);
    auto before = pulses.size();
    for (unsigned p = 0; p < period; ++p) {
      bool noise = p > 3 && p == fireAt;
      tick(p < 3 || noise);
      if (noise) assert(state == S_IDLE && !gate);
    }
    assert(pulses.size() == before && synchronized);
    cycles(period, 1);
    assert(pulses.size() == before + 1);
  }
  std::puts("PASS: noise HIGH at firing cancels that half-cycle; next clean half-cycle recovers");
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
    assert(synchronized && outputPhase != DIM_OFF);

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
  responseLatency();
  slowInputSweep();
  legacyLowRange();
  maximumOutputCap();
  noiseAtFiring();
  pulseWidths();
  std::puts("All timing regression tests passed.");
}
