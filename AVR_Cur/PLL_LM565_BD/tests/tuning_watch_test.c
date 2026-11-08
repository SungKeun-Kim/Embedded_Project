#include <assert.h>
#include <stdio.h>
#include "../Atmega16A_LM565_BD/tuning_watch.c"

int main(void)
{
   unsigned long i;
   assert(!tuning_watch_update(0,0,0,1));
   for (i = 0; i < 100; ++i) assert(!tuning_watch_update(1,0,0,1));
   for (i = 0; i < 19; ++i) assert(!tuning_watch_update(1,1,0,1));
   assert(tuning_watch_update(1,1,0,1));
   assert(!tuning_watch_update(0,0,0,0));
   assert(!tuning_watch_update(1,1,0x17,0));
   /* 장시간에도 진행 메시지가 있으면 누적 시간 때문에 중단하지 않는다. */
   for (i = 0; i < 100000UL; ++i)
      assert(!tuning_watch_update(1,1,(i % 5) ? 0 : ATMEGA8_TUNE_PROGRESS,1));
   assert(tuning_peer_seen);
   assert(!tuning_watch_update(1,1,ATMEGA8_TUNE_PROGRESS,0));
   for (i = 0; i < 19; ++i) assert(!tuning_watch_update(1,1,0,1));
   assert(!tuning_watch_update(1,1,0x19,1));
   for (i = 0; i < 20; ++i) (void)tuning_watch_update(1,1,0,1);
   assert(tuning_watch_update(1,1,0,0));
   assert(!tuning_watch_update(0,0,0,0) && !tuning_peer_seen);
   puts("PASS: indefinite convergence time, progress heartbeat, communication timeout");
   return 0;
}
