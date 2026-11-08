#include <iom8.h>
#include <stdio.h>
#include <inavr.h>
#include <comp_a90.h>
#include <string.h>
#include "Atmega8.h"
#include "TSO8.H"
#include "Uart.h"
#include "ready_notice.c"
static unsigned char service_control(void);
#define TUNING_STORE_CAN_COMMIT() service_control()
#include "tuning_store.c"
#include "phase_search.c"
#include "runtime_control.c"
#include "tuning_deadline.c"

volatile unsigned char f_cnt,t0_cnt,t2_cnt;
volatile unsigned short f_data,f_data2,pll_ticks,adc_stamp;
volatile unsigned char runtime_permit_ticks;
volatile unsigned char timeout_reset_requested, timeout_reset_capable;
volatile unsigned short adc_samples,adc_minimum=1023,adc_maximum;
volatile unsigned long adc_sum;
volatile unsigned char pll_active,ucRxDATA,ucError_flag;
unsigned char ucOrgpt,ucTxDATA,ucRUNFlag;
static unsigned char control_fault,operation_active,settings_valid;
static unsigned char timeout_fault, timeout_reset_ack;
static unsigned char active_code, tune_prepared, tune_restore_code;
static runtime_control runtime;
static unsigned short progress_at;

static unsigned short now_ticks(void)
{
   unsigned short value;
   unsigned char saved = SREG;
   _CLI(); value = pll_ticks; SREG = saved;
   return value;
}

static unsigned char take_command(void)
{
   unsigned char value, saved = SREG;
   _CLI(); value = ucRxDATA; ucRxDATA = 0; SREG = saved;
   return value;
}

void ready_notice_clear_upload_marker(void)
{
   unsigned char i;
   for (i = 0; i < ATMEGA8_UPLOAD_MARKER_LENGTH; ++i)
      funEEprom_Write(ATMEGA8_UPLOAD_MARKER_ADDRESS + i, 0xFF);
   (void)funEEprom_Read(ATMEGA8_UPLOAD_MARKER_ADDRESS + ATMEGA8_UPLOAD_MARKER_LENGTH - 1);
}

static unsigned char apply_code(unsigned char code)
{
   if (!TWI_data(code)) { timeout_fault = 0; control_fault = 1; return 0; }
   active_code = code;
   return 1;
}

static unsigned char service_control(void)
{
   ready_notice_process();
   if (ucError_flag) { timeout_fault = 0; control_fault = 1; return 0; }
   if (control_fault || ucRxDATA == 0x29 || ucRxDATA == 0x21) return 0;
   if ((operation_active || tune_prepared) && !tune_clock_ok(now_ticks())) return 0;
   if (operation_active && (unsigned short)(now_ticks() - progress_at) >= PLL_TUNE_PROGRESS_TICKS) {
      progress_at = now_ticks();
      funPutChar(ATMEGA8_TUNE_PROGRESS);
   }
   return 1;
}

static unsigned char wait_ticks(unsigned short ticks)
{
   unsigned short started = now_ticks();
   while ((unsigned short)(now_ticks() - started) < ticks)
      if (!service_control()) return 0;
   return service_control();
}

static unsigned char measure_code(unsigned char code, unsigned short *current, unsigned short *frequency_count)
{
   unsigned char saved;
   unsigned short samples, low, high, count;
   unsigned long sum;
   unsigned short started;
   if (!service_control() || !apply_code(code) || !wait_ticks(PLL_SETTLE_TICKS)) return 0;
   saved = SREG;
   _CLI(); adc_samples = 0; adc_sum = 0; adc_minimum = 1023; adc_maximum = 0; SREG = saved;
   started = now_ticks();
   do {
      if (!service_control()) return 0;
      saved = SREG; _CLI(); samples = adc_samples; SREG = saved;
      if ((unsigned short)(now_ticks() - started) > PLL_SAMPLE_COUNT + 20U) return 0;
   } while (samples < PLL_SAMPLE_COUNT);
   saved = SREG;
   _CLI(); sum = adc_sum; low = adc_minimum; high = adc_maximum; count = f_data2; SREG = saved;
   /* CNT는 발진기 출력이다. 이 검사만으로 실제 브릿지 피드백 유무를 알 수는 없다. */
   if (!count || count == 65535U || low < 4) return PHASE_MEASURE_FAILED;
   if (high - low > PLL_CURRENT_SPREAD_MAX) return PHASE_MEASURE_RETRY;
   if ((PLL_FREQ_COUNT_MIN && count < PLL_FREQ_COUNT_MIN) ||
       (PLL_FREQ_COUNT_MAX && count > PLL_FREQ_COUNT_MAX)) return 0;
   *current = (unsigned short)sum; /* 평균의 소수 정보를 보존: ADC 32개 합계 */
   *frequency_count = count;
   return 1;
}

#include "control_sequence.c"
#include "fault_recovery.c"

