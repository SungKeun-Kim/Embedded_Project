#include <iom8.h>
#include <stdio.h>
#include <inavr.h>
#include <comp_a90.h>
#include <string.h>
#include "Atmega8.h"
#include "TSO8.H"
#include "Uart.h"
#include "ready_notice.c"
#include "tuning_store.c"
#include "phase_search.c"

volatile unsigned char f_cnt,t0_cnt,t2_cnt;
volatile unsigned short f_data,f_data2,pll_ticks;
volatile unsigned short adc_samples,adc_minimum=1023,adc_maximum;
volatile unsigned long adc_sum;
volatile unsigned char pll_active,ucRxDATA,ucError_flag;
unsigned char ucOrgpt,ucTxDATA,ucRUNFlag;
static unsigned char control_fault,operation_active;
#if PLL_RUNTIME_TRACKING_ENABLED
static unsigned char active_code;
#endif
static unsigned short operation_start;

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
   if (!TWI_data(code)) { control_fault = 1; return 0; }
#if PLL_RUNTIME_TRACKING_ENABLED
   active_code = code;
#endif
   return 1;
}

static unsigned char service_control(void)
{
   ready_notice_process();
   if (ucError_flag) { control_fault = 1; return 0; }
   if (control_fault || ucRxDATA == 0x29 || ucRxDATA == 0x21) return 0;
   if (operation_active && (unsigned short)(now_ticks() - operation_start) >= PLL_TUNE_TIMEOUT_TICKS) return 0;
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
   if (!count || count == 65535U || low < 4 || high - low > PLL_CURRENT_SPREAD_MAX) return 0;
   if ((PLL_FREQ_COUNT_MIN && count < PLL_FREQ_COUNT_MIN) ||
       (PLL_FREQ_COUNT_MAX && count > PLL_FREQ_COUNT_MAX)) return 0;
   *current = (unsigned short)sum; /* 평균의 소수 정보를 보존: ADC 32개 합계 */
   *frequency_count = count;
   return 1;
}

static void manual_tune(unsigned char command)
{
   unsigned char result = ucOrgpt;
   unsigned short frequency_count;
   (void)command;
   /* 이전 0x11 단계 출력 상승도 같은 제한 탐색으로 처리한다. */
   ucError_flag = 0;
   pll_active = 1;
   operation_active = 1;
   operation_start = now_ticks();
   funPutChar(0x17);
   if (phase_find_minimum(ucOrgpt, &result, &frequency_count, measure_code) &&
       service_control() && tuning_store_save(result, frequency_count) && service_control()) {
      ucOrgpt = result;
      ucFreqOld = frequency_count;
      /* EEPROM 완료 및 읽기 검증 뒤에만 완료를 알린다. */
      funPutChar(0x19);
   } else {
      funPutChar(0x12);
      (void)apply_code(ucOrgpt);
   }
   operation_active = 0;
   pll_active = ucRUNFlag;
}

int main(void)
{
   unsigned char command;
   unsigned short fault_sent = 0;
#if PLL_RUNTIME_TRACKING_ENABLED
   unsigned char next, low, high;
   signed char direction = 1;
   unsigned short track_at = 0;
#endif
   funAtmega8_PortInit();
   funAtmega8_ADC_Init();
   funAtmega8_Timer0Init();
   TWI_init_master();
   funInit_ext1();
   funAtmega8_UartInit();
   (void)tuning_store_load(&ucOrgpt, &ucFreqOld);
   _SEI();
   TIMER0_ENABLE;
   (void)apply_code(ucOrgpt);
   ready_notice_start();

   while (1) {
      ready_notice_process();
      if (ucError_flag) control_fault = 1;
      if (control_fault) {
         pll_active = 0;
         ucRUNFlag = 0;
         if ((unsigned short)(now_ticks() - fault_sent) >= 20U) {
            fault_sent = now_ticks();
            funPutChar(ATMEGA8_CONTROL_FAULT);
         }
         continue;
      }
      command = take_command();
      if (command == 0x29) {
         ucRUNFlag = 0;
         pll_active = 0;
         (void)apply_code(ucOrgpt);
      } else if (command == 0x21) {
         ucRUNFlag = 1;
         pll_active = 1;
#if PLL_RUNTIME_TRACKING_ENABLED
         track_at = now_ticks();
#endif
      } else if ((command == 0x17 || command == 0x11) && !ucRUNFlag) {
         manual_tune(command);
      }
#if PLL_RUNTIME_TRACKING_ENABLED
      if (ucRUNFlag && (unsigned short)(now_ticks() - track_at) >= PLL_RUNTIME_INTERVAL_TICKS) {
         low = ucOrgpt > PLL_POT_MIN + PLL_RUNTIME_RADIUS ? ucOrgpt - PLL_RUNTIME_RADIUS : PLL_POT_MIN;
         high = ucOrgpt < PLL_POT_MAX - PLL_RUNTIME_RADIUS ? ucOrgpt + PLL_RUNTIME_RADIUS : PLL_POT_MAX;
         next = active_code;
         operation_start = now_ticks();
         operation_active = 1;
         if (phase_track_once(active_code, low, high, direction, &next, measure_code) && service_control())
            (void)apply_code(next);
         else if (!control_fault) (void)apply_code(next);
         operation_active = 0;
         direction = -direction;
         track_at = now_ticks();
      }
#endif
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
