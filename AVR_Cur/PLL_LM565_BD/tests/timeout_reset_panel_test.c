#include <assert.h>
#include <stdio.h>
#include "../firmware_status_protocol.h"
#include "../Atmega16A_LM565_BD/tuning_key.c"
static unsigned char SREG,ErrorFlag,auto_tun_flag,sonic_line,OCR2,ocrdata_cnt,PORTA;
static unsigned char error_count,ucPortdFlag,beeper,last_message,remote_key=1,start_key=1;
static unsigned char adc_fault,tuning_start_sent;
static unsigned short ad_data[3];
static unsigned int sends,resets;
#define SONIC_OUTPUT sonic_line
#define RemoteKeyTest remote_key
#define autotunkeyTest start_key
#define __disable_interrupt() ((void)0)
void ready_notice_cancel(void) {}
void sonic_off(void) { ucPortdFlag=0; }
void beeper_on(void) { beeper=1; }
void funPutChar(unsigned char c) { last_message=c; ++sends; }
void display(unsigned char a,unsigned char b) { (void)a;(void)b; }
void disp_error2(void);
/* Test runner converts only encoding, preserving the production source text. */
#include "../.build/tests/error_utf8.c"
static void ResetInit(void) {
   ++resets;ErrorFlag=beeper=error_count=0;
   timeout_reset_allowed=reset_wait=reset_ticks=0;
   auto_tun_flag=1;ucPortdFlag=0;OCR2=77;
   tuning_key_block_until_release();
}
#include "../Atmega16A_LM565_BD/error_recovery.c"
static void step(unsigned char message) {
   tuning_key_tick(!start_key,auto_tun_flag && !ucPortdFlag && remote_key && (!ErrorFlag || timeout_reset_allowed));
   if (ErrorFlag) error_recovery_process(message,1);
}
static void setup(void) {
   ErrorFlag=timeout_reset_allowed=reset_wait=reset_ticks=adc_fault=0;
   error_count=ucPortdFlag=beeper=tuning_start_sent=0;
   remote_key=start_key=1;ad_data[2]=300;auto_tun_flag=1;
   OCR2=ocrdata_cnt=77;sonic_line=1;resets=sends=0;
   tuning_key_tick(0,1);control_error(2);
}
static void request(void) {
   unsigned int i;
   start_key=1;step(0);start_key=0;
   for(i=0;i<31;++i) step(0);
   assert(reset_wait && ErrorFlag && !sonic_line && !OCR2);
   assert(last_message==ATMEGA8_TIMEOUT_RESET);
}
int main(void) {
   unsigned int i,previous_sends;
   setup();assert(ErrorFlag && timeout_reset_allowed && beeper && !sonic_line && !OCR2);
   step(ATMEGA8_TIMEOUT_RESET_ACK);assert(!resets && ErrorFlag); /* unsolicited ACK */
   start_key=0;for(i=0;i<30;++i)step(0);assert(!reset_wait);
   step(0);assert(reset_wait);
   previous_sends=sends;step(0);assert(sends>previous_sends); /* retry */
   step(ATMEGA8_TIMEOUT_RESET_ACK);
   assert(resets==1 && !ErrorFlag && !beeper && sonic_line && OCR2==77 && !ucPortdFlag);
   for(i=0;i<100;++i)step(0);
   assert(!tuning_key_take()); /* holding reset does not trigger tune */
   start_key=1;step(0);start_key=0;
   for(i=0;i<31;++i)step(0);assert(tuning_key_take()); /* fresh tune press works */

   setup();start_key=0; /* must release a key held when fault occurred */
   for(i=0;i<100;++i)step(0);assert(!reset_wait);
   request();for(i=0;i<20;++i)step(0);
   assert(ErrorFlag && !timeout_reset_allowed && !reset_wait && beeper && !sonic_line);
   step(ATMEGA8_TIMEOUT_RESET_ACK);assert(!resets);
   setup();request();control_error(1);step(ATMEGA8_TIMEOUT_RESET_ACK);
   assert(ErrorFlag && !timeout_reset_allowed && !resets);
   control_error(2);assert(!timeout_reset_allowed); /* timeout cannot downgrade hard fault */
   setup();request();ad_data[2]=500;step(ATMEGA8_TIMEOUT_RESET_ACK);
   assert(ErrorFlag && !timeout_reset_allowed && !resets);
   setup();ad_data[2]=500;for(i=0;i<5;++i)error_chk();
   assert(ErrorFlag && !timeout_reset_allowed && !sonic_line);
   setup();request();remote_key=0;step(ATMEGA8_TIMEOUT_RESET_ACK);
   assert(ErrorFlag && !timeout_reset_allowed && !resets);
   setup();start_key=0;for(i=0;i<15;++i)step(0);start_key=1;step(0);
   start_key=0;for(i=0;i<30;++i)step(0);assert(!reset_wait);
   puts("PASS: 3-second release/repress, ACK gating/retry/timeout, hard-fault priority, STOP only");
   return 0;
}
