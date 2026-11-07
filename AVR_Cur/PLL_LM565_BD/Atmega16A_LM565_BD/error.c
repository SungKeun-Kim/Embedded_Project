#include <string.h>

void control_error(unsigned char tuning)
{
   if (ErrorFlag) return;
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
   if (ErrorFlag) return;
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



