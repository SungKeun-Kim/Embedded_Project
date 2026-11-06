#include "ready_notice.h"

volatile unsigned char ready_notice_active;
static volatile unsigned char ready_notice_ticks;
static volatile unsigned char ready_notice_expired;
static volatile unsigned char boot_pending;
static volatile unsigned char reply_pending;
static volatile unsigned char upload_pending;
static unsigned char peer_seen;
static unsigned char message_pending;
static unsigned char poll_ticks;
static unsigned char upload_active;

void ready_notice_tick(void)
{
   if (ready_notice_ticks) {
      ready_notice_ticks--;
      if (!ready_notice_ticks) ready_notice_expired = 1;
   }
}

void ready_notice_receive(unsigned char data)
{
   if (data == ATMEGA8_BOOT_NOTICE) boot_pending = 1;
   else if (data == ATMEGA8_STATUS_REPLY) reply_pending = 1;
   else if (data == ATMEGA8_UPLOAD_NOTICE) upload_pending = 1;
}

void ready_notice_poll(void)
{
   if (!peer_seen && auto_tun_flag && RemoteKeyTest && !ucPortdFlag) {
      poll_ticks++;
      if (poll_ticks >= 10) {
         poll_ticks = 0;
         funPutChar(ATMEGA8_STATUS_REQUEST);
      }
   }
}

void ready_notice_cancel(void)
{
   ready_notice_active = 0;
   ready_notice_ticks = 0;
   ready_notice_expired = 0;
   upload_active = 0;
}

static void ready_notice_restore(void)
{
   unsigned char i;

   ready_notice_cancel();
   lcd_locate(0, 1);
   lcd_data_mode();
   for (i = 0; i < LCD_WIDTH; i++) lcd_disp_data(line2[4][i]);
}

void ready_notice_process(void)
{
   if (boot_pending) {
      boot_pending = 0;
      funPutChar(ATMEGA8_BOOT_ACK);
      peer_seen = 1;
      message_pending = 1;
   }
   if (reply_pending) {
      reply_pending = 0;
      if (!peer_seen) {
         peer_seen = 1;
         message_pending = 1;
      }
   }

   if (upload_pending && ready_notice_active && upload_active) {
      upload_pending = 0;
      funPutChar(ATMEGA8_UPLOAD_ACK);
   }

   if (ready_notice_active &&
       (ready_notice_expired || !auto_tun_flag || !RemoteKeyTest)) {
      ready_notice_restore();
   }

   if (upload_pending && auto_tun_flag && RemoteKeyTest &&
       !ucPortdFlag && !ErrorFlag) {
      upload_pending = 0;
      message_pending = 0;
      ready_notice_active = 1;
      upload_active = 1;
      ready_notice_ticks = 20;
      ready_notice_expired = 0;
      lcd_locate(0, 1);
      lcd_disp_str("UPLOAD COMPLETE ");
      funPutChar(ATMEGA8_UPLOAD_ACK);
   } else if (message_pending && !ready_notice_active && auto_tun_flag && RemoteKeyTest &&
       !ucPortdFlag && !ErrorFlag) {
      message_pending = 0;
      ready_notice_active = 1;
      ready_notice_ticks = 30;
      ready_notice_expired = 0;
      lcd_locate(0, 1);
      lcd_disp_str("ATMEGA8 ONLINE  ");
   }
}
