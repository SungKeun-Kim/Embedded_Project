//#include "_iom8.h"

#define CLK_8MHz        8000000
#define CLK_16MHz       16000000
/*
#ifndef SysClock
#define SysClock	CLK_16MHz
#endif
*/
#define __CLK_SPEED__	SysClock

#define NORMAL_SPEED	FALSE
#define DOUBLE_SPEED	TRUE
/*
#if (__CLK_SPEED__ == CLK_8MHz)
#define BAUDRATE_250000(_U2X_Mode) 	_U2X_Mode ?  3   : 1
#define BAUDRATE_38400(_U2X_Mode) 	_U2X_Mode ?  25  : 12
#define BAUDRATE_19200(_U2X_Mode) 	_U2X_Mode ?  51  : 25
#define BAUDRATE_9600(_U2X_Mode) 	_U2X_Mode ?  103 : 51
#elif (__CLK_SPEED__ == CLK_16MHz)
#define BAUDRATE_230400(_U2X_Mode) 	_U2X_Mode ?  8   : 3
#define BAUDRATE_115200(_U2X_Mode) 	_U2X_Mode ?  16  : 8
#define BAUDRATE_57600(_U2X_Mode) 	_U2X_Mode ?  34  : 16
#define BAUDRATE_38400(_U2X_Mode) 	_U2X_Mode ?  51  : 25
#define BAUDRATE_19200(_U2X_Mode) 	_U2X_Mode ?  103 : 51
#define BAUDRATE_9600(_U2X_Mode) 	_U2X_Mode ?  207 : 103
#else
#endif
*/
#if (__CLK_SPEED__ == CLK_8MHz)
#define BAUDRATE_250000     1
#define BAUDRATE_38400 	    12
#define BAUDRATE_19200 	    25
#define BAUDRATE_9600 	    51
#elif (__CLK_SPEED__ == CLK_16MHz)
#define BAUDRATE_230400     3
#define BAUDRATE_115200     8
#define BAUDRATE_57600 	    16
#define BAUDRATE_38400 	    25
#define BAUDRATE_19200 	    51
#define BAUDRATE_9600	    103
#else
#endif


extern void funUartInit(unsigned short usBaud);
extern void funPutChar(unsigned char cb);
extern void funPutString(unsigned char *st);


