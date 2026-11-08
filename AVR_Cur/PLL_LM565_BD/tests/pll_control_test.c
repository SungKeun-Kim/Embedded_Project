#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "../Atmega8_LM565_BD/phase_search.h"
#include "../Atmega8_LM565_BD/tuning_store.h"

static unsigned char ee[512];
static int writes, drop_writes, cut_after = -1, tear_value = -1;
static int commit_allowed = 1, service_ok = 1;
static unsigned char ucError_flag;
static unsigned char ucOrgpt,pll_active,ucRUNFlag,operation_active,control_fault,settings_valid;
static unsigned char active_code,tune_prepared,tune_restore_code;
static unsigned char timeout_fault,timeout_reset_ack,timeout_reset_capable;
static unsigned short fake_ticks,measurement_ticks;
#include "../Atmega8_LM565_BD/tuning_deadline.c"
static jmp_buf power_cut;
static unsigned char service_control(void) {
   return (unsigned char)(service_ok && !ucError_flag &&
      (!(operation_active || tune_prepared) || tune_clock_ok(fake_ticks)));
}
unsigned char funEEprom_Read(unsigned short a) { assert(a < 512); return ee[a]; }
void funEEprom_Write(unsigned short a, unsigned char v)
{
   assert(a < 512);
   ++writes;
   if (!drop_writes) ee[a] = v;
   if (writes == cut_after) {
      if (tear_value >= 0) ee[a] = (unsigned char)tear_value;
      longjmp(power_cut, 1);
   }
}
#define TUNING_STORE_CAN_COMMIT() (commit_allowed && service_control())
#include "../Atmega8_LM565_BD/tuning_store.c"
#include "../Atmega8_LM565_BD/phase_search.c"

static int optimum = 237, measured, fail_at, curve_mode, shift_after, retry_until;
static unsigned char measured_code;
static unsigned char measure_code(unsigned char code, unsigned short *current, unsigned short *frequency)
{
   int d = code - optimum;
   unsigned short i;
   if (!service_control()) return PHASE_MEASURE_FAILED;
   assert(code >= PLL_POT_MIN && code <= PLL_POT_MAX);
   measured_code = code;
   active_code = code;
   ++measured;
   for (i=0; i<measurement_ticks; ++i) {
      ++fake_ticks;
      if (!service_control()) return PHASE_MEASURE_FAILED;
   }
   assert(measured < 2000);
   if (fail_at && measured == fail_at) return 0;
   if (measured <= retry_until) return PHASE_MEASURE_RETRY;
   if (curve_mode == 4 && measured <= shift_after) d = code - (optimum-1);
   /* 넓은 코드 범위에서도 실제 ADC 32개 합계 범위 안의 단봉 곡선을 만든다. */
   if (d < 0) d = -d;
   *current = (unsigned short)(6400 + (d <= 8 ? d*d*32 : 8*8*32 + (d-8)*256));
   *frequency = (unsigned short)(400 + code);
   if (curve_mode == 1) *current = 6400; /* 연결 오류/분해능 부족에 해당하는 평탄 응답 */
   if (curve_mode == 2 && measured > shift_after) *current += 128; /* 압력 변화 */
   if (curve_mode == 3 && measured > shift_after) *frequency += 4;
   return 1;
}

static unsigned short ucFreqOld,progress_at;
static unsigned char sent[32], sent_count;
static unsigned short now_ticks(void) { return fake_ticks; }
static unsigned char apply_code(unsigned char code) { measured_code = active_code = code; return 1; }
void funPutChar(unsigned char value)
{
   unsigned char code;
   unsigned short count;
   assert(sent_count < sizeof(sent));
   sent[sent_count++] = value;
   if (value == 0x19) {
      assert(tuning_store_load(&code, &count) == 2);
      assert(code == ucOrgpt && code == measured_code);
      assert(count == ucFreqOld && count == 400 + code);
   }
}
#include "../Atmega8_LM565_BD/control_sequence.c"

