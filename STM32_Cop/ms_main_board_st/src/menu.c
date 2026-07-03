/**
 * @file  menu.c
 * @brief 선택 모드/설정 모드 기반 메뉴 FSM 구현
 */
#include "menu.h"
#include "button.h"
#include "menu_screen.h"
#include "megasonic_ctrl.h"
#include "modbus_rtu.h"
#include "settings_store.h"
#include "adc_control.h"
#include "buck_dac.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"

/* 기본값 테이블 (×0.1kHz) */
static const uint16_t s_freq_presets_01khz[FREQ_EDIT_CH_COUNT] = {
    FREQ_CH0_DEFAULT,
    FREQ_CH1_DEFAULT,
    FREQ_CH2_DEFAULT,
    FREQ_CH3_DEFAULT,
    FREQ_CH4_DEFAULT,
    FREQ_CH5_DEFAULT,
    FREQ_CH6_DEFAULT,
    FREQ_CH7_DEFAULT,
    FREQ_CH8_DEFAULT,
    FREQ_CH9_DEFAULT,
};

/* 기본 LC 릴레이 조합 프로파일 (l_step = combo + 1)
 * - CH4(1MHz, index 3): combo 0b1111 -> l_step 16 (릴레이 4개 ON)
 * - CH9(2MHz, index 8): combo 0b0001 -> l_step 2  (bit0, LC_RELAY1 ON)
 * - 그 외 채널: combo 0b0000 -> l_step 1
 */
static const uint8_t s_freq_presets_l_step[FREQ_EDIT_CH_COUNT] = {
    1U,  /* CH1  400kHz */
    1U,  /* CH2  600kHz */
    1U,  /* CH3  800kHz */
    16U, /* CH4 1000kHz -> 1111 */
    1U,  /* CH5 1200kHz */
    1U,  /* CH6 1400kHz */
    1U,  /* CH7 1600kHz */
    1U,  /* CH8 1800kHz */
    2U,  /* CH9 2000kHz -> 0001 */
    1U,  /* CH10 2200kHz */
};

#define MENU_BEEP_ALARM_MS 400U
#define MENU_BEEP_LONG_MS 120U
#define MENU_BEEP_CLICK_MS 30U
#define POWER_EDIT_FAST_STEP 10U /* W 설정은 0.10W 단위로 증감 */
#define RUN_POWER_STEP_01W 10U  /* RUN 중 UP/DOWN은 0.10W 단위로 즉시 조정 */
#define SET_EXIT_HOLD_EXTRA_MS 500U /* LONG(1.5s) 이후 추가 0.5s 유지 = 총 2.0s */
#define MENU_FLASH_SAVE_DELAY_MS 200U /* 버튼 뗀 뒤 0.2s 후 백그라운드 FLASH 저장 */
#define MENU_LOW_ALARM_ENABLED 0U /* LM5005/DAC 피드백 검증 중 LOW 알람 임시 보류 */
#define MENU_HIGH_ALARM_ENABLED 0U /* 전압 루프 검증 중 H값 HIGH 알람 정지 보류 */
#define RUN_POWER_EST_FILTER_INTERVAL_MS 20U
#define RUN_POWER_EST_DISPLAY_MS 150U
#define RUN_POWER_DISPLAY_HOLD_MS 100U

static MenuState_t s_state;
static SelectItem_t s_select_item;
static SettingItem_t s_setting_item;
static FreqEditField_t s_freq_field;
static PowerEditField_t s_power_field;
static Rs485EditField_t s_rs485_field;
static uint8_t s_select_mode_active; /* 0=초기화면, 1=선택모드 */
static uint8_t s_run_freq_adjust_active; /* RUN 중 0=출력 조정, 1=주파수 조정 */
static uint8_t s_run_freq_manual_hold;   /* RUN 중 UP/DOWN으로 수동 주파수 조정 시 자동 추적 정지 */

static OperatingMode_t s_selected_mode;
static uint8_t s_selected_freq_ch;   /* 0~9 */
static uint8_t s_selected_power_ch;  /* 0~7 */

static FreqChannelProfile_t s_freq_profiles[FREQ_EDIT_CH_COUNT];
static PowerChannelProfile_t s_power_profiles[POWER_8STEP_COUNT];

/* 설정 편집 버퍼 */
static uint8_t s_freq_edit_ch;       /* 0~9 */
static uint16_t s_freq_edit_freq;
static uint8_t s_freq_edit_l_step;   /* 1~16 */

static uint8_t s_power_edit_ch;      /* 0~7 */
static uint16_t s_power_edit_low;
static uint16_t s_power_edit_high;
static uint16_t s_power_edit_def;
static uint8_t s_rs485_baud_idx;     /* 0~3 */
static uint8_t s_rs485_addr;         /* 1~247 */
static uint8_t s_rs485_term_on;      /* 0=120R OFF, 1=120R ON */
static uint8_t s_rs485_parity;       /* 0=None-8-1, 1=Even-8-1 */
static uint8_t s_rs485_term_enabled; /* 런타임 상태 보관 (하드웨어 제어 예약) */

static uint8_t s_edit_saved;

/* 출력값/알람 */
static uint16_t s_output_set_01w;
static uint16_t s_output_est_01w;
static uint32_t s_output_est_filter_x32;
static uint8_t s_output_est_filter_ready;
static uint32_t s_output_est_display_sum;
static uint8_t s_output_est_display_count;
static uint32_t s_output_est_display_tick;
static uint32_t s_output_est_filter_tick;
static uint32_t s_output_est_hold_until_tick;
static ErrorCode_t s_error_code;
static uint8_t s_freq_nd_flags[FREQ_EDIT_CH_COUNT];

/* 경고음 */
static uint8_t s_alarm_beep_on;
static uint32_t s_alarm_beep_off_tick;

/* 버튼 클릭음 */
static uint8_t s_click_beep_on;
static uint32_t s_click_beep_off_tick;

/* 오토튜닝 약 2.5초 누름 보정: LONG(1.5초) + REPEAT(1초) */
static uint8_t s_autotune_armed;
static uint32_t s_autotune_long_tick;

/* 오토튜닝 진행 상태 (UI 표시용) */
static uint8_t s_autotune_running;
static AutoTuneStatus_t s_autotune_status;
static uint8_t s_autotune_progress;
static uint16_t s_autotune_probe_freq;
static uint8_t s_autotune_probe_l_step;
static uint16_t s_autotune_power_01w;
static uint16_t s_autotune_score;
static ADCFeedback_t s_autotune_feedback;
static uint8_t s_phase_tune_prompt;
static uint16_t s_autotune_work_voltage_01v;

/* 설정 모드 초기화면 복귀용 SET 장기 누름 추적 */
static uint8_t s_set_exit_armed;
static uint32_t s_set_exit_long_tick;
static uint8_t s_store_dirty;
static uint32_t s_store_due_tick;
static uint8_t s_store_fail_count;
static OperatingMode_t s_saved_mode_shadow;
static uint8_t s_saved_freq_ch_shadow;
static uint8_t s_saved_power_ch_shadow;
static uint8_t s_remote_bcd_power_ch;
static uint8_t s_power1_default_migrated;
static uint8_t s_low_alarm_pending;
static uint8_t s_high_alarm_pending;
static uint32_t s_low_alarm_start_tick;
static uint32_t s_high_alarm_start_tick;
static uint32_t s_run_power_control_tick;
static uint16_t s_overcurrent_latch_ma;
static uint16_t s_overcurrent_latch_voltage_01v;
static uint8_t s_remote_run_latched;
static uint8_t s_remote_run_armed;
static uint8_t s_run_freq_track_ready;
static uint32_t s_run_freq_track_tick;
static uint16_t s_run_freq_track_base;
static uint16_t s_run_freq_track_best_freq;
static uint16_t s_run_freq_track_best_cur;
static int8_t s_run_freq_track_dir;
static uint8_t s_run_retune_pending;
static uint8_t s_run_retune_active;
static uint32_t s_run_retune_due_tick;
static uint32_t s_run_power_maintain_tick;
static uint8_t s_run_power_miss_count;
static uint16_t s_run_retune_safe_freq;
static uint16_t s_run_retune_safe_duty;
static uint16_t s_run_retune_safe_voltage;
static uint8_t s_run_retune_guard_hit;
static uint8_t s_run_power_hold_active;

static uint16_t ClampFreq01kHz(uint16_t f);
static uint16_t ClampBaseVoltage01V(uint16_t voltage_01v);
static uint16_t AbsDiffU16(uint16_t a, uint16_t b);
static uint16_t RunTargetVoltage01V(void);
static uint8_t IsGuidedTuneChannel(uint8_t ch);
static uint8_t StoredPowerFreqLooksStaleNominal(uint8_t freq_ch, uint16_t freq_01khz);
static uint8_t RunDutyOnlyGoodEnough(uint16_t power_01w, uint16_t target_power_01w);
static void SetOutputPowerEstimateImmediate(uint16_t power_01w);

static uint16_t ClampU16(uint16_t v, uint16_t min, uint16_t max)
{
    if (v < min) return min;
    if (v > max) return max;
    return v;
}

static uint16_t PowerProfileDefaultForChannel(uint8_t ch)
{
    uint16_t def = (uint16_t)(POWER_PROFILE_DEF_BASE
                              + ((uint16_t)ch * POWER_PROFILE_DEF_STEP));
    return ClampU16(def, POWER_PROFILE_DEF_MIN, POWER_PROFILE_HIGH_DEFAULT);
}

static uint8_t Is2MHzTuneChannel(uint8_t freq_ch)
{
    return (freq_ch == 8U) ? 1U : 0U; /* CH09 = 2000kHz */
}

static uint16_t RunGateDutyMinForFreqCh(uint8_t freq_ch)
{
    return (Is2MHzTuneChannel(freq_ch) != 0U)
         ? RUN_GATE_DUTY_TUNE_2MHZ_MIN_01PCT
         : RUN_GATE_DUTY_TUNE_MIN_01PCT;
}

static uint16_t RunGateDutyMaxForFreqCh(uint8_t freq_ch)
{
    return (Is2MHzTuneChannel(freq_ch) != 0U)
         ? RUN_GATE_DUTY_TUNE_2MHZ_MAX_01PCT
         : RUN_GATE_DUTY_TUNE_MAX_01PCT;
}

static uint16_t QuantizeRunGateDuty01Pct(uint16_t duty_01pct)
{
    uint16_t step = RUN_GATE_DUTY_TUNE_STEP_01PCT;

    if (step == 0U) {
        return duty_01pct;
    }

    return (uint16_t)((((uint32_t)duty_01pct + ((uint32_t)step / 2U)) / (uint32_t)step)
                      * (uint32_t)step);
}

static uint16_t ClampRunGateDutyForFreqCh(uint16_t duty_01pct, uint8_t freq_ch)
{
    uint16_t lo = RunGateDutyMinForFreqCh(freq_ch);
    uint16_t hi = RunGateDutyMaxForFreqCh(freq_ch);

    duty_01pct = ClampU16(duty_01pct, lo, hi);
    duty_01pct = QuantizeRunGateDuty01Pct(duty_01pct);
    return ClampU16(duty_01pct, lo, hi);
}

static uint16_t ClampRunGateDutyForSelectedFreq(uint16_t duty_01pct)
{
    return ClampRunGateDutyForFreqCh(duty_01pct, s_selected_freq_ch);
}

static uint16_t RunRetuneSettleMs(void)
{
    return (Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
         ? RUN_RETUNE_2MHZ_SETTLE_MS
         : RUN_RETUNE_SETTLE_MS;
}

static uint8_t RunRetuneSampleCount(void)
{
    return (Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
         ? RUN_RETUNE_2MHZ_SAMPLE_COUNT
         : RUN_RETUNE_SAMPLE_COUNT;
}

static uint32_t RunPowerMaintainIntervalMs(void)
{
    return (Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
         ? RUN_POWER_2MHZ_HOLD_INTERVAL_MS
         : RUN_POWER_MAINTAIN_INTERVAL_MS;
}

static void SetRunRetuneSafePoint(uint16_t freq_01khz,
                                  uint16_t gate_duty_01pct,
                                  uint16_t voltage_01v)
{
    s_run_retune_safe_freq = ClampFreq01kHz(freq_01khz);
    s_run_retune_safe_duty = ClampRunGateDutyForSelectedFreq(gate_duty_01pct);
    s_run_retune_safe_voltage = ClampBaseVoltage01V(voltage_01v);
}

static void RestoreRunRetuneSafePoint(void)
{
    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(s_run_retune_safe_voltage));
    MegasonicCtrl_SetFrequency(s_run_retune_safe_freq);
    MegasonicCtrl_SetRunGateDuty(s_run_retune_safe_duty);
    MegasonicCtrl_Update();
    MenuScreen_ForceRefresh();
}

static uint8_t RunRetuneCurrentGuardHit(uint16_t *power_out)
{
    if (power_out != NULL) {
        *power_out = ADC_Control_GetIvPower01W();
        SetOutputPowerEstimateImmediate(*power_out);
    }
    s_run_retune_guard_hit = 1U;
    RestoreRunRetuneSafePoint();
    return 0U;
}

static uint8_t Run2MHzLockGoodEnough(uint16_t power_01w, uint16_t target_power_01w)
{
    uint16_t low_limit;
    uint16_t high_limit;

    if ((Is2MHzTuneChannel(s_selected_freq_ch) == 0U)
        || (target_power_01w < 150U)) {
        return RunDutyOnlyGoodEnough(power_01w, target_power_01w);
    }

    low_limit = (target_power_01w > RUN_DUTY_2MHZ_LOCK_LOW_01W)
              ? (uint16_t)(target_power_01w - RUN_DUTY_2MHZ_LOCK_LOW_01W)
              : 0U;
    high_limit = (uint16_t)(target_power_01w + RUN_DUTY_2MHZ_LOCK_HIGH_01W);

    return ((power_01w >= low_limit) && (power_01w <= high_limit)) ? 1U : 0U;
}

static void RequestRunPowerRetuneNow(uint32_t now)
{
    s_run_power_hold_active = 0U;
    s_run_power_miss_count = 0U;
    s_run_retune_pending = 1U;
    s_run_retune_due_tick = now;
    s_run_power_maintain_tick = 0U;
}

static void LockRunPowerAtCurrentPoint(uint16_t freq_01khz,
                                       uint16_t gate_duty_01pct,
                                       uint16_t power_01w)
{
    MegasonicCtrl_SetFrequency(freq_01khz);
    MegasonicCtrl_SetRunGateDuty(gate_duty_01pct);
    SetOutputPowerEstimateImmediate(power_01w);
    SetRunRetuneSafePoint(freq_01khz, gate_duty_01pct, RunTargetVoltage01V());
    s_run_power_hold_active = 1U;
    s_run_retune_pending = 0U;
    s_run_power_miss_count = 0U;
    s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
}

static uint8_t PowerTuneDutyCodeFrom01Pct(uint16_t duty_01pct, uint8_t freq_ch)
{
    duty_01pct = ClampRunGateDutyForFreqCh(duty_01pct, freq_ch);
    return (uint8_t)((duty_01pct - RUN_GATE_DUTY_TUNE_MIN_01PCT + 1U) / 2U);
}

static uint16_t PowerTuneDuty01PctFromCode(uint8_t code)
{
    uint16_t duty = (uint16_t)(RUN_GATE_DUTY_TUNE_MIN_01PCT
                               + ((uint16_t)code * 2U));
    return ClampU16(duty,
                    RUN_GATE_DUTY_TUNE_MIN_01PCT,
                    RUN_GATE_DUTY_TUNE_MAX_01PCT);
}

static uint16_t PowerTuneFreqForChannel(uint8_t power_ch, uint8_t freq_ch)
{
    const PowerChannelProfile_t *p;
    int32_t freq;

    if ((power_ch >= POWER_8STEP_COUNT) || (freq_ch >= FREQ_EDIT_CH_COUNT)) {
        return 0U;
    }

    p = &s_power_profiles[power_ch];
    freq = (int32_t)s_freq_profiles[freq_ch].freq_01khz
         + (int32_t)p->tuned_freq_offset_01khz[freq_ch];

    if (freq < (int32_t)FREQ_EDIT_MIN) return FREQ_EDIT_MIN;
    if (freq > (int32_t)FREQ_EDIT_MAX) return FREQ_EDIT_MAX;
    return (uint16_t)freq;
}

static uint16_t PowerTuneDutyForChannel(uint8_t power_ch, uint8_t freq_ch)
{
    uint16_t duty;

    if ((power_ch >= POWER_8STEP_COUNT) || (freq_ch >= FREQ_EDIT_CH_COUNT)) {
        return HRTIM_RUN_DUTY_01PCT;
    }

    duty = PowerTuneDuty01PctFromCode(
        s_power_profiles[power_ch].tuned_gate_duty_02pct[freq_ch]);
    return ClampRunGateDutyForFreqCh(duty, freq_ch);
}

static uint8_t PowerProfileTuneValidFor(uint8_t power_ch, uint8_t freq_ch)
{
    const PowerChannelProfile_t *p;
    uint16_t freq;
    uint16_t duty;

    if ((power_ch >= POWER_8STEP_COUNT) || (freq_ch >= FREQ_EDIT_CH_COUNT)) {
        return 0U;
    }

    p = &s_power_profiles[power_ch];
    if ((p->tuned_valid_mask & (uint16_t)(1U << freq_ch)) == 0U) return 0U;

    freq = PowerTuneFreqForChannel(power_ch, freq_ch);
    duty = PowerTuneDutyForChannel(power_ch, freq_ch);
    if ((freq < FREQ_EDIT_MIN) || (freq > FREQ_EDIT_MAX)) return 0U;
    if (StoredPowerFreqLooksStaleNominal(freq_ch, freq) != 0U) {
        return 0U;
    }
    if ((duty < RunGateDutyMinForFreqCh(freq_ch))
        || (duty > RunGateDutyMaxForFreqCh(freq_ch))) return 0U;
    return 1U;
}

static uint8_t PowerProfileTuneValid(uint8_t power_ch)
{
    return PowerProfileTuneValidFor(power_ch, s_selected_freq_ch);
}

static void ClearPowerTunePoints(uint8_t power_ch)
{
    uint8_t i;
    PowerChannelProfile_t *p;

    if (power_ch >= POWER_8STEP_COUNT) return;

    p = &s_power_profiles[power_ch];
    p->tuned_freq_01khz = 0U;
    p->tuned_gate_duty_01pct = 0U;
    p->tuned_freq_ch = 0U;
    p->tuned_valid = 0U;
    p->tuned_valid_mask = 0U;
    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        p->tuned_freq_offset_01khz[i] = 0;
        p->tuned_gate_duty_02pct[i] = 0U;
    }
}

static void ClearPowerTunePointsForFreqChannel(uint8_t freq_ch)
{
    uint8_t i;

    if (freq_ch >= FREQ_EDIT_CH_COUNT) return;

    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        PowerChannelProfile_t *p = &s_power_profiles[i];

        p->tuned_valid_mask &= (uint16_t)~(uint16_t)(1U << freq_ch);
        p->tuned_freq_offset_01khz[freq_ch] = 0;
        p->tuned_gate_duty_02pct[freq_ch] = 0U;
        if ((p->tuned_valid != 0U) && (p->tuned_freq_ch == freq_ch)) {
            p->tuned_freq_01khz = 0U;
            p->tuned_gate_duty_01pct = 0U;
            p->tuned_freq_ch = 0U;
            p->tuned_valid = 0U;
        }
    }
}

static void StorePowerTunePoint(uint8_t power_ch,
                                uint8_t freq_ch,
                                uint16_t freq_01khz,
                                uint16_t gate_duty_01pct)
{
    PowerChannelProfile_t *p;
    int32_t offset;

    if ((power_ch >= POWER_8STEP_COUNT) || (freq_ch >= FREQ_EDIT_CH_COUNT)) {
        return;
    }

    p = &s_power_profiles[power_ch];
    freq_01khz = ClampU16(freq_01khz, FREQ_EDIT_MIN, FREQ_EDIT_MAX);
    gate_duty_01pct = ClampRunGateDutyForFreqCh(gate_duty_01pct, freq_ch);

    offset = (int32_t)freq_01khz - (int32_t)s_freq_profiles[freq_ch].freq_01khz;
    if (offset < -32768L) offset = -32768L;
    if (offset > 32767L) offset = 32767L;

    p->tuned_freq_offset_01khz[freq_ch] = (int16_t)offset;
    p->tuned_gate_duty_02pct[freq_ch] = PowerTuneDutyCodeFrom01Pct(gate_duty_01pct, freq_ch);
    p->tuned_valid_mask |= (uint16_t)(1U << freq_ch);

    /* 구버전 화면/저장값과의 호환용 대표값도 마지막 튜닝 지점으로 유지한다. */
    p->tuned_freq_01khz = freq_01khz;
    p->tuned_gate_duty_01pct = gate_duty_01pct;
    p->tuned_freq_ch = freq_ch;
    p->tuned_valid = 1U;
}

static uint16_t ClampFreq01kHz(uint16_t f)
{
    return ClampU16(f, FREQ_EDIT_MIN, FREQ_EDIT_MAX);
}

static uint8_t IsGuidedTuneChannel(uint8_t ch)
{
    return ((ch == 3U) || (ch == 8U)) ? 1U : 0U; /* CH04=1MHz, CH09=2MHz */
}

static uint32_t MenuFreqPeriodForStaleCheck(uint16_t freq_01khz)
{
    uint32_t freq_hz;

    freq_01khz = ClampFreq01kHz(freq_01khz);
    freq_hz = (uint32_t)freq_01khz * 100UL;
    if (freq_hz == 0UL) {
        return 0UL;
    }

    return (uint32_t)((uint64_t)HRTIM_CLOCK_HZ / (uint64_t)freq_hz);
}

static uint8_t StoredPowerFreqLooksStaleNominal(uint8_t freq_ch, uint16_t freq_01khz)
{
    uint16_t nominal;

    if (freq_ch >= FREQ_EDIT_CH_COUNT) return 0U;
    if (IsGuidedTuneChannel(freq_ch) == 0U) return 0U;

    nominal = s_freq_presets_01khz[freq_ch];
    if (s_freq_profiles[freq_ch].freq_01khz == nominal) return 0U;

    return (MenuFreqPeriodForStaleCheck(freq_01khz)
            == MenuFreqPeriodForStaleCheck(nominal)) ? 1U : 0U;
}

static uint16_t ClampBaseVoltage01V(uint16_t voltage_01v)
{
    return ClampU16(voltage_01v,
                    RUN_BASE_VOLTAGE_MIN_01V,
                    RUN_BASE_VOLTAGE_MAX_01V);
}

static void GetGuidedTuneLimit(uint8_t ch, uint16_t *lo, uint16_t *hi)
{
    uint16_t center;
    uint16_t min_f;
    uint16_t max_f;

    if ((lo == NULL) || (hi == NULL)) {
        return;
    }

    if (ch >= FREQ_EDIT_CH_COUNT) {
        *lo = FREQ_EDIT_MIN;
        *hi = FREQ_EDIT_MAX;
        return;
    }

    center = s_freq_presets_01khz[ch];
    min_f = (center > TUNE_GUIDED_DOWN_01KHZ)
          ? (uint16_t)(center - TUNE_GUIDED_DOWN_01KHZ)
          : FREQ_EDIT_MIN;
    max_f = (uint16_t)(center + TUNE_GUIDED_UP_01KHZ);

    *lo = ClampFreq01kHz(min_f);
    *hi = ClampFreq01kHz(max_f);
}

static void GetAutoTuneCoarseRange(uint8_t ch,
                                   uint16_t edit_freq,
                                   uint16_t *lo,
                                   uint16_t *hi)
{
    if (IsGuidedTuneChannel(ch) != 0U) {
        GetGuidedTuneLimit(ch, lo, hi);
        return;
    }

    *lo = ClampFreq01kHz((edit_freq > TUNE_FREQ_COARSE_RANGE_01KHZ)
                       ? (uint16_t)(edit_freq - TUNE_FREQ_COARSE_RANGE_01KHZ)
                       : FREQ_EDIT_MIN);
    *hi = ClampFreq01kHz((uint16_t)(edit_freq + TUNE_FREQ_COARSE_RANGE_01KHZ));
}

static void GetAutoTuneFineRange(uint8_t ch,
                                 uint16_t center_freq,
                                 uint16_t *lo,
                                 uint16_t *hi)
{
    uint16_t fine_lo;
    uint16_t fine_hi;

    fine_lo = ClampFreq01kHz((center_freq > TUNE_FREQ_FINE_RANGE_01KHZ)
                           ? (uint16_t)(center_freq - TUNE_FREQ_FINE_RANGE_01KHZ)
                           : FREQ_EDIT_MIN);
    fine_hi = ClampFreq01kHz((uint16_t)(center_freq + TUNE_FREQ_FINE_RANGE_01KHZ));

    if (IsGuidedTuneChannel(ch) != 0U) {
        uint16_t limit_lo;
        uint16_t limit_hi;

        GetGuidedTuneLimit(ch, &limit_lo, &limit_hi);
        if (fine_lo < limit_lo) fine_lo = limit_lo;
        if (fine_hi > limit_hi) fine_hi = limit_hi;
    }

    *lo = fine_lo;
    *hi = fine_hi;
}

