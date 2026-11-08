/* ATmega8.c에서 포함. 테스트에서는 TWI 레지스터 쓰기만 모의 구현한다. */
#ifndef DIGIPOT_CONTROL
#define DIGIPOT_CONTROL(value) (TWCR = (value))
#endif

/* 전송 완료와 상태를 분리해서 검사한다. NACK에서는 즉시 실패한다. */
static unsigned char twi_wait(unsigned char expected)
{
   unsigned short limit = 60000U;
   while (!(TWCR & (1 << TWINT))) if (--limit == 0) return 0;
   return (TWSR & 0xF8) == expected;
}

static unsigned char twi_stop(void)
{
   unsigned short limit = 60000U;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWSTO) | (1 << TWEN));
   while (TWCR & (1 << TWSTO)) {
      if (--limit == 0) { DIGIPOT_CONTROL(0); return 0; }
   }
   return 1;
}

unsigned char TWI_data(unsigned char data)
{
   unsigned char actual;
   if (data < PLL_POT_MIN || PLL_POT_CODE_ABOVE_MAX(data)) return 0;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWSTA) | (1 << TWEN));
   if (!twi_wait(0x08)) goto failed;
   TWDR = 0x58;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWEN));
   if (!twi_wait(0x18)) goto failed;
   TWDR = 0;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWEN));
   if (!twi_wait(0x28)) goto failed;
   TWDR = data;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWEN));
   if (!twi_wait(0x28)) goto failed;
   /* 반복 START로 RDAC 값을 읽어, ACK만 받은 상태와 실제 설정을 구분한다. */
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWSTA) | (1 << TWEN));
   if (!twi_wait(0x10)) goto failed;
   TWDR = 0x59;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWEN));
   if (!twi_wait(0x40)) goto failed;
   DIGIPOT_CONTROL((1 << TWINT) | (1 << TWEN));
   if (!twi_wait(0x58)) goto failed;
   actual = TWDR;
   if (!twi_stop()) return 0;
   return actual == data;
failed:
   (void)twi_stop();
   return 0;
}

