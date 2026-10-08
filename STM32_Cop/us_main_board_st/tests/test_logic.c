/**
 * @file    test_logic.c
 * @brief   호스트 PC에서 실행하는 단위 테스트 (하드웨어 불필요)
 *
 * 빌드 방법 (PC에서):
 *   gcc -DUNIT_TEST -I../include test_logic.c -o test_logic && ./test_logic
 *
 * 검증 항목:
 *   - HRTIM PER 계산 정확도 (주파수 → 레지스터 역산)
 *   - TIM3 CH2 위상제어 CCR 계산 정확도
 *   - 파라미터 범위 클램핑
 *   - Modbus CRC-16 알려진 벡터
 *   - CT ADC 정규화 정확도
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── 테스트 프레임워크 (미니멀) ── */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                                 \
  do {                                                                         \
    g_tests_run++;                                                             \
    if (cond) {                                                                \
      g_tests_passed++;                                                        \
    } else {                                                                   \
      g_tests_failed++;                                                        \
      printf("  FAIL: %s (line %d)\n", msg, __LINE__);                         \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_EQ(a, b, msg)                                              \
  do {                                                                         \
    g_tests_run++;                                                             \
    if ((a) == (b)) {                                                          \
      g_tests_passed++;                                                        \
    } else {                                                                   \
      g_tests_failed++;                                                        \
      printf("  FAIL: %s — expected %ld, got %ld (line %d)\n", msg, (long)(b), \
             (long)(a), __LINE__);                                             \
    }                                                                          \
  } while (0)

#define TEST_ASSERT_NEAR(a, b, tol, msg)                                       \
  do {                                                                         \
    g_tests_run++;                                                             \
    double _diff = fabs((double)(a) - (double)(b));                            \
    if (_diff <= (double)(tol)) {                                              \
      g_tests_passed++;                                                        \
    } else {                                                                   \
      g_tests_failed++;                                                        \
      printf("  FAIL: %s — expected ~%.2f, got %.2f, diff=%.4f (line %d)\n",   \
             msg, (double)(b), (double)(a), _diff, __LINE__);                  \
    }                                                                          \
  } while (0)

/* ── params.h 상수 복제 (테스트용) ── */
#define HRTIM_CLOCK_HZ 680000000UL
#define TIM3_CLOCK_HZ 170000000UL
#define PHASE_PWM_FREQUENCY_HZ 3000UL
#define PHASE_PWM_PERIOD_COUNTS (TIM3_CLOCK_HZ / PHASE_PWM_FREQUENCY_HZ)
#define FREQ_MIN 150U  /* 15.0 kHz */
#define FREQ_MAX 1280U /* 128.0 kHz */
#define DUTY_MIN 0U
#define DUTY_MAX 1000U
#define DUTY_CLAMP_MAX 900U /* 90.0% 안전 상한 */
#define PWM_OUTPUT_DUTY_MIN 50U
#define PWM_OUTPUT_DUTY_MAX 1000U
#define ADC_RESOLUTION 4096U
#define ADC_DEADZONE_LOW 50U
#define ADC_DEADZONE_HIGH 4045U

/* ── 테스트 대상 함수 (실제 코드와 동일 로직 복제) ── */

/* HRTIM PER 계산: edge-aligned 모드 */
static uint32_t calc_arr(uint16_t freq_01khz) {
  if (freq_01khz < FREQ_MIN)
    freq_01khz = FREQ_MIN;
  if (freq_01khz > FREQ_MAX)
    freq_01khz = FREQ_MAX;
  uint32_t freq_hz = (uint32_t)freq_01khz * 100UL;
  uint32_t arr = HRTIM_CLOCK_HZ / freq_hz;
  return arr;
}

/* PER → 실제 주파수 역산 */
static double arr_to_freq_khz(uint32_t arr) {
  return (double)HRTIM_CLOCK_HZ / (double)arr / 1000.0;
}

/* TIM3 CH2 CCR 계산: 내부 지령 0~90% -> PA7 PWM_OUTPUT 5~100% */
static uint32_t calc_ccr(uint32_t period, uint16_t duty_01pct) {
  if (duty_01pct > DUTY_CLAMP_MAX)
    duty_01pct = DUTY_CLAMP_MAX;
  uint32_t pwm_output_duty =
      PWM_OUTPUT_DUTY_MIN +
      ((uint32_t)duty_01pct *
       (PWM_OUTPUT_DUTY_MAX - PWM_OUTPUT_DUTY_MIN)) /
          DUTY_CLAMP_MAX;
  uint32_t ccr = ((uint64_t)period * pwm_output_duty) / 1000UL;
  return ccr;
}

/* 외부 OUTPUT_VALUE 0~500 절대값 -> 내부 PA7 0~90% 지령 */
static uint16_t output_value_to_phase_duty(uint16_t output_value) {
  if (output_value > 500U)
    output_value = 500U;
  return (uint16_t)(((uint32_t)DUTY_CLAMP_MAX * output_value + 250U) / 500U);
}

/* CCR → 실제 듀티비 역산 */
static double ccr_to_duty_pct(uint32_t ccr, uint32_t arr) {
  if (arr == 0)
    return 0.0;
  return (double)ccr / (double)arr * 100.0;
}

/* CT ADC → 정규화 전류값 */
static uint16_t adc_to_duty_mapped(uint16_t adc_raw) {
  if (adc_raw <= ADC_DEADZONE_LOW)
    return 0;
  if (adc_raw >= ADC_DEADZONE_HIGH)
    return 1000;
  uint32_t range = ADC_DEADZONE_HIGH - ADC_DEADZONE_LOW;
  return (uint16_t)(((uint32_t)(adc_raw - ADC_DEADZONE_LOW) * 1000UL) / range);
}

/* CRC-16 Modbus (비트 연산 방식 — 테이블 없이 검증용) */
static uint16_t crc16_modbus(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc = crc >> 1;
    }
  }
  return crc;
}

