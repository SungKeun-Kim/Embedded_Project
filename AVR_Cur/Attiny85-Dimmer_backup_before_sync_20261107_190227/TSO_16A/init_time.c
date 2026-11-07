
//=====================================================================
#pragma vector=INT0_vect
__interrupt void int0_interrupt(void)
{

   __disable_interrupt();
   f_data++;         //주파수 저장   //
   __enable_interrupt();
}

void init_ext0()
{
   GICR |= 0x40;        //int0 enable
   MCUCR |= 0x02;       //하강 에지 인터럽트 //
}

//=====================================================================

char tick;
unsigned delay_int;
char delay_done;

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

#define TIMER_RELOAD    173

// note that the interrupt handler is changed
// from TIMER0 COUNTER OVERFLOW HANDLER
// to TIMER0 OUTPUT COMPARE MATCH HANDLER
#pragma vector=TIMER0_COMP_vect  //
__interrupt void TIMER0_COMP_interrupt(void)
{
   t0_cnt++;         //1ms 증가 변수 //

   f_cnt++;       //주파수 계산 변수 //test

   if(f_cnt>=100){      // frequency count //
      tick = 1;
      f_cnt=0;
      __disable_interrupt();
      f_data2 = f_data;
     f_data=0;
      __enable_interrupt();
   }

   // delay_ms() function interface
   if (delay_int) {
      delay_int--;
      if (!delay_int) delay_done = 1;
   }
}

#define OCF0_MASK    0x02

void init_timer0(void)
{
   OCR0 = TIMER_RELOAD; // register data
   TCNT0 = 0;
   TCCR0 = T0_MODE_CTC+T0_PRESCALE_64;
}

// timer2 is used for pwm output of output power adjustment by pot
void init_timer2()
{

   OCR2 = 0;//MIN_PWM;
   TCCR2 = T2_MODE_PWM_PC+T2_COMP_CLEAR+T2_PRESCALE_8;
   ASSR = 0x00;
   TCNT2 = 0x00;
}

// timer1 is used for sweep control
void init_timer1()
{
   set_timer1_mode(T1_MODE_CTC_2);
   select_timer1_clock(T1_PRESCALE_64);
   select_timer1_output(T1_COMPA_TOGGLE);
}

