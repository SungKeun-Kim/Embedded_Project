#include <iom8.h>
#include <stdio.h>
#include <string.h>


#include "Atmega8.h"
#include "TSO8.H"
#include "Uart.h"


#define FILTER_SIZE  8

unsigned filter[FILTER_SIZE];
unsigned filtered;
char filter_ptr;
unsigned short ucFreqNew,ucFreqOld;


void frequency()
{
   unsigned f_data3;
   unsigned char saved = SREG;

   __disable_interrupt();
   f_data3 = f_data2;
   SREG = saved;

 //  filtered = filtered - filter[filter_ptr] + f_data3;
 //  filter[filter_ptr] = f_data3;
//   filter_ptr++;
 //  if (filter_ptr == FILTER_SIZE) filter_ptr = 0;

   ucFreqNew = f_data3;
 //  eep_write_word(0x0c,uFreqNew);
}
