#include "phase_search.h"

static unsigned short phase_delta(unsigned short a, unsigned short b)
{
   return a > b ? a - b : b - a;
}

unsigned char phase_find_minimum(unsigned char start, unsigned char *result,
   unsigned short *frequency, phase_measure_fn measure)
{
   unsigned short center, again, left, right, count, final_value;
   unsigned char code = start, confirmed = 0, order = 0;
   if (start < PLL_POT_MIN || PLL_POT_CODE_ABOVE_MAX(start)) return 0;
   for (;;) {
      unsigned char status;
      left = right = 65535U;
      status = measure(code, &center, &count);
      if (status == PHASE_MEASURE_FAILED) return 0;
      if (status != PHASE_MEASURE_OK) { confirmed = 0; continue; }
      /* 좌/우 방문 순서를 바꿔 시간 경과에 따른 한쪽 방향 편향을 줄인다. */
      order ^= 1;
      if (order && code > PLL_POT_MIN) {
         status = measure(code - 1, &left, &count);
         if (status == PHASE_MEASURE_FAILED) return 0;
         if (status != PHASE_MEASURE_OK) { confirmed = 0; continue; }
      }
      if (code < PLL_POT_MAX) {
         status = measure(code + 1, &right, &count);
         if (status == PHASE_MEASURE_FAILED) return 0;
         if (status != PHASE_MEASURE_OK) { confirmed = 0; continue; }
      }
      if (!order && code > PLL_POT_MIN) {
         status = measure(code - 1, &left, &count);
         if (status == PHASE_MEASURE_FAILED) return 0;
         if (status != PHASE_MEASURE_OK) { confirmed = 0; continue; }
      }
      status = measure(code, &again, &count);
      if (status == PHASE_MEASURE_FAILED) return 0;
      if (status != PHASE_MEASURE_OK || phase_delta(center, again) > PLL_MEAN_DRIFT_MAX) {
         confirmed = 0;
         continue;
      }
      /* 샘플 합계는 최대 32736이다. 차분 비교로 16비트 덧셈 넘침을 피한다. */
      if (left < center && center - left > PLL_CURRENT_DEADBAND &&
          left < again && again - left > PLL_CURRENT_DEADBAND && left <= right) {
         --code; confirmed = 0; continue;
      }
      if (right < center && center - right > PLL_CURRENT_DEADBAND &&
          right < again && again - right > PLL_CURRENT_DEADBAND) {
         ++code; confirmed = 0; continue;
      }
      /* 경계에서는 양쪽을 확인할 수 없으므로 이전 저장값을 보존하고 실패한다. */
      if (code == PLL_POT_MIN || code == PLL_POT_MAX) return 0;
      if (left <= center || left - center <= PLL_CURRENT_DEADBAND ||
          left <= again || left - again <= PLL_CURRENT_DEADBAND ||
          right <= center || right - center <= PLL_CURRENT_DEADBAND ||
          right <= again || right - again <= PLL_CURRENT_DEADBAND) {
         confirmed = 0;
         continue; /* 평탄하거나 모호하면 완료로 간주하지 않는다. */
      }
      if (++confirmed < PLL_MINIMUM_CONFIRMATIONS) continue;
      /* A-W 저항은 코드가 커지면 작아진다. 최소점의 +1코드를 운전 기준으로 저장한다. */
      status = measure(code + 1, &final_value, &count);
      if (status == PHASE_MEASURE_FAILED) return 0;
      if (status != PHASE_MEASURE_OK || phase_delta(final_value, right) > PLL_CURRENT_DEADBAND) {
         confirmed = 0;
         continue;
      }
      *result = code + 1;
      *frequency = count;
      return 1;
   }
}

unsigned char phase_track_once(unsigned char start, unsigned char low, unsigned char high,
   signed char direction, unsigned char *result, phase_measure_fn measure)
{
   unsigned short a, b, again, fa, fb, fagain;
   unsigned char candidate;
   *result = start;
   if (low < PLL_POT_MIN || PLL_POT_CODE_ABOVE_MAX(high) || start < low || start > high) return 0;
   if (direction != 1 && direction != -1) return 0;
   if ((direction > 0 && start == high) || (direction < 0 && start == low)) return 1;
   candidate = (unsigned char)(start + direction);
   if (measure(start, &a, &fa) != PHASE_MEASURE_OK ||
       measure(candidate, &b, &fb) != PHASE_MEASURE_OK ||
       measure(start, &again, &fagain) != PHASE_MEASURE_OK) return 0;
   /* 다른 압력 상태끼리 비교하지 않는다. 같은 설정의 전류/주파수부터 검사한다. */
   if (phase_delta(a, again) > PLL_CURRENT_DEADBAND || phase_delta(fa, fagain) > 1) return 0;
   if (b + PLL_CURRENT_DEADBAND < a && b + PLL_CURRENT_DEADBAND < again) *result = candidate;
   return 1;
}