static uint16_t RoundPowerDisplay01W(uint16_t power_01w)
{
    /* RUN 화면은 0.01W 단위를 유지하고, 표시 갱신 주기를 늦춰 가독성을 확보한다. */
    return power_01w;
}

static void HoldOutputPowerDisplay(void)
{
    if (g_us_state.running == 0U) {
        return;
    }

    s_output_est_hold_until_tick = HAL_GetTick() + RUN_POWER_DISPLAY_HOLD_MS;
    s_output_est_display_sum = 0U;
    s_output_est_display_count = 0U;
    s_output_est_display_tick = HAL_GetTick();
}

static void SetOutputPowerEstimateImmediate(uint16_t power_01w)
{
    uint16_t display_01w = RoundPowerDisplay01W(power_01w);

    s_output_est_01w = display_01w;
    s_output_est_filter_x32 = (uint32_t)display_01w * 32UL;
    s_output_est_filter_ready = 1U;
    s_output_est_display_sum = 0U;
    s_output_est_display_count = 0U;
    s_output_est_display_tick = HAL_GetTick();
    s_output_est_filter_tick = HAL_GetTick();
    s_output_est_hold_until_tick = 0U;
}

static uint16_t RunTargetVoltage01V(void);
static uint8_t IsGuidedTuneChannel(uint8_t ch);
static void HoldOutputPowerDisplay(void);
static void SetError(ErrorCode_t err);
static void SetCurrentProtectionError(void);
static uint8_t AutoTuneFaultActive(void);
static uint8_t CurrentSensorFaultActive(void);
static uint8_t PrepareBuckVoltageTargetBeforeStart(uint16_t target_voltage_01v);
static uint8_t PrepareBuckVoltageBeforeStart(void);
static void StartOutputCommon(void);
static uint16_t SamplePower01W(uint8_t samples, uint16_t delay_ms);
static uint8_t RunDutyOnlyGoodEnough(uint16_t power_01w, uint16_t target_power_01w);
static uint8_t SaveStoreNow(void);
static void MarkStoreDirty(void);
static void PlayStoreFeedback(uint8_t save_ok);
static void ApplyPowerChannel(uint8_t ch);
static void ResetRunFrequencyTracking(void);
static void ScheduleRunRetune(void);

static void ResetRunControlState(void)
{
    s_run_power_control_tick = 0U;
    ResetRunFrequencyTracking();
}

static void ResetRunFrequencyTracking(void)
{
    s_run_freq_track_ready = 0U;
    s_run_freq_track_tick = 0U;
    s_run_freq_track_base = 0U;
    s_run_freq_track_best_freq = 0U;
    s_run_freq_track_best_cur = 0U;
    s_run_freq_track_dir = 1;
    s_run_retune_pending = 0U;
    s_run_retune_active = 0U;
    s_run_retune_due_tick = 0U;
    s_run_power_maintain_tick = 0U;
    s_run_power_miss_count = 0U;
    s_run_power_hold_active = 0U;
}

static void LatchOvercurrentSnapshot(void)
{
    uint32_t current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;

    s_overcurrent_latch_ma = (current_ma > 9999UL) ? 9999U : (uint16_t)current_ma;
    s_overcurrent_latch_voltage_01v = ADC_Control_GetVoltage01V();
}

static void SetCurrentProtectionError(void)
{
    uint32_t current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;

    LatchOvercurrentSnapshot();
    s_autotune_running = 0U;
    if (current_ma >= RUN_HIGH_CUR_PROTECT_MA) {
        SetError(ERR_OVERCURRENT);
    } else if ((ADC_Control_GetCurrent01mV() >= ADC_CUR_SENSOR_FAULT_01MV)
               || (current_ma >= ADC_CUR_SENSOR_FAULT_MA)) {
        SetError(ERR_CUR_SENSOR);
    } else {
        SetError(ERR_OVERCURRENT);
    }
}

static uint8_t CurrentSensorFaultActive(void)
{
    uint32_t current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;

    return ((ADC_Control_GetCurrent01mV() >= ADC_CUR_SENSOR_FAULT_01MV)
            || (current_ma >= ADC_CUR_SENSOR_FAULT_MA)) ? 1U : 0U;
}

static uint8_t IsSensorInputOk(void)
{
    if (SENSOR_RUN_INTERLOCK_ENABLED == 0U) {
        return 1U;
    }

    /* SENSOR 입력은 내부 풀업, 정상 SHORT 시 Active LOW로 판단한다. */
    return (HAL_GPIO_ReadPin(SENSOR_INPUT_PORT, SENSOR_INPUT_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

static uint8_t ReadRemoteBcdPowerChannel(void)
{
    uint8_t bcd = 0U;

    if (HAL_GPIO_ReadPin(EXT_BCD1_PORT, EXT_BCD1_PIN) == GPIO_PIN_RESET) bcd |= 0x01U;
    if (HAL_GPIO_ReadPin(EXT_BCD2_PORT, EXT_BCD2_PIN) == GPIO_PIN_RESET) bcd |= 0x02U;
    if (HAL_GPIO_ReadPin(EXT_BCD3_PORT, EXT_BCD3_PIN) == GPIO_PIN_RESET) bcd |= 0x04U;

    return (bcd < POWER_8STEP_COUNT) ? bcd : (uint8_t)(POWER_8STEP_COUNT - 1U);
}

static uint8_t ReadRemoteRunActive(void)
{
    return (HAL_GPIO_ReadPin(EXT_REMOTE_PORT, EXT_REMOTE_PIN) == GPIO_PIN_RESET) ? 1U : 0U;
}

static void SetAutoTuneFeedback(uint16_t score, const ADCFeedback_t *feedback)
{
    s_autotune_score = score;
    if (feedback != NULL) {
        s_autotune_feedback = *feedback;
        if (((ADC_Control_GetCurrentuA() + 500UL) / 1000UL) >= RUN_HIGH_CUR_PROTECT_MA) {
            SetCurrentProtectionError();
        }
    }
}

static void SetAutoTuneState(AutoTuneStatus_t status,
                             uint8_t running,
                             uint8_t progress,
                             uint16_t probe_freq_01khz,
                             uint8_t probe_l_step)
{
    if (AutoTuneFaultActive() != 0U) {
        running = 0U;
    }

    s_autotune_status = status;
    s_autotune_running = running;
    s_autotune_progress = (uint8_t)ClampU16(progress, 0U, 100U);
    s_autotune_probe_freq = ClampFreq01kHz(probe_freq_01khz);
    s_autotune_probe_l_step = (uint8_t)ClampU16(probe_l_step, 1U, 16U);

    MenuScreen_ForceRefresh();
    MenuScreen_Refresh();
}

static uint8_t IsAutoTuneTimedOut(uint32_t start_tick)
{
    if (AutoTuneFaultActive() != 0U) {
        return 1U;
    }
    return ((HAL_GetTick() - start_tick) >= TUNE_TIMEOUT_MS) ? 1U : 0U;
}

static void ClearAutoTuneResultIfAnyInput(ButtonEvent_t evt_start,
                                          ButtonEvent_t evt_mode,
                                          ButtonEvent_t evt_up,
                                          ButtonEvent_t evt_down,
                                          ButtonEvent_t evt_set)
{
    if (s_autotune_running != 0U) {
        return;
    }

    if (s_phase_tune_prompt != 0U) {
        return;
    }

    if ((evt_start == BTN_EVT_NONE)
        && (evt_mode == BTN_EVT_NONE)
        && (evt_up == BTN_EVT_NONE)
        && (evt_down == BTN_EVT_NONE)
        && (evt_set == BTN_EVT_NONE)) {
        return;
    }

    if ((s_autotune_status == TUNE_STATUS_DONE)
        || (s_autotune_status == TUNE_STATUS_PHASE_DONE)
        || (s_autotune_status == TUNE_STATUS_NO_DETECTED)
        || (s_autotune_status == TUNE_STATUS_TIMEOUT)) {
        s_autotune_status = TUNE_STATUS_IDLE;
        s_autotune_progress = 0U;
        s_autotune_score = 0U;
        MenuScreen_ForceRefresh();
    }
}

static uint16_t ScoreFeedbackCandidate(const ADCFeedback_t *fb)
{
    if (fb == NULL) {
        return 0xFFFFU;
    }

    if ((fb->cur_adc < TUNE_FEEDBACK_MIN_SIGNAL_ADC)
        && (fb->fwd_adc < TUNE_FEEDBACK_MIN_SIGNAL_ADC)
        && (fb->ref_adc < TUNE_FEEDBACK_MIN_SIGNAL_ADC)) {
        return 0xFFFFU;
    }

    /* FET 보호 우선: 공진점은 INA190A3 전류가 가장 작게 나오는 지점으로 판단한다. */
    return fb->cur_adc;
}

static uint16_t ScorePhaseCandidate(const ADCFeedback_t *fb)
{
    uint32_t diff;
    uint32_t signal_penalty;
    uint32_t score;
    uint16_t min_signal;

    if (fb == NULL) {
        return 0xFFFFU;
    }

    min_signal = (fb->fwd_adc < fb->ref_adc) ? fb->fwd_adc : fb->ref_adc;
    if (min_signal < TUNE_PHASE_MIN_SIGNAL_ADC) {
        return 0xFFFFU;
    }

    diff = (fb->fwd_adc > fb->ref_adc)
         ? (uint32_t)(fb->fwd_adc - fb->ref_adc)
         : (uint32_t)(fb->ref_adc - fb->fwd_adc);
    signal_penalty = (uint32_t)(ADC_RESOLUTION - 1U - min_signal) / 64U;
    score = diff + signal_penalty;

    return (score > 0xFFFFU) ? 0xFFFFU : (uint16_t)score;
}

static uint16_t AbsDiffU16(uint16_t a, uint16_t b)
{
    return (a > b) ? (uint16_t)(a - b) : (uint16_t)(b - a);
}

static uint8_t TuneCandidateIsBetter(uint8_t guided,
                                     uint16_t freq,
                                     uint16_t score,
                                     uint16_t best_freq,
                                     uint16_t best_score,
                                     uint16_t preferred_freq)
{
    uint16_t dist;
    uint16_t best_dist;

    if (score == 0xFFFFU) {
        return 0U;
    }
    if (score < best_score) {
        return 1U;
    }
    if ((guided == 0U) || (score != best_score)) {
        return 0U;
    }

    dist = AbsDiffU16(freq, preferred_freq);
    best_dist = AbsDiffU16(best_freq, preferred_freq);
    if (dist < best_dist) {
        return 1U;
    }
    if ((dist == best_dist) && (freq < best_freq)) {
        return 1U;
    }

    return 0U;
}

static uint8_t StopIfOvercurrentNow(void)
{
    uint32_t current_ma;

    ADC_Control_Process();
    current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
    if (current_ma >= RUN_HIGH_CUR_PROTECT_MA) {
        SetCurrentProtectionError();
        return 0U;
    }

    return 1U;
}

static uint8_t DelayWithCurrentGuard(uint32_t delay_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < delay_ms) {
        uint32_t elapsed = HAL_GetTick() - start;
        uint32_t remain = (elapsed < delay_ms) ? (delay_ms - elapsed) : 0U;
        uint32_t step = (remain > 5U) ? 5U : remain;

        if (StopIfOvercurrentNow() == 0U) {
            return 0U;
        }
        MegasonicCtrl_Update();
        if (step == 0U) {
            break;
        }
        HAL_Delay(step);
    }

    return StopIfOvercurrentNow();
}

static uint16_t ClampTuneShift(uint16_t candidate, uint16_t reference)
{
    if (candidate > reference) {
        if ((uint16_t)(candidate - reference) > TUNE_RESULT_MAX_SHIFT_01KHZ) {
            return (uint16_t)(reference + TUNE_RESULT_MAX_SHIFT_01KHZ);
        }
    } else {
        if ((uint16_t)(reference - candidate) > TUNE_RESULT_MAX_SHIFT_01KHZ) {
            return (reference > TUNE_RESULT_MAX_SHIFT_01KHZ)
                ? (uint16_t)(reference - TUNE_RESULT_MAX_SHIFT_01KHZ)
                : FREQ_EDIT_MIN;
        }
    }

    return candidate;
}

static uint16_t StableBestIndex(const uint16_t *score_tbl,
                                uint16_t count,
                                uint16_t preferred_freq,
                                uint16_t start_freq,
                                uint16_t step_01khz)
{
    uint16_t best_idx = 0U;
    uint32_t best_avg = 0xFFFFFFFFUL;
    uint16_t best_dist = 0xFFFFU;

    if ((score_tbl == NULL) || (count == 0U)) {
        return 0U;
    }

    for (uint16_t i = 0U; i < count; i++) {
        uint32_t sum = score_tbl[i];
        uint32_t used = 1U;
        uint32_t avg;
        uint16_t freq;
        uint16_t dist;

        if (score_tbl[i] == 0xFFFFU) {
            continue;
        }
        if ((i > 0U) && (score_tbl[i - 1U] != 0xFFFFU)) {
            sum += score_tbl[i - 1U];
            used++;
        }
        if (((uint16_t)(i + 1U) < count) && (score_tbl[i + 1U] != 0xFFFFU)) {
            sum += score_tbl[i + 1U];
            used++;
        }

        avg = (sum + (used / 2U)) / used;
        freq = (uint16_t)(start_freq + (uint16_t)i * step_01khz);
        dist = AbsDiffU16(freq, preferred_freq);

        if ((avg < best_avg) || ((avg == best_avg) && (dist < best_dist))) {
            best_avg = avg;
            best_dist = dist;
            best_idx = i;
        }
    }

    return best_idx;
}

static uint16_t StableValleyIndex(const uint16_t *score_tbl,
                                  const uint16_t *freq_tbl,
                                  uint16_t count,
                                  uint16_t preferred_freq)
{
    uint16_t best_idx = 0U;
    uint32_t best_avg = 0xFFFFFFFFUL;
    uint16_t best_dist = 0xFFFFU;
    uint8_t found = 0U;

    if ((score_tbl == NULL) || (freq_tbl == NULL) || (count == 0U)) {
        return 0U;
    }

    for (uint16_t i = 0U; i < count; i++) {
        uint32_t sum = score_tbl[i];
        uint32_t used = 1U;
        uint32_t avg;
        uint16_t freq;
        uint16_t dist;

        if (score_tbl[i] == 0xFFFFU) {
            continue;
        }
        if ((i > 0U) && (score_tbl[i - 1U] != 0xFFFFU)) {
            sum += score_tbl[i - 1U];
            used++;
        }
        if (((uint16_t)(i + 1U) < count) && (score_tbl[i + 1U] != 0xFFFFU)) {
            sum += score_tbl[i + 1U];
            used++;
        }

        avg = (sum + (used / 2U)) / used;
        freq = freq_tbl[i];
        dist = AbsDiffU16(freq, preferred_freq);

        if ((found == 0U)
            || ((avg + TUNE_VALLEY_SELECT_HYST_ADC) < best_avg)
            || ((avg <= (best_avg + TUNE_VALLEY_SELECT_HYST_ADC)) && (dist < best_dist))
            || ((avg <= (best_avg + TUNE_VALLEY_SELECT_HYST_ADC))
                && (dist == best_dist)
                && (freq < freq_tbl[best_idx]))) {
            found = 1U;
            best_avg = avg;
            best_dist = dist;
            best_idx = i;
        }
    }

    return best_idx;
}

static void WaitAutoTuneOutputReady(void)
{
    uint32_t start = HAL_GetTick();

    while (((HAL_GetTick() - start) < ((uint32_t)SOFT_START_DURATION_MS + TUNE_FREQ_SETTLE_MS))
           && (AutoTuneFaultActive() == 0U)) {
        if (DelayWithCurrentGuard(10U) == 0U) {
            break;
        }
    }
}

static void RestoreAutoTuneOutput(uint8_t was_running)
{
    if (AutoTuneFaultActive() != 0U) {
        MegasonicCtrl_EmergencyStop();
        return;
    }

    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(RunTargetVoltage01V()));
    MegasonicCtrl_Update();

    if (was_running == 0U) {
        MegasonicCtrl_Stop();
    }
}

static uint8_t SwitchLCRelayForTune(uint8_t l_step, uint32_t run_settle_ms)
{
    uint16_t tune_duty;

    if (StopIfOvercurrentNow() == 0U) {
        return 0U;
    }

    tune_duty = BuckDAC_DutyForVoltage01V((s_autotune_work_voltage_01v != 0U)
                                          ? s_autotune_work_voltage_01v
                                          : TUNE_TEST_VOLTAGE_01V);
    MegasonicCtrl_Stop();
    MegasonicCtrl_PrechargeBuck(tune_duty);
    MegasonicCtrl_SetLCRelay((uint8_t)(ClampU16(l_step, 1U, 16U) - 1U));

    if (DelayWithCurrentGuard(TUNE_L_RELAY_OFF_SETTLE_MS) == 0U) {
        return 0U;
    }

    MegasonicCtrl_Start();
    MegasonicCtrl_ForceRunGateDuty();
    WaitAutoTuneOutputReady();
    if (AutoTuneFaultActive() != 0U) {
        return 0U;
    }

    return DelayWithCurrentGuard(run_settle_ms);
}

static uint8_t AutoTuneFaultActive(void)
{
    return (s_error_code != ERR_NONE) ? 1U : 0U;
}

static uint8_t EventIsStep(ButtonEvent_t evt)
{
    return (evt == BTN_EVT_PRESS || evt == BTN_EVT_REPEAT) ? 1U : 0U;
}

static int8_t EventStepDelta(ButtonEvent_t evt_up, ButtonEvent_t evt_down)
{
    if (EventIsStep(evt_up)) return 1;
    if (EventIsStep(evt_down)) return -1;
    return 0;
}

static int8_t EventStepDeltaPowerEdit(ButtonEvent_t evt_up,
                                      ButtonEvent_t evt_down,
                                      PowerEditField_t field)
{
    if (field == POWER_EDIT_FIELD_CH) {
        return EventStepDelta(evt_up, evt_down);
    }

    if (evt_up == BTN_EVT_REPEAT) return (int8_t)POWER_EDIT_FAST_STEP;
    if (evt_down == BTN_EVT_REPEAT) return (int8_t)(-((int8_t)POWER_EDIT_FAST_STEP));

    if (evt_up == BTN_EVT_PRESS) return (int8_t)POWER_STEP;
    if (evt_down == BTN_EVT_PRESS) return (int8_t)(-((int8_t)POWER_STEP));

    return 0;
}

static uint8_t TuneBuckToVoltage01V(uint16_t target_01v)
{
    uint32_t start = HAL_GetTick();
    uint16_t duty = BuckDAC_DutyForVoltage01V(target_01v);
    uint16_t measured_01v = ADC_Control_GetVoltage01V();
    ADCFeedback_t feedback;

    MegasonicCtrl_SetDuty(duty);
    MegasonicCtrl_Update();

    while ((HAL_GetTick() - start) < TUNE_VOLTAGE_SETTLE_TIMEOUT_MS) {
        uint16_t diff_01v;
        uint16_t step;

        ADC_Control_SampleFeedback(&feedback,
                                   TUNE_FEEDBACK_SAMPLE_COUNT,
                                   TUNE_FEEDBACK_SAMPLE_DELAY_MS);
        measured_01v = ADC_Control_GetVoltage01V();
        s_autotune_power_01w = ADC_Control_GetIvPower01W();
        SetAutoTuneFeedback(ScoreFeedbackCandidate(&feedback), &feedback);
        if (AutoTuneFaultActive() != 0U) {
            return 0U;
        }
        SetAutoTuneState(TUNE_STATUS_PREPARE,
                         1U,
                         (uint8_t)ClampU16((uint16_t)(10U + ((HAL_GetTick() - start) / 120U)),
                                           10U,
                                           95U),
                         s_freq_edit_freq,
                         s_freq_edit_l_step);

        diff_01v = AbsDiffU16(measured_01v, target_01v);
        if (diff_01v <= TUNE_VOLTAGE_TOL_01V) {
            return 1U;
        }

        step = (diff_01v > 300U) ? 20U : 5U;
        if (measured_01v < target_01v) {
            duty = (uint16_t)ClampU16((uint16_t)(duty + step), DUTY_MIN, DUTY_CLAMP_MAX);
        } else {
            duty = (duty > step) ? (uint16_t)(duty - step) : DUTY_MIN;
        }

        MegasonicCtrl_SetDuty(duty);
        MegasonicCtrl_Update();
        if (DelayWithCurrentGuard(80U) == 0U) {
            return 0U;
        }
    }

    return 0U;
}

static uint16_t SamplePower01W(uint8_t samples, uint16_t delay_ms)
{
    ADCFeedback_t feedback;

    ADC_Control_SampleFeedback(&feedback, samples, delay_ms);
    return ADC_Control_GetIvPower01W();
}

static uint8_t CheckBaseVoltageCandidate(uint16_t command_voltage_01v,
                                         uint16_t target_power_01w,
                                         uint16_t *best_voltage_01v,
                                         uint16_t *best_error_01w,
                                         uint16_t *best_distance_01v)
{
    uint16_t power_01w;
    uint16_t error_01w;
    uint16_t measured_voltage_01v;
    uint16_t distance_01v;

    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(command_voltage_01v));
    MegasonicCtrl_Update();
    if (DelayWithCurrentGuard(220U) == 0U) {
        return 0U;
    }

    power_01w = SamplePower01W((uint8_t)(TUNE_FEEDBACK_SAMPLE_COUNT + 4U),
                               TUNE_FEEDBACK_SAMPLE_DELAY_MS);
    measured_voltage_01v = ADC_Control_GetVoltage01V();
    error_01w = AbsDiffU16(power_01w, target_power_01w);
    distance_01v = AbsDiffU16(command_voltage_01v, RUN_BASE_VOLTAGE_DEFAULT_01V);

    if ((error_01w < *best_error_01w)
        || ((error_01w == *best_error_01w) && (distance_01v < *best_distance_01v))) {
        *best_error_01w = error_01w;
        *best_distance_01v = distance_01v;
        *best_voltage_01v = measured_voltage_01v;
    }

    return 1U;
}

static uint16_t TuneBaseVoltageForLowPower(uint8_t ch, uint16_t freq_01khz, uint8_t l_step)
{
    uint16_t target_power = POWER_MIN;
    uint16_t best_voltage = RUN_BASE_VOLTAGE_DEFAULT_01V;
    uint16_t best_error = 0xFFFFU;
    uint16_t best_distance = 0xFFFFU;
    uint16_t offset = 0U;

    if (ch >= FREQ_EDIT_CH_COUNT) {
        return RUN_BASE_VOLTAGE_DEFAULT_01V;
    }

    MegasonicCtrl_SetFrequency(freq_01khz);
    MegasonicCtrl_SetLCRelay((uint8_t)(ClampU16(l_step, 1U, 16U) - 1U));

    while (offset <= (uint16_t)(RUN_BASE_VOLTAGE_MAX_01V - RUN_BASE_VOLTAGE_MIN_01V)) {
        uint8_t sampled = 0U;

        if (RUN_BASE_VOLTAGE_DEFAULT_01V >= (uint16_t)(RUN_BASE_VOLTAGE_MIN_01V + offset)) {
            uint16_t candidate = (uint16_t)(RUN_BASE_VOLTAGE_DEFAULT_01V - offset);
            if (CheckBaseVoltageCandidate(candidate,
                                          target_power,
                                          &best_voltage,
                                          &best_error,
                                          &best_distance) == 0U) {
                break;
            }
            sampled = 1U;
        }

        if ((offset != 0U)
            && ((uint16_t)(RUN_BASE_VOLTAGE_DEFAULT_01V + offset) <= RUN_BASE_VOLTAGE_MAX_01V)) {
            uint16_t candidate = (uint16_t)(RUN_BASE_VOLTAGE_DEFAULT_01V + offset);
            if (CheckBaseVoltageCandidate(candidate,
                                          target_power,
                                          &best_voltage,
                                          &best_error,
                                          &best_distance) == 0U) {
                break;
            }
            sampled = 1U;
        }

        if ((sampled == 0U) || (best_error <= RUN_POWER_TARGET_DEADBAND_01W)) {
            break;
        }
        offset = (uint16_t)(offset + RUN_BASE_VOLTAGE_STEP_01V);
    }

    return ClampBaseVoltage01V(best_voltage);
}

