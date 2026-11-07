#include "tuning_key.h"
static volatile unsigned char tuning_hold, tuning_latched, tuning_request;

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
