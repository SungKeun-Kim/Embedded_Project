#include "phase_search.h"

static unsigned short phase_delta(unsigned short a, unsigned short b)
{
   return a > b ? a - b : b - a;
}

unsigned char phase_find_minimum(unsigned char start, unsigned char *result,
   unsigned short *frequency, phase_measure_fn measure)
{
   unsigned short best, value, count, verify, first;
   unsigned char code = start, next;
   unsigned char response = 0;
   signed char direction = 0;
   if (start < PLL_POT_MIN || start > PLL_POT_MAX) return 0;
   if (!measure(code, &best, &count)) return 0;
   first = best;
   if (code < PLL_POT_MAX) {
      next = code + 1;
      if (!measure(next, &value, &count)) return 0;
      if (phase_delta(first, value) > PLL_CURRENT_DEADBAND) response = 1;
      if (value + PLL_CURRENT_DEADBAND < best) {
         code = next;
         best = value;
         direction = 1;
      }
   }
   if (!direction && code > PLL_POT_MIN) {
      next = code - 1;
      if (!measure(next, &value, &count)) return 0;
      if (phase_delta(first, value) > PLL_CURRENT_DEADBAND) response = 1;
      if (value + PLL_CURRENT_DEADBAND < best) {
         code = next;
         best = value;
         direction = -1;
      }
   }
   while (direction) {
      /* 범위 끝에서는 최소점을 양쪽에서 확인할 수 없으므로 저장하지 않는다. */
      if (code == PLL_POT_MIN || code == PLL_POT_MAX) return 0;
      next = (unsigned char)(code + direction);
      if (!measure(next, &value, &count)) return 0;
      if (value + PLL_CURRENT_DEADBAND >= best) break;
      code = next;
      best = value;
   }
   if (!response || code == PLL_POT_MIN || code == PLL_POT_MAX) return 0;
   /* 최선 코드를 다시 적용한 상태에서 확인하고 그 주파수를 저장한다. */
   if (!measure(code, &verify, &count)) return 0;
   if (phase_delta(best, verify) > PLL_MEAN_DRIFT_MAX) return 0;
   *result = code;
   *frequency = count;
   return 1;
}

unsigned char phase_track_once(unsigned char start, unsigned char low, unsigned char high,
   signed char direction, unsigned char *result, phase_measure_fn measure)
{
   unsigned short a, b, again, fa, fb, fagain;
   unsigned char candidate;
   *result = start;
   if (low < PLL_POT_MIN || high > PLL_POT_MAX || start < low || start > high) return 0;
   if (direction != 1 && direction != -1) return 0;
   if ((direction > 0 && start == high) || (direction < 0 && start == low)) return 1;
   candidate = (unsigned char)(start + direction);
   if (!measure(start, &a, &fa) || !measure(candidate, &b, &fb) ||
       !measure(start, &again, &fagain)) return 0;
   /* 다른 압력 상태끼리 비교하지 않는다. 같은 설정의 전류/주파수부터 검사한다. */
   if (phase_delta(a, again) > PLL_CURRENT_DEADBAND || phase_delta(fa, fagain) > 1) return 0;
   if (b + PLL_CURRENT_DEADBAND < a && b + PLL_CURRENT_DEADBAND < again) *result = candidate;
   return 1;
}
