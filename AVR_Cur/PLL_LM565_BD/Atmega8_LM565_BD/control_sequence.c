/* main.c에 포함한다. 하드웨어 대기와 측정은 measure_code/service_control에 위임한다. */
static void manual_tune(unsigned char command)
{
   unsigned char result = ucOrgpt, success = 0;
   unsigned short frequency_count;
   (void)command;
   /* 이전 0x11의 단계 출력 상승도 같은 제한 탐색으로 처리한다. */
   ucError_flag = 0;
   pll_active = 1;
   operation_active = 1;
   operation_start = now_ticks();
   funPutChar(0x17);
   if (phase_find_minimum(ucOrgpt, &result, &frequency_count, measure_code) &&
       service_control() && tuning_store_save(result, frequency_count)) {
      ucOrgpt = result;
      ucFreqOld = frequency_count;
      success = service_control();
   }
   if (success) {
      /* 실제 RDAC 확인과 EEPROM 검증 뒤에만 완료를 알린다. */
      funPutChar(0x19);
   } else {
      funPutChar(0x12);
      control_fault = 1;
      (void)apply_code(ucOrgpt);
   }
   operation_active = 0;
   pll_active = success ? ucRUNFlag : 0;
}