/* 측정/통신/보호를 계속 처리하는 비차단 운전 보정 서비스. */
static void runtime_service(void)
{
   static unsigned short last_stamp;
   unsigned short stamp, sample, frequency, elapsed;
   unsigned char saved = SREG, permitted;
   _CLI();
   stamp = adc_stamp; sample = ADC_DATA; frequency = f_data2;
   permitted = runtime_permit_ticks;
   SREG = saved;
   if (stamp == last_stamp) {
      if (ucRUNFlag && (unsigned short)(now_ticks() - stamp) > 20U) control_fault = 1;
      return;
   }
   elapsed = (unsigned short)(stamp - last_stamp);
   last_stamp = stamp;
   if (tune_prepared || operation_active) return;
   if (runtime_sample(&runtime, sample, frequency, elapsed, ucRUNFlag,
       permitted && PLL_RUNTIME_TRACKING_ENABLED)) control_fault = 1;
   if (runtime.code != active_code) (void)apply_code(runtime.code);
}

int main(void)
{
   unsigned char command;
   unsigned short fault_sent = 0;
   funAtmega8_PortInit();
   funAtmega8_ADC_Init();
   funAtmega8_Timer0Init();
   TWI_init_master();
   funInit_ext1();
   funAtmega8_UartInit();
   settings_valid = tuning_store_load(&ucOrgpt, &ucFreqOld);
   _SEI();
   TIMER0_ENABLE;
   (void)apply_code(ucOrgpt);
   runtime_init(&runtime, ucOrgpt);
   ready_notice_start();

   while (1) {
      ready_notice_process();
      if (ucError_flag) { timeout_fault = 0; control_fault = 1; }
      handle_timeout_reset();
      if (control_fault) {
         pll_active = 0;
         ucRUNFlag = 0;
         if ((unsigned short)(now_ticks() - fault_sent) >= 20U) {
            fault_sent = now_ticks();
            funPutChar(timeout_fault && timeout_reset_capable ? ATMEGA8_TUNE_TIMEOUT : ATMEGA8_CONTROL_FAULT);
         }
         continue;
      }
      if (tune_prepared && !tune_clock_ok(now_ticks())) {
         tune_abort();
         tune_prepared = 0;
         continue;
      }
      command = take_command();
      if (command == 0x29) {
         ucRUNFlag = 0;
         pll_active = 0;
         tune_prepared = 0;
         rt_suspend(&runtime);
         (void)apply_code(runtime.code);
      } else if (command == ATMEGA8_TUNE_PREPARE && !ucRUNFlag) {
         timeout_reset_ack = 0;
         pll_active = 1;
         if (!tune_prepared) {
            rt_suspend(&runtime);
            tune_restore_code = runtime.code;
            tune_clock_start(now_ticks());
            tune_prepared = 1;
            (void)apply_code(tune_restore_code);
         }
      } else if (command == 0x21) {
         timeout_reset_ack = 0;
         if (!settings_valid || tune_prepared) { control_fault = 1; continue; }
         ucRUNFlag = 1;
         pll_active = 1;
      } else if ((command == 0x17 || command == 0x11) && !ucRUNFlag) {
         /* 구형 16A의 노브 출력에서 새 50% 튜닝을 실행하지 않는다. */
         if (!tune_prepared) { funPutChar(0x12); control_fault = 1; continue; }
         manual_tune(command);
         tune_prepared = 0;
         if (!control_fault) runtime_init(&runtime, ucOrgpt);
      }
      if (!control_fault) runtime_service();
   }
}

void spi_send_data8(unsigned char data1)
  {
   unsigned char cnt,send_data;

    SPI_CS_OFF;
    SPI_SCK_ON;
    DELAY_10US;

    for(cnt=0;cnt<=7;cnt++)
      {
          send_data=0x00;
              send_data = data1 & 0x80;
              data1 = (data1 << 0x01);
              if(send_data){SPI_DATA_ON; }else{ SPI_DATA_OFF; }
          SPI_SCK_OFF;
          DELAY_10US;
          SPI_SCK_ON;
          DELAY_10US;
      }
    SPI_CS_ON;
    SPI_SCK_ON;
    DELAY_100US;
  }

//===================
void spi_send_data(unsigned char data1,unsigned char data2)
  {
    unsigned char cnt,send_data;

    SPI_CS_OFF;
    SPI_SCK_ON;
    DELAY_10US;

    for(cnt=0;cnt<=15;cnt++)
      {
          send_data=0x00;

          if(cnt <= 0x07){
              send_data = data1 & 0x80;
              if(send_data){ SPI_DATA_ON; }else{ SPI_DATA_OFF; }
              data1 = (data1 << 0x01);
          }else{
              send_data = data2 & 0x80;
              if(send_data){ SPI_DATA_ON; }else{ SPI_DATA_OFF; }
              data2 = (data2 << 0x01);
          }
          SPI_SCK_OFF;
          DELAY_10US;
          SPI_SCK_ON;
          DELAY_10US;
      }

    SPI_CS_ON;
    SPI_SCK_ON;
    DELAY_100US;
  }

//252 - 0.45k
//251 - 0.53k
//250 - 0.61k
//249 - 0.69k
//248 - 0.77k
//247 - 0.85k
//246 - 0.93k
//245 - 1.01k
//244 - 1.09k
//243 - 1.17k
//242 - 1.21k
//241 - 1.29k
//240 - 1.37k
//239 - 1.45k
//238 - 1.53k