static void reset_test(void)
{
   memset(ee, 0xFF, sizeof(ee));
   writes = drop_writes = 0;
   cut_after = tear_value = -1;
   commit_allowed = service_ok = 1;
   measured = fail_at = curve_mode = shift_after = retry_until = 0;
   optimum = 237;
   sent_count = 0;
   ucOrgpt = 233;
   ucFreqOld = 0;
   control_fault = ucRUNFlag = ucError_flag = pll_active = operation_active = 0;
   settings_valid = 0;
   active_code = tune_restore_code = ucOrgpt;
   tune_prepared = timeout_fault = timeout_reset_ack = 0;
   timeout_reset_capable = 1;
   fake_ticks = measurement_ticks = 0;
   tune_clock_start(0);
}

static void test_store(void)
{
   unsigned char code;
   unsigned short count;
   unsigned char saved[512];
   int i;
   reset_test();
   assert(tuning_store_load(&code,&count) == 0 && code == PLL_POT_DEFAULT && count == 0);
   ee[2] = 232; ee[3] = 128; ee[255] = 0x55;
   assert(tuning_store_load(&code,&count) == 1 && code == 232 && count == 0);
   assert(tuning_store_save(234,640));
   assert(tuning_store_load(&code,&count) == 2 && code == 234 && count == 640);
   writes = 0;
   assert(tuning_store_save(234,640) && writes == 0);
   assert(!tuning_store_save(PLL_POT_MIN-1,640));
   assert(!tuning_store_save(0,640));
   assert(!tuning_store_save(233,0));
   memcpy(saved,ee,sizeof(ee));
   drop_writes = 1;
   assert(!tuning_store_save(238,644));
   assert(tuning_store_load(&code,&count) == 2 && code == 234 && count == 640);
   drop_writes = 0;
   assert(tuning_store_save(238,644));
   ee[TUNING_SLOT1 + 7] ^= 1;
   assert(tuning_store_load(&code,&count) == 2 && code == 234 && count == 640);
   memcpy(ee,saved,sizeof(ee));
   commit_allowed = 0;
   assert(!tuning_store_save(238,644));
   assert(tuning_store_load(&code,&count) == 2 && code == 234 && count == 640);
   commit_allowed = 1;
   for (i = 0; i < 600; ++i) {
      unsigned char c = (unsigned char)(230 + i % 5);
      assert(tuning_store_save(c,(unsigned short)(640+i)));
      assert(tuning_store_load(&code,&count) == 2 && code == c && count == 640+i);
   }
   for (i = 0; i < 512; ++i) {
      if (i == 2 || i == 3 || i == 255 || (i >= TUNING_SLOT0 && i < TUNING_SLOT1+8)) continue;
      assert(ee[i] == saved[i]);
   }
}

static void test_power_interruption(void)
{
   unsigned char saved[512], code;
   unsigned short count;
   int write_total;
   volatile int point, byte_value;
   reset_test();
   assert(tuning_store_save(233,640));
   assert(tuning_store_save(234,641));
   memcpy(saved,ee,sizeof(ee));
   writes = 0;
   assert(tuning_store_save(235,642));
   write_total = writes;
   for (point = 1; point <= write_total; ++point) {
      for (byte_value = -1; byte_value < 256; ++byte_value) {
         memcpy(ee,saved,sizeof(ee));
         writes = 0; cut_after = point; tear_value = byte_value;
         if (!setjmp(power_cut)) (void)tuning_store_save(235,642);
         cut_after = -1;
         assert(tuning_store_load(&code,&count) == 2);
         assert((code == 234 && count == 641) || (code == 235 && count == 642));
      }
   }
   tear_value = -1;
}

