
#include <iom8.h>
#include <stdio.h>
#include <inavr.h>
#include <comp_a90.h>
#include <string.h>


#include "Atmega8.h"
#include "TSO8.H"
#include "Uart.h"
#include "ready_notice.c"




#define MINOHM  221 //237
#define MAXOHM  249 //253
#define AVEOHM  233 //245
#define INCREASE 1
#define DECREASE 2
#define IDLE     3
#define STOP     0
#define FIRST    0
#define SECOND   1
#define THIRD    2

#define FREQSTEP  2

#define ORGIN_REGADDR 0x0002
#define ORGIN_FRIQADDR 0x0003

#define EEP_CHK 0x00ff



unsigned char f_cnt,f_data,f_data2,t0_cnt,t2_cnt;
unsigned char ucOrgpt,ucRxDATA,ucTxDATA,ucError_flag,ucRUNFlag,ucFriqDel;
unsigned  usADCfchk;

void ready_notice_clear_upload_marker(void)
{
   unsigned char i;
   for (i = 0; i < ATMEGA8_UPLOAD_MARKER_LENGTH; i++) {
      funEEprom_Read(ATMEGA8_UPLOAD_MARKER_ADDRESS + i);
      _CLI();
      funEEprom_Write(ATMEGA8_UPLOAD_MARKER_ADDRESS + i, 0xff);
      _SEI();
   }
   funEEprom_Read(ATMEGA8_UPLOAD_MARKER_ADDRESS + ATMEGA8_UPLOAD_MARKER_LENGTH - 1);
}

void funEeprom_reload()
{
   if(funEEprom_Read(EEP_CHK)==0x55){
      ucOrgpt=funEEprom_Read(ORGIN_REGADDR);
      if(ucOrgpt<MINOHM){ucOrgpt=AVEOHM;funEEprom_Write(ORGIN_REGADDR,ucOrgpt);}
      if(ucOrgpt>MAXOHM){ucOrgpt=AVEOHM;funEEprom_Write(ORGIN_REGADDR,ucOrgpt);}
      ucFreqOld=funEEprom_Read(ORGIN_FRIQADDR);
   }else{
     ucOrgpt=AVEOHM; //2k
     ucFreqOld=0x7d;
     funEEprom_Write(EEP_CHK,0x55);
     funEEprom_Write(ORGIN_REGADDR,AVEOHM);
     funEEprom_Write(ORGIN_FRIQADDR,ucFreqOld);
   }
}


static unsigned short usDAC[3];

void funFrequecy_scan()
  {
    unsigned char  ucRegCnt=0;
    
    if(ucRUNFlag){
            //  bit_set(PORTD,6);
        ucFriqDel++;
        if(ucFriqDel==10){
           ucFriqDel=0;
           frequency();
        //     eep_write_word(0x0c,ucFreqNew);
           if((ucFreqNew>=0x60) && (ucFreqNew<=0x84)){
               if(ucFreqOld>=ucFreqNew){ucRegCnt=((ucFreqOld-ucFreqNew)/FREQSTEP)+1;}
               if(ucFreqOld<=ucFreqNew){ucRegCnt=1;}
                 switch(ucRegCnt){
                   case 1:

                     TWI_data(ucOrgpt);
                     DELAY_100MS;
                     break;
                   case 2:
                 //    DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-1);
                  //   DELAY_100MS;
                 //    DELAY_100MS;
                     break;
                   case 3:
                 //    DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-2);
                  //   DELAY_100MS;
                 //    DELAY_100MS;
                     break;
                   case 4:
                //     DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-3);
                 //    DELAY_100MS;
                 //    DELAY_100MS;
                     break;
                   case 5:
               //      DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-4);
                  //   DELAY_100MS;
                  //   DELAY_100MS;
                     break;
                   case 6:
              //       DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-5);
                 //    DELAY_100MS;
                 //    DELAY_100MS;
                     break;
                   case 7:
              //       DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-6);
                   //  DELAY_100MS;
                   //  DELAY_100MS;
                     break;
                   case 8:
              //       DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-7);
                 //    DELAY_100MS;
                 //    DELAY_100MS;
                     break;
                   case 9:
              //       DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-8);
                  //   DELAY_100MS;
                 //    DELAY_100MS;
                     break;
                   case 10:
              //       DELAY_1S;
                     if(ucOrgpt<MINOHM)ucOrgpt=MINOHM;
                     TWI_data(ucOrgpt-9);
                  //   DELAY_100MS;
                  //   DELAY_100MS;
                     break;
                   default:
                     break;
                 }
           }

        }
    }else{
      ucFriqDel=0;

    }
  }



