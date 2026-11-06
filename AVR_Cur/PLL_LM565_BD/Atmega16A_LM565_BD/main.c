#include <iom16.h>
#include "ready_notice.h"
#include "init_disp.c"
#include "init_adc.c"
#include "key.c"
#include "error.c"
#include "init_time.c"
#include "remotekey.c"
#include "ready_notice.c"

//#include "timeprog.c"

// 12.21 입력 부분 중 run off시
// 12.27 주파수 부분 수정
// 01.6  remote 키 조작 방법 및 동작 상태 변경 (run off 때만 동작)
// 01.6  run 키 동작 방법 변경 (remote off 시에만 동작 )
// 01.6  버그 수정 run on/start 에서 바로 run off 하면 초음파 중지.
// 01.8  run off remote key 동작이 있을때만 출력 하고 run on 시에는 토글로 동작 ..
// 5.12  error 출력때 1초 출력 값 확인후 error를 판단 한는 부분 추가 함.
// 08,06,11 remote 출력시 출력시간  미적용 으로..
// 0806,14 buzzer
// 2020.04 sonic on 유지 하는 부분 수정. 수정 화일 참조
// 202007  CONT .PULSE .AUTO . MANU 기능 추가 TIMER 기능 삭제 리모트 기능 만 사용



//==============================================================
//RUN , REMOTE key 처리 하는 부분


unsigned char ucPwmmin,ucRunOnes,uctuning_cut;




#pragma inline
static void ResetInit()
{
   RemkeyOnlg=0;
   RemkeyOfflg=1;
   RemoteSele=1;
   Oldlin2 = gucLin2;
   RunOnesOff;          // 한 번만 표시off 상태 확인 //
   sonic_off();
   ucPortdFlag=0;             //sonic on/off value 0:off, 10:on
   ucFinish=1;                // timeover value
   PWM_STEP_CNT=2;  //2020207
   ErrorFlag=0;beeper_off();error_count=0;
   auto_tun_flag=1;
   ucPwmmin=77;
   OCR2 = ucPwmmin;     //pwm off
   ucRunOnes=0;

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
    DDRA = 0x82;  //P0. input(7=error,2=AD2,1=led,0=AD0)
    PORTA= 0x02;
    DDRB = 0x00;  //P1. input
    DDRC = 0xff;  //P2. output
    DDRD = 0xf2;  //P3. output(1111 0010)//0xfb2020208
    PORTD= 0x40;  // sonic on 202008
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
    unsigned char ucImoc2;

   init_port();
   init_timer0();

   enable_timer_intr(OCIE0_MASK);

   init_adc();
   init_timer1();
   init_timer2();
   init_lcd();
   init_ext0();
   funUartInit(BAUDRATE_38400);

   __enable_interrupt();

   load_settings();
   ResetInit();
   buzzer_off();
   PWM_STEP=1;   //PWM 증가 속도 값(1~10)2020.04
   SONIC_OUTPUT = 1;  //0202.08 //on
   PORTA=0x00;    //error relay =off, led=on
   led_on();        //LED OFF TEST
   delay_ms(100); delay_ms(100);delay_ms(100);delay_ms(100);delay_ms(100);
   delay_ms(100);delay_ms(100);delay_ms(100);delay_ms(100);delay_ms(100);
   disp_start();
   auto_tun_flag=1; //1: opertion mode  0:tuning mode
   delay_ms(100); delay_ms(100);delay_ms(100);delay_ms(100);delay_ms(100);
   delay_ms(100);delay_ms(100);delay_ms(100);delay_ms(100);delay_ms(100);


   while(1)
   {
     if(auto_tun_flag){
       if(tick == 1){//17.12
         funKey_scan();
         ready_notice_poll();
         tick = 0;
         adc_process(0);      //chann0번을 ad해서 ad_data[0]에 저장 //
  //     adc_process(1);      //          "        //
         adc_process(2);      //       "        // ad_data[2]
         error_chk();
       }
     }

     if(ErrorFlag)while(1);
     funKey_scan();       //run key scan //
     ready_notice_process();

     if(ucRxDATA==0x12){
         ucRxDATA=0x19;
         display(0,1);
         delay_ms(100);      //error out
         delay_ms(100);
         delay_ms(100);
         delay_ms(100);
         delay_ms(100);
     }

     if(ucRxDATA==0x13){
         display(0,2);
         OCR2=0x8c; //55%  //55=0x8c, 65=0xa6,  75=0xc0, 100=f9 //PWM 60%=0x9a, 70%=0xb3,  80%=0xcc, 90%=0xe6  ,100%=0xfd
         ucRxDATA=0;
     }
     if(ucRxDATA==0x14){
         display(0,3);
         OCR2=0xa6; //65%
         ucRxDATA=0;
     }
     if(ucRxDATA==0x15){
         display(0,6);
         OCR2=0xc0; //75%
         ucRxDATA=0;
     }
     if(ucRxDATA==0x16){
         display(0,7);
         OCR2=0xfd; //100%
         ucRxDATA=0;
     }

     if(ucRxDATA==0x17){
         display(0,8);
         ucRxDATA=0;
     }

     if(ucRxDATA==0x19){

        ResetInit();      //end
        auto_tun_flag=1;
        disp_start();
        ucRxDATA=0;
        OCR2=77;    //30% pwm
     }
   }
}