static void test_search(void)
{
   int start, best, i, successful_measurements;
   unsigned char result;
   unsigned short count;
   reset_test();
   for (best = PLL_POT_MIN+1; best < PLL_POT_MAX; ++best) {
      for (start = PLL_POT_MIN; start <= PLL_POT_MAX; ++start) {
         optimum = best; measured = 0;
         assert(phase_find_minimum((unsigned char)start,&result,&count,measure_code));
         assert(result == best+1 && measured_code == best+1 && count == 400+result);
         assert(measured <= 4*(PLL_POT_MAX-PLL_POT_MIN)+20);
      }
   }
   optimum = PLL_POT_MIN;
   assert(!phase_find_minimum(233,&result,&count,measure_code));
   optimum = PLL_POT_MAX;
   assert(!phase_find_minimum(233,&result,&count,measure_code));
   optimum = 237; measured = 0;
   assert(phase_find_minimum(233,&result,&count,measure_code));
   successful_measurements = measured;
   for (i = 1; i <= successful_measurements; ++i) {
      measured = 0; fail_at = i;
      assert(!phase_find_minimum(233,&result,&count,measure_code));
   }
   measured = 0; fail_at = 160; curve_mode = 1;
   assert(!phase_find_minimum(233,&result,&count,measure_code));
   assert(measured == 160); /* 평탄 응답을 임의의 시간/횟수로 완료하지 않는다. */
   measured = fail_at = curve_mode = 0; retry_until = 80;
   assert(phase_find_minimum(233,&result,&count,measure_code) && result == 238);
   assert(measured > retry_until);
   measured = retry_until = 0; curve_mode = 4; shift_after = 12;
   assert(phase_find_minimum(236,&result,&count,measure_code) && result == 238);
   assert(measured > 13); /* 최종 +1 측정에서 최소점 이동을 감지하면 다시 확인한다. */
}

static void test_tracking(void)
{
   unsigned char result;
   reset_test();
   assert(phase_track_once(233,230,236,1,&result,measure_code) && result == 234);
   measured = 0;
   assert(phase_track_once(233,230,236,-1,&result,measure_code) && result == 233);
   measured = 0; curve_mode = 2; shift_after = 1;
   assert(!phase_track_once(233,230,236,1,&result,measure_code) && result == 233);
   measured = 0; curve_mode = 3; shift_after = 2;
   assert(!phase_track_once(233,230,236,1,&result,measure_code) && result == 233);
   measured = 0;
   assert(phase_track_once(236,230,236,1,&result,measure_code) && result == 236 && measured == 0);
   assert(!phase_track_once(233,PLL_POT_MIN-1,236,1,&result,measure_code));
}

static void test_initial_and_fullscale(void)
{
   unsigned char code;
   unsigned short count;
   reset_test();
   ee[2] = 220; ee[3] = 125; ee[255] = 0x55;
   assert(tuning_store_load(&code,&count) == 1 && code == 220 && count == 0);
   ee[3] = 1; /* 구형 참고값은 현재 주파수로 해석하지 않는다. */
   assert(tuning_store_load(&code,&count) == 1 && code == 220 && count == 0);
   ee[255] = 0xFF;
   assert(tuning_store_load(&code,&count) == 0 && code == 220);
   assert(tuning_store_save(255,640));
   assert(tuning_store_load(&code,&count) == 2 && code == 255 && count == 640);
   ee[2] = 220; ee[255] = 0x55;
   assert(tuning_store_load(&code,&count) == 2 && code == 255); /* 새 기록이 구형 사본보다 우선 */
   assert(tuning_store_save(185,640));
   assert(tuning_store_load(&code,&count) == 2 && code == 185);
   reset_test(); optimum = 254;
   assert(phase_find_minimum(255,&code,&count,measure_code));
   assert(code == 255); /* 254 최소점 +1. 0으로 넘치지 않는다. */
   measured = 0;
   assert(phase_track_once(255,185,255,1,&code,measure_code) && code == 255 && measured == 0);
   assert(phase_track_once(185,185,255,-1,&code,measure_code) && code == 185 && measured == 0);
}

