
#include <iom16.h>

/* EEPROM Control Register - EECR */
#define    EERIE     3
#define    EEMWE     2
#define    EEWE      1
#define    EERE      0

#define wait_eep_avail()  while (EECR & (1 << EEWE))
#define eep_write_enable() { EECR = EECR | (1 << EEMWE); EECR = EECR | (1 << EEWE); }
#define eep_read_enable()  { EECR = EECR | (1 << EERE); }

// byte read/write
char eep_read_byte(unsigned offset)
{
   wait_eep_avail();
   EEARH = offset/256;
   EEARL = offset%256;
   eep_read_enable();
   return(EEDR);
}

void eep_write_byte(unsigned offset, char dat)
{
   wait_eep_avail();
   EEARH = offset/256;
   EEARL = offset%256;
   EEDR = dat;
   eep_write_enable();
}

void eep_write_word(unsigned offset, unsigned dat)
{
   eep_write_byte(offset, dat/256);
   eep_write_byte(offset+1, dat%256);
}

unsigned eep_read_word(unsigned offset)
{
   unsigned hi;
   hi = eep_read_byte(offset)*256;
   return(hi+eep_read_byte(offset+1));
}
