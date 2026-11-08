#include <assert.h>
#include <stdio.h>
#include "../pll_control_config.h"
enum { TWINT=7, TWSTA=5, TWSTO=4, TWEN=2 };
static unsigned char TWCR,TWSR,TWDR;
static int step, fail_step, timeout_step, stop_stuck, stops;
static unsigned char requested, readback;
static void control(unsigned char value)
{
   TWCR = value;
   if (!value) return;
   if (value & (1 << TWSTO)) {
      ++stops;
      if (!stop_stuck) TWCR = 0;
      return;
   }
   ++step;
   switch (step) {
   case 1: assert(value & (1 << TWSTA)); TWSR = 0x08; break;
   case 2: assert(TWDR == 0x58); TWSR = 0x18; break;
   case 3: assert(TWDR == 0); TWSR = 0x28; break;
   case 4: assert(TWDR == requested); TWSR = 0x28; break;
   case 5: assert(value & (1 << TWSTA)); TWSR = 0x10; break;
   case 6: assert(TWDR == 0x59); TWSR = 0x40; break;
   case 7: TWSR = 0x58; TWDR = readback; break;
   default: assert(0);
   }
   if (step == fail_step) TWSR = 0x20;
   if (step == timeout_step) TWCR &= ~(1 << TWINT);
}
#define DIGIPOT_CONTROL(value) control(value)
#include "../Atmega8_LM565_BD/digipot.c"

static void reset(void)
{
   step = fail_step = timeout_step = stop_stuck = stops = 0;
   requested = readback = 233;
}
int main(void)
{
   int i;
   reset();
   assert(TWI_data(233) && step == 7 && stops == 1);
   for (i = 1; i <= 7; ++i) {
      reset(); fail_step = i;
      assert(!TWI_data(233) && step == i && stops == 1);
      reset(); timeout_step = i;
      assert(!TWI_data(233) && step == i && stops == 1);
   }
   reset(); readback = 232;
   assert(!TWI_data(233));
   reset(); stop_stuck = 1;
   assert(!TWI_data(233) && TWCR == 0);
   reset();
   assert(!TWI_data(PLL_POT_MIN-1) && !TWI_data(0) && step == 0);
   for (i = PLL_POT_MIN; i <= PLL_POT_MAX; ++i) {
      reset(); requested = readback = (unsigned char)i;
      assert(TWI_data((unsigned char)i) && step == 7 && stops == 1);
   }
   puts("PASS: AD5280 write/readback, NACK, timeout, stuck STOP, range");
   return 0;
}
