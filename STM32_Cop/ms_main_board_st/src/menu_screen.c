/**
 * @file  menu_screen.c
 * @brief LCD 화면 렌더링 — 선택 모드/설정 모드 표시
 */
#include "menu_screen.h"
#include "lcd1602.h"
#include "megasonic_ctrl.h"
#include "adc_control.h"
#include "menu.h"
#include "config.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static uint8_t  s_need_refresh;
static uint32_t s_last_refresh_tick;
static uint8_t  s_last_run_anim_phase = 0xFFU;
static uint8_t  s_last_freq_blink_phase = 0xFFU;
static uint8_t  s_last_led_mask = 0xFFU;
static uint8_t  s_spinner_chars_ready;
static uint8_t  s_run_meter_ready;
static uint8_t  s_run_meter_count;
static uint16_t s_run_meter_power_01w;
static uint16_t s_run_meter_cur_raw;
static uint16_t s_run_meter_cur_ma;
static uint32_t s_run_meter_power_sum;
static uint32_t s_run_meter_cur_raw_sum;
static uint32_t s_run_meter_cur_ma_sum;
static uint32_t s_run_meter_tick;

/* 16자 행 버퍼 (NULL 포함 17바이트) */
static char s_line0[17];
static char s_line1[17];

#define RUN_SPINNER_MS 210U
#define RUN_SPINNER_FRAME_COUNT 4U
#define RUN_SPINNER_CHAR_BASE 1U
#define RUN_LED_BLINK_MS 250U
#define FREQ_EDIT_BLINK_MS 350U
#define RUN_METER_DISPLAY_MS 1600U
#define RUN_METER_DROP_LIMIT_01W 20U

/* 임의 포맷 문자열을 LCD 16자 라인으로 안전하게 정규화 */
static void FormatLine(char *dst, const char *fmt, ...)
{
    char tmp[64];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);

    snprintf(dst, 17, "%-16.16s", tmp);
}

/* 16자 라인 중앙 정렬 */
static void CenterLine(char *dst, const char *text)
{
    size_t len;
    size_t pad;

    memset(dst, ' ', 16U);
    dst[16] = '\0';

    len = strlen(text);
    if (len > 16U) {
        len = 16U;
    }

    pad = (16U - len) / 2U;
    memcpy(&dst[pad], text, len);
}

/* 전방 선언 */
static void RenderSelect(void);
static void RenderSettingMenu(void);
static void RenderSettingFreq(void);
static void RenderSettingPower(void);
static void RenderSettingRs485(void);
static void UpdateStatusLeds(uint32_t now, MenuState_t state);
static void EnsureRunSpinnerChars(void);

static uint16_t LedBitsToPins(uint8_t led_bits)
{
    uint16_t pins = 0U;

    if ((led_bits & LED_BIT_NORMAL) != 0U) { pins |= GPIO_PIN_10; }
    if ((led_bits & LED_BIT_HL_SET) != 0U) { pins |= GPIO_PIN_11; }
    if ((led_bits & LED_BIT_8POWER) != 0U) { pins |= GPIO_PIN_12; }
    if ((led_bits & LED_BIT_REMOTE) != 0U) { pins |= GPIO_PIN_13; }
    if ((led_bits & LED_BIT_EXT) != 0U) { pins |= GPIO_PIN_14; }
    if ((led_bits & LED_BIT_RX) != 0U) { pins |= GPIO_PIN_15; }

    return pins;
}

