#include <string.h>

void error_chk()
{

       if(ad_data[2] >= 500){         // 3.2V확인--> 3.4변경 202009
           error_count++;
           if(error_count>4){
              disp_error2();            // 0xcc = 1V
              ucPortdFlag=0x0;        //동작 상태 변수 //
              ErrorFlag=1;
              beeper_on();
              PORTA=0x82;     //error relay on, led off
              led_on();        //LED OFF TEST
//              relay_on();
              sonic_off();
              OCR2 = 0;     //pwm off
           }
       }else{
           error_count=0;
       }
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



