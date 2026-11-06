#ifndef ATMEGA8_READY_NOTICE_H
#define ATMEGA8_READY_NOTICE_H

#include "..\firmware_status_protocol.h"

void ready_notice_tick(void);
void ready_notice_receive(unsigned char data);
void ready_notice_start(void);
void ready_notice_process(void);
void ready_notice_clear_upload_marker(void);

#endif