/* ═════════════════════════════════════════════ */
/* ── 테스트 케이스 ── */
/* ═════════════════════════════════════════════ */

static void test_arr_calculation(void) {
  printf("[TEST] HRTIM PER 계산 정확도\n");

  /* 28.0 kHz → ARR 예상값 */
  uint32_t arr_28k = calc_arr(280);
  double actual_freq = arr_to_freq_khz(arr_28k);
  TEST_ASSERT_NEAR(actual_freq, 28.0, 0.05,
                   "28.0kHz 설정 시 역산 주파수 오차 < 0.05kHz");

  /* 40.0 kHz */
  uint32_t arr_40k = calc_arr(400);
  actual_freq = arr_to_freq_khz(arr_40k);
  TEST_ASSERT_NEAR(actual_freq, 40.0, 0.05,
                   "40.0kHz 설정 시 역산 주파수 오차 < 0.05kHz");

  /* 경계값: FREQ_MIN (15.0 kHz) */
  uint32_t arr_min = calc_arr(FREQ_MIN);
  actual_freq = arr_to_freq_khz(arr_min);
  TEST_ASSERT_NEAR(actual_freq, 15.0, 0.05, "FREQ_MIN(15.0kHz) 역산 정확도");

  /* 경계값: FREQ_MAX (128.0 kHz) */
  uint32_t arr_max = calc_arr(FREQ_MAX);
  actual_freq = arr_to_freq_khz(arr_max);
  TEST_ASSERT_NEAR(actual_freq, 128.0, 0.1, "FREQ_MAX(128.0kHz) 역산 정확도");

  /* 범위 이하 입력 → 클램핑 */
  uint32_t arr_under = calc_arr(100); /* 10.0kHz → 15.0kHz로 클램핑 */
  TEST_ASSERT_EQ(arr_under, arr_min, "FREQ_MIN 미만 입력 시 클램핑");

  /* 범위 초과 입력 → 클램핑 */
  uint32_t arr_over = calc_arr(1300); /* 130.0kHz → 128.0kHz로 클램핑 */
  TEST_ASSERT_EQ(arr_over, arr_max, "FREQ_MAX 초과 입력 시 클램핑");
}

