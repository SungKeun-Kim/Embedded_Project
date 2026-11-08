#include "../firmware_status_protocol.h"

static unsigned char tuning_peer_seen, tuning_silent_ticks;

/* 튜닝이 오래 걸리는 것은 오류가 아니다. 진행 통지 중단만 감시한다. */
static unsigned char tuning_watch_update(unsigned char active, unsigned char requested,
   unsigned char message, unsigned char tick_now)
{
   if (!active || !requested) {
      tuning_peer_seen = tuning_silent_ticks = 0;
      return 0;
   }
   if (message == 0x17 || message == ATMEGA8_TUNE_PROGRESS) {
      tuning_peer_seen = 1;
      tuning_silent_ticks = 0;
      return 0;
   }
   if (message == 0x19 && tuning_peer_seen) {
      tuning_silent_ticks = 0;
      return 0;
   }
   if (tick_now && tuning_silent_ticks < 20) ++tuning_silent_ticks;
   return tuning_silent_ticks >= 20;
}
