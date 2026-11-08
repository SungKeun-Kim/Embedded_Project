#include <assert.h>
#include <stdio.h>
#include "../firmware_status_protocol.h"
#include "../Atmega8_LM565_BD/runtime_control.c"
static unsigned char SREG,timeout_reset_requested,timeout_reset_capable;
static unsigned char control_fault,timeout_fault,timeout_reset_ack,ucError_flag;
static unsigned char ucRUNFlag,pll_active,operation_active,tune_prepared,ucRxDATA,runtime_permit_ticks;
static unsigned char tune_restore_code,active_code,apply_ok,last_message;
static unsigned short ADC_DATA,adc_stamp,pll_ticks;
static unsigned int sends,applies;
static runtime_control runtime;
#define _CLI() ((void)0)
static unsigned char apply_code(unsigned char c) {
   ++applies;
   if (!apply_ok) { control_fault=1; timeout_fault=0; return 0; }
   active_code=c; return 1;
}
static void funPutChar(unsigned char c) { ++sends;last_message=c; }
#include "../Atmega8_LM565_BD/fault_recovery.c"
static void setup(void) {
   SREG=128; timeout_reset_requested=timeout_reset_capable=1;
   control_fault=timeout_fault=apply_ok=1;
   timeout_reset_ack=ucError_flag=ucRUNFlag=pll_active=operation_active=tune_prepared=0;
   ucRxDATA=0x21;runtime_permit_ticks=100;
   tune_restore_code=active_code=231;
   ADC_DATA=300;adc_stamp=pll_ticks=1000;
   sends=applies=0;last_message=0;
   runtime_init(&runtime,233);
   runtime.code=runtime.good_code=231;
}
int main(void) {
   setup(); handle_timeout_reset();
   assert(!control_fault && !timeout_fault && timeout_reset_ack);
   assert(active_code==231 && runtime.code==231 && runtime.good_code==231 && runtime.origin==233);
   assert(!ucRUNFlag && !pll_active && !ucRxDATA && !runtime_permit_ticks);
   assert(sends==1 && last_message==ATMEGA8_TIMEOUT_RESET_ACK && SREG==128);
   timeout_reset_requested=1;handle_timeout_reset(); /* lost ACK */
   assert(sends==2 && applies==1);
   handle_timeout_reset(); assert(sends==2); /* no unsolicited ACK */
   setup();timeout_fault=0;handle_timeout_reset();
   assert(control_fault && !sends && !applies); /* hard fault cannot clear */
   setup();ucError_flag=1;handle_timeout_reset();
   assert(control_fault && !timeout_fault && !sends && !applies);
   setup();ADC_DATA=451;handle_timeout_reset();
   assert(control_fault && !timeout_fault && !sends);
   ADC_DATA=300;timeout_reset_requested=1;handle_timeout_reset();
   assert(control_fault && !sends); /* once hard, remains hard */
   setup();ADC_DATA=0;handle_timeout_reset();assert(control_fault && !timeout_fault && !sends);
   setup();adc_stamp=970;handle_timeout_reset();assert(control_fault && !timeout_fault && !sends);
   setup();apply_ok=0;handle_timeout_reset();assert(control_fault && !timeout_fault && !sends);
   setup();timeout_reset_capable=0;handle_timeout_reset();assert(control_fault && !sends && !applies);
   setup();control_fault=timeout_fault=0;handle_timeout_reset();assert(!sends && !applies);
   setup();handle_timeout_reset();ucRUNFlag=1;timeout_reset_requested=1;handle_timeout_reset();
   assert(sends==1);
   puts("PASS: timeout-only recovery, ADC/I2C refusal, ACK retry, no RUN, prior RDAC retained");
   return 0;
}
