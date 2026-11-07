
#include <iom8535.h>

#define byte unsigned char		//8bit
#define word unsigned int		//16bit

#define bit_set(ADDRESS,BIT)    (ADDRESS|=(1<<BIT))
#define bit_clear(ADDRESS,BIT)  (ADDRESS&=~(1<<BIT))
#define bit_test(ADDRESS,BIT)   (ADDRESS&(1<<BIT))

#define LCD_RS_SET bit_set(PORTC,0)
#define LCD_RS_CLEAR bit_clear(PORTC,0)

#define LCD_RW_SET bit_set(PORTC,1)
#define LCD_RW_CLEAR bit_clear(PORTC,1)

#define LCD_E_SET bit_set(PORTC,2)
#define LCD_E_CLEAR bit_clear(PORTC,2)

//ADEN, ADSC, ADATE, ADIF, ADIE, ADPS2, ADPS1, ADPS0,=ADCSRA
#define adc_start  (ADCSRA |= 0x40)
#define ADC_EOC	   (ADCSRA & 0x10)

#define TimeSet		0xe0		//¼³d set //
//#define TimeUp		0xd0
//#define TimeDn		0xb0
#define TimeUp		0xb0
#define TimeDn		0xd0
#define TimeStSp	0x70


#define	Zero			0
#define AsciZero		0x30
#define Ascione			0x31
#define AsciNine		0x39


#define Remote		0x02

#define LedOff		bit_clear(PORTD,1)
#define LedOn		bit_set(PORTD,1)

#define BuzzerOff		bit_clear(PORTD,3)
#define BuzzerOn		bit_set(PORTD,3)

#define	RunKeyTest		bit_test(PINB,0)
#define RemoteKeyTest	bit_test(PINB,1)
#define SweepKeyTest	bit_test(PINA,3)

#define SweepKeyoff	bit_clear(PORTA,7)
#define SweepKeyon	bit_set(PORTA,7)

#define RunOnes		bit_test(ucBitKeyOnes,0)
#define RemoteOnes	bit_test(ucBitKeyOnes,1)
#define KeychkFlag	bit_test(ucBitKeyOnes,2)

#define RunOnesOn		bit_set(ucBitKeyOnes,0)
#define RunOnesOff		bit_clear(ucBitKeyOnes,0)
#define RemoteOnesOn	bit_set(ucBitKeyOnes,1)
#define RemoteOnesOff	bit_clear(ucBitKeyOnes,1)
#define KeychkFlagOn	bit_set(ucBitKeyOnes,2)
#define KeychkFlagOff	bit_clear(ucBitKeyOnes,2)


byte    writebuffer,ucLin1=0,gucLin2=0,ucOrgPL_H=0,ucOrgPL_L=0,ucPL_H=0,ucPL_L=0;
byte    ucTimerSetFlag=0,ucBitKeyOnes=0,ucTimeData1=0x30,ucTimeData0=0x30,KeyCnt=0;
byte 	Oldlin2=1,pinbb,pindd,TimeStSpFlag,ucPortdFlag=0,ucT0_min=0,ucFinish=0;
byte	TimeRemoteFlag,OrgTimeData0,OrgTimeData1,ucgErrorDelay=0;
byte	RemkeyOnlg=1,RemkeyOfflg=1,RemoteSele=0,ocrdata;

byte	ucOc1val,ucTcnt1hval,ErrorFlag=0;

word	ad_data[3],f_cnt=0,t0_cnt=0;


unsigned long f_data=0,f_data2=0;



const byte line1[9][16]= {{' ',' ','H','A','N','K','U','K',' ',' ','2','0','0','9',' ',' '}	//0
						 ,{'R','U','N',':','O','F','F',' ','T','I','M','E',':','0','0','M'}	//1
						 ,{'R','U','N',':','O','F','F',' ','P','/','L',':','0','0','0','%'}	//2
						 ,{'R','U','N',':','O','N',' ',' ','T','I','M','E',':','0','0','M'}	//3
						 ,{' ','S','W','E','E','P',' ',' ','T','I','M','E',':','0','0','M'}	//4
						 ,{'R','U','N',':','O','N',' ',' ','T','I','M','E','O','V','E','R'}	//5
						 ,{'R','U','N',':','O','F','F',' ','R','e','m','o','t','e','O','N'}	//6
						 ,{' ',' ',' ','T','R','A','N','S','D','U','C','E','R',' ',' ',' '}	//7
						 ,{' ',' ',' ','O','V','E','R',' ',' ','L','O','A','D',' ',' ',' '}};//8


const byte line2[8][16]= {{' ','H','A','N','K','U','K',' ',' ','S','O','N','I','C','S',' '}//0
						 ,{'T','I','M','E',' ','S','E','T',':','0','0','M','I','N','.',' '}//1
						 ,{'T','I','M','E',' ','S','E','T',':','0','0','S','E','C','.',' '}//2
						 ,{'T','I','M','E',' ','S','E','T',':','F','U','L','L',' ',' ',' '}//3
						 ,{'P','/','L',':','0','0','0','%',' ','0','0','.','0','k','h','z'}//4
						 ,{' ',' ',' ','E','R','R','O','R',' ',' ','!','!','!',' ',' ',' '}//5
						 ,{' ','K','E','Y',' ','E','R','R','O','R',' ','!','!','!',' ',' '}//6
						 ,{'S','W','E','E','P',' ','S','E','T',' ','L','/','V',':','0','0'}};//7


void init_port();
void enable();
void display(byte le1,byte le2);
void init_lcd();
void init_adc();
void delay(unsigned int d);
void delay_ms(unsigned int d);
void write_inst(void);
void write_data(void);
void funKey_scan();
void adc_process(byte ch);
void KeyTime_pross();
void TimeDis();
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

