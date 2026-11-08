

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

//#define TimeSet      0xe0     // d set //
//#define TimeUp     0xd0
//#define TimeDn     0xb0
//#define TimeUp    0xb0
//#define TimeDn    0xd0
//#define TimeStSp  0x70

#define Zero         0
#define AsciZero     0x30
#define Ascione      0x31
#define AsciNine     0x39


#define Remote    0x02

#define LED_OUT      PORTA_Bit1
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

#define SELKeyTest      bit_test(PINB,0)
#define RemoteKeyTest   bit_test(PINB,1)
#define autotunkeyTest  bit_test(PINB,7)
#define upkeyTest       bit_test(PINB,6)
#define donwkeyTest     bit_test(PINB,5)
#define modeKeyTest     bit_test(PINB,4)


#define RunOnes         bit_test(ucBitKeyOnes,0)
#define RemoteOnes      bit_test(ucBitKeyOnes,1)
#define KeychkFlag      bit_test(ucBitKeyOnes,2)


#define RunOnesOn       bit_set(ucBitKeyOnes,0)
#define RunOnesOff      bit_clear(ucBitKeyOnes,0)
#define RemoteOnesOn    bit_set(ucBitKeyOnes,1)
#define RemoteOnesOff   bit_clear(ucBitKeyOnes,1)
#define KeychkFlagOn    bit_set(ucBitKeyOnes,2)
#define KeychkFlagOff   bit_clear(ucBitKeyOnes,2)
#define BAUDRATE_38400 	    25


// initialization command with 4-bit interface
#define LCD_INIT4    0x20        //0x30, 8bit
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

char ucLin1=0;
char gucLin2=0;   // 1=min, 2=sec, 3=full?

char ucOrgPL_H='1';
char ucOrgPL_L='0';
char ucPL_H=0,ucPL_L=0;

byte  ucTimerSetFlag=0,ucBitKeyOnes=0,KeyCnt=0;
byte  Oldlin2=1,pinbb,pindd,TimeStSpFlag,ucT0_min=0,ucFinish=0;
volatile unsigned char ucPortdFlag;
byte  TimeRemoteFlag,ucPwmmin;

char OrgTimeData0 = '0';
char OrgTimeData1 = '1';
char ucgErrorDelay = 0;

byte  RemkeyOnlg=1,RemkeyOfflg=1,RemoteSele=0;

byte  _ucOc1val,ucTcnt1hval,error_count;
volatile unsigned char ErrorFlag;

unsigned ad_data[3];
unsigned t0_cnt=0;

char f_cnt;

volatile unsigned f_data,f_data2;

#define LCD_WIDTH    16

const byte line1[9][LCD_WIDTH]= {
      "  ULTRA SONIC   ",  //0
      "PULSE       STOP",  //1
      "PULSE       RUN.",  //2
      "CONT.       STOP",  //3
      "CONT.       RUN.",  //4
      "PULSE           ",  //5
      "CONT.           ",  //6
      "   TRANSDUCER   ",  //7
      "   OVER  LOAD   ",  //8
   };

const byte line2[10][LCD_WIDTH]= {
      "     WELDER     ",  //0
      " Tuning ERROR.. ",  //1
      " AUTO Tuning.55 ",  //2
      " AUTO Tuning.65 ",  //3
      "P/L:   % 00.0kHz",  //4
      "   ERROR  !!!   ",  //5
      " AUTO Tuning.75 ",  //6
      " AUTO Tuning.100",  //7
      " AUTO Tuning.ex ",  //8
      " FREQ ERROR !!! ",  //9
   };

void init_port();
void lcd_strobe();
void display(byte le1,byte le2);
void init_lcd();
void init_adc();
void delay_ms(unsigned interval);
//void write_data(void);
void funKey_scan();
void adc_process(char chan);
//void KeyTime_pross();
//void display_time();
//void TimeSetDis();
void disp_start();
void funStSpKey_chk();
//void timer_process();
void pl_display();
void init_timer2();
void disp_error();
void init_ext0();
void frequency();
void error_chk();
void control_error(unsigned char tuning);
void disp_error2();
//void sweep_process();
void init_timer1();
void EEPROM_READ();
void sonicOFF();
void sonicON();
//void dispsweep();
//void disprunon();
//void disprunoff();
void ResetInit();
//void TimeDis_sweep();

void funUartInit(unsigned short usBaud);
void funPutChar(unsigned char cb);
void funPutString(unsigned char *st);



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
