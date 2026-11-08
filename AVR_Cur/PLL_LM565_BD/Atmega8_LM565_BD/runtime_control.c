#include "runtime_control.h"
#include <string.h>

static unsigned short rt_difference(unsigned short a, unsigned short b)
{
   return a > b ? a - b : b - a;
}

static void rt_window_clear(runtime_control *r)
{
   r->sum = r->peak_sum = 0;
   r->elapsed = r->samples = r->high = 0;
   r->low = 65535U;
   r->period_low = 65535U;
   r->period_high = 0;
   r->peaks = 0;
   r->window_mode = r->mode;
}

static void rt_pattern_clear(runtime_control *r)
{
   r->above = r->seen_rise = r->fast = r->slow = 0;
   r->rise_age = r->period = 0;
   r->valley = r->peak = r->filtered;
   r->mode = RT_UNKNOWN;
   rt_window_clear(r);
}

void runtime_init(runtime_control *r, unsigned char code)
{
   memset(r, 0, sizeof(*r));
   r->origin = r->code = r->good_code = code;
   rt_pattern_clear(r);
}

static void rt_suspend(runtime_control *r)
{
   r->code = r->good_code; /* 시험 중인 한 칸만 복귀하고 확인된 보정은 RAM에 남긴다. */
   r->phase = RT_MONITOR;
   r->baseline_valid = r->triggers = 0;
   r->wait = 0;
   rt_pattern_clear(r);
}

static void rt_finish(runtime_control *r)
{
   r->code = r->good_code;
   r->phase = RT_COOLDOWN;
   r->wait = RT_COOLDOWN_TICKS;
   r->baseline_valid = r->triggers = 0;
   rt_pattern_clear(r);
}

static void rt_step_lower(runtime_control *r)
{
   unsigned char lower = r->origin > PLL_POT_MIN + PLL_RUNTIME_RADIUS ?
      r->origin - PLL_RUNTIME_RADIUS : PLL_POT_MIN;
   if (r->code <= lower) { rt_finish(r); return; }
   --r->code; /* 실측 관계: 코드 감소 -> 저항 증가 -> 낮은 주파수 쪽 */
   r->phase = RT_SETTLING;
   r->wait = RT_SETTLE_TICKS;
   rt_pattern_clear(r);
}

/* 전류 상승 시작 사이의 간격을 측정한다. 파형 전체를 저장하지 않는다. */
static void rt_pattern(runtime_control *r, unsigned short dt)
{
   unsigned short value = r->filtered;
   if (r->rise_age < RT_EXPIRE_TICKS) r->rise_age += dt;
   if (r->seen_rise && r->rise_age > RT_FAST_TICKS && r->mode == RT_CONTINUOUS) {
      r->mode = RT_UNKNOWN;
      r->fast = 0;
   }
   if (r->rise_age >= RT_EXPIRE_TICKS) {
      r->seen_rise = r->fast = r->slow = 0;
      r->mode = RT_UNKNOWN;
   }
   if (!r->above) {
      if (value < r->valley) r->valley = value;
      if (value >= r->valley + RT_EDGE_Q4 && r->rise_age >= RT_EDGE_MIN_TICKS) {
         if (r->seen_rise) {
            r->period = r->rise_age;
            if (r->period <= RT_FAST_TICKS) {
               if (r->fast < RT_REQUIRED_INTERVALS) ++r->fast;
               r->slow = 0;
               r->mode = r->fast >= RT_REQUIRED_INTERVALS ? RT_CONTINUOUS : RT_UNKNOWN;
            } else if (r->period >= RT_SLOW_TICKS) {
               if (r->slow < RT_REQUIRED_INTERVALS) ++r->slow;
               r->fast = 0;
               r->mode = r->slow >= RT_REQUIRED_INTERVALS ? RT_PRESS : RT_UNKNOWN;
            } else { r->fast = r->slow = 0; r->mode = RT_UNKNOWN; }
         }
         r->seen_rise = r->above = 1;
         r->rise_age = 0;
         r->peak = value;
         if (r->mode == RT_CONTINUOUS && r->window_mode == RT_CONTINUOUS) {
            if (r->period < r->period_low) r->period_low = r->period;
            if (r->period > r->period_high) r->period_high = r->period;
         }
      }
   } else {
      if (value > r->peak) r->peak = value;
      if (value + RT_EDGE_Q4 <= r->peak) {
         if (r->mode == RT_CONTINUOUS && r->window_mode == RT_CONTINUOUS && r->peaks < 255) {
            r->peak_sum += r->peak;
            ++r->peaks;
         }
         r->above = 0;
         r->valley = value;
      }
   }
}

static unsigned char rt_period_matches(unsigned short a, unsigned short b)
{
   return a && b && rt_difference(a,b) <= a / 4U;
}