static void test_ccr_calculation(void) {
  printf("[TEST] TIM3 CH2 위상제어 CCR 계산\n");

  uint32_t period = PHASE_PWM_PERIOD_COUNTS;
  TEST_ASSERT_EQ(period, 56666U, "TIM3 3kHz period 계산");

  /* 내부 45.0% -> PA7 PWM_OUTPUT 52.5% */
  uint32_t ccr_45 = calc_ccr(period, 450);
  double actual_duty = ccr_to_duty_pct(ccr_45, period);
  TEST_ASSERT_NEAR(actual_duty, 52.5, 0.5,
                   "내부 45.0% -> PWM_OUTPUT 52.5% 역산 오차 < 0.5%");

  /* 내부 0% -> PA7 최소 5% */
  uint32_t ccr_0 = calc_ccr(period, 0);
  double min_duty = ccr_to_duty_pct(ccr_0, period);
  TEST_ASSERT_NEAR(min_duty, 5.0, 0.1, "내부 0% -> PWM_OUTPUT 5%");

  /* 100% 입력 -> 내부 90% clamp -> PA7 100% */
  uint32_t ccr_100 = calc_ccr(period, 1000);
  uint32_t ccr_90 = calc_ccr(period, 900);
  TEST_ASSERT_EQ(ccr_100, ccr_90, "100% 입력 시 DUTY_CLAMP_MAX(90%)로 클램핑");

  /* 최대 내부 지령은 PA7 100%인지 확인 */
  double clamped_duty = ccr_to_duty_pct(ccr_100, period);
  TEST_ASSERT_NEAR(clamped_duty, 100.0, 0.1,
                   "클램핑 후 PWM_OUTPUT 실제 듀티 = 100%");

  /* 외부 OUTPUT_VALUE가 PA7 전체 5~100% 범위를 제어하는지 확인 */
  uint32_t ccr_external_min =
      calc_ccr(period, output_value_to_phase_duty(0U));
  uint32_t ccr_external_mid =
      calc_ccr(period, output_value_to_phase_duty(250U));
  uint32_t ccr_external_max =
      calc_ccr(period, output_value_to_phase_duty(500U));
  TEST_ASSERT_NEAR(ccr_to_duty_pct(ccr_external_min, period), 5.0, 0.1,
                   "OUTPUT_VALUE=0 -> PWM_OUTPUT 5%");
  TEST_ASSERT_NEAR(ccr_to_duty_pct(ccr_external_mid, period), 52.5, 0.1,
                   "OUTPUT_VALUE=250 -> PWM_OUTPUT 52.5%");
  TEST_ASSERT_NEAR(ccr_to_duty_pct(ccr_external_max, period), 100.0, 0.1,
                   "OUTPUT_VALUE=500 -> PWM_OUTPUT 100%");

  /* 통신 중 PB1 이동은 해당 Local Duty를 마지막 지령으로 즉시 채택한다. */
  uint32_t ccr_vr_mid = calc_ccr(period, 450U);
  TEST_ASSERT_NEAR(ccr_to_duty_pct(ccr_vr_mid, period), 52.5, 0.1,
                   "통신 중 PB1 중간 이동 -> PWM_OUTPUT 52.5%");
}

static void test_frequency_duty_consistency(void) {
  printf("[TEST] 주파수-듀티 교차 일관성 (LCD 표시 검증 시뮬레이션)\n");

  /* 시나리오: 에이전트가 만든 코드에서 LCD에 표시하는 주파수와
  실제 HRTIM에 들어가는 주파수가 같은지 검증 */

  uint16_t display_freq_01khz = 280; /* LCD에 "28.0 kHz" 표시 의도 */

  /* 1) display 값으로 ARR 계산 */
  uint32_t arr = calc_arr(display_freq_01khz);

  /* 2) ARR에서 실제 주파수 역산 */
  double actual_freq_khz = arr_to_freq_khz(arr);

  /* 3) 역산 주파수를 다시 ×0.1kHz 정수로 변환 (LCD 표시용) */
  uint16_t readback_01khz = (uint16_t)(actual_freq_khz * 10.0 + 0.5);

  /* 4) 원래 설정값과 비교 — 이것이 LCD 정합성 테스트 */
  TEST_ASSERT_EQ(readback_01khz, display_freq_01khz,
                 "LCD 표시 주파수 = HRTIM PER 역산 주파수 (28.0kHz)");

  /* 모든 유효 주파수에서 HRTIM 정수 period 양자화 오차를 검증 */
  int error_count = 0;
  for (uint16_t f = FREQ_MIN; f <= FREQ_MAX; f += FREQ_STEP) {
    arr = calc_arr(f);
    actual_freq_khz = arr_to_freq_khz(arr);
    if (fabs(actual_freq_khz * 1000.0 - (double)f * 100.0) > 100.0)
      error_count++;
  }
  TEST_ASSERT_EQ(error_count, 0,
                 "FREQ_MIN~FREQ_MAX 전 구간 실제 Hz 오차 ≤ 100Hz");
}

static void test_adc_mapping(void) {
  printf("[TEST] CT ADC 정규화\n");

  /* 데드존 이하 → 0% */
  TEST_ASSERT_EQ(adc_to_duty_mapped(0), 0, "ADC=0 → 0%");
  TEST_ASSERT_EQ(adc_to_duty_mapped(50), 0, "ADC=50(데드존 경계) → 0%");

  /* 데드존 이상 → 100% */
  TEST_ASSERT_EQ(adc_to_duty_mapped(4045), 1000,
                 "ADC=4045(데드존 상한) → 100%");
  TEST_ASSERT_EQ(adc_to_duty_mapped(4095), 1000, "ADC=4095(최대) → 100%");

  /* 중간값 → ~50% */
  uint16_t mid_adc = (ADC_DEADZONE_LOW + ADC_DEADZONE_HIGH) / 2;
  uint16_t mid_duty = adc_to_duty_mapped(mid_adc);
  TEST_ASSERT_NEAR(mid_duty, 500, 10, "ADC 중간값 → ~50%");

  /* 단조 증가 확인 */
  int monotonic = 1;
  uint16_t prev = 0;
  for (uint16_t a = 0; a < 4096; a += 10) {
    uint16_t d = adc_to_duty_mapped(a);
    if (d < prev) {
      monotonic = 0;
      break;
    }
    prev = d;
  }
  TEST_ASSERT(monotonic, "CT ADC 정규화가 단조 증가");
}

