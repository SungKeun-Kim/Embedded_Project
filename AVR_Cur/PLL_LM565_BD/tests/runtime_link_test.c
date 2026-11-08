#include <assert.h>
#include <stdio.h>
#include "../Atmega16A_LM565_BD/runtime_link.c"
int main(void)
{
   unsigned char i;
   for(i=0;i<30;++i) assert(runtime_link_tick(0,1,77,128)==ATMEGA8_RUNTIME_HOLD);
   assert(runtime_link_tick(1,1,128,128)==ATMEGA8_RUNTIME_HOLD);
   for(i=0;i<9;++i) assert(runtime_link_tick(1,1,128,128)==ATMEGA8_RUNTIME_HOLD);
   assert(runtime_link_tick(1,1,128,128)==ATMEGA8_RUNTIME_READY);
   assert(runtime_link_tick(1,1,128,129)==ATMEGA8_RUNTIME_HOLD);
   for(i=0;i<15;++i) (void)runtime_link_tick(1,1,129,129);
   assert(runtime_link_tick(1,1,129,129)==ATMEGA8_RUNTIME_READY);
   assert(runtime_link_tick(1,0,129,129)==ATMEGA8_RUNTIME_HOLD);
   assert(runtime_link_tick(0,1,129,129)==ATMEGA8_RUNTIME_HOLD);
   puts("PASS: output-settled permit, knob/ramp/stop/error inhibition");
   return 0;
}
