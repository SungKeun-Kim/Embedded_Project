#include <assert.h>
#include <stdio.h>
static unsigned char ErrorFlag,auto_tun_flag,sonic_line,OCR2,ocrdata_cnt,PORTA;
static unsigned char error_count,ucPortdFlag,beeper,notice_cancelled,last_message;
static unsigned short ad_data[3];
static int display_count;
#define SONIC_OUTPUT sonic_line
void ready_notice_cancel(void) { notice_cancelled = 1; }
void sonic_off(void) { ucPortdFlag = 0; }
void beeper_on(void) { beeper = 1; }
void funPutChar(unsigned char v) { last_message = v; }
void display(unsigned char a,unsigned char b) { (void)a; (void)b; ++display_count; }
void disp_error2(void);
void tuning_key_block_until_release(void) {}
#include "../Atmega16A_LM565_BD/error.c"

int main(void)
{
   int i;
   auto_tun_flag = 0;
   sonic_line = 1; OCR2 = ocrdata_cnt = 77; ucPortdFlag = 0x10;
   ad_data[2] = 500;
   for (i = 0; i < 4; ++i) error_chk();
   assert(!ErrorFlag && OCR2 == 77);
   error_chk();
   assert(ErrorFlag && auto_tun_flag && !sonic_line && !OCR2 && !ocrdata_cnt);
   assert(!ucPortdFlag && PORTA == 0x82 && beeper && notice_cancelled && last_message == 0x29);
   assert(display_count == 1);
   ad_data[2] = 0;
   error_chk(); control_error(1);
   assert(ErrorFlag && display_count == 1);
   puts("PASS: overload during tuning, latched output disable, stop command");
   return 0;
}
