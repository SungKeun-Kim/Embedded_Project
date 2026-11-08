#include <assert.h>
#include <stdio.h>
#include "../Atmega16A_LM565_BD/tuning_key.c"

int main(void)
{
   int i;
   tuning_key_tick(0,1);
   for (i = 0; i < TUNING_HOLD_TICKS-1; ++i) {
      tuning_key_tick(1,1);
      assert(!tuning_key_take());
   }
   tuning_key_tick(1,1);
   assert(tuning_key_take());
   for (i = 0; i < 1000; ++i) {
      tuning_key_tick(1,1);
      assert(!tuning_key_take());
   }
   tuning_key_tick(0,1);
   for (i = 0; i < 20; ++i) tuning_key_tick(1,1);
   tuning_key_tick(0,1);
   for (i = 0; i < 20; ++i) tuning_key_tick(1,1);
   assert(!tuning_key_take());
   tuning_key_tick(1,0);
   for (i = 0; i < 30; ++i) tuning_key_tick(1,1);
   assert(!tuning_key_take());
   tuning_key_tick(1,1);
   assert(tuning_key_take());
   tuning_key_tick(1,0);
   for (i = 0; i < 100; ++i) tuning_key_tick(1,1);
   assert(!tuning_key_take());
   puts("PASS: 3-second hold, release reset, one-shot, run inhibition");
   return 0;
}
