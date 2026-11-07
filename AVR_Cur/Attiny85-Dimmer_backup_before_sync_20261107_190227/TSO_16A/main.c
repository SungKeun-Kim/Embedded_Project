#include "init_disp.c"
#include "init_adc.c"
#include "key.c"
#include "error.c"
#include "init_time.c"


// 12.21 입력 부분 중 run off시
// 12.27 주파수 부분 수정
// 01.6  remote 키 조작 방법 및 동작 상태 변경 (run off 때만 동작)
// 01.6  run 키 동작 방법 변경 (remote off 시에만 동작 )
// 01.6  버그 수정 run on/start 에서 바로 run off 하면 초음파 중지.
// 01.8  run off remote key 동작이 있을때만 출력 하고 run on 시에는 토글로 동작 ..
// 5.12  error 출력때 1초 출력 값 확인후 error를 판단 한는 부분 추가 함.
// 08,06,11 remote 출력시 출력시간  미적용 으로..
// 0806,14 buzzer

//==============================================================
//RUN , REMOTE key 처리 하는 부분
void funKey_scan()      //run key //
{

   if(RunKeyTest){      //run key off
      if(RemoteKeyTest && RemkeyOnlg && RunKeyTest){  //run off
         RemkeyOnlg=0;
         RemkeyOfflg=1;
         RemoteSele=1;
  //     BuzzerOff;  //08,06,12

      }else if((!RemoteKeyTest) && RemkeyOfflg){   //run on
         RemkeyOnlg=1;
         RemkeyOfflg=0;
         RemoteSele=0;
      }

      buzzer_off();
      ErrorFlag=0;
   }

//-------------------------------------------------------------------
   if(RemoteSele){      //remote on,off  처리부분 //
      //remote off
      if(RemoteOnes){      //초기 상태 표시//..
         ucLin1=1;
         gucLin2=0;
         display(ucLin1, gucLin2);     //시간 설정 화면 //
         RemoteOnesOff;       // 한 번만 표시off 상태 확인 //
         PORTD &= ~0x40;         //초음파 0ff//
         PORTD |= 0x02;          //led off
         ucPortdFlag = 0;          // 동작중임을 알리기 위함 변수
         
        OCR2 = 0;       //17.12  runoff시 PWM출력 off
        ocrdata_cnt=0;  //17.12  run off 시 증가값 초기화 

      }
   }else{
      //remote on
      if(RunKeyTest && !RemoteOnes){   //runkey off
         PORTD |=0x40;        //초음파 ON//
         ucLin1=6;               //출력할 내용 변수값 //run off
         gucLin2=4;              //출력할 내용 변수값 //remote on
         RemoteOnesOn;        // 한 번만 표시off 상태 확인 //
         display(ucLin1,gucLin2);
      }

      ucTimeData1 = OrgTimeData1;       // setting 값 재 설정
      ucTimeData0 = OrgTimeData0;   // setting 값 재설정

      ucPortdFlag = 0x10;        // 동작중임을 알리기 위함 변수//
      frequency();            //주파수 계산 및 표시 //
    //  pl_disp_te();         //17.12
   }

//---------------------------------------------------------------
   if(RemoteSele){            //run on
      if(RunKeyTest){         //run key off
         //RUNKEY off
         if(RunOnes){      //초기 상태 표시//..
            ucLin1=1;
            gucLin2=0;
            display(ucLin1,gucLin2);      //시간 설정 화면 //
            RunOnesOff;          // 한 번만 표시off 상태 확인 //
            sonic_off();
         }
         KeyTime_pross();           //키를 이용한 시간 셋 과정 //
         ucPortdFlag=0;             //timer count init
         ucFinish=1;                // timeover value

         ucTimeData1 = OrgTimeData1;         // setting 값 재 설정
         ucTimeData0 = OrgTimeData0;         // setting 값 재설정
      }else{               //run key on
         //run on and remote off and st/sp off
         //if runkey is on ..
         if(!RunOnes){        //key가 off 상태에 있던 것을 확인함//
            ucLin1=3;         //출력할 내용 변수값 //run on...
            gucLin2=4;        //출력할 내용 변수값 //
            display(ucLin1,gucLin2);   //LCD 출력 함수 호출 //
            RunOnesOn;        //특정비트를 1로 함..//
         }
         if(!ErrorFlag){
            if(!bit_test(PINA,3)){     //SWEEP DISPLAY 기능 처리//
               dispsweep();

            }else{            //RUN ON으로 출력 처리 //
               disprunon();
            }
         }
         //=================     run 동작의 중요 부분..
         if(ucFinish){           //설정 시간이 남아 있을때 처리하는부분 //
            TimeSetDis();        //설정한 시간을 표시하는 부분 //sec,minute,full
            funStSpKey_chk();    //stop,start key 확인 하고  초음파 신호 줌.
            frequency();         //주파수 계산 및 표시 //
            //pl_display();        // PL 표시 //17.12
            if(ucgErrorDelay>=20)error_chk();         // 에러 확인 //
            ucgErrorDelay++;
            if(ucgErrorDelay >=20)ucgErrorDelay=20;
         }else{
            funStSpKey_chk();
            ucTimeData1 = OrgTimeData1;       // setting 값 재 설정
            ucTimeData0 = OrgTimeData0;   // setting 값 재설정
            ucgErrorDelay=0;
         }
      }

   }
}



