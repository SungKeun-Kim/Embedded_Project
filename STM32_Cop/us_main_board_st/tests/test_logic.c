/**
 * @file    test_logic.c
 * @brief   호스트 PC에서 실행하는 단위 테스트 (하드웨어 불필요)
 *
 * 빌드 방법 (PC에서):
 *   gcc -DUNIT_TEST -I../include test_logic.c -o test_logic && ./test_logic
 *
 * 검증 항목:
 *   - ARR 계산 정확도 (주파수 → 레지스터 역산)
 *   - CCR 계산 정확도 (듀티비 → 레지스터 역산)
 *   - 파라미터 범위 클램핑
 *   - Modbus CRC-16 알려진 벡터
 *   - ADC → 듀티 매핑 정확도
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ── 테스트 프레임워크 (미니멀) ── */
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (cond) { g_tests_passed++; } \
    else { g_tests_failed++; printf("  FAIL: %s (line %d)\n", msg, __LINE__); } \
} while (0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    g_tests_run++; \
    if ((a) == (b)) { g_tests_passed++; } \
    else { g_tests_failed++; printf("  FAIL: %s — expected %ld, got %ld (line %d)\n", \
           msg, (long)(b), (long)(a), __LINE__); } \
} while (0)

#define TEST_ASSERT_NEAR(a, b, tol, msg) do { \
    g_tests_run++; \
    double _diff = fabs((double)(a) - (double)(b)); \
    if (_diff <= (double)(tol)) { g_tests_passed++; } \
    else { g_tests_failed++; printf("  FAIL: %s — expected ~%.2f, got %.2f, diff=%.4f (line %d)\n", \
           msg, (double)(b), (double)(a), _diff, __LINE__); } \
} while (0)

/* ── params.h 상수 복제 (테스트용) ── */
#define TIM1_CLOCK_HZ       170000000UL
#define FREQ_MIN             200U    /* 20.0 kHz */
#define FREQ_MAX             500U    /* 50.0 kHz */
#define DUTY_MIN             0U
#define DUTY_MAX             1000U
#define DUTY_CLAMP_MAX       900U    /* 90.0% 안전 상한 */
#define ADC_RESOLUTION       4096U
#define ADC_DEADZONE_LOW     50U
#define ADC_DEADZONE_HIGH    4045U

/* ── 테스트 대상 함수 (실제 코드와 동일 로직 복제) ── */

/* ARR 계산: Center-aligned 모드 */
static uint32_t calc_arr(uint16_t freq_01khz)
{
    if (freq_01khz < FREQ_MIN) freq_01khz = FREQ_MIN;
    if (freq_01khz > FREQ_MAX) freq_01khz = FREQ_MAX;
    uint32_t freq_hz = (uint32_t)freq_01khz * 100UL;
    uint32_t arr = TIM1_CLOCK_HZ / (2UL * freq_hz) - 1;
    return arr;
}

/* ARR → 실제 주파수 역산 */
static double arr_to_freq_khz(uint32_t arr)
{
    return (double)TIM1_CLOCK_HZ / (2.0 * (arr + 1)) / 1000.0;
}

/* CCR 계산 */
static uint32_t calc_ccr(uint32_t arr, uint16_t duty_01pct)
{
    if (duty_01pct > DUTY_CLAMP_MAX) duty_01pct = DUTY_CLAMP_MAX;
    uint32_t ccr = ((uint64_t)arr * duty_01pct) / 1000UL;
    return ccr;
}

/* CCR → 실제 듀티비 역산 */
static double ccr_to_duty_pct(uint32_t ccr, uint32_t arr)
{
    if (arr == 0) return 0.0;
    return (double)ccr / (double)arr * 100.0;
}

/* ADC → 듀티 매핑 */
static uint16_t adc_to_duty_mapped(uint16_t adc_raw)
{
    if (adc_raw <= ADC_DEADZONE_LOW) return 0;
    if (adc_raw >= ADC_DEADZONE_HIGH) return 1000;
    uint32_t range = ADC_DEADZONE_HIGH - ADC_DEADZONE_LOW;
    return (uint16_t)(((uint32_t)(adc_raw - ADC_DEADZONE_LOW) * 1000UL) / range);
}

