#include <string.h>

void error_chk()
{
   if((OCR2 >= MAX_PWM/2)&&(ucPortdFlag)){                // 50% //
      //if(ad_data[2] <= 0xcc){         // 1V 확인
       if(ad_data[2] <= 0x66){         // .5확인
         disp_error2();            // 0xcc = 1V
         ucPortdFlag=0x0;        //동작 상태 변수 //
         SONIC_OUTPUT = 0;          //초음파 off//
         //PORTD |= 0x02;              //test
         ucFinish = 0;            //종료 확인 변수
         ErrorFlag=1;
         beeper_on();
      }
      else {
         ErrorFlag=0;
         beeper_off();
      }
   }
}

//=====================================================================
void disp_error()
{
   display(0,6);  //초기 화면 //
}

//=====================================================================
void disp_error2()
{
   display(7,5);  //초기 화면 //
}
