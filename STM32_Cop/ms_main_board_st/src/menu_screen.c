/**
 * @file  menu_screen.c
 * @brief LCD 화면 렌더링 — 메뉴 상태별 표시
 */
#include "menu_screen.h"
#include "lcd1602.h"
#include "megasonic_ctrl.h"
#include "modbus_rtu.h"
#include "adc_control.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include <stdio.h>

static uint8_t  s_need_refresh;
static uint32_t s_last_refresh_tick;

/* 16자 행 버퍼 (NULL 포함 17바이트) */
static char s_line0[17];
static char s_line1[17];

/* 전방 선언 */
static void RenderMain(void);
static void RenderFreq(void);
static void RenderDuty(void);
static void RenderMode(void);
static void RenderModbus(void);
static void RenderModbusAddr(void);
static void RenderModbusBaud(void);
static void RenderModbusParity(void);
static void RenderPulseOn(void);
static void RenderPulseOff(void);
static void RenderSweepStart(void);
static void RenderSweepEnd(void);
static void RenderSweepTime(void);
static void RenderInfo(void);

void MenuScreen_ForceRefresh(void)
{
    s_need_refresh = 1;
}

void MenuScreen_Refresh(void)
{
    if (!s_need_refresh) return;

    uint32_t now = HAL_GetTick();
    if ((now - s_last_refresh_tick) < LCD_REFRESH_MIN_MS) return;
    s_last_refresh_tick = now;
    s_need_refresh = 0;

    switch (Menu_GetState()) {
    case MENU_MAIN:          RenderMain();          break;
    case MENU_FREQ:          RenderFreq();          break;
    case MENU_DUTY:          RenderDuty();          break;
    case MENU_MODE:          RenderMode();          break;
    case MENU_MODBUS:        RenderModbus();        break;
    case MENU_MODBUS_ADDR:   RenderModbusAddr();    break;
    case MENU_MODBUS_BAUD:   RenderModbusBaud();    break;
    case MENU_MODBUS_PARITY: RenderModbusParity();  break;
    case MENU_PULSE_ON:      RenderPulseOn();       break;
    case MENU_PULSE_OFF:     RenderPulseOff();      break;
    case MENU_SWEEP_START:   RenderSweepStart();    break;
    case MENU_SWEEP_END:     RenderSweepEnd();      break;
    case MENU_SWEEP_TIME:    RenderSweepTime();     break;
    case MENU_INFO:          RenderInfo();           break;
    default: break;
    }

    LCD_SetCursor(0, 0);
    LCD_WriteString(s_line0);
    LCD_SetCursor(1, 0);
    LCD_WriteString(s_line1);
}

/* ================================================================
   화면 렌더링 함수
   ================================================================ */

static const char *ModeStr(OperatingMode_t m)
{
    switch (m) {
    case MODE_CONTINUOUS: return "CONT";
    case MODE_PULSE:      return "PULS";
    case MODE_SWEEP:      return "SWEP";
    default:              return "????";
    }
}

static void RenderMain(void)
{
    /* 1행: FREQ:28.0k  RUN */
    uint16_t f = g_us_state.target_freq;
    snprintf(s_line0, sizeof(s_line0), "FREQ:%2u.%ukHz %s",
             f / 10, f % 10,
             g_us_state.running ? "RUN" : "STP");

    /* 2행: DUTY:45.0%  CONT */
    uint16_t d = g_us_state.current_duty;
    snprintf(s_line1, sizeof(s_line1), "DUTY:%3u.%u%% %s",
             d / 10, d % 10,
             ModeStr(g_us_state.mode));
}

static void RenderFreq(void)
{
    uint16_t f = g_us_state.target_freq;
    snprintf(s_line0, sizeof(s_line0), "[FREQ SETTING]  ");
    snprintf(s_line1, sizeof(s_line1), " %2u.%u kHz      ", f / 10, f % 10);
}

static void RenderDuty(void)
{
    uint16_t d = g_us_state.target_duty;
    snprintf(s_line0, sizeof(s_line0), "[DUTY SETTING]  ");
    snprintf(s_line1, sizeof(s_line1), " %3u.%u %%       ", d / 10, d % 10);
}

static void RenderMode(void)
{
    static const char *names[] = {"Continuous", "Pulse", "Sweep"};
    uint8_t m = (uint8_t)g_us_state.mode;
    snprintf(s_line0, sizeof(s_line0), "[MODE SELECT]   ");
    snprintf(s_line1, sizeof(s_line1), ">%-15s", (m < MODE_COUNT) ? names[m] : "???");
}

static void RenderModbus(void)
{
    snprintf(s_line0, sizeof(s_line0), "[MODBUS CONFIG] ");
    snprintf(s_line1, sizeof(s_line1), "ADDR BAUD PARITY");
}

static void RenderModbusAddr(void)
{
    snprintf(s_line0, sizeof(s_line0), "[MODBUS ADDR]   ");
    snprintf(s_line1, sizeof(s_line1), " Addr: %3u      ", g_modbus_cfg.address);
}

static void RenderModbusBaud(void)
{
    snprintf(s_line0, sizeof(s_line0), "[MODBUS BAUD]   ");
    snprintf(s_line1, sizeof(s_line1), " %6lu bps     ", (unsigned long)g_modbus_cfg.baudrate);
}

static void RenderModbusParity(void)
{
    static const char *par[] = {"None", "Even", "Odd"};
    uint8_t p = g_modbus_cfg.parity;
    snprintf(s_line0, sizeof(s_line0), "[MODBUS PARITY] ");
    snprintf(s_line1, sizeof(s_line1), " Parity: %-6s ", (p <= 2) ? par[p] : "???");
}

static void RenderPulseOn(void)
{
    snprintf(s_line0, sizeof(s_line0), "[PULSE ON TIME] ");
    snprintf(s_line1, sizeof(s_line1), " %5u ms       ", g_us_state.pulse_on_ms);
}

static void RenderPulseOff(void)
{
    snprintf(s_line0, sizeof(s_line0), "[PULSE OFF TIME]");
    snprintf(s_line1, sizeof(s_line1), " %5u ms       ", g_us_state.pulse_off_ms);
}

static void RenderSweepStart(void)
{
    uint16_t f = g_us_state.sweep_start_freq;
    snprintf(s_line0, sizeof(s_line0), "[SWEEP START]   ");
    snprintf(s_line1, sizeof(s_line1), " %2u.%u kHz      ", f / 10, f % 10);
}

static void RenderSweepEnd(void)
{
    uint16_t f = g_us_state.sweep_end_freq;
    snprintf(s_line0, sizeof(s_line0), "[SWEEP END]     ");
    snprintf(s_line1, sizeof(s_line1), " %2u.%u kHz      ", f / 10, f % 10);
}

static void RenderSweepTime(void)
{
    snprintf(s_line0, sizeof(s_line0), "[SWEEP TIME]    ");
    snprintf(s_line1, sizeof(s_line1), " %5u ms       ", g_us_state.sweep_time_ms);
}

static void RenderInfo(void)
{
    uint32_t sec = HAL_GetTick() / 1000;
    snprintf(s_line0, sizeof(s_line0), "FW v%u.%u.%u      ",
             FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    snprintf(s_line1, sizeof(s_line1), "Up:%02luh%02lum%02lus   ",
             (unsigned long)(sec / 3600),
             (unsigned long)((sec % 3600) / 60),
             (unsigned long)(sec % 60));
}
