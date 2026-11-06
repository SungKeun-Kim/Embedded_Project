
#include <iom16.h>
#include <intrinsics.h>
#include "tim16.h"

#define EEP_ID             0xaa55

#define ADDR_EEP_ID        0x00
#define ADDR_OrgTimeData0  0x02
#define ADDR_OrgTimeData1  0x03
#define ADDR_gucLin2       0x04
#define ADDR_ucOrgPL_H     0x05
#define ADDR_ucOrgPL_L     0x06

char eep_read_byte(unsigned offset);
void eep_write_byte(unsigned offset, char dat);

unsigned eep_read_word(unsigned offset);
void eep_write_word(unsigned offset, unsigned dat);

#define byte unsigned char    //8bit
#define word unsigned int     //16bit

#define bit_set(ADDRESS,BIT)    (ADDRESS|=(1<<BIT))
#define bit_clear(ADDRESS,BIT)  (ADDRESS&=~(1<<BIT))
#define bit_test(ADDRESS,BIT)   (ADDRESS&(1<<BIT))

//ADEN, ADSC, ADATE, ADIF, ADIE, ADPS2, ADPS1, ADPS0,=ADCSRA

#define SONIC_OUTPUT  PORTD_Bit6

#define TimeSet      0xe0     // d set //
//#define TimeUp     0xd0
//#define TimeDn     0xb0
#define TimeUp    0xb0
#define TimeDn    0xd0
#define TimeStSp  0x70

#define Zero         0
#define AsciZero     0x30
#define Ascione      0x31
#define AsciNine     0x39


#define Remote    0x02

#define LED_OUT      PORTD_Bit1
#define LED_OFF      1
#define LED_ON       0

#define led_off()    LED_OUT = LED_OFF
#define led_on()     LED_OUT = LED_ON

#define BUZZER       PORTD_Bit3
#define BUZZER_OFF   0
#define BUZZER_ON    1

#define BUZZ_COUNT    100

char buzz_counter;

void buzzer_off(void)
{
   BUZZER = BUZZER_OFF;
   buzz_counter = 0;
}

void buzzer_on()
{
   BUZZER = BUZZER_ON;
   buzz_counter = BUZZ_COUNT;
}

#define BEEPER       PORTD_Bit4

#define BEEPER_ON    1
#define BEEPER_OFF   0

#define beeper_on()  BEEPER = BEEPER_ON
#define beeper_off() BEEPER = BEEPER_OFF

#define  RunKeyTest     bit_test(PINB,0)
#define RemoteKeyTest   bit_test(PINB,1)
#define SweepKeyTest    bit_test(PINA,3)

#define SWEEP_OUT    PORTA_Bit7

#define sweep_out_on()     SWEEP_OUT = 1
#define sweep_out_off()    SWEEP_OUT = 0

#define RunOnes         bit_test(ucBitKeyOnes,0)
#define RemoteOnes      bit_test(ucBitKeyOnes,1)
#define KeychkFlag()      bit_test(ucBitKeyOnes,2)

#define RunOnesOn       bit_set(ucBitKeyOnes,0)
#define RunOnesOff      bit_clear(ucBitKeyOnes,0)
#define RemoteOnesOn    bit_set(ucBitKeyOnes,1)
#define RemoteOnesOff   bit_clear(ucBitKeyOnes,1)
#define KeychkFlagOn    bit_set(ucBitKeyOnes,2)
#define KeychkFlagOff   bit_clear(ucBitKeyOnes,2)

char ucLin1=0;
char gucLin2=1;   // 1=min, 2=sec, 3=full?

char ucOrgPL_H='1';
char ucOrgPL_L='0';
char ucPL_H=0,ucPL_L=0;

byte  ucTimerSetFlag=0,ucBitKeyOnes=0,ucTimeData1=0x30,ucTimeData0=0x30,KeyCnt=0;
byte  Oldlin2=1,pinbb,pindd,TimeStSpFlag,ucPortdFlag=0,ucT0_min=0,ucFinish=0;
byte  TimeRemoteFlag;

char OrgTimeData0 = '0';
char OrgTimeData1 = '1';
char ucgErrorDelay = 0;

byte  RemkeyOnlg=1,RemkeyOfflg=1,RemoteSele=0;

byte  _ucOc1val,ucTcnt1hval,ErrorFlag=0;

unsigned ad_data[3];
unsigned t0_cnt=0;

char f_cnt;

unsigned f_data,f_data2;

#define LCD_WIDTH    16

