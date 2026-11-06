#include <iom8.h>
#include <stdio.h>
#include <inavr.h>
#include <comp_a90.h>


#include "Atmega8.h"
#include "TSO8.H"
#include "Uart.h"


unsigned short ADC_DATA;

void TWI_init_master (void) // 마스터 초기화 기능
{
    TWBR = 0x72; // 비트율
    TWSR = (0 << TWPS1) | (0 << TWPS0); // 프리 스칼라 비트 설정
    // SCL 주파수 = F_CPU / (16 + 2 (TWBR) .4 ^ TWPS)
}

void TWI_data( unsigned char data)
{
  TWCR = (1<<TWINT)|(1<<TWSTA)|(0<<TWSTO)|(1<<TWEN);
  while((TWCR_Bit7==0)||((TWSR & 0xf8) != 0x08));

  TWDR = 0x58;
  TWCR = (1<<TWINT)|(0<<TWSTA)|(0<<TWSTO)|(1<<TWEN);
  while((TWCR_Bit7==0)||((TWSR & 0xf8) != 0x18));

  TWDR = 0x00;
  TWCR = (1<<TWINT)|(0<<TWSTA)|(0<<TWSTO)|(1<<TWEN);
  while((TWCR_Bit7==0)||((TWSR & 0xf8) != 0x28));

  TWDR = data;
  TWCR = (1<<TWINT)|(0<<TWSTA)|(0<<TWSTO)|(1<<TWEN);
  while((TWCR_Bit7==0)||((TWSR & 0xf8) != 0x28));

  TWCR = (1<<TWINT)|(0<<TWSTA)|(1<<TWSTO)|(1<<TWEN);

  DELAY_100MS;
  DELAY_100MS;
}



void funAtmega8_PortInit(void)
{
    SFIOR |= (0 << PUD);        //  Internal Pull-UP Resister '0' : Enable, '1' : Disable
    DDRD  = 0xf1;
    PORTD = 0x00;		
    DDRC  = 0xEE;
    PORTC = 0x00;
    DDRB  = 0xFF;
    PORTB = 0x00;
}

void funAtmega8_UartInit(void)
{
    funUartInit(BAUDRATE_38400);    // Baudrate 38400Bps
}





void funAtmega8_Timer0Init(void)
{
    //Timer0_Value = (unsigned char)Timer0_SetValue(50);
    TCNT0 = TIMER_RELOAD;
 //   TCNT0 = 0;
    TCCR0 = T0_MODE_CTC+T0_PRESCALE_1024;//256->64
}

void funAtmega8_Timer1Init(void)
{
    TCCR1A = 0x00;
    TCCR1B = (0 << ICNC1) | (0 << ICES1) | (0 << 5)|(1 << WGM13) | (1 << WGM12) ;
#if 	(T1_PRESCALER == 1024)
    	TCCR1B |= ((1 << CS10) | (0 << CS11) | (1 << CS12));
#elif	(T1_PRESCALER == 256)
        TCCR1B |= ((0 << CS10) | (0 << CS11) | (1 << CS12));
#elif	(T1_PRESCALER == 64)
        TCCR1B |= ((1 << CS10) | (1 << CS11) | (0 << CS12));
#elif	(T1_PRESCALER == 8)
        TCCR1B |= ((0 << CS10) | (1 << CS11) | (0 << CS12));
#elif	(T1_PRESCALER == 1)
        TCCR1B |= ((1 << CS10) | (0 << CS11) | (0 << CS12));
#endif

    //TIMER1_DISABLE;
}

/////////////// EEPROM READ & WRITE Function///////////////////////

void funEEprom_Write(unsigned short adr, unsigned char eeData)
{
    if (adr > 0x1fff)_NOP();
    else
    {
        while(EECR & (1 << EEWE))__watchdog_reset();
        EEAR = adr;
        EEDR = eeData;
        EECR = (1 << EEMWE);
        EECR = (1 << EEWE);
        _NOP();
    }
}

unsigned char funEEprom_Read(unsigned short adr)
{
    while(EECR & (1 << EEWE));
    EEAR = adr;
    EECR = (1 << EERE);
    return EEDR;
}


//////////////////////////////////////////////////////////////////////
void funAtmega8_ADC_Init(void)
{
    ADC_REFERNCE_AVCC;
    ADCSR |= ((1 << ADEN) | (1 << ADFR) | (0 << ADIE));
    ADC_PRESCALER(128);
    ADC = 0;

}

void eep_write_word(unsigned offset, unsigned dat)
{
   funEEprom_Write(offset, dat/256);
   funEEprom_Write(offset+1, dat%256);
}


//////////////////////////////////////////////////////////////////////
unsigned short funADC_Read(void)
{
    unsigned short usAdcData;
    ADC = 0;
    ADC_CH(0);  //ADC Channel 1
	ADC_START;

    while(!(ADCSR & 0x10));
	usAdcData = ADCL;
    usAdcData += (ADCH << 8);
    if(usAdcData>450) ucError_flag=1;
	return (usAdcData);

}



void funUPscan()
{


}

void funDNscan()
{



}

void funScan()
{


}





void funWatchDog_Init(void)
{
#if 	(WATCHDOG_PRESCALE == 2048)
    	WDTCR |= ((1 << WDP0) | (1 << WDP1) | (1 << WDP2));
#elif	(WATCHDOG_PRESCALE == 1024)
		WDTCR |= ((0 << WDP0) | (1 << WDP1) | (1 << WDP2));
#elif	(WATCHDOG_PRESCALE == 512)
		WDTCR |= ((1 << WDP0) | (0 << WDP1) | (1 << WDP2));		
#elif	(WATCHDOG_PRESCALE == 256)
		WDTCR |= ((0 << WDP0) | (0 << WDP1) | (1 << WDP2));
#elif	(WATCHDOG_PRESCALE == 128)
		WDTCR |= ((1 << WDP0) | (1 << WDP1) | (0 << WDP2));		
#elif	(WATCHDOG_PRESCALE == 64)
		WDTCR |= ((0 << WDP0) | (1 << WDP0) | (0 << WDP0));
#elif	(WATCHDOG_PRESCALE == 32)
		WDTCR |= ((1 << WDP0) | (0 << WDP1) | (0 << WDP2));		
#elif	(WATCHDOG_PRESCALE == 16)
		WDTCR |= ((0 << WDP0) | (0 << WDP1) | (0 << WDP2));		
#endif
	
}

/*
void funAtmega8_Timer2Init(void)
{

    TCNT2 = Timer2_Value;
#if 	(T2_PRESCALER == 1024)
    	TCCR2 |= ((1 << CS20) | (1 << CS21) | (1 << CS22));
#elif	(T2_PRESCALER == 256)
        TCCR2 |= ((0 << CS20) | (1 << CS21) | (1 << CS22));
#elif	(T2_PRESCALER == 128)
        TCCR2 |= ((1 << CS20) | (0 << CS21) | (1 << CS22));
#elif	(T2_PRESCALER == 64)
        TCCR2 |= ((0 << CS20) | (0 << CS21) | (1 << CS22));
#elif	(T2_PRESCALER == 32)
        TCCR2 |= ((1 << CS20) | (1 << CS21) | (0 << CS22));
#elif	(T1_PRESCALER == 8)
        TCCR2 |= ((0 << CS20) | (1 << CS21) | (0 << CS22));
#elif	(T2_PRESCALER == 1)
        TCCR2 |= ((1 << CS20) | (0 << CS21) | (0 << CS22));
#endif
}

*/