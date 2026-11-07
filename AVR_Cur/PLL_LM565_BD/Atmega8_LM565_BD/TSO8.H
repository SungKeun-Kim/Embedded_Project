/*
#ifndef SysClock
#define SysClock	CLK_16MHz
#endif
*/
//#define TEST
//#define _SAIDUN_

#include <iom8.h>
#include <inavr.h>
#include <stdio.h>
#include <string.h>



/////////// Version ////////////////
#define VER_H_VALUE		0x08
#define VER_L_VALUE		0x41	//MONTH:VER


#define ID_ALL      0xFF
#define INPUTPORT   0
#define OUTPUTPORT  1
#define TRUE        1
#define FALSE       0
#define SUCCESS     0
#define FAIL        1
#define ENABLE      1
#define DISABLE     0
#define ON          1
#define OFF         0
#define YES         1
#define NO          0
#define ACK         1
#define NAK         0
#define HIGH        1
#define LOW         0
#define SET         1
#define CLEAR       0
#define FALLING     1
#define RISING      0

////////////////////////////////////////////////

// Macro
#define AUTOTUN_ON		(PORTD |= (1 << PORTD0))
#define AUTOTUN_OFF		(PORTD &= ~(1 << PORTD0))

#define LAMP2_OFF		(PORTD |= (1 << PORTD6))
#define LAMP2_ON		(PORTD &= ~(1 << PORTD6))
#define LAMP3_OFF		(PORTD |= (1 << PORTD7))
#define LAMP3_ON		(PORTD &= ~(1 << PORTD7))

#define BUZZER_ON		(PORTC |= (1 << PORTC4))
#define BUZZER_OFF		(PORTC &= ~(1 << PORTC4))
#define BUZZER_IN 		(PINC & (1 << PINC4))

#define SPI_CS_ON		(PORTB |= (1 << PORTB2))
#define SPI_CS_OFF		(PORTB &= ~(1 << PORTB2))
#define SPI_SCK_ON		(PORTB |= (1 << PORTB5))
#define SPI_SCK_OFF		(PORTB &= ~(1 << PORTB5))
#define SPI_DATA_ON		(PORTB |= (1 << PORTB3))
#define SPI_DATA_OFF	        (PORTB &= ~(1 << PORTB3))





extern unsigned char Timer0_Value;
extern unsigned short ucFreqNew,ucFreqOld;


typedef struct tagFlags{
    unsigned char   Active 		: 1,
                    RxComplet 	: 1,
                    TxComplet 	: 1,
                    RxReady 	: 1,
				    ScheduleSet : 1,
   					Limit		: 1,
                    Dummy		:2;
}CommFlags;



//extern ungActiveFlags;
//extern CommFlags Flags;
//extern ungLampFlags;
//extern ungConfigData;
//extern ungIntrruptFlags;

extern void frequency();
extern void funInit_ext1(void);