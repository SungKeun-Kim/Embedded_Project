
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
#define SONIC_ON     1
#define SONIC_OFF    0



//===============================================
void sonic_on()
{
   sonic_state = SONIC_ON;
   SONIC_OUTPUT = 1;        //초음파 ON// 2020.4
   ucPortdFlag = 0x10;        // 동작중임을 알리기 위함 변수//
}


//====================
void sonic_off(void)
{
   sonic_state = SONIC_OFF;
//   SONIC_OUTPUT = 0;        //초음파 0ff   //202008
   ucPortdFlag = 0x00;
   ucT0_min=0;
 //  t0_cnt=0;
}


//===========================================
unsigned char auto_tun_flag;

void funStSpKey_chk()      //
{
   //
   if(autotunkeyTest==0)
   {
//       if(!RemoteKeyTest){
       if(auto_tun_flag){
         if(ucPortdFlag==0){
         if(tick == 1)uctuning_cut++;
         if(uctuning_cut>=10){
           if(SELKeyTest){              //
               funPutChar(0x17);        //11-> 17 단계 별 지냉 삭제 로 변경 //
               auto_tun_flag=0;         //tuning
           }else{
               funPutChar(0x17);
               auto_tun_flag=0;         //tuning
           }
           uctuning_cut=0;
         }
//       }
         }
       }
   }else{
       if(auto_tun_flag==0){
           if(SELKeyTest){
               PWM_STEP_CNT++;              //단계별로 진행 을 활성화 하려면 여기를 지우고 위 를 17을 11로
               if(PWM_STEP_CNT>=255){       //
                 pl_disp_te();              //
                 PWM_STEP_CNT=0;            //
               }                            //
           }else{
               PWM_STEP_CNT++;
               if(PWM_STEP_CNT>=255){
                 pl_disp_te();
                 PWM_STEP_CNT=0;
               }
           }
       }
       uctuning_cut=0;
   }
}

//===========================================
void funKey_scan()      //run key //
{

  if(auto_tun_flag){//operation mode
   funRemoteKey_prog();
  }
   funStSpKey_chk(); //tuning mode start check

//---------------------------------------------------------------
  if(auto_tun_flag){  //operation mode
   if(RemoteSele){     //remote key status
     if(SELKeyTest){

         ucLin1=5;     //"pulse" display
         display_Line(ucLin1);
         PWM_STEP=3;      //   5->>3 pulse 20200828
     }else{
         ucLin1=6;     //"cont." display
         display_Line(ucLin1);
         PWM_STEP=1;       //202007
     }
    }
   }
}




//=============================================
//(P0.2)ad2 input 14pin out
/*
#define sweep_button_pressed()   (SWEEP_BUTTON == 0)

void sweep_off(void)
{
   sweep_on = 0;
   sweep_out_off();
   stop_timer1();
   if (PIND_Bit5) force_timer1_compare(T1_FOC1A);
}
*/
/*
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
*/

/*
void change_sweep(void)
{
   char sweep_index;

   sweep_index = (ucPL_H-'0')*10+(ucPL_L-'0');
   OCR1A = sweep_freq_div[sweep_index];
}

*/
//전면 키로 상태값을 변경 설정 하는 부분 runoff일때만 실행//
/*
void KeyTime_pross()
{
   unsigned char uckey;             //전면 키 입력 및 조작 상태 처리부분 //

   uckey = PINB & 0xf0;             //제어키 상태 처리//
   if ((uckey!=0xf0) && KeychkFlag) {     //키가 눌리려졌고 내부를 한번도 실행 하지 않았을때 실행//
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


      KeychkFlagOff;        //키를 한번만 처리 할때..//
   }

   //-------------------------------
   // key를 누르고 있는 상태 확인 //
   if(KeychkFlag==0 ){
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


*/
