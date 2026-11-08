#ifndef RUNTIME_CONTROL_H
#define RUNTIME_CONTROL_H
#include "runtime_config.h"

#define RT_UNKNOWN 0
#define RT_CONTINUOUS 1
#define RT_PRESS 2
#define RT_IDLE 3
#define RT_MONITOR 0
#define RT_SETTLING 1
#define RT_OBSERVING 2
#define RT_COOLDOWN 3

typedef struct {
   unsigned long sum, peak_sum;
   unsigned short filtered, valley, peak, rise_age, period;
   unsigned short elapsed, samples, low, high, period_low, period_high;
   unsigned short wait, baseline, baseline_period, good_value, good_period, good_freq;
   unsigned short standby, standby_low, standby_high, standby_ticks;
   unsigned char origin, code, good_code, phase, mode, window_mode;
   unsigned char valid, above, seen_rise, fast, slow, peaks, baseline_valid, triggers, standby_valid;
} runtime_control;

void runtime_init(runtime_control *r, unsigned char code);
/* code는 요청 RDAC값이다. 반환값 1이면 상위에서 출력 정지를 요청한다. */
unsigned char runtime_sample(runtime_control *r, unsigned short adc, unsigned short frequency,
   unsigned short dt, unsigned char running, unsigned char permitted);
#endif
