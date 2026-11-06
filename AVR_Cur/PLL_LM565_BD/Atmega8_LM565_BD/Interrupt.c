#include <iom8.h>
#include <stdio.h>
#include <inavr.h>
#include <comp_a90.h>


#include "Atmega8.h"
#include "Uart.h"
#include "TSO8.H"
#include "ready_notice.h"

//#define TEST

#pragma vector=INT1_vect
__interrupt void int1_interrupt(void)
{

    _CLI();
    f_data++;
    _SEI();
    ADC_DATA=0;
    bit_set(PORTD,5);
    DELAY_10US;
    DELAY_1US;
    bit_clear(PORTD,5);
    ADC_DATA=funADC_Read();  //202008
    t0_cnt++;
    if(t0_cnt>150){t0_cnt=0;t2_cnt++;}
}

void funInit_ext1()
{
  GICR |= 0x80;        //int1 enable
  INT1_RISING_EDGE;//202008
}


#pragma vector=TIMER0_OVF_vect                  //Make 100uS Step Interrupt
__interrupt void Timer0_OverFlow(void)
{
    TCNT0 = TIMER_RELOAD;
    ready_notice_tick();
//    _CLI();
    f_cnt++;       //주파수 계산 변수 //test

     if(f_cnt>=50){      // frequency count //
        f_cnt=0;
      //  t0_cnt++;
        __disable_interrupt();
       f_data2 = f_data;
       f_data=0;
     //
        __enable_interrupt();
     }
      
   //  if(t0_cnt>5){t0_cnt=0;t2_cnt=1;}
     // delay_ms() function interface
 //    if (delay_int) {
 //       delay_int--;
 //       if (!delay_int) delay_done = 1;
 //    }
	
//     _SEI();

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
    unsigned char status, data;
    _CLI();

    status = UCSRA;
    data = UDR;
    if (!(status & ((1 << FE) | (1 << DOR) | (1 << PE))) &&
        (data == ATMEGA8_BOOT_ACK || data == ATMEGA8_STATUS_REQUEST ||
         data == ATMEGA8_UPLOAD_ACK)) {
       ready_notice_receive(data);
    } else {
       ucRxDATA = data;
    }

    _SEI();
}

#pragma vector=USART_TXC_vect
__interrupt void USART_TX_Complet(void)
{
	
	_CLI();
	

	
	_SEI();
}
