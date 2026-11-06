#include <iom16.h>


void timer_process()
{
   if(t0_cnt>=1000) {                        //sec count
      t0_cnt=0;   // sec clear..
      ucT0_min++; // minute count

      if(Oldlin2==3 || Oldlin2==2) ucT0_min=0;  //평상시에는 t0...변수 값을 초기화하려고 //

      //======================================================================
      if(Oldlin2==2 && ucPortdFlag==0x10){      //"S Select"

         if(ucTimeData1==AsciZero && ucTimeData0==AsciZero){//시간이 완료 되면 이부분 실행//
            finish_sonic();
         }else{
            ucTimeData0--;    //설정 시간 감소 //
         }

         if(ucTimeData0==0x2f){
            if(ucTimeData1>=0x31)
            {
               ucTimeData1--;
               ucTimeData0=0x39;
            }
         }
      }

      if(ucPortdFlag==0x10)         //초음파 동작 중인 표시 "10동작" "0중지"
      {
         if(bit_test(PIND,1)){

         }else{
            PORTD |= 0x02;    //led off
         }
      }

      //===============================================================
      if(ucT0_min>=59){                      //1 MINUTE
         ucT0_min=0;
         if(Oldlin2==1 && ucPortdFlag==0x10){      //"M Select"
            if(ucTimeData1==AsciZero && ucTimeData0==AsciZero){      //00이 되면 처리부분//
               finish_sonic();
            }else{
               ucTimeData0--;
            }
            if(ucTimeData0==0x2f){           //분 단위 계산 //
               if(ucTimeData1>=Ascione)      //십분 단위 값 보고 하위분 값 설정//
               {
                  ucTimeData1--;
                  ucTimeData0=AsciNine;
               }
            }
         }
      }

   }
}
