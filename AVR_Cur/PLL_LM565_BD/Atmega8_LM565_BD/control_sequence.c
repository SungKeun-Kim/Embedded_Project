/* main.c에 포함한다. 하드웨어 대기와 측정은 measure_code/service_control에 위임한다. */
#include "../firmware_status_protocol.h"
static void tune_abort(void)
{
   /* 완료가 아닌 오류/출력 정지로 통지하고, 튜닝 직전 실제 RDAC로 되돌린다. */
   timeout_fault = tune_expired && !control_fault && !ucError_flag;
   timeout_reset_ack = 0;
   control_fault = 1;
   (void)apply_code(tune_restore_code);
   operation_active = pll_active = 0;
   funPutChar(timeout_fault && timeout_reset_capable ? ATMEGA8_TUNE_TIMEOUT : 0x12);
}

static void manual_tune(unsigned char command)
{
   unsigned char result = ucOrgpt, success = 0;
   unsigned short frequency_count;
   (void)command;
   /* 이전 0x11의 단계 출력 상승도 같은 제한 탐색으로 처리한다. */
   /* 출력 준비 중 발생한 과전류도 보존한다. */
   pll_active = 1;
   operation_active = 1;
   if (!tune_prepared) {
      tune_restore_code = active_code;
      tune_clock_start(now_ticks());
   }
   progress_at = now_ticks();
   funPutChar(0x17);
   if (phase_find_minimum(tune_restore_code, &result, &frequency_count, measure_code) &&
       service_control() && tune_clock_can_save(now_ticks()) && tuning_store_save(result, frequency_count)) {
      ucOrgpt = result;
      ucFreqOld = frequency_count;
      settings_valid = 2;
      success = service_control();
   }
   if (success) {
      /* 실제 RDAC 확인과 EEPROM 검증 뒤에만 완료를 알린다. */
      funPutChar(0x19);
   } else {
      tune_abort();
   }
   operation_active = 0;
   pll_active = success ? ucRUNFlag : 0;
}