/* 약 3초의 동등한 작업 구간이 모인 경우에만 비교한다. */
static void rt_evaluate(runtime_control *r, unsigned short frequency)
{
   unsigned short value;
   unsigned char kind = RT_UNKNOWN;
   if (r->mode == RT_CONTINUOUS && r->peaks >= RT_REQUIRED_PEAKS &&
       r->period_high && (unsigned short)(r->period_high - r->period_low) <= r->period_high / 4U) {
      kind = RT_CONTINUOUS;
      value = (unsigned short)(r->peak_sum / r->peaks);
   } else if (r->standby_valid && r->mode != RT_PRESS &&
      r->high - r->low <= RT_IDLE_RANGE_Q4 &&
      r->high <= r->standby + RT_IDLE_ABOVE_STANDBY_Q4) {
      kind = RT_IDLE;
      value = (unsigned short)(r->sum / r->samples);
   } else {
      if (r->phase == RT_OBSERVING) rt_finish(r);
      else {
         r->triggers = 0;
         if (r->mode == RT_PRESS) r->baseline_valid = 0;
      }
      return;
   }
   if (r->phase == RT_OBSERVING) {
      if (r->baseline_valid != kind ||
          (kind == RT_CONTINUOUS && !rt_period_matches(r->good_period,r->period)) ||
          frequency > r->good_freq + 1U ||
          value + RT_IMPROVE_Q4 >= r->good_value) {
         rt_finish(r);
         return;
      }
      r->good_code = r->code;
      r->good_value = value;
      r->good_period = r->period;
      r->good_freq = frequency;
      rt_step_lower(r);
      return;
   }
   if (r->baseline_valid != kind ||
       (kind == RT_CONTINUOUS && !rt_period_matches(r->baseline_period,r->period))) {
      r->baseline_valid = kind;
      r->baseline = value;
      r->baseline_period = r->period;
      r->triggers = 0;
   } else if (value >= r->baseline + RT_TRIGGER_Q4) {
      if (++r->triggers >= RT_REQUIRED_WINDOWS) {
         r->good_value = value;
         r->good_period = r->period;
         r->good_freq = frequency;
         rt_step_lower(r);
      }
   } else {
      r->triggers = 0;
      if (value + RT_TRIGGER_Q4 < r->baseline) r->baseline = value;
   }
}

unsigned char runtime_sample(runtime_control *r, unsigned short adc, unsigned short frequency,
   unsigned short dt, unsigned char running, unsigned char permitted)
{
   unsigned short input = adc << 4;
   if (!r->valid) { r->filtered = input; r->valid = 1; rt_pattern_clear(r); }
   else if (input > r->filtered) r->filtered += (input - r->filtered + 3U) / 4U;
   else r->filtered -= (r->filtered - input + 3U) / 4U;
   if (!dt || dt > 2U || adc < 4U || adc >= 1023U) {
      rt_suspend(r);
      r->standby_ticks = 0;
      return running && (adc < 4U || adc >= 1023U);
   }
   if (!running) {
      rt_suspend(r);
      if (!r->standby_ticks) r->standby_low = r->standby_high = r->filtered;
      if (r->filtered < r->standby_low) r->standby_low = r->filtered;
      if (r->filtered > r->standby_high) r->standby_high = r->filtered;
      r->standby_ticks += dt;
      if (r->standby_ticks >= RT_WINDOW_TICKS) {
         if (r->standby_high - r->standby_low <= RT_IDLE_RANGE_Q4) {
            r->standby = (r->standby_high + r->standby_low) / 2U;
            r->standby_valid = 1;
         }
         r->standby_ticks = 0;
      }
      return 0;
   }
   r->standby_ticks = 0;
   if (!permitted) { rt_suspend(r); return 0; }
   if (frequency < PLL_FREQ_COUNT_MIN || frequency > PLL_FREQ_COUNT_MAX) {
      rt_suspend(r);
      return 1;
   }
   if (r->phase == RT_SETTLING || r->phase == RT_COOLDOWN) {
      if (r->wait > dt) { r->wait -= dt; return 0; }
      r->wait = 0;
      r->phase = r->phase == RT_SETTLING ? RT_OBSERVING : RT_MONITOR;
      rt_pattern_clear(r);
   }
   if (r->phase == RT_OBSERVING && r->filtered > r->good_value + RT_TRIGGER_Q4) {
      rt_finish(r); /* 큰 악화는 관찰 종료까지 기다리지 않고 시험 위치를 되돌린다. */
      return 0;
   }
   rt_pattern(r,dt);
   if (r->mode != r->window_mode) rt_window_clear(r);
   r->sum += r->filtered;
   ++r->samples;
   if (r->filtered < r->low) r->low = r->filtered;
   if (r->filtered > r->high) r->high = r->filtered;
   r->elapsed += dt;
   if (r->elapsed >= RT_WINDOW_TICKS) {
      rt_evaluate(r,frequency);
      rt_window_clear(r);
   }
   return 0;
}