static void test_control_sequence(void)
{
   unsigned char saved[512], code;
   unsigned short count;
   reset_test();
   assert(tuning_store_save(233,638));
   manual_tune(0x17);
   assert(sent_count == 2 && sent[0] == 0x17 && sent[1] == 0x19);
   assert(ucOrgpt == 238 && !control_fault && !operation_active && !pll_active);
   assert(settings_valid == 2);
   measured = 0; sent_count = 0; writes = 0;
   manual_tune(0x17);
   assert(ucOrgpt == 238 && writes == 0); /* 반복 튜닝으로 +1이 누적되지 않는다. */
   memcpy(saved,ee,sizeof(ee));
   optimum = 240; fail_at = 1; measured = 0; sent_count = 0;
   manual_tune(0x17);
   assert(sent[1] == 0x12 && control_fault && measured_code == 238);
   assert(memcmp(saved,ee,sizeof(ee)) == 0);
   fail_at = 0; drop_writes = 1; measured = 0; control_fault = 0; sent_count = 0;
   manual_tune(0x17);
   assert(sent[1] == 0x12 && control_fault && ucOrgpt == 238);
   assert(tuning_store_load(&code,&count) == 2 && code == 238 && count == 638);
   drop_writes = 0; control_fault = 0; sent_count = 0; measured = 0; writes = 0;
   ucError_flag = 1; /* 출력 준비 직후 발생한 과전류를 탐색 진입 시 지우면 안 된다. */
   manual_tune(0x17);
   assert(ucError_flag && control_fault && measured == 0 && writes == 0);
   assert(sent[1] == 0x12 && measured_code == 238);
}

static void test_tuning_deadline(void)
{
   unsigned char saved[512];
   unsigned short begin;
   reset_test();
   assert(tuning_store_save(233,640));
   memcpy(saved,ee,sizeof(ee));
   writes = 0;
   active_code = 231; /* 운전 보정값과 EEPROM 초기값이 다른 경우 */
   curve_mode = 1; /* 진동자 미연결처럼 평탄한 응답 */
   measurement_ticks = 192;
   fake_ticks = begin = 60000; /* 탐색 중 16비트 타이머 순환 */
   manual_tune(0x17);
   assert((unsigned short)(fake_ticks-begin) == PLL_TUNE_LIMIT_TICKS);
   assert(tune_expired && control_fault && !operation_active && !pll_active);
   assert(active_code == 231 && ucOrgpt == 233 && writes == 0);
   assert(memcmp(saved,ee,sizeof(ee)) == 0 && sent[1] == ATMEGA8_TUNE_TIMEOUT && timeout_fault);
   assert(!tune_clock_ok(begin)); /* 한 바퀴 더 돌아도 만료 상태를 해제하지 않는다. */

   reset_test();
   assert(tuning_store_save(233,640));
   memcpy(saved,ee,sizeof(ee)); writes = 0;
   tune_prepared = 1;
   tune_restore_code = active_code = 232;
   tune_clock_start(0);
   fake_ticks = PLL_TUNE_LIMIT_TICKS - PLL_TUNE_SAVE_GUARD_TICKS;
   manual_tune(0x17); /* 최소점을 찾았어도 저장 완료 시간 부족이면 저장하지 않는다. */
   assert(control_fault && writes == 0 && active_code == 232 && ucOrgpt == 233);
   assert(timeout_fault && sent[1] == ATMEGA8_TUNE_TIMEOUT);
   assert(memcmp(saved,ee,sizeof(ee)) == 0);

   reset_test();
   timeout_reset_capable = 0; curve_mode = 1; measurement_ticks = 192;
   manual_tune(0x17);
   assert(timeout_fault && sent[1] == 0x12); /* old panel must still stop output */

   reset_test();
   measurement_ticks = 192;
   manual_tune(0x17);
   assert(!control_fault && sent[1] == 0x19 && ucOrgpt == 238);
   assert(fake_ticks < PLL_TUNE_LIMIT_TICKS);

   reset_test();
   ucOrgpt = active_code = 220;
   assert(tuning_store_save(220,640));
   memcpy(saved,ee,sizeof(ee)); writes = 0;
   optimum = 250; measurement_ticks = 192;
   manual_tune(0x17); /* 넓힌 범위에서도 1분 제한을 우회하지 않는다. */
   assert(timeout_fault && control_fault && active_code == 220 && writes == 0);
   assert(memcmp(saved,ee,sizeof(ee)) == 0);
}

int main(void)
{
   test_store();
   test_power_interruption();
   test_search();
   test_tracking();
   test_initial_and_fullscale();
   test_control_sequence();
   test_tuning_deadline();
   puts("PASS: tuning, tracking guards, save/restore, torn writes, completion order");
   return 0;
}
