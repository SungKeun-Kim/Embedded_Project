/**
 * @file  menu.c
 * @brief 메뉴 FSM — 상태 전이 및 값 편집 로직
 */
#include "menu.h"
#include "menu_screen.h"
#include "button.h"
#include "megasonic_ctrl.h"
#include "modbus_rtu.h"
#include "params.h"
#include "stm32g4xx_hal.h"

static MenuState_t s_state;
static uint8_t     s_menu_idx;  /* 메뉴 항목 인덱스 (서브메뉴 내 위치) */

/* 메인 메뉴 항목 수 */
#define MAIN_MENU_ITEMS  5  /* FREQ, DUTY, MODE, MODBUS, INFO */

/* 모드 서브메뉴 항목 수 */
#define MODE_ITEMS       ((uint8_t)MODE_COUNT)

/* Modbus 서브메뉴 항목 수 */
#define MODBUS_SUB_ITEMS 3  /* ADDR, BAUD, PARITY */

/* Baud 인덱스 → 실제 baud 변환 */
static uint8_t s_baud_idx;
static const uint32_t BAUD_TABLE[MODBUS_BAUD_INDEX_COUNT] = {
    9600U, 19200U, 38400U, 115200U
};

/* 전방 선언 */
static void HandleMainMenu(ButtonEvent_t up, ButtonEvent_t down,
                            ButtonEvent_t ok, ButtonEvent_t back);
static void HandleValueEdit(ButtonEvent_t up, ButtonEvent_t down,
                            ButtonEvent_t ok, ButtonEvent_t back);

void Menu_Init(void)
{
    s_state    = MENU_MAIN;
    s_menu_idx = 0;
    s_baud_idx = 0;
    MenuScreen_ForceRefresh();
}

void Menu_Update(void)
{
    ButtonEvent_t up   = Button_GetEvent(BTN_ID_UP);
    ButtonEvent_t down = Button_GetEvent(BTN_ID_DOWN);
    ButtonEvent_t ok   = Button_GetEvent(BTN_ID_OK);
    ButtonEvent_t back = Button_GetEvent(BTN_ID_BACK);

    /* BACK 장기 누름 → 비상 정지 (모든 상태에서) */
    if (back == BTN_EVT_LONG_PRESS) {
        MegasonicCtrl_EmergencyStop();
        MenuScreen_ForceRefresh();
        return;
    }

    MenuState_t prev = s_state;

    if (s_state == MENU_MAIN) {
        HandleMainMenu(up, down, ok, back);
    } else {
        HandleValueEdit(up, down, ok, back);
    }

    /* 상태 변경 시 화면 강제 갱신 */
    if (s_state != prev) {
        MenuScreen_ForceRefresh();
    }
}

MenuState_t Menu_GetState(void)
{
    return s_state;
}

/* ================================================================
   메인 메뉴 네비게이션
   ================================================================ */
static void HandleMainMenu(ButtonEvent_t up, ButtonEvent_t down,
                            ButtonEvent_t ok, ButtonEvent_t back)
{
    /* UP/DOWN: 항목 이동 */
    if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
        if (s_menu_idx > 0) s_menu_idx--;
        MenuScreen_ForceRefresh();
    }
    if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
        if (s_menu_idx < MAIN_MENU_ITEMS - 1) s_menu_idx++;
        MenuScreen_ForceRefresh();
    }

    /* OK: 하위 메뉴 진입 */
    if (ok == BTN_EVT_PRESS) {
        switch (s_menu_idx) {
        case 0: s_state = MENU_FREQ;   break;
        case 1: s_state = MENU_DUTY;   break;
        case 2: s_state = MENU_MODE;   break;
        case 3: s_state = MENU_MODBUS; s_menu_idx = 0; break;
        case 4: s_state = MENU_INFO;   break;
        }
    }

    /* OK 장기 누름: 출력 ON/OFF 토글 */
    if (ok == BTN_EVT_LONG_PRESS) {
        if (g_us_state.running) {
            MegasonicCtrl_Stop();
        } else {
            MegasonicCtrl_Start();
        }
        MenuScreen_ForceRefresh();
    }
}

/* ================================================================
   값 편집 (하위 메뉴)
   ================================================================ */
