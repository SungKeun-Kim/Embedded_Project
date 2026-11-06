#ifndef ATMEGA16_READY_NOTICE_H
#define ATMEGA16_READY_NOTICE_H

#include "..\firmware_status_protocol.h"

extern volatile unsigned char ready_notice_active;

void ready_notice_tick(void);
void ready_notice_receive(unsigned char data);
void ready_notice_poll(void);
void ready_notice_process(void);
void ready_notice_cancel(void);

#endif