static uint8_t PrepareBuckVoltageTargetBeforeStart(uint16_t target_voltage_01v)
{
    uint32_t start = HAL_GetTick();
    uint16_t duty;
    uint16_t voltage_01v = ADC_Control_GetVoltage01V();

    if (target_voltage_01v < RUN_START_MIN_VOLT_01V) {
        target_voltage_01v = RUN_START_MIN_VOLT_01V;
    }
    if (target_voltage_01v > RUN_POWER_CONTROL_MAX_VOLT_01V) {
        target_voltage_01v = RUN_POWER_CONTROL_MAX_VOLT_01V;
    }

    duty = BuckDAC_DutyForVoltage01V(target_voltage_01v);

    MegasonicCtrl_PrechargeBuck(duty);

    while ((HAL_GetTick() - start) < RUN_START_PRECHARGE_TIMEOUT_MS) {
        uint32_t current_ma;

        ADC_Control_Process();
        voltage_01v = ADC_Control_GetVoltage01V();
        current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;

        if (current_ma >= RUN_HIGH_CUR_PROTECT_MA) {
            SetCurrentProtectionError();
            return 0U;
        }

        if (AbsDiffU16(voltage_01v, target_voltage_01v) <= TUNE_VOLTAGE_TOL_01V) {
            return 1U;
        }

        if (voltage_01v < target_voltage_01v) {
            duty = (uint16_t)ClampU16((uint16_t)(duty + RUN_START_PRECHARGE_STEP_01PCT),
                                      DUTY_MIN,
                                      DUTY_CLAMP_MAX);
            MegasonicCtrl_PrechargeBuck(duty);
        } else if (duty > DUTY_MIN) {
            duty = (duty > RUN_START_PRECHARGE_STEP_01PCT)
                 ? (uint16_t)(duty - RUN_START_PRECHARGE_STEP_01PCT)
                 : DUTY_MIN;
            MegasonicCtrl_PrechargeBuck(duty);
        }

        if (DelayWithCurrentGuard(RUN_START_PRECHARGE_SETTLE_MS) == 0U) {
            return 0U;
        }
    }

    ADC_Control_Process();
    voltage_01v = ADC_Control_GetVoltage01V();

    if (voltage_01v > (uint16_t)(target_voltage_01v + TUNE_VOLTAGE_TOL_01V)) {
        MegasonicCtrl_Stop();
        SetError(ERR_BUCK_OVERVOLT);
        return 0U;
    }

    if (AbsDiffU16(voltage_01v, target_voltage_01v) > TUNE_VOLTAGE_TOL_01V) {
        MegasonicCtrl_Stop();
        SetError(ERR_START_VOLT);
        return 0U;
    }

    return 1U;
}

static uint8_t PrepareBuckVoltageBeforeStart(void)
{
    /* 저장된 S값에 매핑된 목표 전압이 준비된 뒤에만 PWM을 시작한다. */
    return PrepareBuckVoltageTargetBeforeStart(RunTargetVoltage01V());
}

static void StartOutputCommon(void)
{
    ResetRunControlState();
    s_run_freq_manual_hold = 0U;
    ApplyPowerChannel(s_selected_power_ch);

    if (PrepareBuckVoltageBeforeStart() != 0U) {
        uint16_t start_power_01w;

        ADC_Control_Process();
        ADC_Control_CapturePowerZeroCurrent();
        MegasonicCtrl_Start();

        if (DelayWithCurrentGuard(RUN_START_QUICK_CHECK_MS) == 0U) {
            return;
        }

        start_power_01w = SamplePower01W(RunRetuneSampleCount(),
                                         RUN_RETUNE_SAMPLE_DELAY_MS);
        SetOutputPowerEstimateImmediate(start_power_01w);

        if (PowerProfileTuneValid(s_selected_power_ch) != 0U) {
            s_run_power_hold_active = 1U;
            s_run_retune_pending = 0U;
            s_run_retune_active = 0U;
            s_run_power_miss_count = 0U;
            s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
            return;
        }

        if (IsGuidedTuneChannel(s_selected_freq_ch) != 0U) {
            s_run_power_hold_active = 1U;
            s_run_retune_pending = 0U;
            s_run_retune_active = 0U;
            s_run_power_miss_count = 0U;
            s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
            return;
        }

        if (RunDutyOnlyGoodEnough(start_power_01w, s_output_set_01w) != 0U) {
            s_run_retune_pending = 0U;
            s_run_retune_active = 0U;
            s_run_power_miss_count = 0U;
            s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
            return;
        }

        s_run_retune_pending = 1U;
        s_run_retune_due_tick = HAL_GetTick();
        s_run_power_maintain_tick = 0U;
    }
}

uint8_t Menu_RequestOutputStart(void)
{
    if (s_error_code != ERR_NONE) {
        return 0U;
    }
    if (g_us_state.running != 0U) {
        return 1U;
    }
    if (IsSensorInputOk() == 0U) {
        SetError(ERR_SENSOR_OFF);
        return 0U;
    }

    ADC_Control_Process();
    if (CurrentSensorFaultActive() != 0U) {
        SetCurrentProtectionError();
        return 0U;
    }

    if (SaveStoreNow() == 0U) {
        PlayStoreFeedback(0U);
        return 0U;
    }

    StartOutputCommon();
    return (g_us_state.running != 0U) ? 1U : 0U;
}

static uint8_t StoreRunTunePointBySet(void)
{
    uint16_t measured_power_01w;
    uint8_t save_ok;

    if (g_us_state.running == 0U) return 0U;
    if (s_error_code != ERR_NONE) return 0U;
    if (s_autotune_running != 0U) return 0U;
    if (s_selected_power_ch >= POWER_8STEP_COUNT) return 0U;
    if (s_selected_freq_ch >= FREQ_EDIT_CH_COUNT) return 0U;
    if (StopIfOvercurrentNow() == 0U) return 0U;

    measured_power_01w = SamplePower01W(RunRetuneSampleCount(),
                                        RUN_RETUNE_SAMPLE_DELAY_MS);
    SetOutputPowerEstimateImmediate(measured_power_01w);

    StorePowerTunePoint(s_selected_power_ch,
                        s_selected_freq_ch,
                        g_us_state.target_freq,
                        MegasonicCtrl_GetRunGateDuty());
    MarkStoreDirty();
    save_ok = SaveStoreNow();
    if (save_ok != 0U) {
        SetRunRetuneSafePoint(g_us_state.target_freq,
                              MegasonicCtrl_GetRunGateDuty(),
                              ADC_Control_GetVoltage01V());
        s_run_power_hold_active = 1U;
        s_run_retune_pending = 0U;
        s_run_retune_active = 0U;
        s_run_power_miss_count = 0U;
        s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
        ResetRunFrequencyTracking();
    }
    return save_ok;
}

void Menu_RequestOutputStop(void)
{
    MegasonicCtrl_Stop();
    MegasonicCtrl_SetFrequency(s_freq_profiles[s_selected_freq_ch].freq_01khz);
    MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_profiles[s_selected_freq_ch].l_step - 1U));
    ADC_Control_ClearPowerZeroCurrent();
    s_run_freq_adjust_active = 0U;
    s_run_freq_manual_hold = 0U;
    s_remote_run_latched = 0U;
    ResetRunControlState();
    ApplyPowerChannel(s_selected_power_ch);
    if (s_store_dirty != 0U) {
        (void)SaveStoreNow();
    }
}

static uint8_t BaudToIndex(uint32_t baud)
{
    uint8_t i;

    for (i = 0U; i < MODBUS_BAUD_INDEX_COUNT; i++) {
        if (MODBUS_BAUD_TABLE[i] == baud) {
            return i;
        }
    }

    return 0U;
}

static uint8_t NormalizeRs485Parity(uint8_t parity)
{
    return (parity == MODBUS_PARITY_EVEN) ? MODBUS_PARITY_EVEN : MODBUS_PARITY_NONE;
}

static void BuildStoreData(SettingsStoreData_t *data)
{
    uint8_t i;

    data->selected_mode = s_selected_mode;
    data->selected_freq_ch = s_selected_freq_ch;
    data->selected_power_ch = s_selected_power_ch;

    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        data->freq_profiles[i] = s_freq_profiles[i];
    }

    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        data->power_profiles[i] = s_power_profiles[i];
    }

    data->modbus_addr = (uint8_t)ClampU16(g_modbus_cfg.address, MODBUS_ADDR_MIN, MODBUS_ADDR_MAX);
    data->modbus_baud_idx = BaudToIndex(g_modbus_cfg.baudrate);
    data->rs485_term_on = (s_rs485_term_enabled != 0U) ? 1U : 0U;
    data->rs485_parity = NormalizeRs485Parity(g_modbus_cfg.parity);
}

static void MarkStoreDirty(void)
{
    s_store_dirty = 1U;
    s_store_due_tick = HAL_GetTick() + MENU_FLASH_SAVE_DELAY_MS;
}

static void CommitFreqEditBuffer(void);
static void CommitPowerEditBuffer(void);
static void SaveRs485EditBuffer(void);
static void StartClickBeep(void);
static void StartLongPressBeep(void);
static void StartAlarmBeep(void);

static uint8_t SaveStoreNow(void)
{
    SettingsStoreData_t data;
    uint8_t was_running = 0U;

    if (s_state == MENU_STATE_SETTING_FREQ) {
        CommitFreqEditBuffer();
    } else if (s_state == MENU_STATE_SETTING_POWER) {
        CommitPowerEditBuffer();
    } else if (s_state == MENU_STATE_SETTING_RS485) {
        SaveRs485EditBuffer();
    }
    BuildStoreData(&data);

    if (g_us_state.running != 0U) {
        was_running = 1U;
        MegasonicCtrl_Stop();
    }

    if (SettingsStore_Save(&data) != 0U) {
        s_store_dirty = 0U;
        s_store_fail_count = 0U;
        s_saved_mode_shadow = s_selected_mode;
        s_saved_freq_ch_shadow = s_selected_freq_ch;
        s_saved_power_ch_shadow = s_selected_power_ch;
        if (was_running != 0U) {
            MegasonicCtrl_PrechargeBuck(BuckDAC_DutyForVoltage01V(RunTargetVoltage01V()));
            MegasonicCtrl_Start();
        }
        return 1U;
    }

    s_store_dirty = 1U;
    if (s_store_fail_count < 255U) {
        s_store_fail_count++;
    }

    if (was_running != 0U) {
        MegasonicCtrl_PrechargeBuck(BuckDAC_DutyForVoltage01V(RunTargetVoltage01V()));
        MegasonicCtrl_Start();
    }
    return 0U;
}

static void ServiceStoreFlush(void)
{
    uint8_t need_save;
    uint32_t backoff;

    if (g_us_state.running != 0U) {
        return;
    }

    /* 설정 편집 중 UP/DOWN마다 저장하면 seq만 올라가고 값이 덮어써진다.
     * SET / 설정 메뉴 퇴장 / ForceStoreFlush 경로에서만 저장한다. */
    if ((s_state == MENU_STATE_SETTING_MENU)
        || (s_state == MENU_STATE_SETTING_FREQ)
        || (s_state == MENU_STATE_SETTING_POWER)
        || (s_state == MENU_STATE_SETTING_RS485)) {
        return;
    }

    need_save = (s_store_dirty != 0U) ? 1U : 0U;
    if ((s_selected_mode != s_saved_mode_shadow)
        || (s_selected_freq_ch != s_saved_freq_ch_shadow)
        || (s_selected_power_ch != s_saved_power_ch_shadow)) {
        need_save = 1U;
    }

    if (need_save == 0U) {
        return;
    }

    if ((int32_t)(HAL_GetTick() - s_store_due_tick) < 0) {
        return;
    }

    if (SaveStoreNow() == 0U) {
        /* 실패할수록 재시도 간격을 늘림: 100, 500, 2000, 5000, 10000 ms */
        backoff = (uint32_t)s_store_fail_count * (uint32_t)s_store_fail_count * 500UL;
        if (backoff < MENU_FLASH_SAVE_DELAY_MS) {
            backoff = MENU_FLASH_SAVE_DELAY_MS;
        }
        s_store_due_tick = HAL_GetTick() + backoff;
    }
}

static void PlayStoreFeedback(uint8_t save_ok)
{
    uint8_t count = (save_ok != 0U) ? 2U : 3U;
    uint16_t on_ms = (save_ok != 0U) ? 80U : 150U;
    uint8_t i;

    s_click_beep_on = 0U;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    for (i = 0U; i < count; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        HAL_Delay(on_ms);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        if ((uint8_t)(i + 1U) < count) {
            HAL_Delay(80U);
        }
    }
}

static uint8_t ForceStoreFlush(void)
{
    uint8_t ok = SaveStoreNow();

    PlayStoreFeedback(ok);
    return ok;
}

static void LoadStoredDataIfValid(void)
{
    SettingsStoreData_t data;
    uint8_t i;
    uint8_t j;

    if (SettingsStore_Load(&data) == 0U) {
        return;
    }

    s_selected_mode = (data.selected_mode < MODE_COUNT)
        ? data.selected_mode
        : MODE_NORMAL;

    s_selected_freq_ch = (uint8_t)ClampU16(data.selected_freq_ch,
                                           0U,
                                           (uint16_t)(FREQ_EDIT_CH_COUNT - 1U));
    s_selected_power_ch = (uint8_t)ClampU16(data.selected_power_ch,
                                            0U,
                                            (uint16_t)(POWER_8STEP_COUNT - 1U));

    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        s_freq_profiles[i].freq_01khz = ClampU16(data.freq_profiles[i].freq_01khz,
                                                 FREQ_EDIT_MIN,
                                                 FREQ_EDIT_MAX);
        s_freq_profiles[i].base_voltage_01v = ClampBaseVoltage01V(data.freq_profiles[i].base_voltage_01v);
        s_freq_profiles[i].l_step = (uint8_t)ClampU16(data.freq_profiles[i].l_step,
                                                      1U,
                                                      16U);

        if (IsGuidedTuneChannel(i) != 0U) {
            uint16_t limit_lo;
            uint16_t limit_hi;
            uint16_t nominal_freq = s_freq_presets_01khz[i];

            GetGuidedTuneLimit(i, &limit_lo, &limit_hi);
            if ((s_freq_profiles[i].freq_01khz < limit_lo)
                || (s_freq_profiles[i].freq_01khz > limit_hi)
                || (s_freq_profiles[i].freq_01khz > nominal_freq)) {
                s_freq_profiles[i].freq_01khz = nominal_freq;
                s_power1_default_migrated = 1U;
            }
        }

        /* 이전 튜닝값이 Flash에 남아 있으면 새 기본 테이블이 반영되지 않는다.
         * CH04는 1MHz 계열 기본 LC로 1111(l_step=16)을 사용한다. */
        if ((i == 3U) && (s_freq_profiles[i].l_step != 16U)) {
            s_freq_profiles[i].l_step = 16U;
            s_power1_default_migrated = 1U;
        }
    }

    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        uint16_t raw_low = data.power_profiles[i].low_01w;
        uint16_t raw_high = data.power_profiles[i].high_01w;
        uint16_t raw_def = data.power_profiles[i].def_01w;
        uint16_t default_def = PowerProfileDefaultForChannel(i);
        uint16_t tuned_freq = data.power_profiles[i].tuned_freq_01khz;
        uint16_t tuned_gate = data.power_profiles[i].tuned_gate_duty_01pct;
        uint8_t tuned_freq_ch = data.power_profiles[i].tuned_freq_ch;
        uint8_t tuned_valid = data.power_profiles[i].tuned_valid;
        uint8_t looks_old_default;
        uint16_t low = ClampU16(raw_low,
                                POWER_PROFILE_LOW_MIN,
                                POWER_PROFILE_LOW_MAX);
        uint16_t high = ClampU16(raw_high,
                                 POWER_PROFILE_HIGH_MIN,
                                 POWER_PROFILE_HIGH_MAX);
        uint16_t def = ClampU16(raw_def,
                                POWER_PROFILE_DEF_MIN,
                                POWER_PROFILE_DEF_MAX);

        looks_old_default = ((raw_low <= 20U)
                             && ((raw_high > POWER_PROFILE_HIGH_MAX)
                                 || (raw_def == 50U)
                                 || (raw_def == 60U))
                             && ((raw_def == 50U)
                                 || (raw_def == 60U)
                                 || (raw_def == POWER_PROFILE_HIGH_MAX)
                                 || (raw_def == POWER_PROFILE_HIGH_LEGACY_MAX)
                                 || (raw_def == POWER_PROFILE_DEF_MAX))) ? 1U : 0U;

        if (looks_old_default != 0U) {
            low = POWER_PROFILE_LOW_DEFAULT;
            high = POWER_PROFILE_HIGH_DEFAULT;
            def = default_def;
            s_power1_default_migrated = 1U;
        }

        if (low != POWER_PROFILE_LOW_DEFAULT) {
            low = POWER_PROFILE_LOW_DEFAULT;
            s_power1_default_migrated = 1U;
        }
        if (high < POWER_PROFILE_HIGH_DEFAULT) {
            high = POWER_PROFILE_HIGH_DEFAULT;
            s_power1_default_migrated = 1U;
        }
        if (def != default_def) {
            def = default_def;
            s_power1_default_migrated = 1U;
        }

        if (low >= high) {
            high = (uint16_t)(low + 1U);
            if (high > POWER_PROFILE_HIGH_MAX) {
                high = POWER_PROFILE_HIGH_MAX;
                low = (uint16_t)(high - 1U);
            }
        }
        if (def < low) def = low;
        if (def > high) def = high;

        s_power_profiles[i].low_01w = low;
        s_power_profiles[i].high_01w = high;
        s_power_profiles[i].def_01w = def;
        ClearPowerTunePoints(i);

        for (j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            if ((data.power_profiles[i].tuned_valid_mask & (uint16_t)(1U << j)) != 0U) {
                uint8_t stale_nominal_tune = 0U;
                int32_t raw_freq = (int32_t)s_freq_profiles[j].freq_01khz
                                 + (int32_t)data.power_profiles[i].tuned_freq_offset_01khz[j];
                uint16_t freq = (raw_freq < (int32_t)FREQ_EDIT_MIN)
                              ? FREQ_EDIT_MIN
                              : ((raw_freq > (int32_t)FREQ_EDIT_MAX)
                                 ? FREQ_EDIT_MAX
                                 : (uint16_t)raw_freq);
                uint16_t duty = PowerTuneDuty01PctFromCode(
                    data.power_profiles[i].tuned_gate_duty_02pct[j]);

                if (StoredPowerFreqLooksStaleNominal(j, freq) != 0U) {
                    stale_nominal_tune = 1U;
                    s_power1_default_migrated = 1U;
                }

                if (stale_nominal_tune == 0U) {
                    StorePowerTunePoint(i, j, freq, duty);
                }
            }
        }

        if ((tuned_valid != 0U)
            && (tuned_freq_ch < FREQ_EDIT_CH_COUNT)
            && ((data.power_profiles[i].tuned_valid_mask & (uint16_t)(1U << tuned_freq_ch)) == 0U)
            && (tuned_freq >= FREQ_EDIT_MIN)
            && (tuned_freq <= FREQ_EDIT_MAX)
            && (tuned_gate >= RUN_GATE_DUTY_TUNE_MIN_01PCT)
            && (tuned_gate <= RUN_GATE_DUTY_TUNE_MAX_01PCT)) {
            uint8_t legacy_nominal_stale = 0U;

            if (StoredPowerFreqLooksStaleNominal(tuned_freq_ch, tuned_freq) != 0U) {
                legacy_nominal_stale = 1U;
            }

            if (legacy_nominal_stale == 0U) {
                StorePowerTunePoint(i, tuned_freq_ch, tuned_freq, tuned_gate);
            }
        }
    }

    s_rs485_addr = (uint8_t)ClampU16(data.modbus_addr, MODBUS_ADDR_MIN, MODBUS_ADDR_MAX);
    s_rs485_baud_idx = (data.modbus_baud_idx < MODBUS_BAUD_INDEX_COUNT)
        ? data.modbus_baud_idx
        : 0U;
    s_rs485_term_on = (data.rs485_term_on != 0U) ? 1U : 0U;
    s_rs485_term_enabled = s_rs485_term_on;
    s_rs485_parity = NormalizeRs485Parity(data.rs485_parity);

    g_modbus_cfg.address = s_rs485_addr;
    g_modbus_cfg.baudrate = MODBUS_BAUD_TABLE[s_rs485_baud_idx];
    g_modbus_cfg.parity = s_rs485_parity;

}

static void StartAlarmBeep(void)
{
    s_alarm_beep_on = 1U;
    s_alarm_beep_off_tick = HAL_GetTick() + MENU_BEEP_ALARM_MS;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

static void StartClickBeep(void)
{
    s_click_beep_on = 1U;
    s_click_beep_off_tick = HAL_GetTick() + MENU_BEEP_CLICK_MS;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

static void StartLongPressBeep(void)
{
    s_click_beep_on = 1U;
    s_click_beep_off_tick = HAL_GetTick() + MENU_BEEP_LONG_MS;
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
}

static void ServiceAlarmBeep(void)
{
    if ((s_alarm_beep_on != 0U) && ((int32_t)(HAL_GetTick() - s_alarm_beep_off_tick) >= 0)) {
        s_alarm_beep_on = 0U;
    }
}

static void ServiceClickBeep(void)
{
    if ((s_click_beep_on != 0U) && ((int32_t)(HAL_GetTick() - s_click_beep_off_tick) >= 0)) {
        s_click_beep_on = 0U;
    }
}

static void ServiceSharedBuzzer(void)
{
    uint8_t buzzer_on = 0U;

    if ((s_alarm_beep_on != 0U) || (s_click_beep_on != 0U)) {
        buzzer_on = 1U;
    }

    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN,
                      (buzzer_on != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void EnforceRunUiLock(void)
{
    if ((g_us_state.running != 0U)
        && ((s_state != MENU_STATE_SELECT) || (s_select_mode_active != 0U))) {
        s_state = MENU_STATE_SELECT;
        s_select_mode_active = 0U;
        s_autotune_armed = 0U;
        MenuScreen_ForceRefresh();
    }
}

static void ApplySelectedMode(void)
{
    MegasonicCtrl_SetMode(s_selected_mode);
}

static void ApplyFreqChannel(uint8_t ch)
{
    if (ch >= FREQ_EDIT_CH_COUNT) return;

    s_selected_freq_ch = ch;
    MegasonicCtrl_SetFrequency(s_freq_profiles[ch].freq_01khz);
    MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_profiles[ch].l_step - 1U));
    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(RunTargetVoltage01V()));
    s_run_power_hold_active = 0U;
    s_run_freq_manual_hold = 0U;
    ResetRunControlState();
}
static void ApplyFreqEditPreview(void)
{
    MegasonicCtrl_SetFrequency(s_freq_edit_freq);
    MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_edit_l_step - 1U));
}

static void ApplyPowerChannel(uint8_t ch)
{
    const PowerChannelProfile_t *p;
    uint8_t tune_valid;

    if (ch >= POWER_8STEP_COUNT) return;

    p = &s_power_profiles[ch];
    tune_valid = PowerProfileTuneValid(ch);
    s_selected_power_ch = ch;
    s_output_set_01w = p->def_01w;
    s_run_power_hold_active = 0U;
    s_run_freq_manual_hold = 0U;
    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(RunTargetVoltage01V()));
    if (tune_valid != 0U) {
        MegasonicCtrl_SetFrequency(PowerTuneFreqForChannel(ch, s_selected_freq_ch));
        MegasonicCtrl_SetRunGateDuty(PowerTuneDutyForChannel(ch, s_selected_freq_ch));
        if (g_us_state.running != 0U) {
            s_run_power_hold_active = 1U;
            s_run_retune_pending = 0U;
            s_run_power_miss_count = 0U;
            s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
        }
    } else {
        MegasonicCtrl_SetFrequency(s_freq_profiles[s_selected_freq_ch].freq_01khz);
        MegasonicCtrl_ResetRunGateDuty();
    }
    HoldOutputPowerDisplay();
    s_run_power_control_tick = 0U;
    if (tune_valid == 0U) {
        ScheduleRunRetune();
    }
}

static uint8_t AdjustRunOutput(int8_t delta)
{
    const PowerChannelProfile_t *p = &s_power_profiles[s_selected_power_ch];
    uint16_t prev = s_output_set_01w;
    int32_t next;

    if (delta == 0) {
        return 0U;
    }

    next = (int32_t)s_output_set_01w + ((int32_t)delta * (int32_t)RUN_POWER_STEP_01W);
    s_output_set_01w = ClampU16((uint16_t)((next < 0) ? 0 : next),
                                p->low_01w,
                                POWER_PROFILE_HIGH_MAX);
    if (s_output_set_01w == prev) {
        return 0U;
    }

    s_run_power_hold_active = 0U;
    s_run_freq_manual_hold = 0U;
    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(RunTargetVoltage01V()));
    HoldOutputPowerDisplay();
    s_run_power_control_tick = 0U;
    ScheduleRunRetune();
    MenuScreen_ForceRefresh();
    return 1U;
}

static uint8_t AdjustRunFrequency(int8_t delta)
{
    uint16_t prev;
    uint16_t next_freq;
    int32_t next;

    if (delta == 0) {
        return 0U;
    }

    prev = g_us_state.target_freq;
    next = (int32_t)prev + ((int32_t)delta * (int32_t)FREQ_STEP);
    next_freq = ClampU16((uint16_t)((next < 0) ? 0 : next), FREQ_EDIT_MIN, FREQ_EDIT_MAX);
    if (next_freq == prev) {
        return 0U;
    }

    MegasonicCtrl_SetFrequency(next_freq);
    ResetRunControlState();
    s_run_freq_manual_hold = 1U;
    MenuScreen_ForceRefresh();
    return 1U;
}

