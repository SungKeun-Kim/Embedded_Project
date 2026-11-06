#include <iom8.h>
#include <stdio.h>
#include <string.h>

//#include "_iom8.h"
#include "Uart.h"


void funUartInit(unsigned short usBaud)
{
    //UCSRB = 0x9B;               // disable while setting baud rate
    UCSRA ^= 0x40;
    UCSRC = 0x86;
    UCSRB = (1 << RXCIE)|(0 << TXCIE)|(0 << UDRIE)|(1 << RXEN)|(1 << TXEN)|(0 << UCSZ2)|(1 << RXB8)|(1 << TXB8);
    UBRRH = (unsigned char)(usBaud >> 8);// Set baud rate
    UBRRL = (unsigned char) usBaud;

}

void funPutChar(unsigned char cb)
{
    while(!(UCSRA & (1 << UDRE)));
    UDR = cb;

}

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



