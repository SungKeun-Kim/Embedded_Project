//#include "Slave.h"
#include <iom8.h>

#define CLK_8MHz        8000000
#define CLK_16MHz       16000000

#ifndef SysClock
#define SysClock	CLK_16MHz
#endif

// Ext. Interrupt

#define INT0_FALLING_EDGE	(MCUCR = (MCUCR & 0xFC) | 0x02)
#define INT0_RISING_EDGE	(MCUCR = (MCUCR & 0xFC) | 0x03)
#define INT0_LOGIC_CHANGE	(MCUCR = (MCUCR & 0xFC) | 0x01)
#define INT0_LOW_LEVEL		(MCUCR = (MCUCR & 0xFC))
#define INT0_ENABLE			(GICR |= (1<<INT0))
#define INT0_DISABLE		(GICR &= ~(1<<INT0))
#define INT0_FLAG_CLEAR		(GIFR &= ~(1<< INTF0))
#define INT0_FLAG_SET		(GIFR |= (1<< INTF0))


#define INT1_FALLING_EDGE	(MCUCR = (MCUCR & 0xF3) | 0x08)
#define INT1_RISING_EDGE	(MCUCR = (MCUCR & 0xF3) | 0x0C)
#define INT1_LOW_LEVEL		(MCUCR = (MCUCR & 0xF3))
#define INT1_ENABLE		(GICR |= (1<<INT1))
#define INT1_DISABLE		(GICR &= ~(1<<INT1))


//bit
#define bit_set(ADDRESS,BIT)    (ADDRESS|=(1<<BIT))
#define bit_clear(ADDRESS,BIT)  (ADDRESS&=~(1<<BIT))
#define bit_test(ADDRESS,BIT)   (ADDRESS&(1<<BIT))


// Timer
#ifndef T0_PRESCALER
#define T0_PRESCALER	8	
#endif

#define TIMER0_ENABLE	(TIMSK |= (1 << TOIE0))
#define TIMER0_DISABLE	(TIMSK &= ~(1 << TOIE0))

#define TO_STOP		(TCCR0 &= 0xF8)

#ifndef T1_PRESCALER
#define T1_PRESCALER	8		
#endif

#define TIMER1_ENABLE	(TIMSK |= (1 << TOIE1))
#define TIMER1_DISABLE	(TIMSK &= ~(1 << TOIE1))

#define TIMER1_COMPA_ENABLE		(TIMSK |= (1 << OCIE1A))
#define TIMER1_COMPA_DISABLE	(TIMSK &= ~(1 << OCIE1A))

#define T1_STOP		(TCCR1B &= 0xF8)

#ifndef T2_PRESCALER
#define T2_PRESCALER	1024		
#endif

#define TIMER2_ENABLE	(TIMSK |= (1 << TOIE2))
#define TIMER2_DISABLE	(TIMSK &= ~(1 << TOIE2))

#define T2_STOP		(TCCR2 &= 0xF8)

#if (SysClock == CLK_16MHz)
#define Timer0_SetValue(US) (0xFF - ((SysClock / 1000000 ) / T0_PRESCALER)*US +4)
#else
#define Timer0_SetValue(US) (0xFF - ((SysClock / 1000000 * (unsigned char)(US)) / T0_PRESCALER - 4 ))
#endif

#define Timer1_SetValue(US) (0xFFFF - ((SysClock / 1000000 * (unsigned short)(US)) / T1_PRESCALER -40 ))
#define Timer2_SetValue(MS) (0xFF - (unsigned char)((SysClock * (unsigned long)(MS)) / T2_PRESCALER / 1000 -1))

// WatchDog

#define WATCHDOG_ENABLE		WDTCR |= (1 << WDE)
#ifndef WATCHDOG_PRESCALE	
#define WATCHDOG_PRESCALE	512
#endif






// ADC_REFERENCE

