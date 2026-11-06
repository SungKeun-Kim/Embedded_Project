#include <assert.h>
#include <stddef.h>

#define LCD_INIT4 0x20
#define LCD_INIT LCD_INIT4
#define LCD_LINES 0x08
#define DISP_CTRL 0x08
#define DISP_OFF 0x00
#define DISP_ON 0x04
#define DISP_CLEAR 0x01
#define ENTRY_MODE 0x04
#define ADDR_INC 0x02

enum event_type { CONTROL, WRITE_MODE, DELAY, NIBBLE, COMMAND };

struct event {
   enum event_type type;
   unsigned value;
};

static struct event events[20];
static size_t event_count;
static unsigned char LCD_STB = 1;

static void record(enum event_type type, unsigned value)
{
   assert(event_count < sizeof(events) / sizeof(events[0]));
   events[event_count].type = type;
   events[event_count].value = value;
   event_count++;
}

static void lcd_control_mode(void)
{
   assert(LCD_STB == 0);
   record(CONTROL, 0);
}

static void lcd_wrmode(void) { record(WRITE_MODE, 0); }
static void delay_ms(unsigned value) { record(DELAY, value); }
static void write_high_nibble(unsigned value) { record(NIBBLE, value); }
static void lcd_write_command(unsigned value) { record(COMMAND, value); }

#include "../Atmega16A_LM565_BD/lcd_init.c"

int main(void)
{
   static const struct event expected[] = {
      { CONTROL, 0 }, { WRITE_MODE, 0 }, { DELAY, 50 },
      { NIBBLE, 0x30 }, { DELAY, 5 },
      { NIBBLE, 0x30 }, { DELAY, 1 },
      { NIBBLE, 0x30 }, { DELAY, 1 },
      { NIBBLE, 0x20 }, { DELAY, 1 },
      { COMMAND, 0x28 }, { COMMAND, 0x08 },
      { COMMAND, 0x01 }, { DELAY, 5 },
      { COMMAND, 0x06 }, { COMMAND, 0x0c }, { DELAY, 5 }
   };
   size_t i;

   init_lcd();
   assert(event_count == sizeof(expected) / sizeof(expected[0]));
   for (i = 0; i < event_count; i++) {
      assert(events[i].type == expected[i].type);
      assert(events[i].value == expected[i].value);
   }
   return 0;
}
