/* main.c의 ResetInit 뒤에 포함한다. 오류 상태에서만 호출한다. */
static void error_recovery_process(unsigned char command, unsigned char tick_now)
{
   unsigned char requested, saved = SREG;
   __disable_interrupt();
   requested = tuning_key_take();
   SREG = saved;
   if (!timeout_reset_allowed) return;
   if (reset_wait && (!RemoteKeyTest || ucPortdFlag)) {
      control_error(1);
      return;
   }
   if (requested && RemoteKeyTest && !autotunkeyTest && !ucPortdFlag && !reset_wait) {
      if (adc_fault || ad_data[2] >= 500) { control_error(0); return; }
      reset_wait = 1;
      reset_ticks = 0;
      funPutChar(ATMEGA8_TIMEOUT_RESET);
   }
   if (!reset_wait) return;
   if (command == ATMEGA8_TIMEOUT_RESET_ACK) {
      if (adc_fault || ad_data[2] >= 500) { control_error(0); return; }
      /* 확인 응답 전에는 출력 차단을 유지한다. 성공해도 STOP 미세 출력만 복원한다. */
      ResetInit();
      tuning_start_sent = 0;
      ocrdata_cnt = 77;
      PORTA = 0;
      SONIC_OUTPUT = 1;
      display(1,4);
      return;
   }
   if (tick_now) {
      if (++reset_ticks >= 20) { control_error(1); return; }
      funPutChar(ATMEGA8_TIMEOUT_RESET);
   }
}