static void SetError(ErrorCode_t err)
{
    if (err == ERR_NONE) return;
    if (s_error_code == err) return;

    s_low_alarm_pending = 0U;
    s_high_alarm_pending = 0U;
    s_error_code = err;
    MegasonicCtrl_EmergencyStop();
    StartAlarmBeep();
    MenuScreen_ForceRefresh();
}

static void ClearError(void)
{
    s_error_code = ERR_NONE;
    s_alarm_beep_on = 0U;
    s_low_alarm_pending = 0U;
    s_high_alarm_pending = 0U;
    s_overcurrent_latch_ma = 0U;
    s_overcurrent_latch_voltage_01v = 0U;
}

static void LoadFreqEditBuffer(uint8_t ch)
{
    if (ch >= FREQ_EDIT_CH_COUNT) ch = (uint8_t)(FREQ_EDIT_CH_COUNT - 1U);

    s_freq_edit_ch = ch;
    s_freq_edit_freq = s_freq_profiles[ch].freq_01khz;
    s_freq_edit_l_step = s_freq_profiles[ch].l_step;
}

static void LoadPowerEditBuffer(uint8_t ch)
{
    if (ch >= POWER_8STEP_COUNT) ch = (uint8_t)(POWER_8STEP_COUNT - 1U);

    s_power_edit_ch = ch;
    s_power_edit_low = s_power_profiles[ch].low_01w;
    s_power_edit_high = s_power_profiles[ch].high_01w;
    s_power_edit_def = s_power_profiles[ch].def_01w;
}

static void LoadRs485EditBuffer(void)
{
    s_rs485_baud_idx = BaudToIndex(g_modbus_cfg.baudrate);
    s_rs485_addr = (uint8_t)ClampU16(g_modbus_cfg.address, MODBUS_ADDR_MIN, MODBUS_ADDR_MAX);
    s_rs485_parity = NormalizeRs485Parity(g_modbus_cfg.parity);
    s_rs485_term_on = (s_rs485_term_enabled != 0U) ? 1U : 0U;
}

static void CommitFreqEditBuffer(void)
{
    uint8_t ch = s_freq_edit_ch;

    s_freq_profiles[ch].freq_01khz =
        ClampU16(s_freq_edit_freq, FREQ_EDIT_MIN, FREQ_EDIT_MAX);
    s_freq_profiles[ch].l_step =
        (uint8_t)ClampU16(s_freq_edit_l_step, 1U, 16U);
    s_edit_saved = 1U;
    MarkStoreDirty();
}

static void SaveFreqEditBuffer(void)
{
    CommitFreqEditBuffer();
    ApplyFreqChannel(s_freq_edit_ch);
}

static void CommitPowerEditBuffer(void)
{
    uint8_t ch = s_power_edit_ch;
    uint16_t old_def = s_power_profiles[ch].def_01w;

    s_power_edit_low = ClampU16(s_power_edit_low, POWER_PROFILE_LOW_MIN, POWER_PROFILE_LOW_MAX);
    s_power_edit_high = ClampU16(s_power_edit_high, POWER_PROFILE_HIGH_MIN, POWER_PROFILE_HIGH_MAX);
    s_power_edit_high = POWER_PROFILE_HIGH_MAX;

    if (s_power_edit_low >= s_power_edit_high) {
        s_power_edit_high = (uint16_t)(s_power_edit_low + 1U);
        if (s_power_edit_high > POWER_PROFILE_HIGH_MAX) {
            s_power_edit_high = POWER_PROFILE_HIGH_MAX;
            s_power_edit_low = (uint16_t)(s_power_edit_high - 1U);
        }
    }

    s_power_edit_def = ClampU16(s_power_edit_def, POWER_PROFILE_DEF_MIN, POWER_PROFILE_DEF_MAX);
    if (s_power_edit_def < s_power_edit_low) s_power_edit_def = s_power_edit_low;
    if (s_power_edit_def > s_power_edit_high) s_power_edit_def = s_power_edit_high;

    s_power_profiles[ch].low_01w = s_power_edit_low;
    s_power_profiles[ch].high_01w = s_power_edit_high;
    s_power_profiles[ch].def_01w = s_power_edit_def;
    if (s_power_edit_def != old_def) {
        ClearPowerTunePoints(ch);
    }
    s_edit_saved = 1U;
    MarkStoreDirty();
}

static void SavePowerEditBuffer(void)
{
    CommitPowerEditBuffer();
    if (s_power_edit_ch == s_selected_power_ch) {
        ApplyPowerChannel(s_power_edit_ch);
    }
}

static void SaveRs485EditBuffer(void)
{
    uint8_t need_uart_reconfig = 0U;
    uint32_t new_baud = MODBUS_BAUD_TABLE[s_rs485_baud_idx];
    uint8_t new_parity = NormalizeRs485Parity(s_rs485_parity);

    if (g_modbus_cfg.baudrate != new_baud) {
        g_modbus_cfg.baudrate = new_baud;
        need_uart_reconfig = 1U;
    }

    if (g_modbus_cfg.parity != new_parity) {
        g_modbus_cfg.parity = new_parity;
        need_uart_reconfig = 1U;
    }

    g_modbus_cfg.address = (uint8_t)ClampU16(s_rs485_addr, MODBUS_ADDR_MIN, MODBUS_ADDR_MAX);
    s_rs485_term_enabled = (s_rs485_term_on != 0U) ? 1U : 0U;

    if (need_uart_reconfig != 0U) {
        Modbus_ReconfigUART();
    }

    s_edit_saved = 1U;
    MarkStoreDirty();
}

static void RunAutoTuneAndSave(void)
{
    uint8_t ch = s_freq_edit_ch;
    uint32_t start_tick = HAL_GetTick();
    uint8_t timed_out = 0U;
    uint16_t best_freq_score = 0xFFFFU;
    uint16_t best_l_score = 0xFFFFU;
    uint32_t final_score;
    uint16_t best_freq = s_freq_edit_freq;
    uint8_t best_l = s_freq_edit_l_step;
    uint8_t tune_valid;
    uint16_t backup_freq = s_freq_edit_freq;
    uint8_t backup_l = s_freq_edit_l_step;
    uint8_t tune_l = ((s_freq_field == FREQ_EDIT_FIELD_CH)
                      || (s_freq_field == FREQ_EDIT_FIELD_L)) ? 1U : 0U;
    uint8_t tune_freq = ((s_freq_field == FREQ_EDIT_FIELD_CH)
                         || (s_freq_field == FREQ_EDIT_FIELD_FREQ)) ? 1U : 0U;
    uint8_t was_running = (g_us_state.running != 0U) ? 1U : 0U;
    uint16_t coarse_lo;
    uint16_t coarse_hi;
    uint16_t fine_lo;
    uint16_t fine_hi;
    uint16_t score;
    uint16_t guided_preferred_freq;
    uint8_t guided_tune;
    uint16_t point_idx;
    uint16_t point_count;
    uint8_t l;
    uint8_t l_lo;
    uint8_t l_hi;
    int32_t probe_freq;
    ADCFeedback_t feedback;
    ADCFeedback_t best_freq_feedback = {0U, 0U, 0U, 0U};
    ADCFeedback_t best_l_feedback = {0U, 0U, 0U, 0U};
    uint16_t scan_freq_tbl[TUNE_SCAN_POINT_MAX];
    uint16_t scan_score_tbl[TUNE_SCAN_POINT_MAX];
    ADCFeedback_t scan_feedback_tbl[TUNE_SCAN_POINT_MAX];
    uint16_t scan_count;

    guided_tune = IsGuidedTuneChannel(ch);
    guided_preferred_freq = (guided_tune != 0U) ? s_freq_presets_01khz[ch] : s_freq_edit_freq;
    s_phase_tune_prompt = 0U;
    s_autotune_power_01w = 0U;
    s_autotune_work_voltage_01v = TUNE_TEST_VOLTAGE_01V;
    MegasonicCtrl_SetFrequency(s_freq_edit_freq);
    MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_edit_l_step - 1U));
    MegasonicCtrl_SetDuty(RUN_BUCK_DUTY_MIN_01PCT);

    if (was_running == 0U) {
        if (PrepareBuckVoltageTargetBeforeStart(TUNE_TEST_VOLTAGE_01V) == 0U) {
            RestoreAutoTuneOutput(was_running);
            SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
            return;
        }
        ADC_Control_Process();
        ADC_Control_CapturePowerZeroCurrent();
        MegasonicCtrl_Start();
    }
    MegasonicCtrl_ForceRunGateDuty();

    SetAutoTuneState(TUNE_STATUS_PREPARE, 1U, 1U, s_freq_edit_freq, s_freq_edit_l_step);
    WaitAutoTuneOutputReady();
    if (TuneBuckToVoltage01V(TUNE_TEST_VOLTAGE_01V) == 0U) {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
        return;
    }

    /* 1~2단계: 주파수 튜닝 */
    if ((timed_out == 0U) && (tune_freq != 0U)) {
        GetAutoTuneCoarseRange(ch, s_freq_edit_freq, &coarse_lo, &coarse_hi);

        MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_edit_l_step - 1U));
        best_freq_score = 0xFFFFU;
        point_count = (uint16_t)(((coarse_hi - coarse_lo) / TUNE_FREQ_COARSE_STEP_01KHZ) + 1U);
        point_idx = 0U;
        for (probe_freq = (int32_t)coarse_hi;
             probe_freq >= (int32_t)coarse_lo;
             probe_freq -= (int32_t)TUNE_FREQ_COARSE_STEP_01KHZ) {
            uint16_t f = ClampFreq01kHz((uint16_t)probe_freq);

            if (IsAutoTuneTimedOut(start_tick) != 0U) {
                timed_out = 1U;
                break;
            }

            MegasonicCtrl_SetFrequency(f);
            if (DelayWithCurrentGuard(TUNE_FREQ_SETTLE_MS) == 0U) {
                timed_out = 1U;
                break;
            }

            ADC_Control_SampleFeedback(&feedback,
                                       TUNE_FEEDBACK_SAMPLE_COUNT,
                                       TUNE_FEEDBACK_SAMPLE_DELAY_MS);
            score = ScoreFeedbackCandidate(&feedback);
            SetAutoTuneFeedback(score, &feedback);
            if (AutoTuneFaultActive() != 0U) {
                timed_out = 1U;
                break;
            }
            if (TuneCandidateIsBetter(guided_tune,
                                      f,
                                      score,
                                      best_freq,
                                      best_freq_score,
                                      guided_preferred_freq) != 0U) {
                best_freq_score = score;
                best_freq = f;
                best_freq_feedback = feedback;
            }

            point_idx++;
            SetAutoTuneState(TUNE_STATUS_SCAN_FREQ,
                             1U,
                             (uint8_t)((tune_l != 0U)
                                        ? (5U + ((point_idx * 25U) / point_count))
                                        : (5U + ((point_idx * 45U) / point_count))),
                             f,
                             s_freq_edit_l_step);
        }
    } else {
        best_freq_score = 0U;
    }

    if ((timed_out == 0U) && (tune_freq != 0U)) {
        uint16_t rough_best_freq = best_freq;

        best_freq_score = 0xFFFFU;
        GetAutoTuneFineRange(ch, rough_best_freq, &fine_lo, &fine_hi);
        point_count = (uint16_t)(((fine_hi - fine_lo) / TUNE_FREQ_FINE_STEP_01KHZ) + 1U);
        if (point_count > TUNE_SCAN_POINT_MAX) {
            point_count = TUNE_SCAN_POINT_MAX;
        }
        point_idx = 0U;
        scan_count = 0U;

        MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_edit_l_step - 1U));
        for (probe_freq = (int32_t)fine_hi;
             (probe_freq >= (int32_t)fine_lo) && (scan_count < point_count);
             probe_freq -= (int32_t)TUNE_FREQ_FINE_STEP_01KHZ) {
            uint16_t f = ClampFreq01kHz((uint16_t)probe_freq);

            if (IsAutoTuneTimedOut(start_tick) != 0U) {
                timed_out = 1U;
                break;
            }

            MegasonicCtrl_SetFrequency(f);
            if (DelayWithCurrentGuard(TUNE_FREQ_SETTLE_MS) == 0U) {
                timed_out = 1U;
                break;
            }

            ADC_Control_SampleFeedback(&feedback,
                                       TUNE_FEEDBACK_SAMPLE_COUNT,
                                       TUNE_FEEDBACK_SAMPLE_DELAY_MS);
            score = ScoreFeedbackCandidate(&feedback);
            scan_freq_tbl[scan_count] = f;
            scan_score_tbl[scan_count] = score;
            scan_feedback_tbl[scan_count] = feedback;
            scan_count++;
            SetAutoTuneFeedback(score, &feedback);
            if (AutoTuneFaultActive() != 0U) {
                timed_out = 1U;
                break;
            }
            if (TuneCandidateIsBetter(guided_tune,
                                      f,
                                      score,
                                      best_freq,
                                      best_freq_score,
                                      guided_preferred_freq) != 0U) {
                best_freq_score = score;
                best_freq = f;
                best_freq_feedback = feedback;
            }

            point_idx++;
            SetAutoTuneState(TUNE_STATUS_FINE_FREQ,
                             1U,
                             (uint8_t)((tune_l != 0U)
                                        ? (31U + ((point_idx * 29U) / point_count))
                                        : (51U + ((point_idx * 48U) / point_count))),
                             f,
                             s_freq_edit_l_step);
        }

        if (scan_count != 0U) {
            uint16_t stable_idx = StableValleyIndex(scan_score_tbl,
                                                    scan_freq_tbl,
                                                    scan_count,
                                                    (guided_tune != 0U) ? guided_preferred_freq : rough_best_freq);
            best_freq_score = scan_score_tbl[stable_idx];
            best_freq = scan_freq_tbl[stable_idx];
            best_freq_feedback = scan_feedback_tbl[stable_idx];
        }
    }

    /* 3~4단계: 인덕턴스 튜닝 */
    if ((timed_out == 0U) && (tune_l != 0U)) {
        best_l_score = 0xFFFFU;
        MegasonicCtrl_SetFrequency(best_freq);
        point_count = 16U;
        point_idx = 0U;

        for (l = 1U; l <= 16U; l++) {
            if (IsAutoTuneTimedOut(start_tick) != 0U) {
                timed_out = 1U;
                break;
            }

            if (SwitchLCRelayForTune(l, TUNE_L_SETTLE_MS) == 0U) {
                timed_out = 1U;
                break;
            }

            ADC_Control_SampleFeedback(&feedback,
                                       TUNE_FEEDBACK_SAMPLE_COUNT,
                                       TUNE_FEEDBACK_SAMPLE_DELAY_MS);
            score = ScoreFeedbackCandidate(&feedback);
            SetAutoTuneFeedback(score, &feedback);
            if (AutoTuneFaultActive() != 0U) {
                timed_out = 1U;
                break;
            }
            if (score < best_l_score) {
                best_l_score = score;
                best_l = l;
                best_l_feedback = feedback;
            }

            point_idx++;
            SetAutoTuneState(TUNE_STATUS_SCAN_L,
                             1U,
                             (uint8_t)((tune_freq != 0U)
                                        ? (61U + ((point_idx * 19U) / point_count))
                                        : (5U + ((point_idx * 45U) / point_count))),
                             best_freq,
                             l);
        }
    } else {
        best_l_score = 0U;
    }

    if ((timed_out == 0U) && (tune_l != 0U)) {
        uint8_t rough_best_l = best_l;

        best_l_score = 0xFFFFU;
        MegasonicCtrl_SetFrequency(best_freq);

        l_lo = (uint8_t)((rough_best_l > TUNE_L_FINE_NEIGHBOR_RANGE)
                ? (rough_best_l - TUNE_L_FINE_NEIGHBOR_RANGE) : 1U);
        l_hi = (uint8_t)((rough_best_l + TUNE_L_FINE_NEIGHBOR_RANGE <= 16U)
                ? (rough_best_l + TUNE_L_FINE_NEIGHBOR_RANGE) : 16U);
        point_count = (uint16_t)(l_hi - l_lo + 1U);
        point_idx = 0U;

        for (l = l_lo; l <= l_hi; l++) {
            if (IsAutoTuneTimedOut(start_tick) != 0U) {
                timed_out = 1U;
                break;
            }

            if (SwitchLCRelayForTune(l,
                                     (uint32_t)TUNE_L_SETTLE_MS
                                     + (uint32_t)TUNE_L_FINE_EXTRA_SETTLE_MS) == 0U) {
                timed_out = 1U;
                break;
            }

            ADC_Control_SampleFeedback(&feedback,
                                       TUNE_FEEDBACK_SAMPLE_COUNT,
                                       TUNE_FEEDBACK_SAMPLE_DELAY_MS);
            score = ScoreFeedbackCandidate(&feedback);
            SetAutoTuneFeedback(score, &feedback);
            if (AutoTuneFaultActive() != 0U) {
                timed_out = 1U;
                break;
            }
            if (score < best_l_score) {
                best_l_score = score;
                best_l = l;
                best_l_feedback = feedback;
            }

            point_idx++;
            SetAutoTuneState(TUNE_STATUS_FINE_L,
                             1U,
                             (uint8_t)((tune_freq != 0U)
                                        ? (81U + ((point_idx * 9U) / point_count))
                                        : (51U + ((point_idx * 48U) / point_count))),
                             best_freq,
                             l);
        }
    }

    /* 최종 보정: 주파수와 LC를 동시에 튜닝하는 경우에만 사용한다. */
    if ((timed_out == 0U) && (tune_freq != 0U) && (tune_l != 0U)) {
        uint16_t rough_best_freq = best_freq;

        best_freq_score = 0xFFFFU;
        if (SwitchLCRelayForTune(best_l,
                                 (uint32_t)TUNE_L_SETTLE_MS
                                 + (uint32_t)TUNE_L_FINE_EXTRA_SETTLE_MS) == 0U) {
            timed_out = 1U;
        }

        fine_lo = ClampFreq01kHz((rough_best_freq > TUNE_FREQ_FINAL_RANGE_01KHZ)
                               ? (uint16_t)(rough_best_freq - TUNE_FREQ_FINAL_RANGE_01KHZ)
                               : FREQ_EDIT_MIN);
        fine_hi = ClampFreq01kHz((uint16_t)(rough_best_freq + TUNE_FREQ_FINAL_RANGE_01KHZ));
        point_count = (uint16_t)(((fine_hi - fine_lo) / TUNE_FREQ_FINAL_STEP_01KHZ) + 1U);
        if (point_count > TUNE_SCAN_POINT_MAX) {
            point_count = TUNE_SCAN_POINT_MAX;
        }
        point_idx = 0U;
        scan_count = 0U;

        for (probe_freq = (int32_t)fine_hi;
             (timed_out == 0U) && (probe_freq >= (int32_t)fine_lo) && (scan_count < point_count);
             probe_freq -= (int32_t)TUNE_FREQ_FINAL_STEP_01KHZ) {
            uint16_t f = ClampFreq01kHz((uint16_t)probe_freq);

            if (IsAutoTuneTimedOut(start_tick) != 0U) {
                timed_out = 1U;
                break;
            }

            MegasonicCtrl_SetFrequency(f);
            if (DelayWithCurrentGuard(TUNE_FREQ_SETTLE_MS) == 0U) {
                timed_out = 1U;
                break;
            }

            ADC_Control_SampleFeedback(&feedback,
                                       TUNE_FEEDBACK_SAMPLE_COUNT,
                                       TUNE_FEEDBACK_SAMPLE_DELAY_MS);
            score = ScoreFeedbackCandidate(&feedback);
            scan_freq_tbl[scan_count] = f;
            scan_score_tbl[scan_count] = score;
            scan_feedback_tbl[scan_count] = feedback;
            scan_count++;
            SetAutoTuneFeedback(score, &feedback);
            if (AutoTuneFaultActive() != 0U) {
                timed_out = 1U;
                break;
            }
            if (score < best_freq_score) {
                best_freq_score = score;
                best_freq = f;
                best_freq_feedback = feedback;
            }

            point_idx++;
            SetAutoTuneState(TUNE_STATUS_FINE_FREQ,
                             1U,
                             (uint8_t)(91U + ((point_idx * 8U) / point_count)),
                             f,
                             best_l);
        }

        if (scan_count != 0U) {
            uint16_t stable_idx = StableValleyIndex(scan_score_tbl,
                                                    scan_freq_tbl,
                                                    scan_count,
                                                    rough_best_freq);
            best_freq_score = scan_score_tbl[stable_idx];
            best_freq = scan_freq_tbl[stable_idx];
            best_freq_feedback = scan_feedback_tbl[stable_idx];
        }
    }

    final_score = (uint32_t)best_freq_score + (uint32_t)best_l_score;
    tune_valid = 1U;
    if ((tune_freq != 0U) && (best_freq_score == 0xFFFFU)) tune_valid = 0U;
    if ((tune_l != 0U) && (best_l_score == 0xFFFFU)) tune_valid = 0U;

    if (timed_out != 0U) {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
        return;
    }

    if (tune_valid != 0U) {
        s_freq_edit_freq = best_freq;
        s_freq_edit_l_step = best_l;
        s_freq_profiles[ch].base_voltage_01v = TuneBaseVoltageForLowPower(ch, best_freq, best_l);
        s_freq_nd_flags[ch] = 0U;
        ClearPowerTunePointsForFreqChannel(ch);
        SaveFreqEditBuffer();
        (void)SaveStoreNow();
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneFeedback((uint16_t)((final_score > 0xFFFFUL) ? 0xFFFFU : final_score),
                            (tune_freq != 0U) ? &best_freq_feedback : &best_l_feedback);
        s_phase_tune_prompt = (tune_freq != 0U) ? 1U : 0U;
        SetAutoTuneState(TUNE_STATUS_DONE, 0U, 100U, best_freq, best_l);
    } else {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_NO_DETECTED, 0U, 100U, backup_freq, backup_l);
    }
}

