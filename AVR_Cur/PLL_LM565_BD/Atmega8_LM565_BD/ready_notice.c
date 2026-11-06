#include "ready_notice.h"

static volatile unsigned char boot_acknowledged;
static volatile unsigned char announce_due;
static volatile unsigned char request_pending;
static volatile unsigned char notices_sent;
static unsigned char announce_ticks;
static volatile unsigned char upload_acknowledged;
static volatile unsigned char upload_due;
static volatile unsigned char upload_pending;
static unsigned char upload_ticks;
static const unsigned char upload_marker[] = { ATMEGA8_UPLOAD_MARKER_BYTES };

void ready_notice_tick(void)
{
   if (!boot_acknowledged && notices_sent < 20) {
      announce_ticks++;
      if (announce_ticks >= 100) {
         announce_ticks = 0;
         announce_due = 1;
      }
   }
   if (upload_pending && !upload_acknowledged) {
      upload_ticks++;
      if (upload_ticks >= 100) {
         upload_ticks = 0;
         upload_due = 1;
      }
   }
}

void ready_notice_receive(unsigned char data)
{
   if (data == ATMEGA8_BOOT_ACK) boot_acknowledged = 1;
   else if (data == ATMEGA8_STATUS_REQUEST) request_pending = 1;
   else if (data == ATMEGA8_UPLOAD_ACK && upload_pending) upload_acknowledged = 1;
}

void ready_notice_start(void)
{
   unsigned char i;

   upload_pending = 1;
   for (i = 0; i < sizeof(upload_marker); i++) {
      if (funEEprom_Read(ATMEGA8_UPLOAD_MARKER_ADDRESS + i) != upload_marker[i]) {
         upload_pending = 0;
         break;
      }
   }
   notices_sent = 1;
   funPutChar(ATMEGA8_BOOT_NOTICE);
   if (upload_pending) funPutChar(ATMEGA8_UPLOAD_NOTICE);
}

void ready_notice_process(void)
{
   if (announce_due) {
      announce_due = 0;
      if (!boot_acknowledged && notices_sent < 20) {
         notices_sent++;
         funPutChar(ATMEGA8_BOOT_NOTICE);
      }
   }
   if (request_pending) {
      request_pending = 0;
      funPutChar(ATMEGA8_STATUS_REPLY);
   }
   if (upload_pending && upload_acknowledged) {
      upload_pending = 0;
      upload_due = 0;
      ready_notice_clear_upload_marker();
   } else if (upload_due) {
      upload_due = 0;
      if (upload_pending) funPutChar(ATMEGA8_UPLOAD_NOTICE);
   }
}