static void LatchLedMask(uint8_t led_bits)
{
    uint16_t set_pins = LedBitsToPins(led_bits);

    HAL_GPIO_WritePin(DISP_DATA_PORT, DISP_DATA_PINS, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(DISP_DATA_PORT, set_pins, GPIO_PIN_SET);

    HAL_GPIO_WritePin(LED_CLK_PORT, LED_CLK_PIN, GPIO_PIN_SET);
    __NOP();
    __NOP();
    HAL_GPIO_WritePin(LED_CLK_PORT, LED_CLK_PIN, GPIO_PIN_RESET);
}

static void UpdateStatusLeds(uint32_t now, MenuState_t state)
{
    uint8_t led_mask = 0U;
    uint8_t mode_led = 0U;
    uint8_t run_blink_phase = (uint8_t)((now / RUN_LED_BLINK_MS) & 0x01U);

    switch (Menu_GetSelectedMode()) {
    case MODE_NORMAL: mode_led = LED_BIT_NORMAL; break;
    case MODE_REMOTE: mode_led = LED_BIT_REMOTE; break;
    case MODE_EXT:    mode_led = LED_BIT_EXT; break;
    default:          mode_led = LED_BIT_NORMAL; break;
    }

    /* RUN 중에는 현재 모드 LED를 점멸시켜 동작 중임을 표시 */
    if (g_us_state.running) {
        if (run_blink_phase == 0U) {
            led_mask |= mode_led;
        }
    } else {
        led_mask |= mode_led;
    }

    /* H/L SET LED: POWER 설정의 LOW/HIGH 편집에서만 점등 */
    if (state == MENU_STATE_SETTING_POWER) {
        PowerEditField_t field = Menu_GetPowerEditField();
        if ((field == POWER_EDIT_FIELD_LOW) || (field == POWER_EDIT_FIELD_HIGH)) {
            led_mask |= LED_BIT_HL_SET;
        }

        /* 8POWER LED: POWER 설정의 CH/DEF 편집에서 점등 */
        if ((field == POWER_EDIT_FIELD_CH)
            || (field == POWER_EDIT_FIELD_DEFAULT)) {
            led_mask |= LED_BIT_8POWER;
        }
    }

    /* 선택모드에서 8POWER 항목 활성 시 8POWER LED 점등 */
    if ((state == MENU_STATE_SELECT)
        && (Menu_IsSelectModeActive() != 0U)
        && (Menu_GetSelectItem() == SELECT_ITEM_8POWER)) {
        led_mask |= LED_BIT_8POWER;
    }

    if (led_mask == s_last_led_mask) {
        return;
    }

    LatchLedMask(led_mask);
    s_last_led_mask = led_mask;
}

static void EnsureRunSpinnerChars(void)
{
    static const uint8_t s_spinner_patterns[RUN_SPINNER_FRAME_COUNT][8] = {
        /* + : 세로줄을 길게, 중심은 3행(0-base) 고정 */
        { 0x04U, 0x04U, 0x04U, 0x1FU, 0x04U, 0x04U, 0x04U, 0x00U },
        /* / : 중심 픽셀(3행, 0x04)을 지나가도록 정렬 */
        { 0x00U, 0x10U, 0x08U, 0x04U, 0x02U, 0x01U, 0x00U, 0x00U },
        /* - : 중심 행 단일 라인 */
        { 0x00U, 0x00U, 0x00U, 0x1FU, 0x00U, 0x00U, 0x00U, 0x00U },
        /* \\ : 중심 픽셀(3행, 0x04)을 지나가도록 정렬 */
        { 0x00U, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x00U, 0x00U },
    };
    uint8_t i;

    if (s_spinner_chars_ready != 0U) {
        return;
    }

    for (i = 0U; i < RUN_SPINNER_FRAME_COUNT; i++) {
        LCD_CreateChar((uint8_t)(RUN_SPINNER_CHAR_BASE + i), s_spinner_patterns[i]);
    }

    s_spinner_chars_ready = 1U;
}

static const char *ModeName(OperatingMode_t mode)
{
    switch (mode) {
    case MODE_NORMAL: return "NORMAL";
    case MODE_REMOTE: return "REMOTE";
    case MODE_EXT:    return "EXT";
    default:          return "NORMAL";
    }
}

static void FormatPower01W(char *dst, size_t size, uint16_t p01w)
{
    snprintf(dst, size, "%u.%02uW", (unsigned)(p01w / 100U), (unsigned)(p01w % 100U));
}

static void ResetRunMeterDisplay(void)
{
    s_run_meter_ready = 0U;
    s_run_meter_count = 0U;
    s_run_meter_power_01w = 0U;
    s_run_meter_cur_raw = 0U;
    s_run_meter_cur_ma = 0U;
    s_run_meter_power_sum = 0U;
    s_run_meter_cur_raw_sum = 0U;
    s_run_meter_cur_ma_sum = 0U;
    s_run_meter_tick = HAL_GetTick();
}

static void UpdateRunMeterDisplay(uint16_t power_01w,
                                  uint16_t cur_raw,
                                  uint16_t cur_ma)
{
    uint32_t now = HAL_GetTick();

    if (s_run_meter_ready == 0U) {
        s_run_meter_power_01w = power_01w;
        s_run_meter_cur_raw = cur_raw;
        s_run_meter_cur_ma = cur_ma;
        s_run_meter_tick = now;
        s_run_meter_ready = 1U;
    }

    if (s_run_meter_count < 40U) {
        s_run_meter_power_sum += power_01w;
        s_run_meter_cur_raw_sum += cur_raw;
        s_run_meter_cur_ma_sum += cur_ma;
        s_run_meter_count++;
    }

    if ((now - s_run_meter_tick) >= RUN_METER_DISPLAY_MS) {
        if (s_run_meter_count != 0U) {
            uint16_t avg_power_01w = (uint16_t)((s_run_meter_power_sum + (s_run_meter_count / 2U))
                                                / s_run_meter_count);

            if ((s_run_meter_power_01w > avg_power_01w)
                && ((uint16_t)(s_run_meter_power_01w - avg_power_01w) > RUN_METER_DROP_LIMIT_01W)) {
                s_run_meter_power_01w = (uint16_t)(s_run_meter_power_01w - RUN_METER_DROP_LIMIT_01W);
            } else {
                s_run_meter_power_01w = avg_power_01w;
            }
            s_run_meter_cur_raw = (uint16_t)((s_run_meter_cur_raw_sum + (s_run_meter_count / 2U))
                                             / s_run_meter_count);
            s_run_meter_cur_ma = (uint16_t)((s_run_meter_cur_ma_sum + (s_run_meter_count / 2U))
                                            / s_run_meter_count);
        }

        s_run_meter_power_sum = 0U;
        s_run_meter_cur_raw_sum = 0U;
        s_run_meter_cur_ma_sum = 0U;
        s_run_meter_count = 0U;
        s_run_meter_tick = now;
    }
}

static void PutPowerTagAt(char *dst, uint8_t start, char tag, uint16_t p01w)
{
    uint16_t p10 = (uint16_t)((p01w + 5U) / 10U);
    uint8_t i_part;
    uint8_t d_part;

    if (p10 > 999U) {
        p10 = 999U;
    }

    if (p10 >= 100U) {
        dst[start + 0U] = tag;
        dst[start + 1U] = (char)('0' + ((p10 / 100U) % 10U));
        dst[start + 2U] = (char)('0' + ((p10 / 10U) % 10U));
        dst[start + 3U] = '.';
        dst[start + 4U] = (char)('0' + (p10 % 10U));
    } else {
        i_part = (uint8_t)(p10 / 10U);
        d_part = (uint8_t)(p10 % 10U);

        dst[start + 0U] = tag;
        dst[start + 1U] = ':';
        dst[start + 2U] = (char)('0' + i_part);
        dst[start + 3U] = ',';
        dst[start + 4U] = (char)('0' + d_part);
    }
}

static void FormatPowerHldTopLine(char *dst, uint16_t high_01w, uint16_t low_01w, uint16_t def_01w)
{
    memset(dst, ' ', 16U);
    dst[16] = '\0';

    PutPowerTagAt(dst, 0U, 'H', high_01w);  /* 좌측 */
    PutPowerTagAt(dst, 5U, 'L', low_01w);   /* 중간 */
    PutPowerTagAt(dst, 11U, 'D', def_01w);  /* 우측 */
}

static void ComboToBits(uint8_t combo, char bits[5])
{
    bits[0] = ((combo & 0x08U) != 0U) ? '1' : '0';
    bits[1] = ((combo & 0x04U) != 0U) ? '1' : '0';
    bits[2] = ((combo & 0x02U) != 0U) ? '1' : '0';
    bits[3] = ((combo & 0x01U) != 0U) ? '1' : '0';
    bits[4] = '\0';
}

static const char *ErrorText(ErrorCode_t err)
{
    switch (err) {
    case ERR_LOW:        return "ERR1 LOW ALARM";
    case ERR_HIGH:       return "ERR2 HIGH ALRM";
    case ERR_TRANSDUCER: return "ERR5 TRANS ALM";
    case ERR_SETTING:    return "ERR6 SET ERROR";
    case ERR_SENSOR_OFF: return "ERR7 SNS OFF";
    case ERR_SENSOR_RUN: return "ERR8 SNS RUN";
    case ERR_OVERCURRENT:return "OVER CURRENT";
    case ERR_START_VOLT: return "LOW START VOLT";
    case ERR_CUR_SENSOR: return "CUR SENSOR FLT";
    case ERR_BUCK_OVERVOLT:return "BUCK OVER VOLT";
    default:             return "ERR UNKNOWN";
    }
}

void MenuScreen_ForceRefresh(void)
{
    s_need_refresh = 1U;
}

void MenuScreen_Refresh(void)
{
    uint32_t now;
    MenuState_t state;
    uint8_t run_anim_refresh_needed = 0U;
    uint8_t freq_blink_refresh_needed = 0U;
    uint8_t run_anim_phase = 0U;
    uint8_t freq_blink_phase = 0U;
    uint8_t run_anim_changed;
    uint8_t freq_blink_changed;

    now = HAL_GetTick();
    state = Menu_GetState();

    EnsureRunSpinnerChars();

    UpdateStatusLeds(now, state);

    if ((state == MENU_STATE_SELECT)
        && (g_us_state.running)
        && (Menu_HasError() == 0U)) {
        if ((Menu_IsSelectModeActive() == 0U)
            || (Menu_GetSelectItem() == SELECT_ITEM_MODE)) {
            run_anim_refresh_needed = 1U;
            run_anim_phase = (uint8_t)((now / RUN_SPINNER_MS) % RUN_SPINNER_FRAME_COUNT);
        }
    }
    if (state == MENU_STATE_SETTING_FREQ) {
        freq_blink_refresh_needed = 1U;
        freq_blink_phase = (uint8_t)((now / FREQ_EDIT_BLINK_MS) & 0x01U);
    }

    run_anim_changed = ((run_anim_refresh_needed != 0U) && (run_anim_phase != s_last_run_anim_phase)) ? 1U : 0U;
    freq_blink_changed = ((freq_blink_refresh_needed != 0U) && (freq_blink_phase != s_last_freq_blink_phase)) ? 1U : 0U;

    if (s_need_refresh == 0U) {
        if ((run_anim_changed == 0U) && (freq_blink_changed == 0U)) {
            return;
        }
    }

    if ((now - s_last_refresh_tick) < LCD_REFRESH_MIN_MS) {
        return;
    }

    s_last_refresh_tick = now;
    s_need_refresh = 0U;

    if (run_anim_refresh_needed != 0U) {
        s_last_run_anim_phase = run_anim_phase;
    } else {
        s_last_run_anim_phase = 0xFFU;
    }
    if (freq_blink_refresh_needed != 0U) {
        s_last_freq_blink_phase = freq_blink_phase;
    } else {
        s_last_freq_blink_phase = 0xFFU;
    }

    switch (state) {
    case MENU_STATE_SELECT:
        RenderSelect();
        break;

    case MENU_STATE_SETTING_MENU:
        RenderSettingMenu();
        break;

    case MENU_STATE_SETTING_FREQ:
        RenderSettingFreq();
        break;

    case MENU_STATE_SETTING_POWER:
        RenderSettingPower();
        break;

    case MENU_STATE_SETTING_RS485:
        RenderSettingRs485();
        break;

    default:
        RenderSelect();
        break;
    }

    LCD_SetCursor(0U, 0U);
    LCD_WriteString(s_line0);
    LCD_SetCursor(1U, 0U);
    LCD_WriteString(s_line1);
}

/* ================================================================
   화면 렌더링
   ================================================================ */

static void RenderSelect(void)
{
    const FreqChannelProfile_t *f_tbl = Menu_GetFreqProfiles();
    const PowerChannelProfile_t *p_tbl = Menu_GetPowerProfiles();
    char power_text[8];
    char ch_text[8];
    char freq_text[12];
    uint16_t disp_power_01w;
    uint16_t measured_power_01w;
    uint16_t set_power_01w;
    uint16_t vol_adc;
    uint16_t disp_freq_khz;
    uint16_t target_freq_01khz;
    uint16_t freq_khz_u16;
    uint16_t freq_khz_rem;
    uint16_t sel_freq_khz;
    uint8_t expected_combo;
    uint8_t actual_combo;
    char expected_bits[5];
    char actual_bits[5];
    char run_spinner_char;
    uint8_t run_phase;
    const char *run_text;
    uint8_t freq_ch = Menu_GetSelectedFreqChannel();
    uint8_t power_ch = Menu_GetSelectedPowerChannel();

    if (Menu_HasError() != 0U) {
        if ((Menu_GetErrorCode() == ERR_OVERCURRENT)
            || (Menu_GetErrorCode() == ERR_CUR_SENSOR)) {
            uint16_t oc_ma = Menu_GetOvercurrentLatchmA();
            uint16_t oc_voltage_01v = Menu_GetOvercurrentLatchVoltage01V();

            FormatLine(s_line0,
                       "%-16s",
                       (Menu_GetErrorCode() == ERR_CUR_SENSOR) ? "CUR SENSOR FLT" : "OVER CURRENT");
            FormatLine(s_line1, "I%04umA V%02u.%u",
                       (unsigned)oc_ma,
                       (unsigned)(oc_voltage_01v / 100U),
                       (unsigned)((oc_voltage_01v / 10U) % 10U));
        } else {
            CenterLine(s_line0, "ALARM STOP");
            FormatLine(s_line1, "%-16s", ErrorText(Menu_GetErrorCode()));
        }
        return;
    }

    if (g_us_state.running) {
        measured_power_01w = Menu_GetOutputEstPower01W();
        UpdateRunMeterDisplay(measured_power_01w, 0U, 0U);
        disp_power_01w = s_run_meter_power_01w;
        target_freq_01khz = MegasonicCtrl_GetActualFrequency01kHz();
        disp_freq_khz = (uint16_t)((target_freq_01khz + 5U) / 10U);
        run_phase = (uint8_t)((HAL_GetTick() / RUN_SPINNER_MS) % RUN_SPINNER_FRAME_COUNT);
        run_spinner_char = (char)(RUN_SPINNER_CHAR_BASE + run_phase);
        run_text = "RUN";
    } else {
        ResetRunMeterDisplay();
        disp_power_01w = 0U;
        disp_freq_khz = 0U;
        run_spinner_char = ' ';
        run_text = "STOP";
    }

    FormatPower01W(power_text, sizeof(power_text), disp_power_01w);

    freq_khz_u16 = (uint16_t)(disp_freq_khz / 1000U);
    freq_khz_rem = (uint16_t)(disp_freq_khz % 1000U);
    sel_freq_khz = (uint16_t)((f_tbl[freq_ch - 1U].freq_01khz + 5U) / 10U);

    FormatLine(s_line0, "%s%c   %u%03ukHz",
               power_text,
               run_spinner_char,
               (unsigned)freq_khz_u16,
               (unsigned)freq_khz_rem);

    if (Menu_IsSelectModeActive() == 0U) {
        if (g_us_state.running) {
            if (Menu_IsRunFreqAdjustActive() != 0U) {
                target_freq_01khz = MegasonicCtrl_GetActualFrequency01kHz();
                FormatLine(s_line1, "F%u.%uk CH%02u",
                           (unsigned)(target_freq_01khz / 10U),
                           (unsigned)(target_freq_01khz % 10U),
                           (unsigned)freq_ch);
            } else {
                set_power_01w = Menu_GetOutputSetPower01W();
                vol_adc = ADC_Control_GetVoltage01V();
                FormatLine(s_line1, "W%u.%u %02u.%uV CH%02u",
                       (unsigned)(set_power_01w / 100U),
                       (unsigned)((set_power_01w / 10U) % 10U),
                       (unsigned)(vol_adc / 100U),
                       (unsigned)((vol_adc / 10U) % 10U),
                       (unsigned)freq_ch);
            }
        } else {
            FormatLine(s_line1, "%-6s %s C%02u",
                       ModeName(Menu_GetSelectedMode()),
                       run_text,
                       (unsigned)freq_ch);
        }
        return;
    }

    switch (Menu_GetSelectItem()) {
    case SELECT_ITEM_MODE:
        FormatLine(s_line1, ">%s CH%02u P%u",
                   ModeName(Menu_GetSelectedMode()),
                   (unsigned)freq_ch,
                   (unsigned)power_ch);
        break;
    case SELECT_ITEM_FREQ_CH:
        if ((f_tbl[freq_ch - 1U].freq_01khz < FREQ_EDIT_MIN)
            || (f_tbl[freq_ch - 1U].freq_01khz > FREQ_EDIT_MAX)) {
            sel_freq_khz = 0U;
        }

        snprintf(ch_text, sizeof(ch_text), "%02uCH", (unsigned)freq_ch);
        snprintf(freq_text, sizeof(freq_text), "%04ukHz", (unsigned)sel_freq_khz);

        /* 상단: 채널은 좌측 정렬, 주파수는 우측 정렬 */
        FormatLine(s_line0, "%-4s%12s", ch_text, freq_text);
        expected_combo = (uint8_t)(f_tbl[freq_ch - 1U].l_step - 1U);
        actual_combo = MegasonicCtrl_GetLCRelay();
        ComboToBits(expected_combo, expected_bits);
        ComboToBits(actual_combo, actual_bits);
        FormatLine(s_line1, "CH%02u E%s A%s", (unsigned)freq_ch, expected_bits, actual_bits);
        break;
    case SELECT_ITEM_8POWER:
        FormatPowerHldTopLine(s_line0,
                              p_tbl[power_ch - 1U].high_01w,
                              p_tbl[power_ch - 1U].low_01w,
                              p_tbl[power_ch - 1U].def_01w);
        FormatLine(s_line1, ">PWR%u CH%02u", (unsigned)power_ch, (unsigned)freq_ch);
        break;
    default:
        FormatLine(s_line1, ">NORMAL");
        break;
    }
}

static void RenderSettingMenu(void)
{
    CenterLine(s_line0, "SETTING MODE");
    if (Menu_GetSettingItem() == SETTING_ITEM_FREQ) {
        FormatLine(s_line1, ">1.FREQ SET");
    } else if (Menu_GetSettingItem() == SETTING_ITEM_POWER) {
        FormatLine(s_line1, ">2.POWER SET");
    } else {
        FormatLine(s_line1, ">3.EXT:RS485");
    }
}

static void RenderSettingFreq(void)
{
    AutoTuneStatus_t tune_status;
    uint8_t tune_running;
    uint8_t tune_progress;
    uint8_t expected_combo;
    uint8_t actual_combo;
    uint8_t combo;
    char bits[5];
    char expected_bits[5];
    char actual_bits[5];
    uint8_t ch = Menu_GetFreqEditChannel();
    uint16_t freq_khz = (Menu_GetFreqEditFreq01kHz() + 5U) / 10U;
    uint8_t l_step = Menu_GetFreqEditLStep();
    uint16_t probe_freq_khz = (Menu_GetAutoTuneProbeFreq01kHz() + 5U) / 10U;
    uint8_t probe_l_step = Menu_GetAutoTuneProbeLStep();
    uint8_t edit_visible = (uint8_t)(((HAL_GetTick() / FREQ_EDIT_BLINK_MS) & 0x01U) == 0U);

    tune_status = Menu_GetAutoTuneStatus();
    tune_running = Menu_IsAutoTuneRunning();
    tune_progress = Menu_GetAutoTuneProgress();
    expected_combo = (uint8_t)(l_step - 1U);
    actual_combo = MegasonicCtrl_GetLCRelay();
    ComboToBits(expected_combo, expected_bits);
    ComboToBits(actual_combo, actual_bits);

    combo = expected_combo;
    ComboToBits(combo, bits);

    if (Menu_IsPhaseTunePromptActive() != 0U) {
        FormatLine(s_line0, "PHASE TUNE?");
        FormatLine(s_line1, "SET:RUN MODE:NO");
        return;
    }

    if (tune_running != 0U) {
        switch (tune_status) {
        case TUNE_STATUS_PREPARE:
            FormatLine(s_line0, "CH%02u PREPARE", (unsigned)ch);
            break;
        case TUNE_STATUS_SCAN_FREQ:
            FormatLine(s_line0, "CH%02u FREQ %3u%%", (unsigned)ch, (unsigned)tune_progress);
            break;
        case TUNE_STATUS_SCAN_L:
            FormatLine(s_line0, "CH%02u LSTEP%3u%%", (unsigned)ch, (unsigned)tune_progress);
            break;
        case TUNE_STATUS_FINE_FREQ:
            FormatLine(s_line0, "CH%02u FFINE%3u%%", (unsigned)ch, (unsigned)tune_progress);
            break;
        case TUNE_STATUS_FINE_L:
            FormatLine(s_line0, "CH%02u LFINE%3u%%", (unsigned)ch, (unsigned)tune_progress);
            break;
        case TUNE_STATUS_PHASE_CHECK:
            FormatLine(s_line0, "ADC1 CHECK%3u%%", (unsigned)tune_progress);
            break;
        case TUNE_STATUS_PHASE_FINE:
            FormatLine(s_line0, "PHASE FINE%3u%%", (unsigned)tune_progress);
            break;
        default:
            FormatLine(s_line0, "AUTO TUNING...");
            break;
        }

        FormatLine(s_line1, "F%04ukHz L%02u", (unsigned)probe_freq_khz, (unsigned)probe_l_step);
        return;
    }

    switch (tune_status) {
    case TUNE_STATUS_PHASE_DONE:
        FormatLine(s_line0, "PHASE TUNE DONE");
        FormatLine(s_line1, "F%04ukHz L%02u", (unsigned)probe_freq_khz, (unsigned)probe_l_step);
        return;
    case TUNE_STATUS_DONE:
        FormatLine(s_line0, "CH%02u TUNE DONE", (unsigned)ch);
        FormatLine(s_line1, "F%04ukHz L%02u", (unsigned)probe_freq_khz, (unsigned)probe_l_step);
        return;
    case TUNE_STATUS_NO_DETECTED:
        FormatLine(s_line0, "CH%02u NO DETECT", (unsigned)ch);
        FormatLine(s_line1, "F%04ukHz L%02u", (unsigned)probe_freq_khz, (unsigned)probe_l_step);
        return;
    case TUNE_STATUS_TIMEOUT:
        FormatLine(s_line0, "CH%02u TIMEOUT", (unsigned)ch);
        FormatLine(s_line1, "F%04ukHz L%02u", (unsigned)probe_freq_khz, (unsigned)probe_l_step);
        return;
    default:
        if (Menu_GetFreqEditField() == FREQ_EDIT_FIELD_CH) {
            FormatLine(s_line0, "FREQ:%04ukHz", (unsigned)freq_khz);
        } else {
            FormatLine(s_line0, "START(3SEC):AUTO");
        }
        break;
    }

    switch (Menu_GetFreqEditField()) {
    case FREQ_EDIT_FIELD_CH:
        if (edit_visible != 0U) {
            FormatLine(s_line1, "CH%02u E%s A%s", (unsigned)ch, expected_bits, actual_bits);
        } else {
            FormatLine(s_line1, "CH   E%s A%s", expected_bits, actual_bits);
        }
        break;

    case FREQ_EDIT_FIELD_FREQ:
        if (edit_visible != 0U) {
            FormatLine(s_line1, "FREQ:%04ukHz", (unsigned)freq_khz);
        } else {
            FormatLine(s_line1, "FREQ:    kHz");
        }
        break;

    case FREQ_EDIT_FIELD_L:
        if (edit_visible != 0U) {
            FormatLine(s_line1, "BIT:%s %02u/16", bits, (unsigned)l_step);
        } else {
            FormatLine(s_line1, "BIT:       /16");
        }
        break;

    default:
        FormatLine(s_line1, "SET:MODE NEXT");
        break;
    }
}

static void RenderSettingPower(void)
{
    const FreqChannelProfile_t *f_tbl = Menu_GetFreqProfiles();
    uint8_t ch = Menu_GetPowerEditChannel();
    uint8_t freq_ch = Menu_GetSelectedFreqChannel();
    uint16_t freq_khz = 0U;
    uint16_t low = Menu_GetPowerEditLow01W();
    uint16_t high = Menu_GetPowerEditHigh01W();
    uint16_t def = Menu_GetPowerEditDefault01W();
    char ptxt[8];

    if ((freq_ch >= 1U) && (freq_ch <= FREQ_EDIT_CH_COUNT)) {
        freq_khz = (uint16_t)((f_tbl[freq_ch - 1U].freq_01khz + 5U) / 10U);
    }

    FormatLine(s_line0, "P%u CH%02u %04uk", (unsigned)ch, (unsigned)freq_ch, (unsigned)freq_khz);

    switch (Menu_GetPowerEditField()) {
    case POWER_EDIT_FIELD_CH:
        FormatLine(s_line1, "CH:%u SET:SAVE", (unsigned)ch);
        break;

    case POWER_EDIT_FIELD_LOW:
        FormatPower01W(ptxt, sizeof(ptxt), low);
        FormatLine(s_line1, "LOW:%s", ptxt);
        break;

    case POWER_EDIT_FIELD_HIGH:
        FormatPower01W(ptxt, sizeof(ptxt), high);
        FormatLine(s_line1, "HIGH:%s", ptxt);
        break;

    case POWER_EDIT_FIELD_DEFAULT:
        FormatPower01W(ptxt, sizeof(ptxt), def);
        FormatLine(s_line1, "DEF:%s", ptxt);
        break;

    default:
        FormatLine(s_line1, "MODE:NEXT");
        break;
    }
}

static void RenderSettingRs485(void)
{
    uint32_t baud = Menu_GetRs485EditBaud();
    uint8_t addr = Menu_GetRs485EditAddr();
    uint8_t term_on = Menu_GetRs485EditTermEnabled();
    uint8_t parity = Menu_GetRs485EditParity();

    CenterLine(s_line0, "EXT:RS485 SET");

    switch (Menu_GetRs485EditField()) {
    case RS485_EDIT_FIELD_BAUD:
        FormatLine(s_line1, "BAUD:%lu", (unsigned long)baud);
        break;

    case RS485_EDIT_FIELD_ADDR:
        FormatLine(s_line1, "ADDR:%03u", (unsigned)addr);
        break;

    case RS485_EDIT_FIELD_TERM:
        FormatLine(s_line1, "TERM:SW SET %s", (term_on != 0U) ? "ON" : "OFF");
        break;

    case RS485_EDIT_FIELD_PARITY:
        FormatLine(s_line1,
                   "%s",
                   (parity == MODBUS_PARITY_EVEN) ? "PARI:EVEN-8-1" : "PARI:NONE-8-1");
        break;

    default:
        FormatLine(s_line1, "MODE:NEXT");
        break;
    }
}
