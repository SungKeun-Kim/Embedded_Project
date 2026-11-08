#include <iom16.h>
#include "ready_notice.h"
#include "init_disp.c"
#include "init_adc.c"
#include "tuning_key.c"
#include "tuning_output.c"
#include "tuning_watch.c"
#include "runtime_link.c"
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
   timeout_reset_allowed = reset_wait = reset_ticks = 0;
   tuning_key_block_until_release();
   auto_tun_flag=1;
   ucPwmmin=77;
   OCR2 = ucPwmmin;     //pwm off
   ucRunOnes=0;

}

#include "error_recovery.c"

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
    unsigned char tick_now, command, saved, runtime_knob, runtime_target;
    unsigned char runtime_running, runtime_allowed, runtime_pwm;

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
      saved = SREG;
      __disable_interrupt();
      tick_now = tick;
      tick = 0;
      command = ucRxDATA;
      ucRxDATA = 0;
      SREG = saved;
      if (command == 0x12 || command == ATMEGA8_CONTROL_FAULT) control_error(1);
      else if (command == ATMEGA8_TUNE_TIMEOUT) control_error(2);

      if (tick_now) {
         /* 튜닝 중에도 같은 과전류 검사 경로를 실행한다. */
         adc_process(0);
         adc_process(2);
         if (adc_fault) control_error(0);
         error_chk();
         ready_notice_poll();
         funPutChar(ATMEGA16_TIMEOUT_RESET_CAPABLE);
         runtime_knob = ad_data[0] / 10;
         if (runtime_knob > 90) runtime_knob = 90;
         runtime_target = (unsigned short)runtime_knob * 173U / 90U + 77U;
         runtime_running = !RemoteKeyTest;
         if (!ucPortdFlag) runtime_running = 0;
         runtime_allowed = auto_tun_flag;
         if (ErrorFlag) runtime_allowed = 0;
         runtime_pwm = OCR2;
         funPutChar(runtime_link_tick(runtime_running, runtime_allowed, runtime_pwm, runtime_target));
         if (!auto_tun_flag && !tuning_start_sent) tuning_output_tick();
      }
      if (tuning_watch_update(!auto_tun_flag, tuning_start_sent, command, tick_now)) control_error(1);
      if (ErrorFlag) {
         error_recovery_process(command, tick_now);
         ready_notice_process();
         continue;
      }
      if (!auto_tun_flag) {
         if (command == 0x17) display(0,8);
         if (command == 0x19 && tuning_peer_seen) {
            ResetInit();
            display(1,4);
         }
      }
      funKey_scan();
      ready_notice_process();
   }
}
