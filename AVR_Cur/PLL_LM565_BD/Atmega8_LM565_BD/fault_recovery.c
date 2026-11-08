/* 시간 초과 복구 요청은 일반 명령 우편함과 분리하여 STOP에 덮이지 않게 한다. */
static void handle_timeout_reset(void)
{
   unsigned char requested, saved = SREG;
   unsigned short sample, stamp, now;
   _CLI();
   requested = timeout_reset_requested;
   timeout_reset_requested = 0;
   sample = ADC_DATA; stamp = adc_stamp; now = pll_ticks;
   SREG = saved;
   if (!requested || !timeout_reset_capable) return;
   if (control_fault && timeout_fault) {
      /* 차단 상태에서도 센서 갱신과 AD5280 응답을 확인한다. 오류를 억지로 지우지 않는다. */
      if (ucError_flag || sample < 4U || sample > PLL_ADC_TRIP ||
          (unsigned short)(now - stamp) > 20U) {
         timeout_fault = 0;
         return;
      }
      if (!apply_code(tune_restore_code)) return;
      rt_suspend(&runtime);
      runtime.code = runtime.good_code = tune_restore_code;
      ucRUNFlag = pll_active = operation_active = tune_prepared = 0;
      saved = SREG;
      _CLI(); ucRxDATA = 0; runtime_permit_ticks = 0; SREG = saved;
      timeout_fault = control_fault = 0;
      timeout_reset_ack = 1;
   }
   /* ACK가 유실되면 재요청에 재응답한다. RUN/새 튜닝이 시작되면 무효화한다. */
   if (!control_fault && timeout_reset_ack && !ucRUNFlag && !tune_prepared)
      funPutChar(ATMEGA8_TIMEOUT_RESET_ACK);
}