const byte line1[9][LCD_WIDTH]= {
      "     WELDER     ",  //0
      "RUN:OFF TIME:00M",  //1
      "RUN:OFF P/L:000%",  //2
      "RUN:ON  TIME:00M",  //3
      " SWEEP  TIME:00M",  //4
      "RUN:ON  TIMEOVER",  //5
      "RUN:OFF RemoteON",  //6
      "   TRANSDUCER   ",  //7
      "   OVER  LOAD   ",  //8
   };

const byte line2[8][LCD_WIDTH]= {
      "    SS-SONIC    ",  //0
      "TIME SET:00MIN. ",  //1
      "TIME SET:00SEC. ",  //2
      "TIME SET:FULL   ",  //3
      "P/L:000% 00.0kHz",  //4
      "   ERROR  !!!   ",  //5
      " KEY ERROR !!!  ",  //6
      "SWEEP SET L/V:00",  //7
   };

void init_port();
void lcd_strobe();
void display(byte le1,byte le2);
void init_lcd();
void init_adc();
void delay_ms(unsigned interval);
void write_data(void);
void funKey_scan();
void adc_process(char chan);
void KeyTime_pross();
void display_time();
void TimeSetDis();
void disp_start();
void funStSpKey_chk();
void timer_process();
void pl_display();
void init_timer2();
void disp_error();
void init_ext0();
void frequency();
void error_chk();
void disp_error2();
void sweep_process();
void init_timer1();
void EEPROM_READ();
void sonicOFF();
void sonicON();
void dispsweep();
void disprunon();
void disprunoff();
void ResetInit();
void TimeDis_sweep();


void disp_start()
{
   display(0,0);     //초기 화면 //
   delay_ms(1500);      // 1.5초 후 다음 화면 출력 //
   display(1,0);     // 기본 대기 화면 //
   TimeSetDis();
}



//=====================================================================

#define LCD_DATA_OUT    PORTC
#define LCD_DATA_IN     PINC

#define LCD_CTRL     PORTC_Bit0
#define LCD_RW       PORTC_Bit1
#define LCD_STB      PORTC_Bit2

#define lcd_control_mode()    LCD_CTRL = 0
#define lcd_data_mode()       LCD_CTRL = 1

#define lcd_wrmode()          { LCD_RW = 0; DDRC = DDRC | 0xf0; }
#define lcd_rdmode()          { LCD_RW = 1; DDRC = DDRC & 0x0f; }

static void _() { asm("NOP"); }
static void __() { _(); _(); _(); _(); _(); _(); }

static void lcd_strobe() { LCD_STB = 1; __(); LCD_STB = 0; }

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

// initialization command with 4-bit interface
#define LCD_INIT4    0x20
#define LCD_INIT     LCD_INIT4

#define LCD_LINE1    0x00
#define LCD_LINES2   0x08

#define LCD_LINES    LCD_LINES2

#define DISP_CLEAR   0x01

#define DISP_CTRL    0x08

   #define DISP_ON      0x04
   #define DISP_OFF     0x00

   #define CURSOR_ON    0x02
   #define CURSOR_OFF   0x00

   #define BLINK_ON     0x01
   #define BLINK_OFF    0x00

#define ENTRY_MODE   0x04

   #define ADDR_INC     0x02
   #define ADDR_DEC     0x00

void init_lcd()
{
   LCD_STB = 0;
   delay_ms(50);
   lcd_control_mode();
   lcd_wrmode();
   write_high_nibble(LCD_INIT+LCD_LINES);
   delay_ms(1);
   lcd_write_command(LCD_INIT+LCD_LINES);
   lcd_write_command(DISP_CTRL+DISP_OFF);
   lcd_write_command(DISP_CLEAR);
   delay_ms(5);
   lcd_write_command(ENTRY_MODE+ADDR_INC);
   lcd_write_command(DISP_CTRL+DISP_ON);
}

//=====================================================================

void dispsweep(void)
{
   lcd_locate(0, 0);
   lcd_disp_str("SWEEP  ");
}

void disprunon()
{
   lcd_locate(0, 0);
   lcd_disp_str("RUN:ON ");
}

void disprunoff()
{
   lcd_locate(0, 0);
   lcd_disp_str("RUN:OFF");
}


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

    a = filtered/10;

    c = a%10;
    a = a/10;
    b = a%10;
    d = a/10;
/*
   while(a >= 0x0a){
      f++;
      if(f>= 0x0a){d++;f=0;}
      a = a - 10;    //점 이하단위 ..//
   }

   b = f & 0x0f;  //일 단위
   c = d & 0x0f;  //십 단위

*/

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