//====================================================
//일정 시간 마다데이터를 확인하고 처리 하기위한 함수 //
void finish_sonic(void)
{
   ucTimeData0=AsciZero;
   ucPortdFlag=0x0;           //동작 상태 변수 //
   SONIC_OUTPUT = 0;          //초음파 off//
   led_off();                 //LED test
   ucLin1=5;                  //출력할 내용 변수값 TIMEOVER//
   gucLin2=4;                 //출력할 내용 변수값 //
   display(ucLin1,gucLin2);      //LCD 출력 함수 호출 //
   ucFinish=0;
   ucgErrorDelay=0;
   buzzer_on();
}

void timer_process()
{
   if(t0_cnt>=1000) {                        //sec count
      t0_cnt=0;   // sec clear..
      ucT0_min++; // minute count

      if(Oldlin2==3 || Oldlin2==2) ucT0_min=0;  //평상시에는 t0...변수 값을 초기화하려고 //

      //======================================================================
      if(Oldlin2==2 && ucPortdFlag==0x10){      //"S Select"

         if(ucTimeData1==AsciZero && ucTimeData0==AsciZero){//시간이 완료 되면 이부분 실행//
            finish_sonic();
         }else{
            ucTimeData0--;    //설정 시간 감소 //
         }

         if(ucTimeData0==0x2f){
            if(ucTimeData1>=0x31)
            {
               ucTimeData1--;
               ucTimeData0=0x39;
            }

         }
      }

      if(ucPortdFlag==0x10)         //초음파 동작 중인 표시 "10동작" "0중지"
      {
         if(bit_test(PIND,1)){

         }else{
            PORTD |= 0x02;    //led off
         }
      }

      //===============================================================
      if(ucT0_min>=59){                      //1 MINUTE
         ucT0_min=0;
         if(Oldlin2==1 && ucPortdFlag==0x10){      //"M Select"
            if(ucTimeData1==AsciZero && ucTimeData0==AsciZero){      //00이 되면 처리부분//
               finish_sonic();
            }else{
               ucTimeData0--;
            }
            if(ucTimeData0==0x2f){           //분 단위 계산 //
               if(ucTimeData1>=Ascione)      //십분 단위 값 보고 하위분 값 설정//
               {
                  ucTimeData1--;
                  ucTimeData0=AsciNine;
               }
            }
         }
      }

   }
}

//==============
//

#pragma inline
static void ResetInit()
{
   RemkeyOnlg=0;
   RemkeyOfflg=1;
   RemoteSele=1;
   Oldlin2 = gucLin2;
   RunOnesOff;          // 한 번만 표시off 상태 확인 //
   sonic_off();
   ucPortdFlag=0;             //timer count init
   ucFinish=1;                // timeover value
   ucTimeData1 = OrgTimeData1;   // setting 값 재 설정
   ucTimeData0 = OrgTimeData0;   // setting 값 재설정
}

//=============================================================

#pragma inline
static void format_settings(void)
{
   eep_write_word(ADDR_EEP_ID, EEP_ID);
   eep_write_byte(ADDR_OrgTimeData0, OrgTimeData0);
   eep_write_byte(ADDR_OrgTimeData1, OrgTimeData1);
   eep_write_byte(ADDR_gucLin2, gucLin2);
   eep_write_byte(ADDR_ucOrgPL_H, ucOrgPL_H);
   eep_write_byte(ADDR_ucOrgPL_L, ucOrgPL_L);
}

#pragma inline
static void load_settings(void)
{
   unsigned eep_id;
   char i;
   for (i = 0; i < 20; i++) {
      delay_ms(100);
      eep_id = eep_read_word(ADDR_EEP_ID);
      if (eep_id == EEP_ID) break;
   }
   if (eep_id != EEP_ID) format_settings();

   OrgTimeData0 = eep_read_byte(ADDR_OrgTimeData0);
   OrgTimeData1 = eep_read_byte(ADDR_OrgTimeData1);
   gucLin2 = eep_read_byte(ADDR_gucLin2);
   ucOrgPL_H = eep_read_byte(ADDR_ucOrgPL_H);
   ucOrgPL_L = eep_read_byte(ADDR_ucOrgPL_L);

   ucPL_H = ucOrgPL_H;  //time copy
   ucPL_L = ucOrgPL_L;
   Oldlin2 = gucLin2;
}

#pragma inline
static void init_port()
{
    DDRA = 0x00;  //P0. input
    DDRB = 0x00;  //P1. input
    DDRC = 0xff;  //P2. output
    DDRD = 0xfb;  //P3. output(1111 1011)
}

void process_buzzer(void)
{
   if (buzz_counter) {
      buzz_counter--;
      if (buzz_counter == 0) buzzer_off();
   }
}

void main()
{

   init_port();

   //while (1) BUZZER = ~BUZZER;
   init_timer0();
   enable_timer_intr(OCIE0_MASK);

   init_adc();
   init_timer1();
   init_timer2();
   init_ext0();

   __enable_interrupt();

   init_lcd();

   led_off();        //LED OFF TEST

   load_settings();

   ResetInit();
   disp_start();
   buzzer_off();
   OCR2 = 0;
   PWM_STEP=1;   //PWM 속도 값(5~25)  17.12
   
   while(1)
   {
     if(tick == 1){//17.12
      tick = 0;
      process_buzzer();
      adc_process(0);      //chann0번을 ad해서 ad_data[0]에 저장 //
      adc_process(1);      //          "        //
      adc_process(2);      //       "        //

      funKey_scan();       //run key scan //
      timer_process();  //timer 가동 여부 판단 및 시간 확인 //
      sweep_process();        //sweep key 에 의한 주파수 출력 부분 //   
     }
     if(!RemoteSele && RunKeyTest){
        pl_disp_te();
     }else{
        pl_display();
     }
   }
}