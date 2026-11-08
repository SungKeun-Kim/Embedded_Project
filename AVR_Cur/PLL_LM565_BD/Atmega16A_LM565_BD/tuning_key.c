#include "tuning_key.h"
static volatile unsigned char tuning_hold, tuning_latched, tuning_request;

/* 오류 진입/복구 때 누르고 있던 키를 새 3초 입력으로 해석하지 않는다. */
void tuning_key_block_until_release(void)
{
   tuning_hold = tuning_request = 0;
   tuning_latched = 1;
}

void tuning_key_tick(unsigned char pressed, unsigned char allowed)
{
   if (!pressed) {
      tuning_hold = tuning_latched = tuning_request = 0;
   } else if (!allowed) {
      tuning_hold = 0;
      tuning_request = 0;
   } else if (!tuning_latched) {
      if (++tuning_hold >= TUNING_HOLD_TICKS) {
         tuning_request = 1;
         tuning_latched = 1;
      }
   }
}

/* AVR에서는 호출자가 인터럽트를 잠깐 막고 요청을 가져온다. */
unsigned char tuning_key_take(void)
{
   unsigned char result = tuning_request;
   tuning_request = 0;
   return result;
}
