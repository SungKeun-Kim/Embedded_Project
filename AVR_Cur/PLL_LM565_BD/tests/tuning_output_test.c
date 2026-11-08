#include <assert.h>
#include <stdio.h>
#include "../firmware_status_protocol.h"
static unsigned short ad_data[3];
static unsigned char OCR2,ocrdata_cnt,sent[8],sent_count;
void funPutChar(unsigned char value) { assert(sent_count < 8); sent[sent_count++] = value; }
#include "../Atmega16A_LM565_BD/tuning_output.c"

int main(void)
{
   int i, previous;
   ad_data[0] = 1023; OCR2 = 77;
   tuning_output_start();
   assert(sent_count == 1 && sent[0] == ATMEGA8_TUNE_PREPARE);
   ad_data[0] = 0; /* 시작 이후 노브 변화는 현재 튜닝 목표에 반영하지 않는다. */
   for (i = 0; i < 100 && !tuning_start_sent; ++i) {
      previous = OCR2;
      tuning_output_tick();
      assert(OCR2 >= previous && OCR2 - previous <= 3 && OCR2 <= PLL_TUNING_PWM);
      if (OCR2 < PLL_TUNING_PWM) assert(sent_count == 1);
   }
   assert(tuning_start_sent && OCR2 == PLL_TUNING_PWM && ocrdata_cnt == PLL_TUNING_PWM);
   assert(sent_count == 2 && sent[1] == 0x17);
   for (i = 0; i < 100; ++i) tuning_output_tick();
   assert(sent_count == 2 && OCR2 == PLL_TUNING_PWM);
   OCR2 = 250;
   tuning_output_start();
   for (i = 0; i < 100 && !tuning_start_sent; ++i) {
      previous = OCR2; tuning_output_tick();
      assert(OCR2 <= previous && previous - OCR2 <= 3 && OCR2 >= PLL_TUNING_PWM);
   }
   assert(tuning_start_sent && OCR2 == PLL_TUNING_PWM);
   assert(PLL_TUNING_PWM == 128);
   puts("PASS: fixed 50-percent PWM, knob ignored, ramp and settling");
   return 0;
}
