#include <assert.h>
#include <string.h>

#define LCD_WIDTH 16
#define RemoteKeyTest remote_key

unsigned char auto_tun_flag;
unsigned char remote_key;
unsigned char ucPortdFlag;
unsigned char ErrorFlag;
char line2[5][LCD_WIDTH];
static char lcd_line[LCD_WIDTH];
static unsigned char cursor;
static unsigned char sent[16];
static unsigned char sent_count;

void funPutChar(unsigned char data) { sent[sent_count++] = data; }
void lcd_locate(char x, char y) { assert(y == 1); cursor = x; }
void lcd_data_mode(void) {}
void lcd_disp_data(char data) { lcd_line[cursor++] = data; }
void lcd_disp_str(char *text) { while (*text) lcd_disp_data(*text++); }

#include "../Atmega16A_LM565_BD/ready_notice.c"

int main(void)
{
   int i;

   memset(line2[4], 'S', LCD_WIDTH);
   auto_tun_flag = 1;
   remote_key = 0;
   ready_notice_receive(ATMEGA8_BOOT_NOTICE);
   ready_notice_process();
   assert(sent_count == 1 && sent[0] == ATMEGA8_BOOT_ACK);
   assert(!ready_notice_active);

   remote_key = 1;
   ready_notice_process();
   assert(ready_notice_active);
   assert(memcmp(lcd_line, "ATMEGA8 ONLINE  ", LCD_WIDTH) == 0);
   for (i = 0; i < 29; i++) ready_notice_tick();
   ready_notice_process();
   assert(ready_notice_active);
   ready_notice_tick();
   ready_notice_process();
   assert(!ready_notice_active);
   assert(memcmp(lcd_line, line2[4], LCD_WIDTH) == 0);

   peer_seen = 0;
   poll_ticks = 0;
   sent_count = 0;
   for (i = 0; i < 9; i++) ready_notice_poll();
   assert(sent_count == 0);
   ready_notice_poll();
   assert(sent_count == 1 && sent[0] == ATMEGA8_STATUS_REQUEST);
   ready_notice_receive(ATMEGA8_STATUS_REPLY);
   ready_notice_process();
   assert(ready_notice_active);

   auto_tun_flag = 0;
   ready_notice_process();
   assert(!ready_notice_active);
   assert(memcmp(lcd_line, line2[4], LCD_WIDTH) == 0);

   auto_tun_flag = 1;
   ErrorFlag = 1;
   ready_notice_receive(ATMEGA8_BOOT_NOTICE);
   ready_notice_process();
   assert(!ready_notice_active);
   ErrorFlag = 0;
   ready_notice_process();
   assert(ready_notice_active);
   ready_notice_cancel();
   for (i = 0; i < 30; i++) ready_notice_tick();
   ready_notice_process();
   assert(!ready_notice_active);

   sent_count = 0;
   ErrorFlag = 1;
   ready_notice_receive(ATMEGA8_UPLOAD_NOTICE);
   ready_notice_process();
   assert(!ready_notice_active && sent_count == 0);
   ErrorFlag = 0;
   ready_notice_process();
   assert(ready_notice_active);
   assert(memcmp(lcd_line, "UPLOAD COMPLETE ", LCD_WIDTH) == 0);
   assert(sent_count == 1 && sent[0] == ATMEGA8_UPLOAD_ACK);
   for (i = 0; i < 19; i++) ready_notice_tick();
   ready_notice_process();
   assert(ready_notice_active);
   ready_notice_receive(ATMEGA8_UPLOAD_NOTICE);
   ready_notice_process();
   assert(sent_count == 2 && sent[1] == ATMEGA8_UPLOAD_ACK);
   ready_notice_tick();
   ready_notice_process();
   assert(!ready_notice_active);
   assert(memcmp(lcd_line, line2[4], LCD_WIDTH) == 0);
   return 0;
}