static void RunPhaseTuneAndSave(void)
{
    uint8_t ch = s_freq_edit_ch;
    uint32_t start_tick = HAL_GetTick();
    uint8_t timed_out = 0U;
    uint8_t was_running = (g_us_state.running != 0U) ? 1U : 0U;
    uint16_t backup_freq = s_freq_edit_freq;
    uint8_t backup_l = s_freq_edit_l_step;
    uint16_t center_freq = s_freq_edit_freq;
    uint16_t phase_lo;
    uint16_t phase_hi;
    uint16_t point_count;
    uint16_t point_idx;
    uint16_t score;
    uint16_t best_score = 0xFFFFU;
    uint16_t best_freq = s_freq_edit_freq;
    int32_t probe_freq;
    ADCFeedback_t feedback;
    ADCFeedback_t best_feedback = {0U, 0U, 0U, 0U};
    uint16_t scan_freq_tbl[TUNE_SCAN_POINT_MAX];
    uint16_t scan_score_tbl[TUNE_SCAN_POINT_MAX];
    ADCFeedback_t scan_feedback_tbl[TUNE_SCAN_POINT_MAX];
    uint16_t scan_count = 0U;

    s_phase_tune_prompt = 0U;
    s_autotune_power_01w = 0U;
    MegasonicCtrl_SetFrequency(center_freq);
    MegasonicCtrl_SetLCRelay((uint8_t)(s_freq_edit_l_step - 1U));
    MegasonicCtrl_SetDuty(RUN_BUCK_DUTY_MIN_01PCT);

    if (was_running == 0U) {
        if (PrepareBuckVoltageTargetBeforeStart(TUNE_TEST_VOLTAGE_01V) == 0U) {
            RestoreAutoTuneOutput(was_running);
            SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
            return;
        }
        MegasonicCtrl_Start();
    }

    SetAutoTuneState(TUNE_STATUS_PHASE_CHECK, 1U, 1U, center_freq, s_freq_edit_l_step);
    WaitAutoTuneOutputReady();
    if (TuneBuckToVoltage01V(TUNE_TEST_VOLTAGE_01V) == 0U) {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
        return;
    }

    ADC_Control_SampleFeedback(&feedback,
                               (uint8_t)(TUNE_FEEDBACK_SAMPLE_COUNT + 8U),
                               TUNE_FEEDBACK_SAMPLE_DELAY_MS);
    SetAutoTuneFeedback(ScorePhaseCandidate(&feedback), &feedback);
    if (AutoTuneFaultActive() != 0U) {
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
        return;
    }
    SetAutoTuneState(TUNE_STATUS_PHASE_CHECK, 1U, 5U, center_freq, s_freq_edit_l_step);

    if ((feedback.fwd_adc < TUNE_PHASE_MIN_SIGNAL_ADC)
        || (feedback.ref_adc < TUNE_PHASE_MIN_SIGNAL_ADC)) {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_NO_DETECTED, 0U, 100U, backup_freq, backup_l);
        return;
    }

    phase_lo = ClampFreq01kHz((center_freq > TUNE_PHASE_RANGE_01KHZ)
                            ? (uint16_t)(center_freq - TUNE_PHASE_RANGE_01KHZ)
                            : FREQ_EDIT_MIN);
    phase_hi = ClampFreq01kHz((uint16_t)(center_freq + TUNE_PHASE_RANGE_01KHZ));
    point_count = (uint16_t)(((phase_hi - phase_lo) / TUNE_PHASE_STEP_01KHZ) + 1U);
    if (point_count > TUNE_SCAN_POINT_MAX) {
        point_count = TUNE_SCAN_POINT_MAX;
    }
    point_idx = 0U;

    for (probe_freq = (int32_t)phase_hi;
         (probe_freq >= (int32_t)phase_lo) && (scan_count < point_count);
         probe_freq -= (int32_t)TUNE_PHASE_STEP_01KHZ) {
        uint16_t f = ClampFreq01kHz((uint16_t)probe_freq);

        if (IsAutoTuneTimedOut(start_tick) != 0U) {
            timed_out = 1U;
            break;
        }

        MegasonicCtrl_SetFrequency(f);
        if (DelayWithCurrentGuard(TUNE_PHASE_SETTLE_MS) == 0U) {
            timed_out = 1U;
            break;
        }

        ADC_Control_SampleFeedback(&feedback,
                                   TUNE_FEEDBACK_SAMPLE_COUNT,
                                   TUNE_FEEDBACK_SAMPLE_DELAY_MS);
        score = ScorePhaseCandidate(&feedback);
        scan_freq_tbl[scan_count] = f;
        scan_score_tbl[scan_count] = score;
        scan_feedback_tbl[scan_count] = feedback;
        scan_count++;
        SetAutoTuneFeedback(score, &feedback);
        if (AutoTuneFaultActive() != 0U) {
            timed_out = 1U;
            break;
        }
        if (score < best_score) {
            best_score = score;
            best_freq = f;
            best_feedback = feedback;
        }

        point_idx++;
        SetAutoTuneState(TUNE_STATUS_PHASE_FINE,
                         1U,
                         (uint8_t)(5U + ((point_idx * 94U) / point_count)),
                         f,
                         s_freq_edit_l_step);
    }

    if (scan_count != 0U) {
        uint16_t stable_idx = StableBestIndex(scan_score_tbl,
                                              scan_count,
                                              center_freq,
                                              scan_freq_tbl[scan_count - 1U],
                                              TUNE_PHASE_STEP_01KHZ);
        best_score = scan_score_tbl[stable_idx];
        best_freq = scan_freq_tbl[stable_idx];
        best_feedback = scan_feedback_tbl[stable_idx];
    }

    if (timed_out != 0U) {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_TIMEOUT, 0U, 100U, backup_freq, backup_l);
        return;
    }

    if (best_score != 0xFFFFU) {
        best_freq = ClampTuneShift(best_freq, backup_freq);
        s_freq_edit_freq = best_freq;
        s_freq_nd_flags[ch] = 0U;
        SaveFreqEditBuffer();
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneFeedback(best_score, &best_feedback);
        SetAutoTuneState(TUNE_STATUS_PHASE_DONE, 0U, 100U, best_freq, s_freq_edit_l_step);
    } else {
        s_freq_nd_flags[ch] = 1U;
        s_freq_edit_freq = backup_freq;
        s_freq_edit_l_step = backup_l;
        MegasonicCtrl_SetFrequency(backup_freq);
        MegasonicCtrl_SetLCRelay((uint8_t)(backup_l - 1U));
        RestoreAutoTuneOutput(was_running);
        SetAutoTuneState(TUNE_STATUS_NO_DETECTED, 0U, 100U, backup_freq, backup_l);
    }
}

static void UpdateEstimatedOutputPower(void)
{
    uint16_t measured_01w = ADC_Control_GetIvPower01W();
    uint16_t filtered_01w;
    uint16_t display_01w;
    int32_t diff_x32;
    int32_t step_x32;
    uint32_t now;

    if (!g_us_state.running) {
        s_output_est_01w = 0U;
        s_output_est_filter_x32 = 0U;
        s_output_est_filter_ready = 0U;
        s_output_est_display_sum = 0U;
        s_output_est_display_count = 0U;
        s_output_est_display_tick = 0U;
        s_output_est_filter_tick = 0U;
        s_output_est_hold_until_tick = 0U;
        return;
    }

    now = HAL_GetTick();

    if (s_output_est_filter_ready == 0U) {
        s_output_est_filter_x32 = (uint32_t)measured_01w * 32UL;
        s_output_est_01w = RoundPowerDisplay01W(measured_01w);
        s_output_est_filter_ready = 1U;
        s_output_est_display_sum = 0U;
        s_output_est_display_count = 0U;
        s_output_est_display_tick = now;
        s_output_est_filter_tick = now;
        return;
    }

    if ((now - s_output_est_filter_tick) < RUN_POWER_EST_FILTER_INTERVAL_MS) {
        return;
    }
    s_output_est_filter_tick = now;

    diff_x32 = (int32_t)((uint32_t)measured_01w * 32UL) - (int32_t)s_output_est_filter_x32;
    if (diff_x32 >= 0) {
        step_x32 = (diff_x32 + 7) / 8;
    } else {
        step_x32 = -(((-diff_x32) + 11) / 12);
    }
    s_output_est_filter_x32 = (uint32_t)((int32_t)s_output_est_filter_x32 + step_x32);
    filtered_01w = (uint16_t)((s_output_est_filter_x32 + 16UL) / 32UL);
    display_01w = RoundPowerDisplay01W(filtered_01w);

    if ((int32_t)(s_output_est_hold_until_tick - now) > 0) {
        s_output_est_display_sum = 0U;
        s_output_est_display_count = 0U;
        s_output_est_display_tick = now;
        return;
    }

    if (s_output_est_display_count < 100U) {
        s_output_est_display_sum += display_01w;
        s_output_est_display_count++;
    }

    if ((now - s_output_est_display_tick) >= RUN_POWER_EST_DISPLAY_MS) {
        if (s_output_est_display_count != 0U) {
            s_output_est_01w = ClampU16((uint16_t)((s_output_est_display_sum + (s_output_est_display_count / 2U))
                                                   / s_output_est_display_count),
                                        0U,
                                        999U);
        }
        s_output_est_display_sum = 0U;
        s_output_est_display_count = 0U;
        s_output_est_display_tick = now;
    }
}

static uint16_t RunTargetVoltage01V(void)
{
    uint16_t voltage_01v;

    if (s_selected_freq_ch >= FREQ_EDIT_CH_COUNT) {
        return RUN_BASE_VOLTAGE_DEFAULT_01V;
    }

    voltage_01v = ClampBaseVoltage01V(s_freq_profiles[s_selected_freq_ch].base_voltage_01v);
    if ((Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
        && (voltage_01v > RUN_VOLTAGE_2MHZ_MAX_01V)) {
        voltage_01v = RUN_VOLTAGE_2MHZ_MAX_01V;
    }
    return voltage_01v;
}

static void ServiceRunPowerControl(void)
{
    uint32_t now = HAL_GetTick();
    uint16_t voltage_01v;
    uint16_t target_voltage_01v;
    uint16_t duty;
    uint32_t current_ma;

    if (RUN_POWER_CONTROL_ENABLED == 0U) return;
    if (s_autotune_running != 0U) return;
    if (s_run_power_hold_active != 0U) return;
    if ((now - s_run_power_control_tick) < RUN_POWER_CONTROL_INTERVAL_MS) return;

    s_run_power_control_tick = now;
    voltage_01v = ADC_Control_GetVoltage01V();
    target_voltage_01v = RunTargetVoltage01V();
    duty = g_us_state.target_duty;
    current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;

    if ((uint16_t)(voltage_01v + RUN_VOLTAGE_CONTROL_DEADBAND_01V) < target_voltage_01v) {
        if (duty < DUTY_CLAMP_MAX) {
            duty = (uint16_t)ClampU16((uint16_t)(duty + RUN_POWER_CONTROL_STEP_FAST),
                                      DUTY_MIN,
                                      DUTY_CLAMP_MAX);
            MegasonicCtrl_SetDuty(duty);
            MegasonicCtrl_Update();
        }
        return;
    }

    if (current_ma >= RUN_POWER_CONTROL_CUR_GUARD_MA) {
        duty = (duty > RUN_POWER_CONTROL_STEP_FAST)
             ? (uint16_t)(duty - RUN_POWER_CONTROL_STEP_FAST)
             : DUTY_MIN;
        MegasonicCtrl_SetDuty(duty);
        MegasonicCtrl_Update();
        return;
    }

    if (voltage_01v > (uint16_t)(target_voltage_01v + RUN_VOLTAGE_CONTROL_DEADBAND_01V)) {
        uint16_t diff_01v = (uint16_t)(voltage_01v - target_voltage_01v);
        uint16_t down_step = (diff_01v >= 100U)
                           ? RUN_POWER_CONTROL_STEP_FAST
                           : RUN_POWER_CONTROL_STEP_SLOW;
        duty = (duty > down_step)
             ? (uint16_t)(duty - down_step)
             : DUTY_MIN;
        MegasonicCtrl_SetDuty(duty);
        MegasonicCtrl_Update();
    }
}

static uint16_t ClampRunTrackFrequency(uint16_t freq_01khz)
{
    uint16_t lo;
    uint16_t hi;

    lo = (s_run_freq_track_base > RUN_FREQ_TRACK_RANGE_01KHZ)
       ? (uint16_t)(s_run_freq_track_base - RUN_FREQ_TRACK_RANGE_01KHZ)
       : FREQ_EDIT_MIN;
    hi = ClampFreq01kHz((uint16_t)(s_run_freq_track_base + RUN_FREQ_TRACK_RANGE_01KHZ));
    lo = ClampFreq01kHz(lo);

    return ClampU16(freq_01khz, lo, hi);
}

static void ServiceRunFrequencyTracking(void)
{
    uint32_t now = HAL_GetTick();
    uint16_t cur_adc;
    uint16_t current_freq;
    uint16_t next_freq;
    int32_t next;
    uint32_t current_ma;
    ADCFeedback_t feedback;

    if (RUN_FREQ_TRACK_ENABLED == 0U) return;
    if (s_autotune_running != 0U) return;
    if (s_run_freq_manual_hold != 0U) return;
    if (s_run_power_hold_active != 0U) return;
    if (g_us_state.soft_starting != 0U) return;

    if (s_run_freq_track_tick == 0U) {
        s_run_freq_track_tick = now + RUN_FREQ_TRACK_START_DELAY_MS;
        return;
    }
    if ((int32_t)(now - s_run_freq_track_tick) < 0) {
        return;
    }

    ADC_Control_SampleFeedback(&feedback,
                               RUN_FREQ_TRACK_SAMPLE_COUNT,
                               RUN_FREQ_TRACK_SAMPLE_DELAY_MS);

    current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
    if (current_ma >= RUN_POWER_CONTROL_CUR_GUARD_MA) {
        s_run_freq_track_tick = now + RUN_FREQ_TRACK_INTERVAL_MS;
        return;
    }

    cur_adc = feedback.cur_adc;

    current_freq = g_us_state.target_freq;
    if (s_run_freq_track_ready == 0U) {
        s_run_freq_track_ready = 1U;
        s_run_freq_track_base = s_freq_profiles[s_selected_freq_ch].freq_01khz;
        s_run_freq_track_best_freq = current_freq;
        s_run_freq_track_best_cur = cur_adc;
        s_run_freq_track_dir = 1;
    } else if (cur_adc > (uint16_t)(s_run_freq_track_best_cur + RUN_FREQ_TRACK_HYST_ADC)) {
        s_run_freq_track_best_cur = cur_adc;
        s_run_freq_track_best_freq = current_freq;
    } else if ((uint16_t)(cur_adc + RUN_FREQ_TRACK_HYST_ADC) < s_run_freq_track_best_cur) {
        s_run_freq_track_dir = (s_run_freq_track_dir > 0) ? -1 : 1;
        current_freq = s_run_freq_track_best_freq;
    }

    next = (int32_t)current_freq
         + ((int32_t)s_run_freq_track_dir * (int32_t)RUN_FREQ_TRACK_STEP_01KHZ);
    next_freq = ClampRunTrackFrequency(ClampFreq01kHz((uint16_t)((next < 0) ? 0 : next)));
    if (next_freq == g_us_state.target_freq) {
        s_run_freq_track_dir = (s_run_freq_track_dir > 0) ? -1 : 1;
        next = (int32_t)current_freq
             + ((int32_t)s_run_freq_track_dir * (int32_t)RUN_FREQ_TRACK_STEP_01KHZ);
        next_freq = ClampRunTrackFrequency(ClampFreq01kHz((uint16_t)((next < 0) ? 0 : next)));
    }

    if (next_freq != g_us_state.target_freq) {
        MegasonicCtrl_SetFrequency(next_freq);
        MenuScreen_ForceRefresh();
    }
    s_run_freq_track_tick = now + RUN_FREQ_TRACK_INTERVAL_MS;
}

static void ScheduleRunRetune(void)
{
    if (RUN_RETUNE_ENABLED == 0U) return;
    if (g_us_state.running == 0U) return;
    if (s_error_code != ERR_NONE) return;
    if (s_autotune_running != 0U) return;
    if (s_run_freq_manual_hold != 0U) return;

    s_run_retune_pending = 1U;
    s_run_retune_due_tick = HAL_GetTick() + RUN_RETUNE_START_DELAY_MS;
    s_run_power_maintain_tick = 0U;
}

static uint8_t RunPowerReachedTarget(uint16_t power_01w, uint16_t target_power_01w)
{
    return (AbsDiffU16(power_01w, target_power_01w) <= RUN_POWER_TARGET_DEADBAND_01W) ? 1U : 0U;
}

static uint8_t RunPowerWithinPercent(uint16_t power_01w, uint16_t target_power_01w, uint8_t percent)
{
    uint16_t allowed;

    if (percent == 0U) {
        return RunPowerReachedTarget(power_01w, target_power_01w);
    }

    allowed = (uint16_t)(((uint32_t)target_power_01w * (uint32_t)percent + 99UL) / 100UL);
    if (allowed == 0U) {
        allowed = 1U;
    }

    return (AbsDiffU16(power_01w, target_power_01w) <= allowed) ? 1U : 0U;
}

static uint8_t RunDutyOnlyGoodEnough(uint16_t power_01w, uint16_t target_power_01w)
{
    uint16_t allowed = (uint16_t)(((uint32_t)target_power_01w
                                   * (uint32_t)RUN_DUTY_ONLY_ACCEPT_PERCENT
                                   + 99UL) / 100UL);

    if (allowed < RUN_DUTY_ONLY_ACCEPT_01W) {
        allowed = RUN_DUTY_ONLY_ACCEPT_01W;
    }
    if ((Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
        && (allowed < RUN_DUTY_2MHZ_ACCEPT_01W)) {
        allowed = RUN_DUTY_2MHZ_ACCEPT_01W;
    }

    return (AbsDiffU16(power_01w, target_power_01w) <= allowed) ? 1U : 0U;
}

static uint8_t SampleRunGateDutyPoint(uint16_t gate_duty_01pct, uint16_t *power_out)
{
    uint32_t current_ma;

    if (StopIfOvercurrentNow() == 0U) return 0U;

    gate_duty_01pct = ClampRunGateDutyForSelectedFreq(gate_duty_01pct);
    MegasonicCtrl_SetRunGateDuty(gate_duty_01pct);
    MenuScreen_ForceRefresh();
    MenuScreen_Refresh();

    {
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < RunRetuneSettleMs()) {
            if (StopIfOvercurrentNow() == 0U) return 0U;
            current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
            if (current_ma >= RUN_RETUNE_CUR_GUARD_MA) {
                return RunRetuneCurrentGuardHit(power_out);
            }
            MegasonicCtrl_Update();
            HAL_Delay(5U);
        }
    }

    *power_out = SamplePower01W(RunRetuneSampleCount(), RUN_RETUNE_SAMPLE_DELAY_MS);
    SetOutputPowerEstimateImmediate(*power_out);
    if (StopIfOvercurrentNow() == 0U) return 0U;
    current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
    if (current_ma >= RUN_RETUNE_CUR_GUARD_MA) {
        return RunRetuneCurrentGuardHit(power_out);
    }

    return 1U;
}

static uint8_t SampleRunRetunePoint(uint16_t freq_01khz, uint16_t *power_out)
{
    uint32_t current_ma;

    if (StopIfOvercurrentNow() == 0U) return 0U;

    MegasonicCtrl_SetFrequency(freq_01khz);
    MenuScreen_ForceRefresh();
    MenuScreen_Refresh();

    {
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < RunRetuneSettleMs()) {
            if (StopIfOvercurrentNow() == 0U) return 0U;
            current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
            if (current_ma >= RUN_RETUNE_CUR_GUARD_MA) {
                return RunRetuneCurrentGuardHit(power_out);
            }
            MegasonicCtrl_Update();
            HAL_Delay(5U);
        }
    }

    *power_out = SamplePower01W(RunRetuneSampleCount(), RUN_RETUNE_SAMPLE_DELAY_MS);
    SetOutputPowerEstimateImmediate(*power_out);
    if (StopIfOvercurrentNow() == 0U) return 0U;
    current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
    if (current_ma >= RUN_RETUNE_CUR_GUARD_MA) {
        return RunRetuneCurrentGuardHit(power_out);
    }

    return 1U;
}

static uint8_t SampleRunVoltagePoint(uint16_t voltage_01v, uint16_t *power_out)
{
    uint32_t current_ma;

    if (StopIfOvercurrentNow() == 0U) return 0U;

    voltage_01v = ClampBaseVoltage01V(voltage_01v);
    MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(voltage_01v));
    MegasonicCtrl_Update();
    MenuScreen_ForceRefresh();
    MenuScreen_Refresh();

    {
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < (uint32_t)(RunRetuneSettleMs() + 80U)) {
            if (StopIfOvercurrentNow() == 0U) return 0U;
            current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
            if (current_ma >= RUN_RETUNE_CUR_GUARD_MA) {
                return RunRetuneCurrentGuardHit(power_out);
            }
            MegasonicCtrl_Update();
            HAL_Delay(5U);
        }
    }

    *power_out = SamplePower01W(RunRetuneSampleCount(), RUN_RETUNE_SAMPLE_DELAY_MS);
    SetOutputPowerEstimateImmediate(*power_out);
    if (StopIfOvercurrentNow() == 0U) return 0U;
    current_ma = (ADC_Control_GetCurrentuA() + 500UL) / 1000UL;
    if (current_ma >= RUN_RETUNE_CUR_GUARD_MA) {
        return RunRetuneCurrentGuardHit(power_out);
    }

    return 1U;
}

