unsigned char ocrdata_cnt,PWM_STEP,PWM_STEP_CNT,DISP_RUN;

void init_adc()
{
   ADCSRA = 0x85;   //adc enable ,ck/32=345.6khz
// ADMUX = 0x00;
}

#define ADC_START    ADCSRA_Bit6
#define ADC_EOC      ADCSRA_Bit4

#define ADMUX_MASK   0x1f

#define adc_start()     ADC_START = 1
#define adc_complete()  ADC_EOC

void adc_process(char chan)
{
// byte i;
   //REFS1 REFS0 ADLAR MUX4 MUX3 MUX2 MUX1 MUX0 = ADMUX//
   ADMUX = (ADMUX & ~ADMUX_MASK) | (chan & ADMUX_MASK);  //ad pin select ..
   adc_start();      //start flag set
   while(!adc_complete());//AD 완료 확인 //

   ad_data[chan] = ADC;
}

unsigned char sonic_state;

#define MAX_PWM      253         // original = 153
#define MIN_PWM      80         // 25-> 80



void pl_disp_te(void)
{
     char input,ocrdata,ucOCR2;

   if(auto_tun_flag==0){ // tuning mode
       PWM_STEP=1;       //202007
     input = ad_data[0]/10;
     if (input > 90) input = 90;
     input = ad_data[0]/10;
     if (input > 90) input = 90;
     ocrdata = (unsigned)input*(MAX_PWM-MIN_PWM)/90 + 77;//MIN_PWM;

     if(ocrdata_cnt < ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt+PWM_STEP;    //17.12
        ucOCR2 = ocrdata_cnt;                   //17.12
        OCR2=ucOCR2;
 
     }
     if(ocrdata_cnt > ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt-PWM_STEP;    //17.12
        ucOCR2 = ocrdata_cnt;                   //17.12
        OCR2=ucOCR2;
     }
        

   }else{// operation mode

   input = ad_data[0]/10;
   if (input > 90) input = 90;
   input = ad_data[0]/10;
   if (input > 90) input = 90;
   ocrdata = (unsigned)input*(MAX_PWM-MIN_PWM)/90 + 77;//MIN_PWM;
   if(!RemoteKeyTest){//sonic_state

     if(ocrdata_cnt < ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt+PWM_STEP;    //17.12
        ucOCR2 = ocrdata_cnt;                   //17.12
        OCR2=ucOCR2;
    //    OCR2=255;  //test
      }
     if(ocrdata_cnt > ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt-PWM_STEP;    //17.12
        ucOCR2 = ocrdata_cnt;                   //17.12
        OCR2=ucOCR2;
   //     OCR2=255;  //test
     }
        

   }else{
      OCR2 = ucPwmmin;
      ocrdata_cnt=77;  //0.45
   }

     input = input+10;
     if (!ready_notice_active) {
        lcd_locate(4, 1);
        lcd_data_mode();
        lcd_disp_data(input/100+'0');
        input = input%100;
        lcd_disp_data(input/10+'0');
        lcd_disp_data(input%10+'0');
     }

     }

}