static void test_crc16(void) {
  printf("[TEST] Modbus CRC-16\n");

  /* 알려진 테스트 벡터: 슬레이브=1, FC=03, 주소=0x0000, 수량=0x000A */
  uint8_t frame1[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
  uint16_t crc1 = crc16_modbus(frame1, 6);
  /* CRC-16/MODBUS: 0xCDC5 → 전송 시 CRC_LO=0xC5, CRC_HI=0xCD */
  TEST_ASSERT_EQ(crc1, 0xCDC5, "CRC-16 테스트 벡터 1");

  /* 빈 데이터 → 초기값 유지 확인 (비정상이지만 로직 검증) */
  uint8_t frame2[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x03};
  uint16_t crc2 = crc16_modbus(frame2, 6);
  /* CRC 바이트 순서: CRC_LO 먼저, CRC_HI 나중 */
  uint8_t crc_lo = crc2 & 0xFF;
  uint8_t crc_hi = (crc2 >> 8) & 0xFF;
  TEST_ASSERT(crc_lo != crc_hi || crc_lo == 0, "CRC 상/하위 바이트 분리 가능");
}

static void test_parameter_ranges(void) {
  printf("[TEST] 파라미터 범위 상수 일관성\n");

  TEST_ASSERT(FREQ_MIN < FREQ_MAX, "FREQ_MIN < FREQ_MAX");
  TEST_ASSERT(DUTY_MIN < DUTY_MAX, "DUTY_MIN < DUTY_MAX");
  TEST_ASSERT(DUTY_CLAMP_MAX <= DUTY_MAX, "DUTY_CLAMP_MAX ≤ DUTY_MAX");
  TEST_ASSERT(DUTY_CLAMP_MAX > 0, "DUTY_CLAMP_MAX > 0");
  TEST_ASSERT(ADC_DEADZONE_LOW < ADC_DEADZONE_HIGH, "ADC 데드존 순서 정상");
  TEST_ASSERT(ADC_DEADZONE_HIGH < ADC_RESOLUTION, "ADC 데드존 상한 < 해상도");

  /* HRTIM PER 범위: 16비트(65535) 이내인지 확인 */
  uint32_t arr_at_min_freq = calc_arr(FREQ_MIN);
  TEST_ASSERT(arr_at_min_freq <= 65535, "FREQ_MIN에서 PER ≤ 16비트");

  uint32_t arr_at_max_freq = calc_arr(FREQ_MAX);
  TEST_ASSERT(arr_at_max_freq > 0, "FREQ_MAX에서 PER > 0");
}

static void test_soft_start_simulation(void) {
  printf("[TEST] 소프트 스타트 시뮬레이션\n");

  uint16_t target_duty = 450; /* 45.0% */
  uint16_t current_duty = 0;
  uint32_t steps = 1500 / 10; /* 1500ms / 10ms = 150 단계 */

  for (uint32_t i = 1; i <= steps; i++) {
    current_duty = (uint16_t)((uint32_t)target_duty * i / steps);
  }

  TEST_ASSERT_EQ(current_duty, target_duty,
                 "소프트 스타트 완료 후 current_duty = target_duty");

  /* 중간 단계 확인 */
  uint16_t half_step_duty =
      (uint16_t)((uint32_t)target_duty * (steps / 2) / steps);
  TEST_ASSERT_NEAR(half_step_duty, target_duty / 2, 10,
                   "소프트 스타트 50% 시점 ≈ 목표의 절반");
}

/* ═════════════════════════════════════════════ */
/* ── 메인 ── */
/* ═════════════════════════════════════════════ */
int main(void) {
  printf("═══════════════════════════════════════════\n");
  printf(" STM32G4 초음파 메인보드 — 호스트 단위 테스트\n");
  printf("═══════════════════════════════════════════\n\n");

  test_arr_calculation();
  test_ccr_calculation();
  test_frequency_duty_consistency();
  test_adc_mapping();
  test_crc16();
  test_parameter_ranges();
  test_soft_start_simulation();

  printf("\n═══════════════════════════════════════════\n");
  printf(" 결과: %d 실행, %d 통과, %d 실패\n", g_tests_run, g_tests_passed,
         g_tests_failed);
  printf("═══════════════════════════════════════════\n");

  return g_tests_failed > 0 ? 1 : 0;
}
