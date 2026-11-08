#include "tuning_store.h"

#ifndef TUNING_STORE_CAN_COMMIT
#define TUNING_STORE_CAN_COMMIT() 1
#endif

/* EEPROM 접근 함수는 AVR 드라이버 또는 호스트 테스트에서 제공한다. */
extern unsigned char funEEprom_Read(unsigned short address);
extern void funEEprom_Write(unsigned short address, unsigned char value);

static unsigned char store_crc(const unsigned char *p)
{
   unsigned char crc = 0, i, bit;
   for (i = 1; i < 7; ++i) {
      crc ^= p[i];
      for (bit = 0; bit < 8; ++bit)
         crc = (unsigned char)((crc & 0x80) ? (crc << 1) ^ 7 : crc << 1);
   }
   return crc;
}

static unsigned char store_read(unsigned short address, unsigned char *r)
{
   unsigned char i;
   for (i = 0; i < TUNING_RECORD_SIZE; ++i) r[i] = funEEprom_Read(address + i);
   return r[0] == 0xA5 && r[1] == 1 &&
      r[3] >= PLL_POT_MIN && r[3] <= PLL_POT_MAX &&
      r[6] == (unsigned char)(r[3] ^ 0xFF) && r[7] == store_crc(r) &&
      ((unsigned short)r[4] | ((unsigned short)r[5] << 8)) != 0;
}

static unsigned char store_latest(unsigned char *a, unsigned char *b)
{
   unsigned char va = store_read(TUNING_SLOT0, a);
   unsigned char vb = store_read(TUNING_SLOT1, b);
   if (!va) return vb ? 2 : 0;
   if (!vb) return 1;
   return (unsigned char)(b[2] - a[2]) < 128 && b[2] != a[2] ? 2 : 1;
}

unsigned char tuning_store_load(unsigned char *code, unsigned short *count)
{
   unsigned char a[8], b[8], *r, latest = store_latest(a, b);
   *code = PLL_POT_DEFAULT;
   *count = 0;
   if (latest) {
      r = latest == 1 ? a : b;
      *code = r[3];
      *count = (unsigned short)r[4] | ((unsigned short)r[5] << 8);
      return 2;
   }
   if (funEEprom_Read(0xFF) == 0x55) {
      unsigned char old = funEEprom_Read(2);
      if (old >= PLL_POT_MIN && old <= PLL_POT_MAX) {
         *code = old;
         /* 구형 8비트 주파수의 상위 바이트는 복원할 수 없다. */
         return 1;
      }
   }
   return 0;
}

static unsigned char store_write_checked(unsigned short address, unsigned char value)
{
   if (funEEprom_Read(address) != value) funEEprom_Write(address, value);
   return funEEprom_Read(address) == value;
}

unsigned char tuning_store_save(unsigned char code, unsigned short count)
{
   unsigned char a[8], b[8], r[8], i, latest;
   unsigned short address;
   if (code < PLL_POT_MIN || PLL_POT_CODE_ABOVE_MAX(code) || count == 0) return 0;
   latest = store_latest(a, b);
   if (latest) {
      unsigned char *old = latest == 1 ? a : b;
      if (old[3] == code && old[4] == (unsigned char)count && old[5] == (unsigned char)(count >> 8)) return 1;
      r[2] = (unsigned char)(old[2] + 1);
   } else r[2] = 0;
   address = latest == 1 ? TUNING_SLOT1 : TUNING_SLOT0;
   r[0] = 0xA5;
   r[1] = 1;
   r[3] = code;
   r[4] = (unsigned char)count;
   r[5] = (unsigned char)(count >> 8);
   r[6] = (unsigned char)(code ^ 0xFF);
   r[7] = store_crc(r);
   /* 이전 슬롯을 보존하고, 새 슬롯의 유효 표시는 마지막에 쓴다. */
   if (!store_write_checked(address, 0)) return 0;
   for (i = 1; i < 8; ++i)
      if (!store_write_checked(address + i, r[i])) return 0;
   if (!TUNING_STORE_CAN_COMMIT() || !store_write_checked(address, r[0])) return 0;
   if (!store_read(address, a)) return 0;
   for (i = 0; i < 8; ++i) if (a[i] != r[i]) return 0;
   /* 구형 펌웨어로 복귀할 때 사용할 호환 사본. 새 형식이 우선이다. */
   funEEprom_Write(2, code);
   funEEprom_Write(3, (unsigned char)count);
   funEEprom_Write(0xFF, 0x55);
   (void)funEEprom_Read(0xFF);
   return 1;
}