//=================================
int main( void )
{

    static unsigned char ucOnschk,ucPortd;
    static unsigned char ucRegsDir=0,ucloopcnt=0;


    funAtmega8_PortInit();
    funAtmega8_ADC_Init();
    funAtmega8_Timer0Init(); //add 202009
    TWI_init_master();
    funInit_ext1();
    funAtmega8_UartInit();

//    INT0_RISING_EDGE;
	_SEI();
//    INT0_ENABLE;
    TCNT0 = TIMER_RELOAD;
    TIMER0_ENABLE;
    funEeprom_reload();


    DELAY_100MS;
    ready_notice_start();
    ucloopcnt=0;
    ucOnschk=0;
    ucError_flag=0;
    ucRegsDir=IDLE;
    ucRUNFlag=0;
 //   DELAY_100MS;
    TWI_data(ucOrgpt);//2k
    ucFriqDel=0;
    INT1_ENABLE;
 //                 ucOrgpt=238;//test

    while(1){
      ready_notice_process();
      INT1_ENABLE;
      if(ucRxDATA == 0x11)     //step  tuning
      {
        ucRxDATA=0;
        ucRegsDir=STOP;
        ucOnschk=2;
      }

      if(ucRxDATA == 0x17)      // direct tuning
      {
        ucRxDATA=0;
        ucRegsDir=STOP;
        ucOnschk=7;
      }

      if(ucRxDATA == 0x21){ucRUNFlag=1;ucRxDATA=0;ucRegsDir=IDLE;t0_cnt=0;}
      if(ucRxDATA == 0x29){ucRUNFlag=0;ucRxDATA=0;ucRegsDir=IDLE;t0_cnt=0;}

      if(ucPortd){
        DELAY_1S;
        ucPortd=0;

          ucloopcnt=0;
          TWI_data(ucOrgpt);
          DELAY_100MS;DELAY_100MS;DELAY_100MS;DELAY_100MS;
          DELAY_100MS;DELAY_100MS;

          usDAC[FIRST]=ADC_DATA;

          TWI_data(ucOrgpt+1);
          DELAY_100MS;DELAY_100MS;DELAY_100MS;DELAY_100MS;
          DELAY_100MS;DELAY_100MS;

          usDAC[SECOND]=ADC_DATA;

          if(usDAC[FIRST] > usDAC[SECOND]){
            usDAC[THIRD]=usDAC[SECOND];
            TWI_data(ucOrgpt-1);
            ucRegsDir=INCREASE;
          }else{
            usDAC[THIRD]=usDAC[FIRST];
            TWI_data(ucOrgpt);
            ucRegsDir=DECREASE;
          }

        //  if(ucError_flag){ucRegsDir=STOP; ucOnschk=1;ucError_flag=0;}
      }else{

        switch(ucRegsDir){
        case STOP:
          ucOnschk++;

          if(ucOnschk==2){
            funPutChar(0x12);  //tuning  error
            ucOnschk=0;
            ucRegsDir=IDLE;       //end
          }
          if(ucOnschk==3){
            ucPortd=1;
            funPutChar(0x13);  //tuning 55
            DELAY_1MS;
          }
          if(ucOnschk==4){
            ucPortd=1;
            funPutChar(0x14);  //tuning 65
            DELAY_1MS;
          }
          if(ucOnschk==5){
            ucPortd=1;
            funPutChar(0x15);  //tuning 75
            DELAY_1MS;
          }
          if(ucOnschk==6){
            ucPortd=1;
            funPutChar(0x16);  //tuning 100
            DELAY_1MS;
          }
          if(ucOnschk==7 || ucOnschk==9){
            funPutChar(0x19);  //tuning end
            ucOnschk=0;
            ucRegsDir=IDLE;
           // ucOrgpt=ucOrgpt+2;
           // TWI_data(ucOrgpt);

            frequency();
            ucFreqOld=ucFreqNew;
            funEEprom_Write(ORGIN_REGADDR,ucOrgpt);
            funEEprom_Write(ORGIN_FRIQADDR,ucFreqOld);
          }
          //==================
         if(ucOnschk==8){
            ucPortd=1;
            funPutChar(0x17);  //tuning lav
          }

         ucloopcnt=0;

        break;
        case INCREASE:
          ucloopcnt++;
          if(ucOrgpt<MAXOHM)ucOrgpt++;
         // ucOrgpt++;
          TWI_data(ucOrgpt);
          DELAY_100MS;DELAY_100MS;DELAY_100MS;DELAY_100MS;
          DELAY_100MS;DELAY_100MS;
          usDAC[FIRST]=ADC_DATA;

          if(usDAC[THIRD] > usDAC[FIRST]){
            ucRegsDir=INCREASE;
            usDAC[THIRD]=usDAC[FIRST];
          }else{
            ucOrgpt--;
            TWI_data(ucOrgpt);
            ucRegsDir=STOP;
          }
          if(ucloopcnt>=20)ucRegsDir=STOP;
       //   if(ucError_flag){ucRegsDir=STOP; ucOnschk=1;ucError_flag=0;}
          break;

        case DECREASE:
          ucloopcnt++;
          if(ucOrgpt>MINOHM)ucOrgpt--;
         // ucOrgpt--;
          TWI_data(ucOrgpt);
          DELAY_100MS;DELAY_100MS;DELAY_100MS;DELAY_100MS;
          DELAY_100MS;DELAY_100MS;
          usDAC[FIRST]=ADC_DATA;

          if(usDAC[THIRD] > usDAC[FIRST]){
            ucRegsDir=DECREASE;
            usDAC[THIRD]=usDAC[FIRST];
          }else{
            ucOrgpt++;
            TWI_data(ucOrgpt);

            ucRegsDir=STOP;
          }
          if(ucloopcnt>=20)ucRegsDir=STOP;
        //  if(ucError_flag){ucRegsDir=STOP; ucOnschk=1;ucError_flag=0;}
          break;
          case IDLE:
            if(t2_cnt>=1){
              t2_cnt=0;
            //  bit_set(PORTD,5);
              funFrequecy_scan();
            //  bit_clear(PORTD,5);
            }
        
            break;

        default:

          break;
        }


      }
    }
}






 //==============================================================

