#include "../pll_control_config.h"
static unsigned short tune_started;
static unsigned char tune_expired;

static void tune_clock_start(unsigned short now)
{
   tune_started = now;
   tune_expired = 0;
}

static unsigned char tune_clock_ok(unsigned short now)
{
   /* 16비트 타이머가 한 바퀴 돌아도 경과 시간은 같은 방식으로 비교한다. */
   if ((unsigned short)(now - tune_started) >= PLL_TUNE_LIMIT_TICKS) tune_expired = 1;
   return !tune_expired;
}

static unsigned char tune_clock_can_save(unsigned short now)
{
   if (!tune_clock_ok(now)) return 0;
   if ((unsigned short)(now - tune_started) >= PLL_TUNE_LIMIT_TICKS - PLL_TUNE_SAVE_GUARD_TICKS) {
      /* 저장 완료 여유 부족도 시간 제한에 따른 중단으로 구분한다. */
      tune_expired = 1;
      return 0;
   }
   return 1;
}