static void ConsiderRunGateDutyPoint(uint16_t gate_duty_01pct,
                                     uint16_t power_01w,
                                     uint16_t prefer_duty,
                                     uint16_t target_power_01w,
                                     uint8_t *found,
                                     uint16_t *best_duty,
                                     uint16_t *best_power,
                                     uint16_t *best_dist)
{
    uint16_t dist = AbsDiffU16(gate_duty_01pct, prefer_duty);
    uint16_t err = AbsDiffU16(power_01w, target_power_01w);
    uint16_t best_err = AbsDiffU16(*best_power, target_power_01w);
    uint8_t power_under = (power_01w <= target_power_01w) ? 1U : 0U;
    uint8_t best_under = (*best_power <= target_power_01w) ? 1U : 0U;

    if ((*found == 0U)
        || ((err <= RUN_POWER_TARGET_DEADBAND_01W)
            && ((best_err > RUN_POWER_TARGET_DEADBAND_01W)
                || (err < best_err)
                || ((err == best_err) && (dist < *best_dist))))
        || ((err + RUN_POWER_TARGET_DEADBAND_01W) < best_err)
        || ((err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (power_under != 0U)
            && (best_under == 0U))
        || ((power_under == best_under)
            && (err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (dist < *best_dist))) {
        *found = 1U;
        *best_duty = gate_duty_01pct;
        *best_power = power_01w;
        *best_dist = dist;
    }
}

static void AcceptRunGateDutyPoint(uint16_t gate_duty_01pct,
                                   uint16_t power_01w,
                                   uint16_t prefer_duty,
                                   uint8_t *found,
                                   uint16_t *best_duty,
                                   uint16_t *best_power,
                                   uint16_t *best_dist)
{
    *found = 1U;
    *best_duty = gate_duty_01pct;
    *best_power = power_01w;
    *best_dist = AbsDiffU16(gate_duty_01pct, prefer_duty);
}

static uint8_t RunPowerCrossedTarget(uint16_t prev_power_01w,
                                     uint16_t power_01w,
                                     uint16_t target_power_01w)
{
    return (((prev_power_01w <= target_power_01w) && (power_01w >= target_power_01w))
            || ((prev_power_01w >= target_power_01w) && (power_01w <= target_power_01w))) ? 1U : 0U;
}

static uint8_t RememberRunGateDutyPoint(uint16_t gate_duty_01pct,
                                        uint16_t power_01w,
                                        uint16_t prefer_duty,
                                        uint16_t target_power_01w,
                                        uint8_t *prev_valid,
                                        uint16_t *prev_duty,
                                        uint16_t *prev_power,
                                        uint8_t *found,
                                        uint16_t *best_duty,
                                        uint16_t *best_power,
                                        uint16_t *best_dist)
{
    if (RunPowerReachedTarget(power_01w, target_power_01w) != 0U) {
        AcceptRunGateDutyPoint(gate_duty_01pct,
                               power_01w,
                               prefer_duty,
                               found,
                               best_duty,
                               best_power,
                               best_dist);
        return 1U;
    }

    if (Run2MHzLockGoodEnough(power_01w, target_power_01w) != 0U) {
        AcceptRunGateDutyPoint(gate_duty_01pct,
                               power_01w,
                               prefer_duty,
                               found,
                               best_duty,
                               best_power,
                               best_dist);
        return 1U;
    }

    if (RunDutyOnlyGoodEnough(power_01w, target_power_01w) != 0U) {
        AcceptRunGateDutyPoint(gate_duty_01pct,
                               power_01w,
                               prefer_duty,
                               found,
                               best_duty,
                               best_power,
                               best_dist);
        return 1U;
    }

    if ((*prev_valid != 0U)
        && (RunPowerCrossedTarget(*prev_power, power_01w, target_power_01w) != 0U)) {
        uint16_t prev_err = AbsDiffU16(*prev_power, target_power_01w);
        uint16_t err = AbsDiffU16(power_01w, target_power_01w);

        if (prev_err <= err) {
            AcceptRunGateDutyPoint(*prev_duty,
                                   *prev_power,
                                   prefer_duty,
                                   found,
                                   best_duty,
                                   best_power,
                                   best_dist);
        } else {
            AcceptRunGateDutyPoint(gate_duty_01pct,
                                   power_01w,
                                   prefer_duty,
                                   found,
                                   best_duty,
                                   best_power,
                                   best_dist);
        }
        return 1U;
    }

    ConsiderRunGateDutyPoint(gate_duty_01pct,
                             power_01w,
                             prefer_duty,
                             target_power_01w,
                             found,
                             best_duty,
                             best_power,
                             best_dist);
    *prev_valid = 1U;
    *prev_duty = gate_duty_01pct;
    *prev_power = power_01w;
    return 0U;
}

#define RUN_DUTY_SCAN_AUTO 0U
#define RUN_DUTY_SCAN_DOWN 1U
#define RUN_DUTY_SCAN_UP   2U

static uint8_t ScanRunGateDutyWindow(uint16_t lo,
                                     uint16_t hi,
                                     uint16_t prefer_duty,
                                     uint16_t step_01pct,
                                     uint16_t target_power_01w,
                                     uint8_t scan_down,
                                     uint8_t *found,
                                     uint16_t *best_duty,
                                     uint16_t *best_power,
                                     uint16_t *best_dist)
{
    uint16_t duty_min = RunGateDutyMinForFreqCh(s_selected_freq_ch);
    uint16_t duty_max = RunGateDutyMaxForFreqCh(s_selected_freq_ch);
    uint16_t power_01w;
    uint16_t prev_duty;
    uint16_t prev_power;
    uint8_t prev_valid = 0U;
    int8_t dir;
    int32_t next_duty;

    lo = ClampU16(lo, duty_min, duty_max);
    hi = ClampU16(hi, duty_min, duty_max);
    if (hi < lo) {
        return 0U;
    }
    if (step_01pct == 0U) {
        step_01pct = RUN_GATE_DUTY_TUNE_STEP_01PCT;
    }
    prefer_duty = ClampRunGateDutyForSelectedFreq(ClampU16(prefer_duty, lo, hi));

    if (SampleRunGateDutyPoint(prefer_duty, &power_01w) == 0U) {
        return 0U;
    }
    if (RememberRunGateDutyPoint(prefer_duty,
                                 power_01w,
                                 prefer_duty,
                                 target_power_01w,
                                 &prev_valid,
                                 &prev_duty,
                                 &prev_power,
                                 found,
                                 best_duty,
                                 best_power,
                                 best_dist) != 0U) {
        return 1U;
    }

    if (scan_down == RUN_DUTY_SCAN_DOWN) {
        dir = -1;
    } else if (scan_down == RUN_DUTY_SCAN_UP) {
        dir = 1;
    } else {
        dir = (power_01w > target_power_01w) ? -1 : 1;
    }
    next_duty = (int32_t)prefer_duty + ((int32_t)dir * (int32_t)step_01pct);

    while ((next_duty >= (int32_t)lo) && (next_duty <= (int32_t)hi)) {
        uint16_t duty = ClampRunGateDutyForSelectedFreq((uint16_t)next_duty);

        if ((dir < 0) && (duty >= prev_duty)) {
            break;
        }
        if ((dir > 0) && (duty <= prev_duty)) {
            break;
        }

        if (SampleRunGateDutyPoint(duty, &power_01w) == 0U) {
            return 0U;
        }
        if (RememberRunGateDutyPoint(duty,
                                     power_01w,
                                     prefer_duty,
                                     target_power_01w,
                                     &prev_valid,
                                     &prev_duty,
                                     &prev_power,
                                     found,
                                     best_duty,
                                     best_power,
                                     best_dist) != 0U) {
            return 1U;
        }

        next_duty += ((int32_t)dir * (int32_t)step_01pct);
    }

    return 1U;
}

static void ConsiderRunVoltagePoint(uint16_t voltage_01v,
                                    uint16_t power_01w,
                                    uint16_t prefer_voltage,
                                    uint16_t target_power_01w,
                                    uint8_t *found,
                                    uint16_t *best_voltage,
                                    uint16_t *best_power,
                                    uint16_t *best_dist)
{
    uint16_t dist = AbsDiffU16(voltage_01v, prefer_voltage);
    uint16_t err = AbsDiffU16(power_01w, target_power_01w);
    uint16_t best_err = AbsDiffU16(*best_power, target_power_01w);
    uint8_t power_under = (power_01w <= target_power_01w) ? 1U : 0U;
    uint8_t best_under = (*best_power <= target_power_01w) ? 1U : 0U;

    if ((*found == 0U)
        || ((err + RUN_POWER_TARGET_DEADBAND_01W) < best_err)
        || ((err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (power_under != 0U)
            && (best_under == 0U))
        || ((power_under == best_under)
            && (err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (dist < *best_dist))) {
        *found = 1U;
        *best_voltage = voltage_01v;
        *best_power = power_01w;
        *best_dist = dist;
    }
}

static uint8_t ScanRunVoltageWindow(uint16_t lo,
                                    uint16_t hi,
                                    uint16_t prefer_voltage,
                                    uint16_t target_power_01w,
                                    uint8_t *found,
                                    uint16_t *best_voltage,
                                    uint16_t *best_power,
                                    uint16_t *best_dist)
{
    uint16_t span_down;
    uint16_t span_up;
    uint16_t max_span;
    uint16_t offset;

    lo = ClampBaseVoltage01V(lo);
    hi = ClampBaseVoltage01V(hi);
    if (hi < lo) {
        return 0U;
    }
    prefer_voltage = ClampU16(prefer_voltage, lo, hi);

    {
        uint16_t power_01w;

        if (SampleRunVoltagePoint(prefer_voltage, &power_01w) == 0U) {
            return 0U;
        }
        ConsiderRunVoltagePoint(prefer_voltage,
                                power_01w,
                                prefer_voltage,
                                target_power_01w,
                                found,
                                best_voltage,
                                best_power,
                                best_dist);
        if ((*found != 0U) && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
            return 1U;
        }
    }

    span_down = prefer_voltage - lo;
    span_up = hi - prefer_voltage;
    max_span = (span_down > span_up) ? span_down : span_up;

    for (offset = RUN_VOLTAGE_FALLBACK_STEP_01V;
         offset <= max_span;
         offset = (uint16_t)(offset + RUN_VOLTAGE_FALLBACK_STEP_01V)) {
        uint8_t tried = 0U;

        if (offset <= span_down) {
            uint16_t power_01w;
            uint16_t voltage = (uint16_t)(prefer_voltage - offset);

            if (SampleRunVoltagePoint(voltage, &power_01w) == 0U) {
                return 0U;
            }
            ConsiderRunVoltagePoint(voltage,
                                    power_01w,
                                    prefer_voltage,
                                    target_power_01w,
                                    found,
                                    best_voltage,
                                    best_power,
                                    best_dist);
            if ((*found != 0U) && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
                return 1U;
            }
            tried = 1U;
        }

        if (offset <= span_up) {
            uint16_t power_01w;
            uint16_t voltage = (uint16_t)(prefer_voltage + offset);

            if (SampleRunVoltagePoint(voltage, &power_01w) == 0U) {
                return 0U;
            }
            ConsiderRunVoltagePoint(voltage,
                                    power_01w,
                                    prefer_voltage,
                                    target_power_01w,
                                    found,
                                    best_voltage,
                                    best_power,
                                    best_dist);
            if ((*found != 0U) && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
                return 1U;
            }
            tried = 1U;
        }

        if ((tried == 0U) || ((max_span - offset) < RUN_VOLTAGE_FALLBACK_STEP_01V)) {
            break;
        }
    }

    return 1U;
}

static void ConsiderRunRetunePoint(uint16_t freq_01khz,
                                   uint16_t power_01w,
                                   uint16_t prefer_freq,
                                   uint16_t target_power_01w,
                                   uint8_t *found,
                                   uint16_t *best_freq,
                                   uint16_t *best_power,
                                   uint16_t *best_dist)
{
    uint16_t dist = AbsDiffU16(freq_01khz, prefer_freq);
    uint16_t err = AbsDiffU16(power_01w, target_power_01w);
    uint16_t best_err = AbsDiffU16(*best_power, target_power_01w);
    uint8_t power_under = (power_01w <= target_power_01w) ? 1U : 0U;
    uint8_t best_under = (*best_power <= target_power_01w) ? 1U : 0U;

    if ((*found == 0U)
        || ((err + RUN_POWER_TARGET_DEADBAND_01W) < best_err)
        || ((err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (power_under != 0U)
            && (best_under == 0U))
        || ((power_under == best_under)
            && (err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (dist < *best_dist))) {
        *found = 1U;
        *best_freq = freq_01khz;
        *best_power = power_01w;
        *best_dist = dist;
    }
}

static uint8_t ScanRunRetuneWindow(uint16_t lo,
                                  uint16_t hi,
                                  uint16_t step_01khz,
                                  uint16_t prefer_freq,
                                  uint16_t target_power_01w,
                                  uint8_t scan_down,
                                  uint8_t *found,
                                  uint16_t *best_freq,
                                  uint16_t *best_power,
                                  uint16_t *best_dist)
{
    uint16_t f;

    lo = ClampFreq01kHz(lo);
    hi = ClampFreq01kHz(hi);
    if (hi < lo) {
        return 0U;
    }
    f = (scan_down != 0U) ? hi : lo;

    while ((scan_down != 0U) ? (f >= lo) : (f <= hi)) {
        uint16_t power_01w;

        if (SampleRunRetunePoint(f, &power_01w) == 0U) {
            return 0U;
        }
        ConsiderRunRetunePoint(f,
                               power_01w,
                               prefer_freq,
                               target_power_01w,
                               found,
                               best_freq,
                               best_power,
                               best_dist);
        if ((*found != 0U) && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
            return 1U;
        }

        if (scan_down != 0U) {
            if ((f - lo) < step_01khz) {
                break;
            }
            f = (uint16_t)(f - step_01khz);
        } else {
            if ((hi - f) < step_01khz) {
                break;
            }
            f = (uint16_t)(f + step_01khz);
        }
    }

    return 1U;
}

static void ConsiderRunPowerCandidate(uint16_t freq_01khz,
                                      uint16_t gate_duty_01pct,
                                      uint16_t power_01w,
                                      uint16_t prefer_freq,
                                      uint16_t target_power_01w,
                                      uint8_t *found,
                                      uint16_t *best_freq,
                                      uint16_t *best_duty,
                                      uint16_t *best_power,
                                      uint16_t *best_dist)
{
    uint16_t dist = AbsDiffU16(freq_01khz, prefer_freq);
    uint16_t err = AbsDiffU16(power_01w, target_power_01w);
    uint16_t best_err = AbsDiffU16(*best_power, target_power_01w);
    uint8_t power_under = (power_01w <= target_power_01w) ? 1U : 0U;
    uint8_t best_under = (*best_power <= target_power_01w) ? 1U : 0U;

    if ((*found == 0U)
        || ((err + RUN_POWER_TARGET_DEADBAND_01W) < best_err)
        || ((err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (power_under != 0U)
            && (best_under == 0U))
        || ((power_under == best_under)
            && (err <= (uint16_t)(best_err + RUN_POWER_TARGET_DEADBAND_01W))
            && (dist < *best_dist))) {
        *found = 1U;
        *best_freq = freq_01khz;
        *best_duty = gate_duty_01pct;
        *best_power = power_01w;
        *best_dist = dist;
    }
}

static uint8_t RunDutyScanAtCurrentFreq(uint16_t prefer_duty,
                                        uint16_t target_power_01w,
                                        uint8_t *found,
                                        uint16_t *best_duty,
                                        uint16_t *best_power,
                                        uint16_t *best_dist)
{
    uint16_t duty_min = RunGateDutyMinForFreqCh(s_selected_freq_ch);
    uint16_t duty_max = RunGateDutyMaxForFreqCh(s_selected_freq_ch);
    uint16_t duty_fine_lo;
    uint16_t duty_fine_hi;
    uint8_t scan_ok;

    scan_ok = ScanRunGateDutyWindow(duty_min,
                                    duty_max,
                                    prefer_duty,
                                    RUN_GATE_DUTY_TUNE_STEP_01PCT,
                                    target_power_01w,
                                    0U,
                                    found,
                                    best_duty,
                                    best_power,
                                    best_dist);

    if ((scan_ok != 0U)
        && (*found != 0U)
        && (RunPowerReachedTarget(*best_power, target_power_01w) != 0U)) {
        MegasonicCtrl_SetRunGateDuty(*best_duty);
        return 1U;
    }

    if ((scan_ok != 0U)
        && (*found != 0U)
        && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
        MegasonicCtrl_SetRunGateDuty(*best_duty);
        return 1U;
    }

    if ((scan_ok != 0U)
        && (Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        scan_ok = ScanRunGateDutyWindow(duty_min,
                                        duty_max,
                                        prefer_duty,
                                        RUN_GATE_DUTY_TUNE_STEP_01PCT,
                                        target_power_01w,
                                        RUN_DUTY_SCAN_DOWN,
                                        found,
                                        best_duty,
                                        best_power,
                                        best_dist);
        if ((scan_ok != 0U)
            && (*found != 0U)
            && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
            MegasonicCtrl_SetRunGateDuty(*best_duty);
            return 1U;
        }
    }

    if ((scan_ok != 0U)
        && (Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        scan_ok = ScanRunGateDutyWindow(duty_min,
                                        duty_max,
                                        prefer_duty,
                                        RUN_GATE_DUTY_TUNE_STEP_01PCT,
                                        target_power_01w,
                                        RUN_DUTY_SCAN_UP,
                                        found,
                                        best_duty,
                                        best_power,
                                        best_dist);
        if ((scan_ok != 0U)
            && (*found != 0U)
            && (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U)) {
            MegasonicCtrl_SetRunGateDuty(*best_duty);
            return 1U;
        }
    }

    if ((scan_ok != 0U)
        && (*found != 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        duty_fine_lo = (*best_duty > RUN_GATE_DUTY_FINE_RANGE_01PCT)
                     ? (uint16_t)(*best_duty - RUN_GATE_DUTY_FINE_RANGE_01PCT)
                     : duty_min;
        duty_fine_hi = (uint16_t)(*best_duty + RUN_GATE_DUTY_FINE_RANGE_01PCT);
        duty_fine_lo = ClampU16(duty_fine_lo, duty_min, duty_max);
        duty_fine_hi = ClampU16(duty_fine_hi, duty_min, duty_max);

        scan_ok = ScanRunGateDutyWindow(duty_fine_lo,
                                        duty_fine_hi,
                                        *best_duty,
                                        RUN_GATE_DUTY_FINE_STEP_01PCT,
                                        target_power_01w,
                                        0U,
                                        found,
                                        best_duty,
                                        best_power,
                                        best_dist);
    }

    if ((scan_ok != 0U)
        && (*found != 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        MegasonicCtrl_SetRunGateDuty(*best_duty);
    }

    return scan_ok;
}

static uint8_t RunFrequencyScanAtDuty(uint16_t center_freq,
                                      uint16_t prefer_duty,
                                      uint16_t target_power_01w,
                                        uint8_t *found,
                                        uint16_t *best_freq,
                                        uint16_t *best_power,
                                        uint16_t *best_dist)
{
    uint16_t freq_lo;
    uint16_t freq_hi;
    uint16_t fine_lo;
    uint16_t fine_hi;
    uint8_t scan_ok = 1U;

    center_freq = ClampFreq01kHz(center_freq);
    MegasonicCtrl_SetRunGateDuty(ClampRunGateDutyForSelectedFreq(prefer_duty));

    if (SampleRunRetunePoint(center_freq, best_power) != 0U) {
        *found = 1U;
        *best_freq = center_freq;
        *best_dist = 0U;
    } else {
        return 0U;
    }

    if (RunPowerWithinPercent(*best_power,
                              target_power_01w,
                              RUN_FREQ_TARGET_WINDOW_PERCENT) != 0U) {
        return 1U;
    }

    freq_lo = (center_freq > RUN_RETUNE_GUIDED_DOWN_01KHZ)
            ? (uint16_t)(center_freq - RUN_RETUNE_GUIDED_DOWN_01KHZ)
            : FREQ_EDIT_MIN;
    freq_hi = ClampFreq01kHz((uint16_t)(center_freq + RUN_RETUNE_GUIDED_UP_01KHZ));
    freq_lo = ClampFreq01kHz(freq_lo);

    if (*best_power > target_power_01w) {
        scan_ok = ScanRunRetuneWindow(freq_lo,
                                      center_freq,
                                      RUN_RETUNE_COARSE_STEP_01KHZ,
                                      center_freq,
                                      target_power_01w,
                                      1U,
                                      found,
                                      best_freq,
                                      best_power,
                                      best_dist);
        if ((scan_ok != 0U)
            && (RunPowerWithinPercent(*best_power,
                                      target_power_01w,
                                      RUN_FREQ_TARGET_WINDOW_PERCENT) == 0U)) {
            scan_ok = ScanRunRetuneWindow(center_freq,
                                          freq_hi,
                                          RUN_RETUNE_COARSE_STEP_01KHZ,
                                          center_freq,
                                          target_power_01w,
                                          0U,
                                          found,
                                          best_freq,
                                          best_power,
                                          best_dist);
        }
    } else {
        scan_ok = ScanRunRetuneWindow(center_freq,
                                      freq_hi,
                                      RUN_RETUNE_COARSE_STEP_01KHZ,
                                      center_freq,
                                      target_power_01w,
                                      0U,
                                      found,
                                      best_freq,
                                      best_power,
                                      best_dist);
        if ((scan_ok != 0U)
            && (RunPowerWithinPercent(*best_power,
                                      target_power_01w,
                                      RUN_FREQ_TARGET_WINDOW_PERCENT) == 0U)) {
            scan_ok = ScanRunRetuneWindow(freq_lo,
                                          center_freq,
                                          RUN_RETUNE_COARSE_STEP_01KHZ,
                                          center_freq,
                                          target_power_01w,
                                          1U,
                                          found,
                                          best_freq,
                                          best_power,
                                          best_dist);
        }
    }

    if ((scan_ok != 0U) && (*found != 0U)) {
        fine_lo = (*best_freq > RUN_RETUNE_FINE_RANGE_01KHZ)
                ? (uint16_t)(*best_freq - RUN_RETUNE_FINE_RANGE_01KHZ)
                : FREQ_EDIT_MIN;
        fine_hi = (uint16_t)(*best_freq + RUN_RETUNE_FINE_RANGE_01KHZ);
        if (fine_lo < freq_lo) fine_lo = freq_lo;
        if (fine_hi > freq_hi) fine_hi = freq_hi;

        (void)ScanRunRetuneWindow(fine_lo,
                                  fine_hi,
                                  RUN_RETUNE_FINE_STEP_01KHZ,
                                  center_freq,
                                  target_power_01w,
                                  (*best_freq < center_freq) ? 1U : 0U,
                                  found,
                                  best_freq,
                                  best_power,
                                  best_dist);
    }

    if ((scan_ok != 0U)
        && (*found != 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        MegasonicCtrl_SetFrequency(*best_freq);
    }

    return scan_ok;
}

static uint8_t Run2MHzFreqDutySearch(uint16_t center_freq,
                                     uint16_t target_power_01w,
                                     uint8_t *found,
                                     uint16_t *best_freq,
                                     uint16_t *best_duty,
                                     uint16_t *best_power,
                                     uint16_t *best_dist)
{
    uint16_t base_freq;
    uint16_t offset;
    uint8_t first_done = 0U;

    if ((found == NULL) || (best_freq == NULL) || (best_duty == NULL)
        || (best_power == NULL) || (best_dist == NULL)) {
        return 0U;
    }

    base_freq = s_freq_profiles[s_selected_freq_ch].freq_01khz;
    center_freq = ClampFreq01kHz(center_freq);

    for (offset = 0U;
         offset <= RUN_RETUNE_2MHZ_DOWN_01KHZ;
         offset = (uint16_t)(offset + RUN_RETUNE_2MHZ_FREQ_STEP_01KHZ)) {
        uint16_t freq_list[3];
        uint8_t freq_count = 0U;

        if (first_done == 0U) {
            freq_list[freq_count++] = center_freq;
            first_done = 1U;
        }
        if (target_power_01w >= 150U) {
            if ((offset != 0U) && (offset <= RUN_RETUNE_2MHZ_UP_01KHZ)) {
                freq_list[freq_count++] = ClampFreq01kHz((uint16_t)(center_freq + offset));
            }
            if ((offset != 0U) && (center_freq > offset)) {
                freq_list[freq_count++] = ClampFreq01kHz((uint16_t)(center_freq - offset));
            }
        } else {
            if ((offset != 0U) && (center_freq > offset)) {
                freq_list[freq_count++] = ClampFreq01kHz((uint16_t)(center_freq - offset));
            }
            if ((offset != 0U) && (offset <= RUN_RETUNE_2MHZ_UP_01KHZ)) {
                freq_list[freq_count++] = ClampFreq01kHz((uint16_t)(center_freq + offset));
            }
        }
        if (freq_count == 0U) {
            continue;
        }

        for (uint8_t i = 0U; i < freq_count; i++) {
            uint8_t duty_found = 0U;
            uint16_t duty = HRTIM_RUN_DUTY_01PCT;
            uint16_t power = 0U;
            uint16_t duty_dist = 0xFFFFU;
            uint16_t freq = freq_list[i];

            if ((freq < (uint16_t)(base_freq - RUN_RETUNE_2MHZ_DOWN_01KHZ))
                || (freq > (uint16_t)(base_freq + RUN_RETUNE_2MHZ_UP_01KHZ))) {
                continue;
            }

            MegasonicCtrl_SetFrequency(freq);
            MegasonicCtrl_SetRunGateDuty(HRTIM_RUN_DUTY_01PCT);

            if (RunDutyScanAtCurrentFreq(HRTIM_RUN_DUTY_01PCT,
                                         target_power_01w,
                                         &duty_found,
                                         &duty,
                                         &power,
                                         &duty_dist) == 0U) {
                if (s_run_retune_guard_hit != 0U) {
                    s_run_retune_guard_hit = 0U;
                    RestoreRunRetuneSafePoint();
                    continue;
                }
                return 0U;
            }

            if (duty_found != 0U) {
                ConsiderRunPowerCandidate(freq,
                                          duty,
                                          power,
                                          base_freq,
                                          target_power_01w,
                                          found,
                                          best_freq,
                                          best_duty,
                                          best_power,
                                          best_dist);
                if (Run2MHzLockGoodEnough(power, target_power_01w) != 0U) {
                    *found = 1U;
                    *best_freq = freq;
                    *best_duty = duty;
                    *best_power = power;
                    *best_dist = AbsDiffU16(freq, base_freq);
                    LockRunPowerAtCurrentPoint(freq, duty, power);
                    return 1U;
                }
                if (RunDutyOnlyGoodEnough(*best_power, target_power_01w) != 0U) {
                    MegasonicCtrl_SetFrequency(*best_freq);
                    MegasonicCtrl_SetRunGateDuty(*best_duty);
                    s_run_power_hold_active = 1U;
                    SetRunRetuneSafePoint(*best_freq, *best_duty, s_run_retune_safe_voltage);
                    return 1U;
                }
                SetRunRetuneSafePoint(*best_freq, *best_duty, s_run_retune_safe_voltage);
            }

            if (s_error_code != ERR_NONE || g_us_state.running == 0U) {
                return 0U;
            }
        }

        if ((RUN_RETUNE_2MHZ_DOWN_01KHZ - offset) < RUN_RETUNE_2MHZ_FREQ_STEP_01KHZ) {
            break;
        }
    }

    if (*found != 0U) {
        MegasonicCtrl_SetFrequency(*best_freq);
        MegasonicCtrl_SetRunGateDuty(*best_duty);
    }

    return 1U;
}

static void RunPowerRetuneNow(void)
{
    uint8_t preset_valid = PowerProfileTuneValid(s_selected_power_ch);
    uint16_t start_freq = (s_selected_freq_ch < FREQ_EDIT_CH_COUNT)
                        ? s_freq_profiles[s_selected_freq_ch].freq_01khz
                        : g_us_state.target_freq;
    uint16_t center_freq = start_freq;
    uint16_t target_power_01w = s_output_set_01w;
    uint16_t base_voltage = RunTargetVoltage01V();
    uint16_t best_voltage = base_voltage;
    uint16_t best_duty;
    uint16_t best_power = 0U;
    uint16_t freq_best_dist;
    uint16_t duty_best_dist;
    uint16_t voltage_best_dist = 0xFFFFU;
    uint16_t voltage_lo;
    uint16_t voltage_hi;
    uint16_t attempt_freq;
    uint16_t attempt_duty;
    uint16_t attempt_power;
    uint16_t overall_freq = start_freq;
    uint16_t overall_duty;
    uint16_t overall_power = 0U;
    uint16_t overall_dist = 0xFFFFU;
    uint8_t overall_found = 0U;
    uint8_t voltage_found = 0U;
    uint8_t tune_done = 0U;
    uint8_t scan_ok = 1U;
    uint8_t attempt;

    if (RunDutyOnlyGoodEnough(s_output_est_01w, target_power_01w) != 0U) {
        s_run_retune_pending = 0U;
        s_run_retune_active = 0U;
        s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
        return;
    }

    if (preset_valid != 0U) {
        start_freq = PowerTuneFreqForChannel(s_selected_power_ch, s_selected_freq_ch);
        center_freq = start_freq;
        overall_freq = start_freq;
    }

    s_run_retune_active = 1U;
    s_run_retune_pending = 0U;

    /* RUN W 맞춤 중에는 Buck 전압을 먼저 건드리지 않는다.
     * START/채널 적용 때 준비된 기준 전압을 유지하고, duty -> freq 순서가
     * 실패했을 때만 마지막 fallback에서 전압을 소폭 보정한다. */
    best_duty = (preset_valid != 0U)
              ? PowerTuneDutyForChannel(s_selected_power_ch, s_selected_freq_ch)
              : HRTIM_RUN_DUTY_01PCT;
    best_duty = ClampU16(best_duty,
                         RUN_GATE_DUTY_TUNE_MIN_01PCT,
                         RUN_GATE_DUTY_TUNE_MAX_01PCT);
    overall_duty = best_duty;
    MegasonicCtrl_SetFrequency(start_freq);
    MegasonicCtrl_SetRunGateDuty(best_duty);
    s_run_retune_guard_hit = 0U;
    SetRunRetuneSafePoint(start_freq, best_duty, base_voltage);

    if ((Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
        && (target_power_01w >= 150U)) {
        scan_ok = Run2MHzFreqDutySearch(center_freq,
                                        target_power_01w,
                                        &overall_found,
                                        &overall_freq,
                                        &overall_duty,
                                        &overall_power,
                                        &overall_dist);
        if ((scan_ok != 0U)
            && (overall_found != 0U)
            && ((RunDutyOnlyGoodEnough(overall_power, target_power_01w) != 0U)
                || (Run2MHzLockGoodEnough(overall_power, target_power_01w) != 0U))) {
            LockRunPowerAtCurrentPoint(overall_freq, overall_duty, overall_power);
            tune_done = 1U;
        }
    } else {
        /* 1단계: 현재 주파수에서 gate duty를 1%씩 움직여 먼저 맞춰본다. */
        duty_best_dist = 0xFFFFU;
        if (RunDutyScanAtCurrentFreq(best_duty,
                                     target_power_01w,
                                     &overall_found,
                                     &overall_duty,
                                     &overall_power,
                                     &duty_best_dist) == 0U) {
            if ((Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
                && (s_run_retune_guard_hit != 0U)) {
                s_run_retune_guard_hit = 0U;
                RestoreRunRetuneSafePoint();
                if (overall_found == 0U) {
                    overall_found = 1U;
                    overall_freq = start_freq;
                    overall_duty = best_duty;
                    overall_power = s_output_est_01w;
                    overall_dist = 0xFFFFU;
                }
                scan_ok = 1U;
            } else {
                scan_ok = 0U;
            }
        } else if (overall_found != 0U) {
            overall_freq = start_freq;
            overall_dist = 0U;
            ConsiderRunPowerCandidate(overall_freq,
                                      overall_duty,
                                      overall_power,
                                      start_freq,
                                      target_power_01w,
                                      &overall_found,
                                      &overall_freq,
                                      &overall_duty,
                                      &overall_power,
                                      &overall_dist);
            if ((RunPowerReachedTarget(overall_power, target_power_01w) != 0U)
                || (RunDutyOnlyGoodEnough(overall_power, target_power_01w) != 0U)
                || (RunPowerWithinPercent(overall_power,
                                          target_power_01w,
                                          RUN_FREQ_TARGET_WINDOW_PERCENT) != 0U)) {
                tune_done = 1U;
            }
            SetRunRetuneSafePoint(overall_freq, overall_duty, base_voltage);
        }
    }

    if ((tune_done == 0U)
        && (scan_ok != 0U)
        && (Is2MHzTuneChannel(s_selected_freq_ch) != 0U)
        && (target_power_01w < 150U)
        && (s_run_retune_guard_hit == 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        scan_ok = Run2MHzFreqDutySearch(center_freq,
                                        target_power_01w,
                                        &overall_found,
                                        &overall_freq,
                                        &overall_duty,
                                        &overall_power,
                                        &overall_dist);
        if ((scan_ok != 0U)
            && (overall_found != 0U)
            && ((RunDutyOnlyGoodEnough(overall_power, target_power_01w) != 0U)
                || (Run2MHzLockGoodEnough(overall_power, target_power_01w) != 0U))) {
            LockRunPowerAtCurrentPoint(overall_freq, overall_duty, overall_power);
            tune_done = 1U;
        }
    }

    if (Is2MHzTuneChannel(s_selected_freq_ch) == 0U) {
        for (attempt = 0U;
             (tune_done == 0U)
             && (scan_ok != 0U)
             && (attempt < RUN_RETUNE_ATTEMPT_COUNT)
             && (s_error_code == ERR_NONE)
             && (g_us_state.running != 0U);
             attempt++) {
            uint8_t freq_found = 0U;

            if ((overall_found != 0U)
                && ((RunPowerReachedTarget(overall_power, target_power_01w) != 0U)
                    || (RunDutyOnlyGoodEnough(overall_power, target_power_01w) != 0U)
                    || (RunPowerWithinPercent(overall_power,
                                              target_power_01w,
                                              RUN_FREQ_TARGET_WINDOW_PERCENT) != 0U))) {
                break;
            }

            /* 2단계 반복: 현재 best duty를 유지한 채 주파수 detune으로 접근한다. */
            attempt_freq = center_freq;
            attempt_power = 0U;
            freq_best_dist = 0xFFFFU;
            scan_ok = RunFrequencyScanAtDuty(center_freq,
                                             overall_duty,
                                             target_power_01w,
                                             &freq_found,
                                             &attempt_freq,
                                             &attempt_power,
                                             &freq_best_dist);
            if ((scan_ok == 0U) || (freq_found == 0U)) {
                break;
            }

            attempt_duty = ClampRunGateDutyForSelectedFreq(overall_duty);
            ConsiderRunPowerCandidate(attempt_freq,
                                      attempt_duty,
                                      attempt_power,
                                      start_freq,
                                      target_power_01w,
                                      &overall_found,
                                      &overall_freq,
                                      &overall_duty,
                                      &overall_power,
                                      &overall_dist);

            center_freq = attempt_freq;
            if (RunPowerWithinPercent(attempt_power,
                                      target_power_01w,
                                      RUN_FREQ_TARGET_WINDOW_PERCENT) != 0U) {
                /* 주파수 단계에서 이미 목표 10% band에 들어오면 더 이상 duty/freq를 건드리지 않는다. */
                tune_done = 1U;
                continue;
            }
        }
    }

    /* 마지막 단계: 3회 반복 후보 중 가장 가까운 주파수 지점에서만 전압을 소폭 보정한다. */
    if ((tune_done == 0U)
        && (scan_ok != 0U)
        && (overall_found != 0U)
        && (Is2MHzTuneChannel(s_selected_freq_ch) == 0U)
        && (s_error_code == ERR_NONE)
        && (g_us_state.running != 0U)) {
        MegasonicCtrl_SetFrequency(overall_freq);
        MegasonicCtrl_SetRunGateDuty(overall_duty);

        if (Is2MHzTuneChannel(s_selected_freq_ch) != 0U) {
            voltage_lo = RUN_VOLTAGE_2MHZ_LOW_01V;
            voltage_hi = base_voltage;
        } else {
            voltage_lo = (base_voltage > RUN_VOLTAGE_FALLBACK_RANGE_01V)
                       ? (uint16_t)(base_voltage - RUN_VOLTAGE_FALLBACK_RANGE_01V)
                       : RUN_BASE_VOLTAGE_MIN_01V;
            voltage_hi = (uint16_t)(base_voltage + RUN_VOLTAGE_FALLBACK_RANGE_01V);
        }
        voltage_lo = ClampBaseVoltage01V(voltage_lo);
        voltage_hi = ClampBaseVoltage01V(voltage_hi);
        voltage_found = 1U;
        best_voltage = base_voltage;
        best_power = overall_power;
        voltage_best_dist = 0U;
        scan_ok = ScanRunVoltageWindow(voltage_lo,
                                       voltage_hi,
                                       base_voltage,
                                       target_power_01w,
                                       &voltage_found,
                                       &best_voltage,
                                       &best_power,
                                       &voltage_best_dist);
    }

    if ((s_error_code == ERR_NONE) && (g_us_state.running != 0U)) {
        if (overall_found != 0U) {
            MegasonicCtrl_SetFrequency(overall_freq);
            MegasonicCtrl_SetRunGateDuty(overall_duty);
        } else {
            MegasonicCtrl_SetFrequency(start_freq);
            MegasonicCtrl_SetRunGateDuty(best_duty);
        }
        if ((voltage_found != 0U)
            && (AbsDiffU16(best_power, target_power_01w)
                <= AbsDiffU16(overall_power, target_power_01w))) {
            MegasonicCtrl_SetDuty(BuckDAC_DutyForVoltage01V(best_voltage));
            overall_power = best_power;
        }
        MenuScreen_ForceRefresh();
    }

    s_run_retune_active = 0U;
    s_run_power_maintain_tick = HAL_GetTick() + RunPowerMaintainIntervalMs();
}

static void ServiceRunPowerMaintain(void)
{
    uint32_t now = HAL_GetTick();
    uint16_t target_01w;
    uint16_t measured_01w;

    if (RUN_RETUNE_ENABLED == 0U) return;
    if (g_us_state.running == 0U) return;
    if (s_error_code != ERR_NONE) return;
    if (s_autotune_running != 0U) return;
    if (s_run_freq_manual_hold != 0U) return;
    if (s_run_retune_active != 0U) return;
    if (s_run_retune_pending != 0U) return;
    if (g_us_state.soft_starting != 0U) return;

    if (s_run_power_maintain_tick == 0U) {
        s_run_power_maintain_tick = now + RunPowerMaintainIntervalMs();
        return;
    }
    if ((int32_t)(now - s_run_power_maintain_tick) < 0) {
        return;
    }

    target_01w = s_output_set_01w;
    measured_01w = s_output_est_01w;

    if (s_run_power_hold_active != 0U) {
        if (RunPowerWithinPercent(measured_01w,
                                  target_01w,
                                  RUN_DUTY_ONLY_ACCEPT_PERCENT) != 0U) {
            s_run_power_miss_count = 0U;
            s_run_power_maintain_tick = now + RunPowerMaintainIntervalMs();
            return;
        }

        if (s_run_power_miss_count < 255U) {
            s_run_power_miss_count++;
        }
        if (s_run_power_miss_count < RUN_POWER_MAINTAIN_MISS_LIMIT) {
            s_run_power_maintain_tick = now + RunPowerMaintainIntervalMs();
            return;
        }

        RequestRunPowerRetuneNow(now);
        return;
    }

    if (RunDutyOnlyGoodEnough(measured_01w, target_01w) != 0U) {
        s_run_power_miss_count = 0U;
        s_run_power_maintain_tick = now + RunPowerMaintainIntervalMs();
        return;
    }

    if (s_run_power_miss_count < 255U) {
        s_run_power_miss_count++;
    }
    if (s_run_power_miss_count < RUN_POWER_MAINTAIN_MISS_LIMIT) {
        s_run_power_maintain_tick = now + RunPowerMaintainIntervalMs();
        return;
    }

    s_run_power_miss_count = 0U;
    RequestRunPowerRetuneNow(now);
}

static void ServiceRunPowerRetune(void)
{
    uint32_t now = HAL_GetTick();

    if (RUN_RETUNE_ENABLED == 0U) return;
    if (s_run_retune_pending == 0U) return;
    if (s_run_retune_active != 0U) return;
    if (s_autotune_running != 0U) return;
    if (s_run_freq_manual_hold != 0U) {
        s_run_retune_pending = 0U;
        return;
    }
    if (s_run_power_hold_active != 0U) {
        s_run_retune_pending = 0U;
        return;
    }
    if (g_us_state.soft_starting != 0U) return;
    if ((int32_t)(now - s_run_retune_due_tick) < 0) return;

    if (RunDutyOnlyGoodEnough(s_output_est_01w, s_output_set_01w) != 0U) {
        s_run_retune_pending = 0U;
        s_run_power_miss_count = 0U;
        s_run_power_maintain_tick = now + RunPowerMaintainIntervalMs();
        return;
    }

    RunPowerRetuneNow();
    s_run_power_miss_count = 0U;
}

static void ServiceRunBuckProtect(void)
{
    if (ADC_Control_GetVoltage01V() >= RUN_BUCK_OVERVOLT_01V) {
        MegasonicCtrl_EmergencyStop();
        LatchOvercurrentSnapshot();
        SetError(ERR_BUCK_OVERVOLT);
        return;
    }

    (void)StopIfOvercurrentNow();
}

static void ServiceRemoteRunRequest(void)
{
    uint8_t remote_active;

    if (s_selected_mode != MODE_REMOTE) {
        s_remote_run_latched = 0U;
        s_remote_run_armed = 0U;
        return;
    }

    remote_active = ReadRemoteRunActive();
    if (remote_active != 0U) {
        if ((s_remote_run_armed != 0U) && (s_remote_run_latched == 0U)) {
            if (Menu_RequestOutputStart() != 0U) {
                s_remote_run_latched = 1U;
            }
        }
    } else {
        s_remote_run_armed = 1U;
        s_remote_run_latched = 0U;
        if (g_us_state.running != 0U) {
            Menu_RequestOutputStop();
        }
    }
}

static void ServiceRuntimeSafety(void)
{
    const PowerChannelProfile_t *p;
    uint8_t ext_ch;
    uint32_t now;

    ServiceAlarmBeep();
    ServiceClickBeep();
    ServiceSharedBuzzer();
    UpdateEstimatedOutputPower();

    ServiceRemoteRunRequest();

    if ((s_selected_mode == MODE_REMOTE) && (s_error_code == ERR_NONE)) {
        ext_ch = ReadRemoteBcdPowerChannel();
        if (ext_ch != s_remote_bcd_power_ch) {
            s_remote_bcd_power_ch = ext_ch;
            ApplyPowerChannel(ext_ch);
            MenuScreen_ForceRefresh();
        }
    }

    if (!g_us_state.running) return;
    if (s_error_code != ERR_NONE) return;
    if (IsSensorInputOk() == 0U) {
        SetError(ERR_SENSOR_RUN);
        return;
    }
    ServiceRunBuckProtect();
    if (s_error_code != ERR_NONE) return;
    if (g_us_state.soft_starting) {
        s_low_alarm_pending = 0U;
        s_high_alarm_pending = 0U;
        return;
    }

    p = &s_power_profiles[s_selected_power_ch];
    now = HAL_GetTick();
    ServiceRunPowerControl();
    ServiceRunPowerMaintain();
    ServiceRunPowerRetune();
    ServiceRunFrequencyTracking();

    if ((MENU_LOW_ALARM_ENABLED != 0U) && (s_output_est_01w < p->low_01w)) {
        s_high_alarm_pending = 0U;
        if (s_low_alarm_pending == 0U) {
            s_low_alarm_pending = 1U;
            s_low_alarm_start_tick = now;
        } else if ((now - s_low_alarm_start_tick) >= ALARM_HOLD_TIME_MS) {
            SetError(ERR_LOW);
        }
    } else if ((MENU_HIGH_ALARM_ENABLED != 0U) && (s_output_est_01w > p->high_01w)) {
        s_low_alarm_pending = 0U;
        if (s_high_alarm_pending == 0U) {
            s_high_alarm_pending = 1U;
            s_high_alarm_start_tick = now;
        } else if ((now - s_high_alarm_start_tick) >= ALARM_HOLD_TIME_MS) {
            SetError(ERR_HIGH);
        }
    } else {
        s_low_alarm_pending = 0U;
        s_high_alarm_pending = 0U;
    }
}

static void EnterSettingMenu(void)
{
    s_state = MENU_STATE_SETTING_MENU;
    s_select_mode_active = 0U;
    s_setting_item = SETTING_ITEM_FREQ;
    s_edit_saved = 1U;
    s_autotune_armed = 0U;
    s_phase_tune_prompt = 0U;
    MenuScreen_ForceRefresh();
}

static void EnterSelectMenu(void)
{
    s_state = MENU_STATE_SELECT;
    s_select_mode_active = 0U;
    s_autotune_armed = 0U;
    s_phase_tune_prompt = 0U;
    s_set_exit_armed = 0U;
    MenuScreen_ForceRefresh();
}

static uint8_t TryExitToInitialBySetHold(ButtonEvent_t evt_set)
{
    if (evt_set == BTN_EVT_LONG_PRESS) {
        s_set_exit_armed = 1U;
        s_set_exit_long_tick = HAL_GetTick();
        StartLongPressBeep();
        return 0U;
    }

    if ((s_set_exit_armed != 0U) && (evt_set == BTN_EVT_REPEAT)) {
        if ((HAL_GetTick() - s_set_exit_long_tick) >= SET_EXIT_HOLD_EXTRA_MS) {
            EnterSelectMenu();
            return 1U;
        }
    }

    if (evt_set == BTN_EVT_PRESS) {
        s_set_exit_armed = 0U;
    }

    return 0U;
}

static void HandleSelectState(ButtonEvent_t evt_start,
                              ButtonEvent_t evt_mode,
                              ButtonEvent_t evt_up,
                              ButtonEvent_t evt_down,
                              ButtonEvent_t evt_set)
{
    int8_t delta;
    uint8_t changed = 0U;

    if ((evt_set == BTN_EVT_PRESS) && (s_error_code != ERR_NONE)) {
        ClearError();
        MenuScreen_ForceRefresh();
        StartClickBeep();
        return;
    }

    if (evt_start == BTN_EVT_PRESS) {
        if (s_error_code == ERR_NONE) {
            if (g_us_state.running) {
                Menu_RequestOutputStop();
                StartClickBeep();
            } else {
                if (Menu_RequestOutputStart() != 0U) {
                    StartClickBeep();
                }
            }

            if (g_us_state.running != 0U) {
                s_select_mode_active = 0U;
                s_run_freq_adjust_active = 0U;
            }

            MenuScreen_ForceRefresh();
        }
    }

    /* 초기화면: MODE 짧게로 선택모드 진입 */
    if (s_select_mode_active == 0U) {
        if (g_us_state.running != 0U) {
            if (evt_mode == BTN_EVT_PRESS) {
                s_run_freq_adjust_active = (s_run_freq_adjust_active == 0U) ? 1U : 0U;
                MenuScreen_ForceRefresh();
                StartClickBeep();
                return;
            }

            if (evt_set == BTN_EVT_PRESS) {
                PlayStoreFeedback(StoreRunTunePointBySet());
                MenuScreen_ForceRefresh();
                return;
            }

            delta = EventStepDelta(evt_up, evt_down);
            if (s_run_freq_adjust_active == 0U) {
                if (AdjustRunOutput(delta) != 0U) {
                    StartClickBeep();
                }
            } else if (delta != 0) {
                s_run_freq_manual_hold = 1U;
                if (AdjustRunFrequency(delta) != 0U) {
                    StartClickBeep();
                }
            }
            return;
        }

        if (evt_set == BTN_EVT_PRESS) {
            (void)ForceStoreFlush();
            MenuScreen_ForceRefresh();
            return;
        }

        if (evt_mode == BTN_EVT_PRESS) {
            s_select_mode_active = 1U;
            s_select_item = SELECT_ITEM_MODE;
            MenuScreen_ForceRefresh();
            StartClickBeep();
            return;
        }

        if (evt_mode == BTN_EVT_LONG_PRESS) {
            EnterSettingMenu();
            StartLongPressBeep();
            return;
        }

        return;
    }

    if (g_us_state.running != 0U) {
        s_select_mode_active = 0U;
        MenuScreen_ForceRefresh();
        return;
    }

    if (evt_set == BTN_EVT_LONG_PRESS) {
        (void)ForceStoreFlush();
        s_select_mode_active = 0U;
        MenuScreen_ForceRefresh();
        return;
    }

    if (evt_mode == BTN_EVT_LONG_PRESS) {
        EnterSettingMenu();
        StartLongPressBeep();
        return;
    }

    if (evt_mode == BTN_EVT_PRESS) {
        s_select_item = (SelectItem_t)((s_select_item + 1U) % SELECT_ITEM_COUNT);
        MenuScreen_ForceRefresh();
        StartClickBeep();
    }

    delta = EventStepDelta(evt_up, evt_down);
    if (delta != 0) {
        switch (s_select_item) {
        case SELECT_ITEM_MODE: {
            uint8_t mode = (uint8_t)s_selected_mode;
            uint8_t prev_mode = mode;
            if (delta > 0) mode = (uint8_t)((mode + 1U) % MODE_COUNT);
            else mode = (mode == 0U) ? (uint8_t)(MODE_COUNT - 1U) : (uint8_t)(mode - 1U);
            s_selected_mode = (OperatingMode_t)mode;
            ApplySelectedMode();
            if (mode != prev_mode) {
                changed = 1U;
            }
            break;
        }

        case SELECT_ITEM_FREQ_CH: {
            uint8_t ch = s_selected_freq_ch;
            uint8_t prev_ch = ch;
            if (delta > 0) ch = (uint8_t)((ch + 1U) % FREQ_EDIT_CH_COUNT);
            else ch = (ch == 0U) ? (uint8_t)(FREQ_EDIT_CH_COUNT - 1U) : (uint8_t)(ch - 1U);
            ApplyFreqChannel(ch);
            ApplyPowerChannel(s_selected_power_ch);
            if (ch != prev_ch) {
                changed = 1U;
            }
            break;
        }

        case SELECT_ITEM_8POWER: {
            uint8_t ch = s_selected_power_ch;
            uint8_t prev_ch = ch;
            if (delta > 0) ch = (uint8_t)((ch + 1U) % POWER_8STEP_COUNT);
            else ch = (ch == 0U) ? (uint8_t)(POWER_8STEP_COUNT - 1U) : (uint8_t)(ch - 1U);
            ApplyPowerChannel(ch);
            if (ch != prev_ch) {
                changed = 1U;
            }
            break;
        }

        default:
            break;
        }

        if (changed != 0U) {
            MenuScreen_ForceRefresh();
            MarkStoreDirty();
            StartClickBeep();
        }
    }

    /* 선택모드: SET 짧게 → 변경 즉시 확정 (idle 대기 없이 force flush) */
    if (evt_set == BTN_EVT_PRESS) {
        switch (s_select_item) {
        case SELECT_ITEM_MODE:
            ApplySelectedMode();
            break;
        case SELECT_ITEM_FREQ_CH:
            ApplyFreqChannel(s_selected_freq_ch);
            ApplyPowerChannel(s_selected_power_ch);
            break;
        case SELECT_ITEM_8POWER:
            ApplyPowerChannel(s_selected_power_ch);
            break;
        default:
            break;
        }
        s_edit_saved = 1U;
        MarkStoreDirty();
        (void)ForceStoreFlush();
        MenuScreen_ForceRefresh();
    }
}

static void HandleSettingMenuState(ButtonEvent_t evt_mode,
                                   ButtonEvent_t evt_up,
                                   ButtonEvent_t evt_down,
                                   ButtonEvent_t evt_set)
{
    int8_t delta;

    if (TryExitToInitialBySetHold(evt_set) != 0U) {
        (void)ForceStoreFlush();
        return;
    }

    if (evt_mode == BTN_EVT_PRESS) {
        StartClickBeep();
    }

    delta = EventStepDelta(evt_up, evt_down);
    if (delta != 0) {
        if (delta > 0) {
            s_setting_item = (SettingItem_t)((s_setting_item + 1U) % SETTING_ITEM_COUNT);
        } else {
            s_setting_item = (s_setting_item == 0U)
                ? (SettingItem_t)(SETTING_ITEM_COUNT - 1U)
                : (SettingItem_t)(s_setting_item - 1U);
        }
        MenuScreen_ForceRefresh();
        StartClickBeep();
    }

    if (evt_set != BTN_EVT_PRESS) return;

    switch (s_setting_item) {
    case SETTING_ITEM_FREQ:
        s_state = MENU_STATE_SETTING_FREQ;
        s_freq_field = FREQ_EDIT_FIELD_CH;
        LoadFreqEditBuffer(s_selected_freq_ch);
        s_edit_saved = 1U;
        break;

    case SETTING_ITEM_POWER:
        s_state = MENU_STATE_SETTING_POWER;
        s_power_field = POWER_EDIT_FIELD_CH;
        LoadPowerEditBuffer(s_selected_power_ch);
        s_edit_saved = 1U;
        break;

    case SETTING_ITEM_EXT_RS485:
        s_state = MENU_STATE_SETTING_RS485;
        s_rs485_field = RS485_EDIT_FIELD_BAUD;
        LoadRs485EditBuffer();
        s_edit_saved = 1U; /* RS485 진입 시 초기값을 항상 즉시 표시 */
        break;

    default:
        s_state = MENU_STATE_SETTING_MENU;
        s_edit_saved = 1U;
        break;
    }

    s_autotune_armed = 0U;
    StartClickBeep();
    MenuScreen_ForceRefresh();
}

static void HandleSettingFreqState(ButtonEvent_t evt_start,
                                   ButtonEvent_t evt_mode,
                                   ButtonEvent_t evt_up,
                                   ButtonEvent_t evt_down,
                                   ButtonEvent_t evt_set)
{
    int8_t delta;
    uint8_t changed = 0U;
    uint8_t preview_changed = 0U;
    uint16_t prev16;

    ClearAutoTuneResultIfAnyInput(evt_start, evt_mode, evt_up, evt_down, evt_set);

    if (TryExitToInitialBySetHold(evt_set) != 0U) {
        SaveFreqEditBuffer();
        (void)ForceStoreFlush();
        return;
    }

    if (s_phase_tune_prompt != 0U) {
        if (evt_mode == BTN_EVT_PRESS) {
            s_phase_tune_prompt = 0U;
            s_autotune_status = TUNE_STATUS_IDLE;
            s_autotune_progress = 0U;
            StartClickBeep();
            MenuScreen_ForceRefresh();
            return;
        }

        if (evt_set == BTN_EVT_PRESS) {
            RunPhaseTuneAndSave();
            StartClickBeep();
            MenuScreen_ForceRefresh();
            return;
        }

        if ((evt_start != BTN_EVT_NONE) || (evt_up != BTN_EVT_NONE) || (evt_down != BTN_EVT_NONE)) {
            s_autotune_armed = 0U;
            MenuScreen_ForceRefresh();
        }
        return;
    }

    if (evt_mode == BTN_EVT_LONG_PRESS) {
        SaveFreqEditBuffer();
        (void)SaveStoreNow();
        s_state = MENU_STATE_SETTING_MENU;
        s_autotune_armed = 0U;
        StartLongPressBeep();
        MenuScreen_ForceRefresh();
        return;
    }

    if (evt_mode == BTN_EVT_PRESS) {
        if (s_edit_saved == 0U) {
            CommitFreqEditBuffer();
        }
        s_freq_field = (FreqEditField_t)((s_freq_field + 1U) % FREQ_EDIT_FIELD_COUNT);
        s_edit_saved = 1U;
        s_autotune_armed = 0U;
        StartClickBeep();
        MenuScreen_ForceRefresh();
    }

    if (evt_set == BTN_EVT_PRESS) {
        if ((s_freq_field == FREQ_EDIT_FIELD_FREQ) || (s_freq_field == FREQ_EDIT_FIELD_L)) {
            s_freq_nd_flags[s_freq_edit_ch] = 0U;
        }
        SaveFreqEditBuffer();
        ApplyFreqEditPreview();
        (void)ForceStoreFlush();
        s_autotune_armed = 0U;
        MenuScreen_ForceRefresh();
    }

    if (evt_start == BTN_EVT_LONG_PRESS) {
        s_autotune_armed = 1U;
        s_autotune_long_tick = HAL_GetTick();
        StartLongPressBeep();
    } else if ((s_autotune_armed != 0U) && (evt_start == BTN_EVT_REPEAT)) {
        if ((HAL_GetTick() - s_autotune_long_tick) >= TUNE_START_HOLD_EXTRA_MS) {
            RunAutoTuneAndSave();
            s_autotune_armed = 0U;
            MenuScreen_ForceRefresh();
        }
    }

    delta = EventStepDelta(evt_up, evt_down);
    if (delta == 0) return;

    s_autotune_armed = 0U;

    switch (s_freq_field) {
    case FREQ_EDIT_FIELD_CH: {
        uint8_t ch = s_freq_edit_ch;
        if (delta > 0) ch = (uint8_t)((ch + 1U) % FREQ_EDIT_CH_COUNT);
        else ch = (ch == 0U) ? (uint8_t)(FREQ_EDIT_CH_COUNT - 1U) : (uint8_t)(ch - 1U);
        if (ch != s_freq_edit_ch) {
            changed = 1U;
            if (s_edit_saved == 0U) {
                CommitFreqEditBuffer();
            }
            LoadFreqEditBuffer(ch);
            ApplyFreqEditPreview();
            s_edit_saved = 1U;
        }
        break;
    }

    case FREQ_EDIT_FIELD_FREQ: {
        s_edit_saved = 0U;
        prev16 = s_freq_edit_freq;
        int32_t next = (int32_t)s_freq_edit_freq + (int32_t)delta * (int32_t)FREQ_STEP;
        s_freq_edit_freq = ClampU16((uint16_t)((next < 0) ? 0 : next), FREQ_EDIT_MIN, FREQ_EDIT_MAX);
        if (s_freq_edit_freq != prev16) {
            changed = 1U;
            preview_changed = 1U;
            s_freq_nd_flags[s_freq_edit_ch] = 0U;
        }
        break;
    }

    case FREQ_EDIT_FIELD_L: {
        s_edit_saved = 0U;
        prev16 = s_freq_edit_l_step;
        int32_t next = (int32_t)s_freq_edit_l_step + (int32_t)delta;
        s_freq_edit_l_step = ClampU16((uint16_t)((next < 0) ? 0 : next), 1U, 16U);
        if (s_freq_edit_l_step != prev16) {
            changed = 1U;
            preview_changed = 1U;
            s_freq_nd_flags[s_freq_edit_ch] = 0U;
        }
        break;
    }

    default:
        break;
    }

    if (changed != 0U) {
        if (preview_changed != 0U) {
            ApplyFreqEditPreview();
        }
        /* CH 외 필드 변경(주파수/L)은 ApplyFreqChannel을 재호출하지 않고
         * 프로파일에만 직접 반영하여 손실을 방지한다. 미리보기는 ApplyFreqEditPreview가
         * 이미 처리한다. 실제 FLASH 쓰기는 idle 또는 메뉴 나가는 시점에 수행된다. */
        if (s_freq_field != FREQ_EDIT_FIELD_CH) {
            uint8_t edit_ch = s_freq_edit_ch;
            s_freq_profiles[edit_ch].freq_01khz =
                ClampU16(s_freq_edit_freq, FREQ_EDIT_MIN, FREQ_EDIT_MAX);
            s_freq_profiles[edit_ch].l_step =
                (uint8_t)ClampU16(s_freq_edit_l_step, 1U, 16U);
        }
        MenuScreen_ForceRefresh();
        StartClickBeep();
    }
}

static void HandleSettingPowerState(ButtonEvent_t evt_start,
                                    ButtonEvent_t evt_mode,
                                    ButtonEvent_t evt_up,
                                    ButtonEvent_t evt_down,
                                    ButtonEvent_t evt_set)
{
    int8_t delta;
    uint8_t changed = 0U;
    uint16_t prev16;

    if (TryExitToInitialBySetHold(evt_set) != 0U) {
        SavePowerEditBuffer();
        (void)ForceStoreFlush();
        return;
    }

    if (evt_mode == BTN_EVT_LONG_PRESS) {
        SavePowerEditBuffer();
        (void)SaveStoreNow();
        s_state = MENU_STATE_SETTING_MENU;
        StartLongPressBeep();
        MenuScreen_ForceRefresh();
        return;
    }

    if (evt_mode == BTN_EVT_PRESS) {
        if (s_edit_saved == 0U) {
            CommitPowerEditBuffer();
        }
        s_power_field = (PowerEditField_t)((s_power_field + 1U) % POWER_EDIT_FIELD_COUNT);
        s_edit_saved = 1U;
        StartClickBeep();
        MenuScreen_ForceRefresh();
    }

    if (evt_set == BTN_EVT_PRESS) {
        SavePowerEditBuffer();
        (void)ForceStoreFlush();
        MenuScreen_ForceRefresh();
    }

    if (evt_start == BTN_EVT_PRESS) {
        s_autotune_armed = 0U;
    }

    delta = EventStepDeltaPowerEdit(evt_up, evt_down, s_power_field);
    if (delta == 0) return;

    switch (s_power_field) {
    case POWER_EDIT_FIELD_CH: {
        uint8_t ch = s_power_edit_ch;
        if (delta > 0) ch = (uint8_t)((ch + 1U) % POWER_8STEP_COUNT);
        else ch = (ch == 0U) ? (uint8_t)(POWER_8STEP_COUNT - 1U) : (uint8_t)(ch - 1U);
        if (ch != s_power_edit_ch) {
            changed = 1U;
            if (s_edit_saved == 0U) {
                CommitPowerEditBuffer();
            }
            LoadPowerEditBuffer(ch);
            s_edit_saved = 1U;
        }
        break;
    }

    case POWER_EDIT_FIELD_LOW: {
        s_edit_saved = 0U;
        prev16 = s_power_edit_low;
        int32_t next = (int32_t)s_power_edit_low + (int32_t)delta;
        s_power_edit_low = ClampU16((uint16_t)((next < 0) ? 0 : next),
                                    POWER_PROFILE_LOW_MIN,
                                    POWER_PROFILE_LOW_MAX);
        if (s_power_edit_low >= s_power_edit_high) {
            s_power_edit_low = (uint16_t)(s_power_edit_high - 1U);
        }
        if (s_power_edit_def < s_power_edit_low) s_power_edit_def = s_power_edit_low;
        if (s_power_edit_low != prev16) changed = 1U;
        break;
    }

    case POWER_EDIT_FIELD_HIGH: {
        s_edit_saved = 0U;
        prev16 = s_power_edit_high;
        int32_t next = (int32_t)s_power_edit_high + (int32_t)delta;
        s_power_edit_high = ClampU16((uint16_t)((next < 0) ? 0 : next),
                                     POWER_PROFILE_HIGH_MIN,
                                     POWER_PROFILE_HIGH_MAX);
        if (s_power_edit_high <= s_power_edit_low) {
            s_power_edit_high = (uint16_t)(s_power_edit_low + 1U);
        }
        if (s_power_edit_def > s_power_edit_high) s_power_edit_def = s_power_edit_high;
        if (s_power_edit_high != prev16) changed = 1U;
        break;
    }

    case POWER_EDIT_FIELD_DEFAULT: {
        s_edit_saved = 0U;
        prev16 = s_power_edit_def;
        int32_t next = (int32_t)s_power_edit_def + (int32_t)delta;
        s_power_edit_def = ClampU16((uint16_t)((next < 0) ? 0 : next),
                                    POWER_PROFILE_DEF_MIN,
                                    POWER_PROFILE_DEF_MAX);
        if (s_power_edit_def < s_power_edit_low) s_power_edit_def = s_power_edit_low;
        if (s_power_edit_def > s_power_edit_high) s_power_edit_def = s_power_edit_high;
        if (s_power_edit_def != prev16) changed = 1U;
        break;
    }

    default:
        break;
    }

    if (changed != 0U) {
        /* CH 외 필드 변경(LOW/HIGH/DEF)은 ApplyPowerChannel을 재호출하지 않고
         * 프로파일에만 직접 반영하여 손실을 방지한다. 상호 제약은 이미 위의
         * 필드별 처리에서 보정되어 있다. */
        if (s_power_field != POWER_EDIT_FIELD_CH) {
            uint8_t edit_ch = s_power_edit_ch;
            uint16_t old_def = s_power_profiles[edit_ch].def_01w;
            s_power_profiles[edit_ch].low_01w = s_power_edit_low;
            s_power_profiles[edit_ch].high_01w = s_power_edit_high;
            s_power_profiles[edit_ch].def_01w = s_power_edit_def;
            if (s_power_edit_def != old_def) {
                ClearPowerTunePoints(edit_ch);
            }
        }
        MenuScreen_ForceRefresh();
        StartClickBeep();
    }
}

static void HandleSettingRs485State(ButtonEvent_t evt_mode,
                                    ButtonEvent_t evt_up,
                                    ButtonEvent_t evt_down,
                                    ButtonEvent_t evt_set)
{
    int8_t delta;
    uint8_t changed = 0U;
    uint8_t prev8;

    if (TryExitToInitialBySetHold(evt_set) != 0U) {
        SaveRs485EditBuffer();
        (void)ForceStoreFlush();
        return;
    }

    if (evt_mode == BTN_EVT_LONG_PRESS) {
        SaveRs485EditBuffer();
        (void)SaveStoreNow();
        s_state = MENU_STATE_SETTING_MENU;
        StartLongPressBeep();
        MenuScreen_ForceRefresh();
        return;
    }

    if (evt_mode == BTN_EVT_PRESS) {
        s_rs485_field = (Rs485EditField_t)((s_rs485_field + 1U) % RS485_EDIT_FIELD_COUNT);
        StartClickBeep();
        MenuScreen_ForceRefresh();
    }

    if (evt_set == BTN_EVT_PRESS) {
        SaveRs485EditBuffer();
        (void)ForceStoreFlush();
        MenuScreen_ForceRefresh();
    }

    delta = EventStepDelta(evt_up, evt_down);
    if (delta == 0) {
        return;
    }

    s_edit_saved = 0U;

    switch (s_rs485_field) {
    case RS485_EDIT_FIELD_BAUD:
        prev8 = s_rs485_baud_idx;
        if (delta > 0) {
            s_rs485_baud_idx = (uint8_t)((s_rs485_baud_idx + 1U) % MODBUS_BAUD_INDEX_COUNT);
        } else {
            s_rs485_baud_idx = (s_rs485_baud_idx == 0U)
                ? (uint8_t)(MODBUS_BAUD_INDEX_COUNT - 1U)
                : (uint8_t)(s_rs485_baud_idx - 1U);
        }
        if (s_rs485_baud_idx != prev8) {
            changed = 1U;
        }
        break;

    case RS485_EDIT_FIELD_ADDR: {
        int32_t next = (int32_t)s_rs485_addr + (int32_t)delta;
        prev8 = s_rs485_addr;
        s_rs485_addr = (uint8_t)ClampU16((uint16_t)((next < 0) ? 0 : next),
                                         MODBUS_ADDR_MIN,
                                         MODBUS_ADDR_MAX);
        if (s_rs485_addr != prev8) {
            changed = 1U;
        }
        break;
    }

    case RS485_EDIT_FIELD_TERM:
        prev8 = s_rs485_term_on;
        s_rs485_term_on = (s_rs485_term_on == 0U) ? 1U : 0U;
        if (s_rs485_term_on != prev8) {
            changed = 1U;
        }
        break;

    case RS485_EDIT_FIELD_PARITY:
        prev8 = s_rs485_parity;
        s_rs485_parity = (s_rs485_parity == MODBUS_PARITY_EVEN)
            ? MODBUS_PARITY_NONE
            : MODBUS_PARITY_EVEN;
        if (s_rs485_parity != prev8) {
            changed = 1U;
        }
        break;

    default:
        break;
    }

    if (changed != 0U) {
        /* RS485 파라미터는 변경 즉시 dirty mark 후 idle 시 저장한다. */
        SaveRs485EditBuffer();
        MenuScreen_ForceRefresh();
        StartClickBeep();
    }
}

void Menu_Init(void)
{
    uint8_t i;

    s_state = MENU_STATE_SELECT;
    s_select_mode_active = 0U;
    s_run_freq_adjust_active = 0U;
    s_select_item = SELECT_ITEM_MODE;
    s_setting_item = SETTING_ITEM_FREQ;
    s_freq_field = FREQ_EDIT_FIELD_CH;
    s_power_field = POWER_EDIT_FIELD_CH;
    s_rs485_field = RS485_EDIT_FIELD_BAUD;

    s_selected_mode = MODE_NORMAL;
    s_selected_freq_ch = 0U;
    s_selected_power_ch = 0U;

    for (i = 0; i < FREQ_EDIT_CH_COUNT; i++) {
        s_freq_profiles[i].freq_01khz = s_freq_presets_01khz[i];
        s_freq_profiles[i].base_voltage_01v = RUN_BASE_VOLTAGE_DEFAULT_01V;
        s_freq_profiles[i].l_step = s_freq_presets_l_step[i];
        s_freq_nd_flags[i] = 0U;
    }

    for (i = 0; i < POWER_8STEP_COUNT; i++) {
        s_power_profiles[i].low_01w = POWER_PROFILE_LOW_DEFAULT;
        s_power_profiles[i].high_01w = POWER_PROFILE_HIGH_DEFAULT;
        s_power_profiles[i].def_01w = PowerProfileDefaultForChannel(i);
        s_power_profiles[i].tuned_freq_01khz = 0U;
        s_power_profiles[i].tuned_gate_duty_01pct = 0U;
        s_power_profiles[i].tuned_freq_ch = 0U;
        s_power_profiles[i].tuned_valid = 0U;
        s_power_profiles[i].tuned_valid_mask = 0U;
        for (uint8_t j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            s_power_profiles[i].tuned_freq_offset_01khz[j] = 0;
            s_power_profiles[i].tuned_gate_duty_02pct[j] = 0U;
        }
    }

    s_rs485_baud_idx = BaudToIndex(MODBUS_BAUD_DEFAULT);
    s_rs485_addr = MODBUS_ADDR_DEFAULT;
    s_rs485_term_on = MODBUS_TERM_DEFAULT;
    s_rs485_parity = MODBUS_PARITY_DEFAULT;
    s_rs485_term_enabled = MODBUS_TERM_DEFAULT;

    g_modbus_cfg.address = MODBUS_ADDR_DEFAULT;
    g_modbus_cfg.baudrate = MODBUS_BAUD_DEFAULT;
    g_modbus_cfg.parity = MODBUS_PARITY_DEFAULT;

    LoadStoredDataIfValid();

    LoadFreqEditBuffer(s_selected_freq_ch);
    LoadPowerEditBuffer(s_selected_power_ch);

    s_edit_saved = 1U;
    s_output_set_01w = POWER_DEFAULT;
    s_output_est_01w = POWER_DEFAULT;
    s_output_est_filter_x32 = 0U;
    s_output_est_filter_ready = 0U;
    s_output_est_display_sum = 0U;
    s_output_est_display_count = 0U;
    s_output_est_display_tick = 0U;
    s_output_est_filter_tick = 0U;
    s_output_est_hold_until_tick = 0U;
    s_error_code = ERR_NONE;

    s_alarm_beep_on = 0U;
    s_alarm_beep_off_tick = 0U;
    s_click_beep_on = 0U;
    s_click_beep_off_tick = 0U;
    s_autotune_armed = 0U;
    s_autotune_long_tick = 0U;
    s_autotune_running = 0U;
    s_autotune_status = TUNE_STATUS_IDLE;
    s_autotune_progress = 0U;
    s_autotune_probe_freq = s_freq_edit_freq;
    s_autotune_probe_l_step = s_freq_edit_l_step;
    s_autotune_power_01w = TUNE_TEST_POWER_01W;
    s_autotune_score = 0U;
    s_autotune_feedback.fwd_adc = 0U;
    s_autotune_feedback.ref_adc = 0U;
    s_autotune_feedback.cur_adc = 0U;
    s_autotune_feedback.vol_adc = 0U;
    s_phase_tune_prompt = 0U;
    s_set_exit_armed = 0U;
    s_set_exit_long_tick = 0U;
    s_store_dirty = 0U;
    s_store_due_tick = 0U;
    s_store_fail_count = 0U;
    s_saved_mode_shadow = s_selected_mode;
    s_saved_freq_ch_shadow = s_selected_freq_ch;
    s_saved_power_ch_shadow = s_selected_power_ch;
    s_remote_bcd_power_ch = s_selected_power_ch;
    s_remote_run_latched = 0U;
    s_remote_run_armed = 0U;
    s_low_alarm_pending = 0U;
    s_high_alarm_pending = 0U;
    s_low_alarm_start_tick = 0U;
    s_high_alarm_start_tick = 0U;
    s_run_freq_adjust_active = 0U;
    s_run_freq_manual_hold = 0U;
    s_overcurrent_latch_ma = 0U;
    s_overcurrent_latch_voltage_01v = 0U;
    ResetRunControlState();

    /* 저장 데이터가 있으면 RS-485 상태를 재동기화한다. */
    s_rs485_baud_idx = BaudToIndex(g_modbus_cfg.baudrate);
    s_rs485_addr = (uint8_t)ClampU16(g_modbus_cfg.address, MODBUS_ADDR_MIN, MODBUS_ADDR_MAX);
    s_rs485_parity = NormalizeRs485Parity(g_modbus_cfg.parity);

    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    ApplySelectedMode();
    ApplyFreqChannel(s_selected_freq_ch);
    ApplyPowerChannel(s_selected_power_ch);
    if (s_power1_default_migrated != 0U) {
        MarkStoreDirty();
    }

    /* 부팅 직후 1회 강제 렌더링으로 초기 화면 공백 방지 */
    MenuScreen_ForceRefresh();
}

void Menu_Update(void)
{
    ButtonEvent_t evt_start = Button_GetEvent(BTN_ID_START_STOP);
    ButtonEvent_t evt_mode  = Button_GetEvent(BTN_ID_MODE);
    ButtonEvent_t evt_up    = Button_GetEvent(BTN_ID_UP);
    ButtonEvent_t evt_down  = Button_GetEvent(BTN_ID_DOWN);
    ButtonEvent_t evt_set   = Button_GetEvent(BTN_ID_SET);

    EnforceRunUiLock();

    switch (s_state) {
    case MENU_STATE_SELECT:
        HandleSelectState(evt_start, evt_mode, evt_up, evt_down, evt_set);
        break;

    case MENU_STATE_SETTING_MENU:
        HandleSettingMenuState(evt_mode, evt_up, evt_down, evt_set);
        break;

    case MENU_STATE_SETTING_FREQ:
        HandleSettingFreqState(evt_start, evt_mode, evt_up, evt_down, evt_set);
        break;

    case MENU_STATE_SETTING_POWER:
        HandleSettingPowerState(evt_start, evt_mode, evt_up, evt_down, evt_set);
        break;

    case MENU_STATE_SETTING_RS485:
        HandleSettingRs485State(evt_mode, evt_up, evt_down, evt_set);
        break;

    default:
        s_state = MENU_STATE_SELECT;
        break;
    }

    ServiceStoreFlush();
    ServiceRuntimeSafety();
}

MenuState_t Menu_GetState(void)
{
    return s_state;
}

SelectItem_t Menu_GetSelectItem(void)
{
    return s_select_item;
}

uint8_t Menu_IsSelectModeActive(void)
{
    return s_select_mode_active;
}

SettingItem_t Menu_GetSettingItem(void)
{
    return s_setting_item;
}

FreqEditField_t Menu_GetFreqEditField(void)
{
    return s_freq_field;
}

PowerEditField_t Menu_GetPowerEditField(void)
{
    return s_power_field;
}

Rs485EditField_t Menu_GetRs485EditField(void)
{
    return s_rs485_field;
}

OperatingMode_t Menu_GetSelectedMode(void)
{
    return s_selected_mode;
}

uint8_t Menu_GetSelectedFreqChannel(void)
{
    return (uint8_t)(s_selected_freq_ch + 1U);
}

uint8_t Menu_GetSelectedPowerChannel(void)
{
    return (uint8_t)(s_selected_power_ch + 1U);
}

uint16_t Menu_GetOutputSetPower01W(void)
{
    return s_output_set_01w;
}

uint16_t Menu_GetOutputEstPower01W(void)
{
    return s_output_est_01w;
}

uint8_t Menu_IsRunFreqAdjustActive(void)
{
    return s_run_freq_adjust_active;
}

uint8_t Menu_GetFreqEditChannel(void)
{
    return (uint8_t)(s_freq_edit_ch + 1U);
}

uint16_t Menu_GetFreqEditFreq01kHz(void)
{
    return s_freq_edit_freq;
}

uint8_t Menu_GetFreqEditLStep(void)
{
    return s_freq_edit_l_step;
}

uint8_t Menu_GetPowerEditChannel(void)
{
    return (uint8_t)(s_power_edit_ch + 1U);
}

uint16_t Menu_GetPowerEditLow01W(void)
{
    return s_power_edit_low;
}

uint16_t Menu_GetPowerEditHigh01W(void)
{
    return s_power_edit_high;
}

uint16_t Menu_GetPowerEditDefault01W(void)
{
    return s_power_edit_def;
}

uint32_t Menu_GetRs485EditBaud(void)
{
    return MODBUS_BAUD_TABLE[s_rs485_baud_idx];
}

uint8_t Menu_GetRs485EditAddr(void)
{
    return s_rs485_addr;
}

uint8_t Menu_GetRs485EditTermEnabled(void)
{
    return (s_rs485_term_on != 0U) ? 1U : 0U;
}

uint8_t Menu_GetRs485EditParity(void)
{
    return NormalizeRs485Parity(s_rs485_parity);
}

uint8_t Menu_IsEditSaved(void)
{
    return s_edit_saved;
}

uint8_t Menu_HasError(void)
{
    return (s_error_code != ERR_NONE) ? 1U : 0U;
}

ErrorCode_t Menu_GetErrorCode(void)
{
    return s_error_code;
}

uint16_t Menu_GetOvercurrentLatchmA(void)
{
    return s_overcurrent_latch_ma;
}

uint16_t Menu_GetOvercurrentLatchVoltage01V(void)
{
    return s_overcurrent_latch_voltage_01v;
}

const FreqChannelProfile_t *Menu_GetFreqProfiles(void)
{
    return s_freq_profiles;
}

const PowerChannelProfile_t *Menu_GetPowerProfiles(void)
{
    return s_power_profiles;
}

uint8_t Menu_IsFreqChannelNoDetected(uint8_t ch_1based)
{
    uint8_t ch;

    if ((ch_1based == 0U) || (ch_1based > FREQ_EDIT_CH_COUNT)) {
        return 0U;
    }

    ch = (uint8_t)(ch_1based - 1U);
    return s_freq_nd_flags[ch];
}

uint8_t Menu_IsAutoTuneRunning(void)
{
    return s_autotune_running;
}

uint8_t Menu_IsPhaseTunePromptActive(void)
{
    return s_phase_tune_prompt;
}

AutoTuneStatus_t Menu_GetAutoTuneStatus(void)
{
    return s_autotune_status;
}

uint8_t Menu_GetAutoTuneProgress(void)
{
    return s_autotune_progress;
}

uint16_t Menu_GetAutoTuneProbeFreq01kHz(void)
{
    return s_autotune_probe_freq;
}

uint8_t Menu_GetAutoTuneProbeLStep(void)
{
    return s_autotune_probe_l_step;
}

uint16_t Menu_GetAutoTunePower01W(void)
{
    return s_autotune_power_01w;
}

uint16_t Menu_GetAutoTuneScore(void)
{
    return s_autotune_score;
}

uint16_t Menu_GetAutoTuneFwdAdc(void)
{
    return s_autotune_feedback.fwd_adc;
}

uint16_t Menu_GetAutoTuneRefAdc(void)
{
    return s_autotune_feedback.ref_adc;
}

uint16_t Menu_GetAutoTuneCurAdc(void)
{
    return s_autotune_feedback.cur_adc;
}

uint16_t Menu_GetAutoTuneVolAdc(void)
{
    return s_autotune_feedback.vol_adc;
}