static void HandleValueEdit(ButtonEvent_t up, ButtonEvent_t down,
                            ButtonEvent_t ok, ButtonEvent_t back)
{
    /* BACK: 상위 메뉴 복귀 */
    if (back == BTN_EVT_PRESS) {
        switch (s_state) {
        case MENU_MODBUS_ADDR:
        case MENU_MODBUS_BAUD:
        case MENU_MODBUS_PARITY:
            s_state = MENU_MODBUS;
            break;
        case MENU_PULSE_ON:
        case MENU_PULSE_OFF:
        case MENU_SWEEP_START:
        case MENU_SWEEP_END:
        case MENU_SWEEP_TIME:
            s_state = MENU_MODE;
            break;
        default:
            s_state = MENU_MAIN;
            break;
        }
        return;
    }

    switch (s_state) {
    /* ---- 주파수 설정 ---- */
    case MENU_FREQ: {
        uint16_t f = g_us_state.target_freq;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (f + FREQ_STEP <= FREQ_MAX) f += FREQ_STEP;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (f >= FREQ_MIN + FREQ_STEP) f -= FREQ_STEP;
        }
        if (ok == BTN_EVT_LONG_PRESS) f = FREQ_DEFAULT;
        if (f != g_us_state.target_freq) {
            MegasonicCtrl_SetFrequency(f);
            MenuScreen_ForceRefresh();
        }
        if (ok == BTN_EVT_PRESS) s_state = MENU_MAIN;
        break;
    }

    /* ---- 듀티비 설정 ---- */
    case MENU_DUTY: {
        uint16_t d = g_us_state.target_duty;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (d + DUTY_STEP <= DUTY_MAX) d += DUTY_STEP;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (d >= DUTY_MIN + DUTY_STEP) d -= DUTY_STEP;
        }
        if (ok == BTN_EVT_LONG_PRESS) d = DUTY_DEFAULT;
        if (d != g_us_state.target_duty) {
            MegasonicCtrl_SetDuty(d);
            MenuScreen_ForceRefresh();
        }
        if (ok == BTN_EVT_PRESS) s_state = MENU_MAIN;
        break;
    }

    /* ---- 동작 모드 선택 ---- */
    case MENU_MODE: {
        uint8_t m = (uint8_t)g_us_state.mode;
        if (up == BTN_EVT_PRESS && m > 0) m--;
        if (down == BTN_EVT_PRESS && m < MODE_ITEMS - 1) m++;
        if (m != (uint8_t)g_us_state.mode) {
            MegasonicCtrl_SetMode((OperatingMode_t)m);
            MenuScreen_ForceRefresh();
        }
        if (ok == BTN_EVT_PRESS) {
            /* 모드 확정 및 하위 파라미터 진입 */
            switch (g_us_state.mode) {
            case MODE_PULSE: s_state = MENU_PULSE_ON;    break;
            case MODE_SWEEP: s_state = MENU_SWEEP_START; break;
            default:         s_state = MENU_MAIN;        break;
            }
        }
        break;
    }

    /* ---- Modbus 설정 서브메뉴 ---- */
    case MENU_MODBUS: {
        if (up == BTN_EVT_PRESS && s_menu_idx > 0) s_menu_idx--;
        if (down == BTN_EVT_PRESS && s_menu_idx < MODBUS_SUB_ITEMS - 1) s_menu_idx++;
        if (ok == BTN_EVT_PRESS) {
            switch (s_menu_idx) {
            case 0: s_state = MENU_MODBUS_ADDR;   break;
            case 1: s_state = MENU_MODBUS_BAUD;   break;
            case 2: s_state = MENU_MODBUS_PARITY;  break;
            }
        }
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- Modbus 주소 ---- */
    case MENU_MODBUS_ADDR: {
        uint8_t a = g_modbus_cfg.address;
        if ((up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) && a < MODBUS_ADDR_MAX) a++;
        if ((down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) && a > MODBUS_ADDR_MIN) a--;
        if (ok == BTN_EVT_LONG_PRESS) a = MODBUS_ADDR_DEFAULT;
        if (a != g_modbus_cfg.address) {
            g_modbus_cfg.address = a;
            MenuScreen_ForceRefresh();
        }
        if (ok == BTN_EVT_PRESS) s_state = MENU_MODBUS;
        break;
    }

    /* ---- Modbus 통신 속도 ---- */
    case MENU_MODBUS_BAUD: {
        if ((up == BTN_EVT_PRESS) && s_baud_idx < MODBUS_BAUD_INDEX_COUNT - 1) s_baud_idx++;
        if ((down == BTN_EVT_PRESS) && s_baud_idx > 0) s_baud_idx--;
        g_modbus_cfg.baudrate = BAUD_TABLE[s_baud_idx];
        if (ok == BTN_EVT_PRESS) {
            Modbus_ReconfigUART();
            s_state = MENU_MODBUS;
        }
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- Modbus 패리티 ---- */
    case MENU_MODBUS_PARITY: {
        uint8_t p = g_modbus_cfg.parity;
        if (up == BTN_EVT_PRESS && p < 2) p++;
        if (down == BTN_EVT_PRESS && p > 0) p--;
        if (p != g_modbus_cfg.parity) {
            g_modbus_cfg.parity = p;
            MenuScreen_ForceRefresh();
        }
        if (ok == BTN_EVT_PRESS) {
            Modbus_ReconfigUART();
            s_state = MENU_MODBUS;
        }
        break;
    }

    /* ---- 펄스 모드: ON 시간 ---- */
    case MENU_PULSE_ON: {
        uint16_t v = g_us_state.pulse_on_ms;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (v + 100 <= PULSE_ON_MAX) v += 100;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (v >= PULSE_ON_MIN + 100) v -= 100;
        }
        if (ok == BTN_EVT_LONG_PRESS) v = PULSE_ON_DEFAULT;
        g_us_state.pulse_on_ms = v;
        if (ok == BTN_EVT_PRESS) s_state = MENU_PULSE_OFF;
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- 펄스 모드: OFF 시간 ---- */
    case MENU_PULSE_OFF: {
        uint16_t v = g_us_state.pulse_off_ms;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (v + 100 <= PULSE_OFF_MAX) v += 100;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (v >= PULSE_OFF_MIN + 100) v -= 100;
        }
        if (ok == BTN_EVT_LONG_PRESS) v = PULSE_OFF_DEFAULT;
        g_us_state.pulse_off_ms = v;
        if (ok == BTN_EVT_PRESS) s_state = MENU_MAIN;
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- 스윕 시작 주파수 ---- */
    case MENU_SWEEP_START: {
        uint16_t v = g_us_state.sweep_start_freq;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (v + FREQ_STEP <= FREQ_MAX) v += FREQ_STEP;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (v >= FREQ_MIN + FREQ_STEP) v -= FREQ_STEP;
        }
        if (ok == BTN_EVT_LONG_PRESS) v = SWEEP_START_DEFAULT;
        g_us_state.sweep_start_freq = v;
        if (ok == BTN_EVT_PRESS) s_state = MENU_SWEEP_END;
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- 스윕 끝 주파수 ---- */
    case MENU_SWEEP_END: {
        uint16_t v = g_us_state.sweep_end_freq;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (v + FREQ_STEP <= FREQ_MAX) v += FREQ_STEP;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (v >= FREQ_MIN + FREQ_STEP) v -= FREQ_STEP;
        }
        if (ok == BTN_EVT_LONG_PRESS) v = SWEEP_END_DEFAULT;
        g_us_state.sweep_end_freq = v;
        if (ok == BTN_EVT_PRESS) s_state = MENU_SWEEP_TIME;
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- 스윕 시간 ---- */
    case MENU_SWEEP_TIME: {
        uint16_t v = g_us_state.sweep_time_ms;
        if (up == BTN_EVT_PRESS || up == BTN_EVT_REPEAT) {
            if (v + 500 <= SWEEP_TIME_MAX) v += 500;
        }
        if (down == BTN_EVT_PRESS || down == BTN_EVT_REPEAT) {
            if (v >= SWEEP_TIME_MIN + 500) v -= 500;
        }
        if (ok == BTN_EVT_LONG_PRESS) v = SWEEP_TIME_DEFAULT;
        g_us_state.sweep_time_ms = v;
        if (ok == BTN_EVT_PRESS) s_state = MENU_MAIN;
        MenuScreen_ForceRefresh();
        break;
    }

    /* ---- 시스템 정보 (읽기 전용) ---- */
    case MENU_INFO:
        if (ok == BTN_EVT_PRESS || back == BTN_EVT_PRESS) {
            s_state = MENU_MAIN;
        }
        break;

    default:
        s_state = MENU_MAIN;
        break;
    }
}
