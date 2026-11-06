#include <assert.h>
#include <string.h>

static unsigned char sent[64];
static unsigned char sent_count;
static unsigned char eeprom[512];
void funPutChar(unsigned char data) { sent[sent_count++] = data; }
unsigned char funEEprom_Read(unsigned short address) { return eeprom[address]; }
void ready_notice_clear_upload_marker(void);

#include "../Atmega8_LM565_BD/ready_notice.c"

void ready_notice_clear_upload_marker(void)
{
   memset(eeprom + ATMEGA8_UPLOAD_MARKER_ADDRESS, 0xff, sizeof(upload_marker));
}

int main(void)
{
   int i;

   memset(eeprom, 0xff, sizeof(eeprom));
   ready_notice_start();
   assert(sent_count == 1 && sent[0] == ATMEGA8_BOOT_NOTICE);
   for (i = 0; i < 99; i++) ready_notice_tick();
   ready_notice_process();
   assert(sent_count == 1);
   ready_notice_tick();
   ready_notice_process();
   assert(sent_count == 2 && sent[1] == ATMEGA8_BOOT_NOTICE);

   ready_notice_receive(ATMEGA8_BOOT_ACK);
   for (i = 0; i < 200; i++) ready_notice_tick();
   ready_notice_process();
   assert(sent_count == 2);

   ready_notice_receive(ATMEGA8_STATUS_REQUEST);
   ready_notice_process();
   assert(sent_count == 3 && sent[2] == ATMEGA8_STATUS_REPLY);
   ready_notice_process();
   assert(sent_count == 3);

   boot_acknowledged = 0;
   announce_due = 0;
   announce_ticks = 0;
   notices_sent = 0;
   sent_count = 0;
   ready_notice_start();
   for (i = 0; i < 2500; i++) {
      ready_notice_tick();
      ready_notice_process();
   }
   assert(sent_count == 20);
   ready_notice_receive(ATMEGA8_STATUS_REQUEST);
   ready_notice_process();
   assert(sent_count == 21 && sent[20] == ATMEGA8_STATUS_REPLY);

   boot_acknowledged = 0;
   announce_due = 0;
   announce_ticks = 0;
   notices_sent = 0;
   upload_acknowledged = 0;
   upload_due = 0;
   upload_ticks = 0;
   sent_count = 0;
   memcpy(eeprom + ATMEGA8_UPLOAD_MARKER_ADDRESS, upload_marker, sizeof(upload_marker));
   ready_notice_start();
   assert(sent_count == 2 && sent[1] == ATMEGA8_UPLOAD_NOTICE);
   ready_notice_receive(ATMEGA8_BOOT_ACK);
   for (i = 0; i < 100; i++) ready_notice_tick();
   ready_notice_process();
   assert(sent_count == 3 && sent[2] == ATMEGA8_UPLOAD_NOTICE);
   assert(memcmp(eeprom + ATMEGA8_UPLOAD_MARKER_ADDRESS, upload_marker, sizeof(upload_marker)) == 0);
   for (i = 0; i < 2500; i++) {
      ready_notice_tick();
      ready_notice_process();
   }
   assert(sent_count == 28 && sent[27] == ATMEGA8_UPLOAD_NOTICE);
   ready_notice_receive(ATMEGA8_UPLOAD_ACK);
   ready_notice_process();
   for (i = 0; i < sizeof(upload_marker); i++)
      assert(eeprom[ATMEGA8_UPLOAD_MARKER_ADDRESS + i] == 0xff);
   for (i = 0; i < 200; i++) ready_notice_tick();
   ready_notice_process();
   assert(sent_count == 28);
   return 0;
}
