unsigned char ocrdata_cnt,PWM_STEP;

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
#define MIN_PWM      25

//======================================
void pl_display()
{
   char input;
   char ocrdata;
   input = ad_data[0]/10;
   if (input > 90) input = 90;
   ocrdata = (unsigned)input*(MAX_PWM-MIN_PWM)/90 + MIN_PWM;
   if(sonic_state){//sonic_state
     if(ocrdata_cnt < ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt+PWM_STEP;    //17.12
        OCR2 = ocrdata_cnt;                   //17.12
     }
     if(ocrdata_cnt > ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt-PWM_STEP;    //17.12
        OCR2 = ocrdata_cnt;                   //17.12
     }
   }else{
      OCR2 = 0;
      ocrdata_cnt=0;
   }
   if(RunKeyTest==0){
   input = input+10;
   lcd_locate(4, 1);
   lcd_data_mode();
   lcd_disp_data(input/100+'0');
   input = input%100;
   lcd_disp_data(input/10+'0');
   lcd_disp_data(input%10+'0');
   }
}

void pl_disp_te(void)
{
     char input,ocrdata;
     
     input = ad_data[0]/10;
   if (input > 90) input = 90;
   input = ad_data[0]/10;
   if (input > 90) input = 90;
   ocrdata = (unsigned)input*(MAX_PWM-MIN_PWM)/90 + MIN_PWM;
   if(!RemoteKeyTest){//sonic_state
     if(ocrdata_cnt < ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt+PWM_STEP;    //17.12
        OCR2 = ocrdata_cnt;                   //17.12
     }
     if(ocrdata_cnt > ocrdata){            //17.12
        ocrdata_cnt=ocrdata_cnt-PWM_STEP;    //17.12
        OCR2 = ocrdata_cnt;                   //17.12
     }
   }else{
      OCR2 = 0;
      ocrdata_cnt=0;
   }
   input = input+10;
   lcd_locate(4, 1);
   lcd_data_mode();
   lcd_disp_data(input/100+'0');
   input = input%100;
   lcd_disp_data(input/10+'0');
   lcd_disp_data(input%10+'0');
}



