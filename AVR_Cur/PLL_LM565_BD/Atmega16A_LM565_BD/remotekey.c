#include <iom16.h>


void funRemote_off()
{
    if(RemoteOnes){      //초기 상태 표시//..
          if(SELKeyTest){
            ucLin1=1;
         }else{
           ucLin1=3;
         }
         gucLin2=4;
         display(ucLin1, gucLin2);     //시간 설정 화면 //
         RemoteOnesOff;       // 한 번만 표시off 상태 확인 //
//         PORTD &= ~0x40;         //초음파 0ff//  0202.04
    //     SONIC_OUTPUT = 0; //0202.08  스위치  off 시 0.1 초 on 시작.

         PORTD |= 0x02;            //led off
         ucPortdFlag = 0;          // 동작중임을 알리기 위함 변수

        OCR2 = ucPwmmin;       //17.12  runoff시 PWM출력 off
        ocrdata_cnt=77; //0>45;  //17.12  run off 시 증가값 초기화
      }
      pl_disp_te();
      DISP_RUN=0;
}

//==================
void funRemote_on()
{
  unsigned char i;

    if(!RemoteOnes){   //runkey off
         if(SELKeyTest){
           ucLin1=2;
          }else{
           ucLin1=4;
          }
         gucLin2=4;
         RemoteOnesOn;        // 한 번만 표시off 상태 확인 //
         display(ucLin1,gucLin2);
      }
 //     display_active_0();       //202007 okk

      ucPortdFlag = 0x10;        // 동작중임을 알리기 위함 변수// add20201101
      ucFreqcnt++;
      if(ucFreqcnt>150){        //주파수 소수 표시 고정 //
        ucFreqcnt=0;
        frequency();            //주파수 계산 및 표시 //
      }
      //202007 pwm step cont process
      if(SELKeyTest){
          PWM_STEP_CNT++;
          if(PWM_STEP_CNT>=3){
            pl_disp_te();         //2020.08   pulse 20200828
            PWM_STEP_CNT=0;
          }
      }else{
          PWM_STEP_CNT++;
          if(PWM_STEP_CNT>=3){
            pl_disp_te();         //2020.08   cont.
            PWM_STEP_CNT=0;
          }
      }
      //"*" " "  display
      DISP_RUN++;
      if( DISP_RUN>200)DISP_RUN=0;
      if(DISP_RUN>100){
        display_active_0();
      }else{
        display_active();
      }
}



//======================//
void funRemoteKey_prog()
{

   if(!RemoteKeyTest){
     sonic_on();
     funRemote_on();
     if(ucRunOnes==1){
       funPutChar(0x21);
       ucRunOnes=0;
     }
   }else{
     sonic_off();
     funRemote_off();
     if(ucRunOnes==0){
        funPutChar(0x29);
        ucRunOnes=1;
     }

  }
   ucFinish=1;
}













/*
void funRun_Key()
{

   if(SELKeyTest){      //run key off
   //   if(RunOnes){
       //if(RemoteKeyTest && RemkeyOnlg && RunKeyTest){  //run off
       //  RemkeyOnlg=0;
       //  RemkeyOfflg=1;
       //  RemoteSele=1;
         ucLin1=1;
         gucLin2=4;
         display(ucLin1, gucLin2);     //시간 설정 화면 //
 //     }
   }else{
      //run on
       //  RemkeyOnlg=1;
      //   RemkeyOfflg=0;
       //  RemoteSele=0;
         ucLin1=2;
         gucLin2=4;
         display(ucLin1, gucLin2);     //시간 설정 화면 //
   }
    buzzer_off();
//    ErrorFlag=0;
}
   */