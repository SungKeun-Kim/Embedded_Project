#include "../firmware_status_protocol.h"
static unsigned char runtime_last_pwm, runtime_last_target, runtime_stable_ticks;

/* main의 100.8ms 틱에서 호출한다. PWM/노브 변경 때 즉시 보정을 보류한다. */
static unsigned char runtime_link_tick(unsigned char running, unsigned char allowed,
   unsigned char pwm, unsigned char target)
{
   if (!running || !allowed || pwm != target || pwm != runtime_last_pwm || target != runtime_last_target)
      runtime_stable_ticks = 0;
   else if (runtime_stable_ticks < 10) ++runtime_stable_ticks;
   runtime_last_pwm = pwm;
   runtime_last_target = target;
   return runtime_stable_ticks >= 10 ? ATMEGA8_RUNTIME_READY : ATMEGA8_RUNTIME_HOLD;
}
