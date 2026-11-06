
__flash unsigned sweep_freq_div[20] = {
      4320,  // 0   20 Hz
      2880,  // 1   30 Hz
      2160,  // 2   40 Hz
      1728,  // 3   50 Hz
      1440,  // 4   60 Hz
      1234,  // 5   70 Hz
      1080,  // 6   80 Hz
      960,   // 7   90 Hz
      864,   // 8  100 Hz
      785,   // 9  110 Hz
      720,   // 10 120 Hz
      665,   // 11 130 Hz
      617,   // 12 140 Hz
      576,   // 13 150 Hz
      540,   // 14 160 Hz
      508,   // 15 170 Hz
      480,   // 16 180 Hz
      455,   // 17 190 Hz
      432,   // 18 200 Hz
      411,   // 19 210 Hz
   };

void change_sweep(void)
{
   char sweep_index;

   sweep_index = (ucPL_H-'0')*10+(ucPL_L-'0');
   OCR1A = sweep_freq_div[sweep_index];
}

//전면 키로 상태값을 변경 설정 하는 부분 runoff일때만 실행//
void KeyTime_pross()
{
   unsigned char uckey;             //전면 키 입력 및 조작 상태 처리부분 //

   uckey = PINB & 0xf0;             //제어키 상태 처리//
   if ((uckey!=0xf0) && KeychkFlag()) {     //키가 눌리려졌고 내부를 한번도 실행 하지 않았을때 실행//
      if(uckey== TimeSet){             //timer set 입력 일때../
         //ucTimerSetFlag = ~ucTimerSetFlag;    //timeset  변수 토클 //
         ucTimerSetFlag++;
         switch (ucTimerSetFlag) {    //key setting 시 키 메뉴 기능 추가 부분 으로 수정 추가  됨  //
            case 1:
               //S,M,FULL 를 read form eeprom
               gucLin2 = eep_read_byte(ADDR_gucLin2);

               Oldlin2 = gucLin2;   //위줄에서 출력 하려는 값으로 전달//
               ucLin1=2;

               display(ucLin1,gucLin2);
               if(gucLin2!=0x03) display_time();   //Full 일때는 출력 하지 않음//

               ucTimeData1 = OrgTimeData1;   // setting 값 재 설정
               ucTimeData0 = OrgTimeData0;   // setting 값 재설정
               break;
            case 2:     //SWEEP SET L/V 표시 부분 //
               gucLin2= 7;    //sweep out
               ucLin1=2;      //sweep out
               display(ucLin1,gucLin2);   //display

               ucPL_H = ucOrgPL_H;  //time copy
               ucPL_L = ucOrgPL_L;

               TimeDis_sweep();
               break;

            default :
               ucLin1=1;         //
               gucLin2=4;        //
               display(ucLin1,gucLin2);

               ucTimeData1 = OrgTimeData1;      // setting 값 재 설정
               ucTimeData0 = OrgTimeData0;      // setting 값 재설정

               ucPL_H = ucOrgPL_H;  //time copy
               ucPL_L = ucOrgPL_L;

               eep_write_byte(ADDR_OrgTimeData0, OrgTimeData0);
               eep_write_byte(ADDR_OrgTimeData1, OrgTimeData1);
               eep_write_byte(ADDR_ucOrgPL_H, ucOrgPL_H);
               eep_write_byte(ADDR_ucOrgPL_L, ucOrgPL_L);

               TimeSetDis();     // 설정한 시간을 표시하는 부분 //윗 부분에 표시하는 부분 //
               ucTimerSetFlag=0;
               break;
         }
      }

      //---------------------------------
      if(ucTimerSetFlag==1){  //080616 추가
       // time 단위  선택 /m,s,full/..
         if(ucTimerSetFlag && (uckey == TimeStSp)){// 출력 변경 //
            ucLin1=2;
            gucLin2++;
            if(gucLin2 >= 0x04) gucLin2=1;

            eep_write_byte(ADDR_gucLin2, gucLin2);

            Oldlin2 = gucLin2;         //3개의 메뉴에서 1개를 선택 할때 상태값저장 //
                                 //oldlin은 출력 값을 저장 하기위한 변수 //
            display(ucLin1,gucLin2);      // lCD 출력 함수//
            if(gucLin2!=0x03)display_time();   // 시간 표시 함수 3은 full 일때로 시간을 표시하지않음//
         }
         if(gucLin2!=0x03){               // up/down키가 1,2메뉴에서만사용하게...//
            if(ucTimerSetFlag && (uckey == TimeUp)){
               ucTimeData0++;
               if (ucTimeData0 > '9') {
                  ucTimeData0='0';
                  ucTimeData1++;
                  if (ucTimeData1 > '9') ucTimeData1='0';
               }
               display_time();
               OrgTimeData0 = ucTimeData0;
               OrgTimeData1 = ucTimeData1;
            }

            if(ucTimerSetFlag && (uckey == TimeDn)){
               ucTimeData0--;
               if (ucTimeData0 < '0') {
                  ucTimeData0='9';
                  ucTimeData1--;
                  if (ucTimeData1 < '0') ucTimeData1='9';
               }
               display_time();
               OrgTimeData0 = ucTimeData0;
               OrgTimeData1 = ucTimeData1;
            }
         }
      }
      //--------------------------------------------
      // sweep 일때  설정 변경 // switch 의 2 에서 실행
      if (ucTimerSetFlag==2) {  // 08,06,16 추가
         if (gucLin2!=0x03) {               // up/down키가 1,2메뉴에서만사용하게...//
            if (ucTimerSetFlag && (uckey == TimeUp)) {
               ucPL_L++;
               if (ucPL_L > '9') {
                  ucPL_L= '0';
                  ucPL_H++;
                  if (ucPL_H > '1') ucPL_H = '0';
               }
               TimeDis_sweep();     //sweep 위치에 시간 표시
               ucOrgPL_H = ucPL_H;
               ucOrgPL_L = ucPL_L;
               change_sweep();
            }

            if(ucTimerSetFlag && (uckey == TimeDn)){
               ucPL_L--;
               if (ucPL_L <'0') {
                  ucPL_H--;
                  ucPL_L = '9';
                  if (ucPL_H< '0') ucPL_H = '1';
               }
               TimeDis_sweep();
               ucOrgPL_H = ucPL_H;
               ucOrgPL_L = ucPL_L;
               change_sweep();
            }
         }
      }

      KeychkFlagOff;        //키를 한번만 처리 할때..//
   }

   //-------------------------------
   // key를 누르고 있는 상태 확인 //
   if(KeychkFlag()==0 ){
      KeyCnt++;
      if(KeyCnt >=10){
         KeyCnt=10;
         KeychkFlagOn;       //키 한번만 처리하는 것을 여러번 하려고 할때//..
      }
   }

   //-----------------------------------------------------------------------
   if(uckey==0xf0){               //키가 아무것도 눌리지 않았을때//
      KeychkFlagOn;        //KeychkFlag=1;
      KeyCnt=0;
   }
}

