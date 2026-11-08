/* main.c에 포함한다. 노브와 무관하게 약 50%까지 부드럽게 접근한다. */
#include "../pll_control_config.h"
static unsigned char tuning_start_sent, tuning_target_pwm, tuning_settle;

static void tuning_output_start(void)
{
   tuning_target_pwm = PLL_TUNING_PWM;
   tuning_start_sent = 0;
   tuning_settle = 0;
   funPutChar(ATMEGA8_TUNE_PREPARE);
}

static void tuning_output_tick(void)
{
   unsigned char value = OCR2;
   if (tuning_start_sent) return;
   if (value < tuning_target_pwm) {
      value += tuning_target_pwm - value > 3 ? 3 : tuning_target_pwm - value;
      tuning_settle = 0;
   } else if (value > tuning_target_pwm) {
      value -= value - tuning_target_pwm > 3 ? 3 : value - tuning_target_pwm;
      tuning_settle = 0;
   } else if (++tuning_settle >= 2) {
      /* 출력이 고정된 뒤 ATmega8에 측정을 요청한다. */
      tuning_start_sent = 1;
      funPutChar(0x17);
   }
   OCR2 = value;
   ocrdata_cnt = value;
}
