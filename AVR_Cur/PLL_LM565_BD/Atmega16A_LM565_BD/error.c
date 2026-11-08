#include <string.h>

static volatile unsigned char timeout_reset_allowed;
static unsigned char reset_wait, reset_ticks;

void control_error(unsigned char tuning)
{
   /* 2는 시간 초과, 0/1은 전원 재시작이 필요한 오류다. 심각한 오류가 우선한다. */
   if (ErrorFlag) {
      if (tuning != 2) {
         timeout_reset_allowed = 0;
         reset_wait = reset_ticks = 0;
         if (!tuning) disp_error2();
      }
      return;
   }
   timeout_reset_allowed = tuning == 2;
   reset_wait = reset_ticks = 0;
   tuning_key_block_until_release();
   ErrorFlag = 1;
   auto_tun_flag = 1;
   ready_notice_cancel();
   sonic_off();
   /* 정상 대기 출력과 구별되는 오류 차단. 실제 SD/HO/LO 파형 검증이 필요하다. */
   SONIC_OUTPUT = 0;
   OCR2 = 0;
   ocrdata_cnt = 0;
   PORTA = 0x82;
   beeper_on();
   funPutChar(0x29);
   if (tuning) display(0,1);
   else disp_error2();
}

void error_chk()
{
   if (ErrorFlag && !timeout_reset_allowed) return;
   if (ad_data[2] >= 500) {
      if (++error_count >= 5) control_error(0);
   } else error_count = 0;
}

//=====================================================================
void disp_error()
{
   display(0,6);  //ULTRA SONIC  //   ERROR  !!!    //
}

//=====================================================================
void disp_error2()
{
   display(8,5);  //TRANSDUCER // ERROR  !!! //
}



