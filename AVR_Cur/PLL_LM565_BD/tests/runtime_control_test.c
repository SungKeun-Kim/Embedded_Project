#include <assert.h>
#include <stdio.h>
#include "../Atmega8_LM565_BD/runtime_control.c"

static runtime_control r;
static unsigned long simulation_tick;
static unsigned char lowest, saw_trial, fault;

static void feed(unsigned short adc, unsigned char running, unsigned char permit)
{
   unsigned short frequency = (unsigned short)(640 - 2 * (233 - r.code));
   fault = runtime_sample(&r,adc,frequency,1,running,permit);
   assert(!fault);
   if (r.code < lowest) lowest = r.code;
   if (r.phase == RT_SETTLING || r.phase == RT_OBSERVING) saw_trial = 1;
   ++simulation_tick;
}

static void flat(unsigned short adc,unsigned short ticks,unsigned char running,unsigned char permit)
{
   unsigned short i;
   for (i=0;i<ticks;++i) feed(adc,running,permit);
}

static void reset(void)
{
   runtime_init(&r,233);
   simulation_tick = 0; lowest = 233; saw_trial = 0;
   flat(300,RT_WINDOW_TICKS*2,0,0);
   assert(r.standby_valid);
}

static void cycle(unsigned short peak,unsigned short period,unsigned short ticks)
{
   unsigned short i;
   for (i=0;i<ticks;++i)
      feed(simulation_tick % period < (unsigned short)(period/2) ? peak : 330,1,1);
}

static void idle_tracking(void)
{
   unsigned short i, adc;
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   assert(r.baseline_valid == RT_IDLE && r.code == 233);
   flat(309,RT_WINDOW_TICKS*3,1,1); /* 명목 0.2A 미만 */
   assert(!saw_trial && r.code == 233);
   for (i=0;i<12000;++i) {
      adc = r.code == 233 ? 312 : r.code == 232 ? 310 : r.code == 231 ? 308 : 310;
      feed(adc,1,1);
   }
   assert(saw_trial && lowest == 230 && r.code == 231 && r.good_code == 231);
   /* 초기 +1 보정과 별개: 운전 중에는 증가 직전 위치로만 복귀한다. */
   flat(300,RT_WINDOW_TICKS*2,0,0);
   assert(r.code == 231);
   runtime_init(&r,233); /* 재부팅 때 EEPROM 기준으로 복원 */
   assert(r.code == 233);
}

static void continuous_tracking(void)
{
   unsigned short i, peak;
   reset();
   cycle(360,100,3000);
   assert(r.mode == RT_CONTINUOUS && r.baseline_valid == RT_CONTINUOUS);
   assert(r.code == 233);
   for (i=0;i<16000;++i) {
      peak = r.code == 233 ? 366 : r.code == 232 ? 364 : r.code == 231 ? 362 : 364;
      feed(simulation_tick % 100 < 50 ? peak : 330,1,1);
   }
   assert(saw_trial && lowest == 230 && r.code == 231);
}

static void press_and_unknown(void)
{
   reset();
   cycle(360,450,4000);
   assert(r.mode == RT_PRESS);
   cycle(375,450,4000);
   assert(!saw_trial && r.code == 233);
   reset();
   cycle(360,290,3000);
   cycle(380,290,3000);
   assert(r.mode == RT_UNKNOWN && !saw_trial);
   reset();
   flat(360,RT_WINDOW_TICKS*6,1,1); /* 높은 고정 전류를 무부하로 분류하지 않는다. */
   assert(!r.baseline_valid && !saw_trial);
}

static void interruptions_and_limits(void)
{
   unsigned short i;
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   for(i=0;i<3000 && r.code==233;++i) feed(312,1,1);
   assert(r.code==232 && r.good_code==233);
   feed(312,1,0); /* PWM 변경/통신 허가 만료: 미확인 한 칸 즉시 복귀 */
   assert(r.code==233 && !r.baseline_valid);
   flat(320,3000,1,0);
   assert(r.code==233);
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   for(i=0;i<3000 && r.code==233;++i) feed(312,1,1);
   assert(r.code==232);
   assert(runtime_sample(&r,312,700,1,1,1));
   assert(r.code==233);
   assert(runtime_sample(&r,0,640,1,1,1));
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   for(i=0;i<3000 && r.code==233;++i) feed(312,1,1);
   assert(!runtime_sample(&r,312,640,10,1,1)); /* 누락 샘플: 비교 폐기 */
   assert(r.code==233 && !r.baseline_valid);
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   for(i=0;i<14000;++i) feed((unsigned short)(312-2*(233-r.code)),1,1);
   assert(r.code==230 && lowest==230); /* 최대 3칸 제한 */
}

static void long_operation(void)
{
   unsigned long i;
   reset();
   for(i=0;i<150000UL;++i) feed(307,1,1);
   assert(r.code==233 && !saw_trial); /* 16비트 타이머 한 바퀴보다 오래 관측 */
}

static void noise_and_changed_work(void)
{
   unsigned short i;
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   for(i=0;i<6000;++i) feed((unsigned short)(307+(i%3)-1),1,1);
   assert(!saw_trial);
   reset();
   cycle(360,100,3000);
   /* 반복 속도가 달라졌으면 상승한 전류를 이전 작업 기준과 비교하지 않는다. */
   cycle(370,170,5000);
   assert(!saw_trial && r.code==233);
   reset();
   flat(307,RT_WINDOW_TICKS*3,1,1);
   for(i=0;i<3000 && r.code==233;++i) feed(312,1,1);
   assert(r.code==232);
   flat(330,RT_SETTLE_TICKS+50,1,1);
   assert(r.code==233); /* 큰 악화는 3초가 끝나기 전에 복귀 */
}

int main(void)
{
   idle_tracking(); continuous_tracking(); press_and_unknown();
   interruptions_and_limits(); long_operation(); noise_and_changed_work();
   printf("PASS: runtime patterns, 0.2A nominal trigger, lower-first, rollback, limits; state %u bytes (host)\n",(unsigned)sizeof(r));
   return 0;
}
