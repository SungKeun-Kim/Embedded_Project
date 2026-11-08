#include <iom8.h>
#include <stdio.h>
#include <inavr.h>
#include <comp_a90.h>


#include "Atmega8.h"
#include "Uart.h"
#include "TSO8.H"
#include "ready_notice.h"
#include "runtime_config.h"
extern volatile unsigned char timeout_reset_requested, timeout_reset_capable;

//#define TEST

#pragma vector=INT1_vect
__interrupt void int1_interrupt(void)
{
   /* 에지 ISR에서는 계수만 한다. ADC 대기와 중첩 인터럽트를 제거했다. */
   if (f_data != 65535U) ++f_data;
}

void funInit_ext1(void)
{
   INT1_RISING_EDGE;
   GICR |= 0x80;
}

#pragma vector=TIMER0_OVF_vect
__interrupt void Timer0_OverFlow(void)
{
   unsigned short value;
   TCNT0 = TIMER_RELOAD;
   ++pll_ticks;
   if (runtime_permit_ticks) --runtime_permit_ticks;
   ready_notice_tick();
   if (++f_cnt >= 50) {
      f_cnt = 0;
      f_data2 = f_data;
      f_data = 0;
   }
   if (ADCSR & (1 << ADIF)) {
      value = ADC;
      ADCSR |= (1 << ADIF);
      ADC_DATA = value;
      adc_stamp = pll_ticks;
      if (pll_active && value > PLL_ADC_TRIP) ucError_flag = 1;
      if (adc_samples < PLL_SAMPLE_COUNT) {
         adc_sum += value;
         if (value < adc_minimum) adc_minimum = value;
         if (value > adc_maximum) adc_maximum = value;
         ++adc_samples;
      }
   }
}

/*

#pragma vector=TIMER1_OVF_vect                  //Make 60uS Step Interrupt
__interrupt void Timer1_OverFlow(void)
{
    _CLI();
	//static unsigned short ussBzCnt = 0;
	
	TCNT1 = Timer1_Value;
	if (ungActiveFlags.Buzzer){
		//if (((ussBzCnt >> 2)&0x01)==0){
		//if (((ussBzCnt) & gucBzDevValue) == 0){ BUZZER_OFF; }
		if (PORTB & (1 << PORTB5)) BUZZER_OFF;
		else BUZZER_ON;
		
		//if (ussBzCnt >= gusBzTime){
		//	ungActiveFlags.Buzzer = CLEAR;
		//	ussBzCnt = 0;
		//}
		//else ussBzCnt++;
		
	}
//	else __delay_cycles(25);
////////////////////////////////////////////////////
    _SEI();
}
*/
#pragma vector = TIMER1_COMPA_vect
__interrupt void TIMER1_COMPA_vect_interrupt(void)
{

	_CLI();
//	if (ungActiveFlags.Buzzer){
//		if (BUZZER_IN) BUZZER_OFF;
//		else BUZZER_ON;
//	}
	_SEI();
}

/*
#pragma vector=TIMER2_OVF_vect                  //Make 60uS Step Interrupt
__interrupt void Timer2_OverFlow(void)
{
    _CLI();
    TCNT2 = Timer2_Value;
    ungIntrruptFlags.Timer2Flag = 1;
    gusTick50ms++;

    _SEI();
}
*/

#pragma vector=INT0_vect                  // Falling Edge interrupt
__interrupt void funAcSyncInput(void)
{
    _CLI();


    _SEI();
}


/*
#pragma vector=ADC_vect
__interrupt void ADC_Complet(void)
{
    _CLI();
    ADC_DATA = ADC;

    _SEI();
}
*/

#pragma vector=USART_RXC_vect
__interrupt void USART_RX_Complet(void)
{
   unsigned char status = UCSRA;
   unsigned char data = UDR;
   if (status & ((1 << FE) | (1 << DOR) | (1 << PE))) return;
   if (data == ATMEGA8_BOOT_ACK || data == ATMEGA8_STATUS_REQUEST || data == ATMEGA8_UPLOAD_ACK)
      ready_notice_receive(data);
   else if (data == ATMEGA8_TIMEOUT_RESET) timeout_reset_requested = 1;
   else if (data == ATMEGA16_TIMEOUT_RESET_CAPABLE) timeout_reset_capable = 1;
   else if (data == ATMEGA8_RUNTIME_READY) runtime_permit_ticks = RT_PERMIT_TICKS;
   else if (data == ATMEGA8_RUNTIME_HOLD) runtime_permit_ticks = 0;
   else if (data == 0x29 || ucRxDATA != 0x29) {
      if (data == 0x29) runtime_permit_ticks = 0;
      ucRxDATA = data;
   }
}

#pragma vector=USART_TXC_vect
__interrupt void USART_TX_Complet(void)
{
	
	_CLI();
	

	
	_SEI();
}
