
#include <iom16.h>
#include <intrinsics.h>
#include "tim16.h"
#include "init_disp.h"


void disp_start()
{
   delay_ms(500);
   display(0,0);     //초기 화면 //
   delay_ms(1000);      // 1.5초 후 다음 화면 출력 //
   display(1,4);     // 기본 대기 화면 //
//   TimeSetDis();
}




static void write_nibble(char dat)
{
   LCD_DATA_OUT = (LCD_DATA_OUT & 0x0f) | (dat & 0xf0);
   lcd_strobe();
}

#define write_high_nibble(x)     write_nibble(x)
#define write_low_nibble(x)      write_nibble((x)*16)

static void lcd_write(char x)
{
   lcd_wrmode();
   write_high_nibble(x);
   write_low_nibble(x);
}

static char lcd_read(void)
{
   char ch;
   lcd_rdmode();
   __();
   LCD_STB = 1;
   __();
   ch = LCD_DATA_IN;
   LCD_STB = 0;

// 4-bit interface is used
   __();
   LCD_STB = 1;
   __();
   // high nibble is used for lcd data port
   ch = (ch&0xf0)+(LCD_DATA_IN/16);
   LCD_STB = 0;
   lcd_wrmode();
   return(ch);
}

#define LCD_DELAY    150

//#define FLAG_POLLING

static void lcd_delay(void)
{
#ifdef FLAG_POLLING
   // busy flag polling algorithm is used
   char dat = 0xff;
   lcd_control_mode();
   while (dat&0x80) dat = lcd_read();
#else
   unsigned i;
   for (i = 0; i < LCD_DELAY; i++) asm("NOP");
#endif
}

static void lcd_write_command(char dat)
{
   lcd_write(dat);
   lcd_delay();
}

static void lcd_disp_data(char dat)
{
   lcd_write(dat);
   lcd_delay();
}

#define FIRST_LINE   0x80
#define SECOND_LINE  0xc0

const static char lcd_left[2] = {FIRST_LINE, SECOND_LINE, };

void lcd_locate(char x, char y)
{
   lcd_control_mode();
   lcd_write_command(lcd_left[y]+x);
}

void lcd_disp_str(char *str)
{
   lcd_data_mode();
   while (*str) lcd_disp_data(*str++);
}



#pragma inline
#include "lcd_init.c"


#define FILTER_SIZE  8

unsigned filter[FILTER_SIZE];
unsigned filtered;
char filter_ptr;

//=====================================================================
void frequency()
{
   unsigned char b,c,d;
   unsigned a;
   unsigned f_data3;

   //  a = f_data2/50; //보정값 //

   __disable_interrupt();
   f_data3 = f_data2;
   __enable_interrupt();

   filtered = filtered - filter[filter_ptr] + f_data3;
   filter[filter_ptr] = f_data3;
   filter_ptr++;
   if (filter_ptr == FILTER_SIZE) filter_ptr = 0;

    a = (filtered-12)/10;   //12->offset value 20200809

    c = a%10;
    a = a/10;
    b = a%10;
    d = a/10;

   //==================================================
   //표시 부분 //
   lcd_locate(9, 1);

      if(ucPortdFlag){              //START 일때 확인
         lcd_data_mode();
         lcd_disp_data(d + '0');
         lcd_disp_data(b + '0');
         lcd_disp_data('.');
         lcd_disp_data(c + '0');
      }
      else {   //2007.12   //start 일때와 stop 일때 주파수 출력 구분 하려고 함.//
         lcd_disp_str("00.0");
      }
}



//=====================================================================
//줄단위로 출력 할때..
void display(byte le1,byte le2)
{
   byte k;
   ready_notice_cancel();

   //윗줄 //
   lcd_locate(0, 0);
   lcd_data_mode();
   for (k=0; k<LCD_WIDTH; k++) {
      lcd_disp_data(line1[le1][k]);
   }
      //아래줄 //
   lcd_locate(0, 1);
   lcd_data_mode();
   for (k=0; k<LCD_WIDTH; k++) {
      lcd_disp_data(line2[le2][k]);
   }
}

void display_Line(byte le1)
{
   byte k;

   //윗줄 //
   lcd_locate(0, 0);
   lcd_data_mode();
   for (k=0; k<5; k++) {
      lcd_disp_data(line1[le1][k]);
   }

}


void display_active()
{
   byte k;

   //윗줄 //
   lcd_locate(15,0);
   lcd_data_mode();
   lcd_disp_data(' ');
}
void display_active_0()
{
   byte k;

   //윗줄 //
   lcd_locate(0x0f, 0);
   lcd_data_mode();
   lcd_disp_data('*');
}



//=====================================================================


/*
//=====================================================================
// 시간 설정을 표시해주는 부분 //
void display_time()
{
     //아래쪽 시간 설정 하는 부분 표시 함수 //
   lcd_locate(9, 1);
   lcd_data_mode();
   lcd_disp_data(ucTimeData1);
   lcd_disp_data(ucTimeData0);
}
*/
/*
//=====================================================================
// sweep 설정을 표시해주는 부분 //  080616 추가 //
void TimeDis_sweep()
{
      //아래쪽 시간 설정 하는 부분 표시 함수 //
   lcd_locate(0x0e, 1);
   lcd_data_mode();
   lcd_disp_data(ucPL_H);
   lcd_disp_data(ucPL_L);
}
*/
/*
//=================================================================

// 시간 설정 부분을 표시하는 처리부분
void TimeSetDis()
{
      //기본  time 단위는 'm'//..
   if(Oldlin2!=3){      //time단위가 's'로 선택//..
      //윗줄 표시 부분 //
      lcd_locate(0x0c, 0);
      lcd_data_mode();
      //12.13add
      lcd_disp_data(':');
      //설정한 시간값 //
      lcd_disp_data(ucTimeData1);
      lcd_disp_data(ucTimeData0);
      if(Oldlin2==2) lcd_disp_data('S');
      else lcd_disp_data('M');
   }
   else {
      //time 단위가 full로 선택//..
      lcd_locate(0x0c, 0);
      lcd_disp_str(":FUL");
   }
}
 */