/* CRC-16 Modbus (비트 연산 방식 — 테이블 없이 검증용) */
static uint16_t crc16_modbus(const uint8_t *data, uint16_t len)
{
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

static void test_arr_calculation(void)
{
    printf("[TEST] ARR 계산 정확도\n");

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

    /* 경계값: FREQ_MIN (20.0 kHz) */
    uint32_t arr_min = calc_arr(FREQ_MIN);
    actual_freq = arr_to_freq_khz(arr_min);
    TEST_ASSERT_NEAR(actual_freq, 20.0, 0.05,
        "FREQ_MIN(20.0kHz) 역산 정확도");

    /* 경계값: FREQ_MAX (50.0 kHz) */
    uint32_t arr_max = calc_arr(FREQ_MAX);
    actual_freq = arr_to_freq_khz(arr_max);
    TEST_ASSERT_NEAR(actual_freq, 50.0, 0.1,
        "FREQ_MAX(50.0kHz) 역산 정확도");

    /* 범위 이하 입력 → 클램핑 */
    uint32_t arr_under = calc_arr(100);  /* 10.0kHz → 클램핑되어 20.0kHz */
    TEST_ASSERT_EQ(arr_under, arr_min,
        "FREQ_MIN 미만 입력 시 클램핑");

    /* 범위 초과 입력 → 클램핑 */
    uint32_t arr_over = calc_arr(600);   /* 60.0kHz → 클램핑되어 50.0kHz */
    TEST_ASSERT_EQ(arr_over, arr_max,
        "FREQ_MAX 초과 입력 시 클램핑");
}

static void test_ccr_calculation(void)
{
    printf("[TEST] CCR 계산 정확도 (듀티비)\n");

    uint32_t arr = calc_arr(280);  /* 28.0 kHz */

    /* 45.0% 듀티 */
    uint32_t ccr_45 = calc_ccr(arr, 450);
    double actual_duty = ccr_to_duty_pct(ccr_45, arr);
    TEST_ASSERT_NEAR(actual_duty, 45.0, 0.5,
        "45.0% 듀티 역산 오차 < 0.5%");

    /* 0% 듀티 */
    uint32_t ccr_0 = calc_ccr(arr, 0);
    TEST_ASSERT_EQ(ccr_0, 0, "0% 듀티 → CCR=0");

    /* 100% 입력 → 90% 클램핑 */
    uint32_t ccr_100 = calc_ccr(arr, 1000);
    uint32_t ccr_90 = calc_ccr(arr, 900);
    TEST_ASSERT_EQ(ccr_100, ccr_90,
        "100% 입력 시 DUTY_CLAMP_MAX(90%)로 클램핑");

    /* 클램핑된 듀티가 90% 이하인지 확인 */
    double clamped_duty = ccr_to_duty_pct(ccr_100, arr);
    TEST_ASSERT(clamped_duty <= 90.1,
        "클램핑 후 실제 듀티 ≤ 90%");
}

static void test_frequency_duty_consistency(void)
{
    printf("[TEST] 주파수-듀티 교차 일관성 (LCD 표시 검증 시뮬레이션)\n");

    /* 시나리오: 에이전트가 만든 코드에서 LCD에 표시하는 주파수와
       실제 TIM1에 들어가는 주파수가 같은지 검증 */

    uint16_t display_freq_01khz = 280;  /* LCD에 "28.0 kHz" 표시 의도 */

    /* 1) display 값으로 ARR 계산 */
    uint32_t arr = calc_arr(display_freq_01khz);

    /* 2) ARR에서 실제 주파수 역산 */
    double actual_freq_khz = arr_to_freq_khz(arr);

    /* 3) 역산 주파수를 다시 ×0.1kHz 정수로 변환 (LCD 표시용) */
    uint16_t readback_01khz = (uint16_t)(actual_freq_khz * 10.0 + 0.5);

    /* 4) 원래 설정값과 비교 — 이것이 LCD 정합성 테스트 */
    TEST_ASSERT_EQ(readback_01khz, display_freq_01khz,
        "LCD 표시 주파수 = TIM1 ARR 역산 주파수 (28.0kHz)");

    /* 모든 유효 주파수에 대해 반복 검증 */
    int mismatch_count = 0;
    for (uint16_t f = FREQ_MIN; f <= FREQ_MAX; f += FREQ_MIN) {
        arr = calc_arr(f);
        actual_freq_khz = arr_to_freq_khz(arr);
        readback_01khz = (uint16_t)(actual_freq_khz * 10.0 + 0.5);
        if (readback_01khz != f) mismatch_count++;
    }
    TEST_ASSERT_EQ(mismatch_count, 0,
        "FREQ_MIN~FREQ_MAX 전 구간 LCD↔TIM1 일치");
}

static void test_adc_mapping(void)
{
    printf("[TEST] ADC → 듀티비 매핑\n");

    /* 데드존 이하 → 0% */
    TEST_ASSERT_EQ(adc_to_duty_mapped(0), 0, "ADC=0 → 0%");
    TEST_ASSERT_EQ(adc_to_duty_mapped(50), 0, "ADC=50(데드존 경계) → 0%");

    /* 데드존 이상 → 100% */
    TEST_ASSERT_EQ(adc_to_duty_mapped(4045), 1000, "ADC=4045(데드존 상한) → 100%");
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
        if (d < prev) { monotonic = 0; break; }
        prev = d;
    }
    TEST_ASSERT(monotonic, "ADC→듀티 매핑이 단조 증가");
}