#define SONIC_ON     1
#define SONIC_OFF    0



//===============================================
void sonic_on()
{
   sonic_state = SONIC_ON;
   SONIC_OUTPUT = 1;        //초음파 ON//
   ucPortdFlag = 0x10;        // 동작중임을 알리기 위함 변수//
   ucT0_min=60;
   t0_cnt=90;
   led_on();         //led on test
}

char sweep_on;

#define SWEEP_BUTTON    PINA_Bit3
#define SWEEP_PULSE     PORTD_Bit5

//====================
void sonic_off(void)
{
   sonic_state = SONIC_OFF;
   SONIC_OUTPUT = 0;        //초음파 0ff//
   ucPortdFlag = 0x00;
   ucT0_min=0;
   t0_cnt=0;
   ucTimeData1 = OrgTimeData1;   // setting 값 재 설정
   ucTimeData0 = OrgTimeData0;   // setting 값 재설정
}

void funStSpKey_chk()      // run 일때 start/stop key 에 따른 15번키 제어 //
{
   byte Inkey,jnKey,pd;

   //-----------------------------------
   // start,stop key를 확인하고 처리하는 부분 //
   Inkey = PINB & 0xf0;
   if((Inkey==TimeStSp)&&(TimeStSpFlag)){
      pd = ucPortdFlag;             //stop,start 확인
      if(!pd){
         sonic_on();
         buzzer_off();
         ErrorFlag=0;
      }
      else {
         sonic_off();
         buzzer_off();
         ErrorFlag=0;
      }
      ucFinish=1;          //add 12.13
      //
      ucgErrorDelay=0;
      ucLin1=3;         //출력할 내용 변수값 //run on...
      gucLin2=4;        //출력할 내용 변수값 //
      display(ucLin1,gucLin2);   //LCD 출력 함수 호출 //
      //
      TimeStSpFlag = 0;       //stsp key의 상태값 저방 변수//
      TimeRemoteFlag = 0;        //   "              "      //
   }

   //------------------
   //리모트 키 확인 부분 //
   jnKey = PINB & 0x02;
   if((!jnKey)&&(TimeRemoteFlag)){
      pd = ucPortdFlag;             //stop,start 확인
      if(!pd){
         sonic_on();
         buzzer_off();
         ErrorFlag=0;
      }else{
         sonic_off();
         buzzer_off();
         ErrorFlag=0;
      }
      ucFinish=1;
      //5,12
      ucgErrorDelay=0;
      ucLin1=3;         //출력할 내용 변수값 //run on...
      gucLin2=4;        //출력할 내용 변수값 //
      display(ucLin1,gucLin2);
      //-----
      TimeStSpFlag = 0;
      TimeRemoteFlag = 0;

      //BuzzerOff;  //08,06,12
   }

   if((Inkey==0xf0) && (jnKey==2)){    //앞키들이 off이고 리모트 키도 off 확인 하기 위한 부분//
      TimeStSpFlag = 1;
      TimeRemoteFlag = 1;
   }
}

//=============================================
//(P0.2)ad2 input 14pin out

#define sweep_button_pressed()   (SWEEP_BUTTON == 0)

void sweep_off(void)
{
   sweep_on = 0;
   sweep_out_off();
   stop_timer1();
   if (PIND_Bit5) force_timer1_compare(T1_FOC1A);
}

void sweep_process()
{
   if (sweep_button_pressed()){    //20 ~ 100 hz 아날로그 에따른 출력 //
      if (!sweep_on) {
         sweep_on = 1;
         sweep_out_on();
         change_sweep();
         start_timer1();
      }
   }
   else if (sweep_on) {
      sweep_off();
   }
}