#define ADC_REFERNCE_AREF	(ADMUX = ((ADMUX & 0x3F) | ((0 << REFS1) | (0 << REFS0))))
#define ADC_REFERNCE_AVCC	(ADMUX = ((ADMUX & 0x3F) | ((0 << REFS1) | (1 << REFS0))))
#define ADC_REFERNCE_RESV	(ADMUX = ((ADMUX & 0x3F) | ((1 << REFS1) | (0 << REFS0))))
#define ADC_REFERNCE_INTN	(ADMUX = ((ADMUX & 0x3F) | ((1 << REFS1) | (1 << REFS0))))

// ADC Channel Sellect
#define ADC_CH(ch)	( ADMUX = (ADMUX & 0xF0) | ch)

// ADC Prescaler
#define ADC_PRESCALER(ps)	\
{\
    unsigned char i;\
    for (i = 0; i < 8;i++)\
    {\
    	if ((ps >> i) & 0x01) break;\
	}\
    ADCSR = ((ADCSR & 0xF8) | i);\
}   //ADCSRA->ADCSR 202008

// ADC Conversion Start
#define ADC_START	(ADCSR |= (1 << ADSC))



//Delay

#define DELAY_1US	__delay_cycles(SysClock/1000000)
#define DELAY_5US	__delay_cycles(SysClock/200000)
#define DELAY_10US	__delay_cycles(SysClock/100000)
#define DELAY_50US	__delay_cycles(SysClock/20000)
#define DELAY_100US	__delay_cycles(SysClock/10000)
#define DELAY_500US	__delay_cycles(SysClock/2000)
#define DELAY_1MS	__delay_cycles(SysClock/1000)
#define DELAY_5MS	__delay_cycles(SysClock/200)
#define DELAY_10MS	__delay_cycles(SysClock/100)
#define DELAY_50MS	__delay_cycles(SysClock/20)
#define DELAY_100MS	__delay_cycles(SysClock/10)
#define DELAY_1S	__delay_cycles(SysClock/1)

// Timer/Counter0 Clock source equates

#define T0_NOCLOCK         0x00
#define T0_PRESCALE_1      0x01
#define T0_PRESCALE_8      0x02
#define T0_PRESCALE_64     0x03
#define T0_PRESCALE_256    0x04
#define T0_PRESCALE_1024   0x05
#define T0_EXT_FALLING     0x06     // external clock from T0 pin
#define T0_EXT_RISING      0x07

#define T0_CLOCK_MASK      0x07

// Timer/Counter0 operating mode equates

#define T0_MODE_NORMAL     0x00  // Mode 0: Normal counter
#define T0_MODE_PWM_PC     0x40  // Mode 1: Phase-Centered PWM
#define T0_MODE_CTC        0x08  // Mode 2: Clear timer on Compare Match
#define T0_MODE_FPWM       0x48  // Mode 3: Fast PWM

#define T0_WGM_MASK        0x48

// Timer/Counter0 compare output mode equates

#define T0_COMP_NO_OUT     0x00
#define T0_COMP_TOGGLE     0x10
#define T0_COMP_CLEAR      0x20
#define T0_COMP_SET        0x30

#define T0_COMP_MASK       0x30
#define TIMER_RELOAD       0xb0//62->fe


extern unsigned short ADC_DATA;
extern unsigned char f_cnt,f_data,f_data2,t0_cnt,t2_cnt;
extern unsigned char ucOrgpt,ucRxDATA,ucTxDATA,ucError_flag;

extern void funAtmega8_PortInit(void);
extern void funAtmega8_UartInit(void);
extern void funWatchDog_Init(void);
extern void funAtmega8_ADC_Init(void);
extern void funAtmega8_SysClockInit(void);
//extern void funAtmega8_EEPromInit(void);
//extern void funAtmega8_TimerInit(void);
extern void funAtmega8_Timer0Init(void);
extern void funAtmega8_Timer1Init(void);
extern void funAtmega8_Timer2Init(void);
extern void funAtmega8_Init(void);

extern void TWI_data( unsigned char data);
extern void TWI_init_master (void);
extern void funAtmega8_Timer0Init(void);

extern unsigned char funEEprom_Read(unsigned short adr);
extern void funEEprom_Write(unsigned short adr, unsigned char eeData);
extern void eep_write_word(unsigned offset, unsigned dat);
extern unsigned short funADC_Read(void);



