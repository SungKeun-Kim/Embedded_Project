

volatile unsigned char tick;
volatile unsigned delay_int;
volatile char delay_done;
unsigned char error_count,ucFreqcnt;
volatile unsigned char ucRxDATA;


//=====================================================================
#pragma vector=INT0_vect
__interrupt void int0_interrupt(void)
{

   f_data++;         //주파수 저장   //
}
//====================================
void init_ext0()
{
   GICR |= 0x40;        //int0 enable
   MCUCR |= 0x02;       //하강 에지 인터럽트 //
}

//=====================================================================



void delay_ms(unsigned interval)
{
   if (!interval) return;
   delay_done = 0;
   interval++;
   __disable_interrupt();
   delay_int = interval;
   __enable_interrupt();
   while (!delay_done);
} // delay_ms()

#define TIMER_RELOAD   62  // 172    //62->11.0592Mhz

// note that the interrupt handler is changed
// from TIMER0 COUNTER OVERFLOW HANDLER
// to TIMER0 OUTPUT COMPARE MATCH HANDLER
#pragma vector=TIMER0_COMP_vect  //
__interrupt void TIMER0_COMP_interrupt(void)
{
            //1ms 증가 변수 //

   f_cnt++;       //주파수 계산 변수 //test

   if(f_cnt>=100){      // frequency count //
      tick = 1;
      ready_notice_tick();
      tuning_key_tick(!autotunkeyTest, auto_tun_flag && !ucPortdFlag && RemoteKeyTest && !ErrorFlag);
      f_cnt=0;
      t0_cnt++;
      f_data2 = f_data;
     f_data=0;
   }

   // delay_ms() function interface
   if (delay_int) {
      delay_int--;
      if (!delay_int) delay_done = 1;
   }
}

#define OCF0_MASK    0x02
//====================================
void init_timer0(void)
{
   OCR0 = TIMER_RELOAD; // register data
   TCNT0 = 0;
   TCCR0 = T0_MODE_CTC+T0_PRESCALE_256;   //64->256//256->11.0592Mhz
}
//====================================
// timer2 is used for pwm output of output power adjustment by pot
void init_timer2()
{

   OCR2 = ucPwmmin;//MAIN_PWM;
   TCCR2 = T2_MODE_PWM_PC+T2_COMP_CLEAR+T2_PRESCALE_8;
   ASSR = 0x00;
   TCNT2 = 0x00;
}
//====================================
// timer1 is used for sweep control
void init_timer1()
{
   set_timer1_mode(T1_MODE_CTC_2);
   select_timer1_clock(T1_PRESCALE_64);
   select_timer1_output(T1_COMPA_TOGGLE);
}


//======================================
void funUartInit(unsigned short usBaud)
{
    //UCSRB = 0x9B;               // disable while setting baud rate
    UCSRA ^= 0x40;
    UCSRC = 0x86;
    UCSRB = (1 << RXCIE)|(0 << TXCIE)|(0 << UDRIE)|(1 << RXEN)|(1 << TXEN)|(0 << UCSZ2)|(1 << RXB8)|(1 << TXB8);
    UBRRH = (unsigned char)(usBaud >> 8);// Set baud rate
    UBRRL = (unsigned char) usBaud;

}
//====================================
void funPutChar(unsigned char cb)
{
    while(!(UCSRA & (1 << UDRE)));
    UDR = cb;

}
//====================================
void funPutString(unsigned char *st)
{
    unsigned char _st;
    while(*st)
    {
        _st = *st;
        funPutChar(_st);
        st++;
    }
}
//====================================
#pragma vector=USART_RXC_vect
__interrupt void USART_RX_Complet(void)
{
   unsigned char status = UCSRA;
   unsigned char data = UDR;
   if (status & ((1 << FE) | (1 << DOR) | (1 << PE))) return;
   if (data == ATMEGA8_BOOT_NOTICE || data == ATMEGA8_STATUS_REPLY || data == ATMEGA8_UPLOAD_NOTICE)
      ready_notice_receive(data);
   else if (data == 0x12 || data == ATMEGA8_CONTROL_FAULT ||
            (ucRxDATA != 0x12 && ucRxDATA != ATMEGA8_CONTROL_FAULT)) ucRxDATA = data;
}
