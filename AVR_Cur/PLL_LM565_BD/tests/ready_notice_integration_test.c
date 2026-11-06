#include <assert.h>
#include <string.h>

#define LCD_WIDTH 16
#define RemoteKeyTest remote_key

unsigned char auto_tun_flag = 1;
unsigned char remote_key = 1;
unsigned char ucPortdFlag;
unsigned char ErrorFlag;
char line2[5][LCD_WIDTH];
static char lcd_line[LCD_WIDTH];
static unsigned char cursor;
static int drop_boot;
static int peer_online = 1;
static unsigned char eeprom[512];

void lcd_locate(char x, char y) { assert(y == 1); cursor = x; }
void lcd_data_mode(void) {}
void lcd_disp_data(char data) { lcd_line[cursor++] = data; }
void lcd_disp_str(char *text) { while (*text) lcd_disp_data(*text++); }
void m16_send(unsigned char data);
void m8_send(unsigned char data);
unsigned char funEEprom_Read(unsigned short address) { return eeprom[address]; }
void ready_notice_clear_upload_marker(void);

#define funPutChar m16_send
#define ready_notice_tick m16_tick
#define ready_notice_receive m16_receive
#define ready_notice_poll m16_poll
#define ready_notice_process m16_process
#define ready_notice_cancel m16_cancel
#define ready_notice_active m16_active
#define upload_pending m16_upload_pending
#include "../Atmega16A_LM565_BD/ready_notice.c"
#undef funPutChar
#undef ready_notice_tick
#undef ready_notice_receive
#undef ready_notice_poll
#undef ready_notice_process
#undef ready_notice_cancel
#undef ready_notice_active
#undef upload_pending

#define funPutChar m8_send
#define ready_notice_tick m8_tick
#define ready_notice_receive m8_receive
#define ready_notice_start m8_start
#define ready_notice_process m8_process
#include "../Atmega8_LM565_BD/ready_notice.c"

void ready_notice_clear_upload_marker(void)
{
   memset(eeprom + ATMEGA8_UPLOAD_MARKER_ADDRESS, 0xff, sizeof(upload_marker));
}

void m16_send(unsigned char data)
{
   if (peer_online) m8_receive(data);
}

void m8_send(unsigned char data)
{
   if (data == ATMEGA8_BOOT_NOTICE && drop_boot) {
      drop_boot = 0;
   } else {
      m16_receive(data);
   }
}

int main(void)
{
   int i;

   memset(line2[4], 'S', LCD_WIDTH);
   memset(eeprom, 0xff, sizeof(eeprom));
   drop_boot = 1;
   m8_start();
   m16_process();
   assert(!m16_active);
   for (i = 0; i < 100; i++) m8_tick();
   m8_process();
   m16_process();
   assert(boot_acknowledged);
   assert(m16_active && memcmp(lcd_line, "ATMEGA8 ONLINE  ", LCD_WIDTH) == 0);
   for (i = 0; i < 30; i++) m16_tick();
   m16_process();
   assert(!m16_active);
   assert(memcmp(lcd_line, line2[4], LCD_WIDTH) == 0);

   peer_seen = 0;
   poll_ticks = 0;
   peer_online = 0;
   for (i = 0; i < 10; i++) m16_poll();
   m8_process();
   m16_process();
   assert(!m16_active);
   peer_online = 1;
   for (i = 0; i < 10; i++) m16_poll();
   m8_process();
   m16_process();
   assert(m16_active);
   m16_cancel();

   boot_acknowledged = 0;
   remote_key = 0;
   m8_start();
   m16_process();
   assert(boot_acknowledged && !m16_active);
   remote_key = 1;
   m16_process();
   assert(m16_active);
   m16_cancel();

   boot_acknowledged = 0;
   upload_acknowledged = 0;
   upload_ticks = 0;
   upload_due = 0;
   notices_sent = 0;
   remote_key = 0;
   memcpy(eeprom + ATMEGA8_UPLOAD_MARKER_ADDRESS, upload_marker, sizeof(upload_marker));
   m8_start();
   m16_process();
   m8_process();
   assert(!m16_active);
   assert(memcmp(eeprom + ATMEGA8_UPLOAD_MARKER_ADDRESS, upload_marker, sizeof(upload_marker)) == 0);
   remote_key = 1;
   m16_process();
   assert(m16_active && memcmp(lcd_line, "UPLOAD COMPLETE ", LCD_WIDTH) == 0);
   m8_process();
   for (i = 0; i < sizeof(upload_marker); i++)
      assert(eeprom[ATMEGA8_UPLOAD_MARKER_ADDRESS + i] == 0xff);
   for (i = 0; i < 20; i++) m16_tick();
   m16_process();
   assert(!m16_active);
   return 0;
}