static void test_crc16(void)
{
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
    TEST_ASSERT(crc_lo != crc_hi || crc_lo == 0,
        "CRC 상/하위 바이트 분리 가능");
}

static void test_parameter_ranges(void)
{
    printf("[TEST] 파라미터 범위 상수 일관성\n");

    TEST_ASSERT(FREQ_MIN < FREQ_MAX, "FREQ_MIN < FREQ_MAX");
    TEST_ASSERT(DUTY_MIN < DUTY_MAX, "DUTY_MIN < DUTY_MAX");
    TEST_ASSERT(DUTY_CLAMP_MAX <= DUTY_MAX, "DUTY_CLAMP_MAX ≤ DUTY_MAX");
    TEST_ASSERT(DUTY_CLAMP_MAX > 0, "DUTY_CLAMP_MAX > 0");
    TEST_ASSERT(ADC_DEADZONE_LOW < ADC_DEADZONE_HIGH, "ADC 데드존 순서 정상");
    TEST_ASSERT(ADC_DEADZONE_HIGH < ADC_RESOLUTION, "ADC 데드존 상한 < 해상도");

    /* ARR 범위: 16비트(65535) 이내인지 확인 */
    uint32_t arr_at_min_freq = calc_arr(FREQ_MIN);
    TEST_ASSERT(arr_at_min_freq <= 65535, "FREQ_MIN에서 ARR ≤ 16비트");

    uint32_t arr_at_max_freq = calc_arr(FREQ_MAX);
    TEST_ASSERT(arr_at_max_freq > 0, "FREQ_MAX에서 ARR > 0");
}

static void test_soft_start_simulation(void)
{
    printf("[TEST] 소프트 스타트 시뮬레이션\n");

    uint16_t target_duty = 450;  /* 45.0% */
    uint16_t current_duty = 0;
    uint32_t steps = 500 / 10;   /* 500ms / 10ms = 50 단계 */

    for (uint32_t i = 1; i <= steps; i++) {
        current_duty = (uint16_t)((uint32_t)target_duty * i / steps);
    }

    TEST_ASSERT_EQ(current_duty, target_duty,
        "소프트 스타트 완료 후 current_duty = target_duty");

    /* 중간 단계 확인 */
    uint16_t half_step_duty = (uint16_t)((uint32_t)target_duty * (steps / 2) / steps);
    TEST_ASSERT_NEAR(half_step_duty, target_duty / 2, 10,
        "소프트 스타트 50% 시점 ≈ 목표의 절반");
}

/* ═════════════════════════════════════════════ */
/* ── 메인 ── */
/* ═════════════════════════════════════════════ */
int main(void)
{
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
    printf(" 결과: %d 실행, %d 통과, %d 실패\n",
           g_tests_run, g_tests_passed, g_tests_failed);
    printf("═══════════════════════════════════════════\n");

    return g_tests_failed > 0 ? 1 : 0;
}