void spi_send_data8(unsigned char data1)
  {
   unsigned char cnt,send_data;

    SPI_CS_OFF;
    SPI_SCK_ON;
    DELAY_10US;

    for(cnt=0;cnt<=7;cnt++)
      {
          send_data=0x00;
              send_data = data1 & 0x80;
              data1 = (data1 << 0x01);
              if(send_data){SPI_DATA_ON; }else{ SPI_DATA_OFF; }
          SPI_SCK_OFF;
          DELAY_10US;
          SPI_SCK_ON;
          DELAY_10US;
      }
    SPI_CS_ON;
    SPI_SCK_ON;
    DELAY_100US;
  }

//===================
void spi_send_data(unsigned char data1,unsigned char data2)
  {
    unsigned char cnt,send_data;

    SPI_CS_OFF;
    SPI_SCK_ON;
    DELAY_10US;

    for(cnt=0;cnt<=15;cnt++)
      {
          send_data=0x00;

          if(cnt <= 0x07){
              send_data = data1 & 0x80;
              if(send_data){ SPI_DATA_ON; }else{ SPI_DATA_OFF; }
              data1 = (data1 << 0x01);
          }else{
              send_data = data2 & 0x80;
              if(send_data){ SPI_DATA_ON; }else{ SPI_DATA_OFF; }
              data2 = (data2 << 0x01);
          }
          SPI_SCK_OFF;
          DELAY_10US;
          SPI_SCK_ON;
          DELAY_10US;
      }

    SPI_CS_ON;
    SPI_SCK_ON;
    DELAY_100US;
  }

//252 - 0.45k
//251 - 0.53k
//250 - 0.61k
//249 - 0.69k
//248 - 0.77k
//247 - 0.85k
//246 - 0.93k
//245 - 1.01k
//244 - 1.09k
//243 - 1.17k
//242 - 1.21k
//241 - 1.29k
//240 - 1.37k
//239 - 1.45k
//238 - 1.53k
