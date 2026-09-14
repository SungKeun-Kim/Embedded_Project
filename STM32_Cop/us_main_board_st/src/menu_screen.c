/**
 * @file  menu_screen.c
 * @brief 기본조작 매뉴얼의 LCD1602 화면 렌더링
 */
#include "menu_screen.h"

#include "adc_control.h"
#include "lcd1602.h"
#include "modbus_rtu.h"
#include "params.h"
#include "resonance_tuning.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_ctrl.h"
#include <stdio.h>
#include <string.h>

static uint8_t s_need_refresh;
static uint32_t s_last_refresh_tick;
static bool s_power_display_initialized;
static uint16_t s_power_display_percent;
static char s_line0[LCD_COLS + 1U];
static char s_line1[LCD_COLS + 1U];

static void SetLine(char *line, const char *text);
static void WriteLines(void);
static void RenderMain(void);
static void RenderSupervisor(void);
static void RenderSupervisorPower(void);
static void RenderSupervisorBaud(void);
static void RenderSupervisorAddress(void);
static void RenderSupervisorTerm(void);
static void RenderTimeSetting(void);
static void RenderFrequency(void);
static void RenderTuneSelect(void);
static void RenderManualTune(void);
static void RenderAutoTune(void);
static void RenderSweepWidth(void);
static void RenderSweepRate(void);
static void RenderOutputMode(void);
static void RenderCurrentSet(void);
static void FormatStatusLine(char *line);
static void FormatRunPrefix(char *prefix, bool running, bool sweep);
static void FormatRunTime(char *text, size_t size, bool remaining);
static uint16_t GetStablePowerPercent(void);

void MenuScreen_ShowSplash(void) {
  SetLine(s_line0, "   ULTRASONIC");
  SetLine(s_line1, "    K-SONICS");
  WriteLines();
}

void MenuScreen_ShowSupervisorSplash(void) {
  SetLine(s_line0, "SUPERVISOR MODE");
  SetLine(s_line1, "MODE: SETTINGS");
  WriteLines();
}

void MenuScreen_ForceRefresh(void) { s_need_refresh = 1U; }

void MenuScreen_Refresh(void) {
  const uint32_t now = HAL_GetTick();
  const uint32_t interval =
      (Menu_GetState() == MENU_MAIN) ? LCD_MAIN_REFRESH_MS : LCD_REFRESH_MIN_MS;
  if (!s_need_refresh && (now - s_last_refresh_tick) < interval) {
    return;
  }
  if ((now - s_last_refresh_tick) < LCD_REFRESH_MIN_MS) {
    return;
  }

  s_last_refresh_tick = now;
  s_need_refresh = 0U;

  switch (Menu_GetState()) {
  case MENU_SUPERVISOR:
    RenderSupervisor();
    break;
  case MENU_SUPERVISOR_PL:
    RenderSupervisorPower();
    break;
  case MENU_SUPERVISOR_BAUD:
    RenderSupervisorBaud();
    break;
  case MENU_SUPERVISOR_ADDR:
    RenderSupervisorAddress();
    break;
  case MENU_SUPERVISOR_TERM:
    RenderSupervisorTerm();
    break;
  case MENU_TIME:
    RenderTimeSetting();
    break;
  case MENU_FREQUENCY:
    RenderFrequency();
    break;
  case MENU_TUNE_SELECT:
    RenderTuneSelect();
    break;
  case MENU_TUNE_MANUAL:
    RenderManualTune();
    break;
  case MENU_TUNE_AUTO:
    RenderAutoTune();
    break;
  case MENU_SWEEP_WIDTH:
    RenderSweepWidth();
    break;
  case MENU_SWEEP_RATE:
    RenderSweepRate();
    break;
  case MENU_OUTPUT_MODE:
    RenderOutputMode();
    break;
  case MENU_CURRENT_SET:
    RenderCurrentSet();
    break;
  case MENU_MAIN:
  default:
    RenderMain();
    break;
  }
  WriteLines();
}

static void RenderMain(void) {
  char time_text[10];
  char prefix[8];
  char formatted[32];
  const bool automatic = Menu_IsAutomaticMode();
  const bool sweep = g_us_state.mode == MODE_SWEEP;
  const bool rs485_communicating =
      !automatic && Modbus_IsCommunicationActive();

  if (g_us_state.error_active) {
    SetLine(s_line0, "TRANSDUCER ERR");
    SetLine(s_line1, "OUTPUT STOPPED");
    return;
  }

  if (!automatic && !g_us_state.running && !rs485_communicating) {
    FormatRunTime(time_text, sizeof(time_text), false);
    snprintf(formatted, sizeof(formatted), "STANDBY %s", time_text);
    SetLine(s_line0, formatted);
    SetLine(s_line1, "    K-SONICS");
    return;
  }

  if (!automatic) {
    FormatRunPrefix(prefix, g_us_state.running, sweep);
    snprintf(formatted, sizeof(formatted), "%-7s %s", prefix,
             rs485_communicating ? "485 COMM" : "RemoteON");
    SetLine(s_line0, formatted);
    FormatStatusLine(s_line1);
    return;
  }

  if (g_us_state.time_over) {
    FormatRunPrefix(prefix, false, sweep);
    snprintf(formatted, sizeof(formatted), "%-7s TIMEOVER", prefix);
    SetLine(s_line0, formatted);
    FormatStatusLine(s_line1);
    return;
  }

  FormatRunPrefix(prefix, g_us_state.running, sweep);

  FormatRunTime(time_text, sizeof(time_text), g_us_state.running);
  if (g_us_state.run_time_mode == RUN_TIME_CONTINUOUS) {
    snprintf(formatted, sizeof(formatted), "%-6.6s %s", prefix, time_text);
  } else {
    snprintf(formatted, sizeof(formatted), "%-7.7s %s", prefix, time_text);
  }
  SetLine(s_line0, formatted);
  FormatStatusLine(s_line1);
}

static void FormatRunPrefix(char *prefix, bool running, bool sweep) {
  if (running && sweep) {
    static const char spinner[] = {'|', '/', '-', '\\'};
    snprintf(prefix, 8U, "RUN%cSWP",
             spinner[(HAL_GetTick() / LCD_MAIN_REFRESH_MS) & 0x03U]);
  } else if (running) {
    snprintf(prefix, 8U, "RUN    ");
  } else {
    snprintf(prefix, 8U, "%s", sweep ? "RDY/SWP" : "RDY    ");
  }
}

static void RenderSupervisor(void) {
  SetLine(s_line0, "SUPERVISOR MODE");
  SetLine(s_line1, "MODE: SETTINGS");
}

static void RenderSupervisorPower(void) {
  char formatted[32];
  SetLine(s_line0, "PL SETTING");
  const uint16_t watts = Menu_GetPowerRangeWatts();
  if (watts == 0U) {
    SetLine(s_line1, "PWR RNG: PERCENT");
  } else {
    snprintf(formatted, sizeof(formatted), "PWR RNG: %uW", watts);
    SetLine(s_line1, formatted);
  }
  if (Menu_HasSupervisorSaveError()) {
    SetLine(s_line1, "FLASH SAVE ERROR");
  }
}

static void RenderSupervisorBaud(void) {
  char formatted[32];
  SetLine(s_line0, "RS485 SETTING");
  snprintf(formatted, sizeof(formatted), "BAUD RATE:%lu",
           (unsigned long)MODBUS_BAUD_TABLE[Menu_GetSupervisorBaudIndex()]);
  SetLine(s_line1, formatted);
}

static void RenderSupervisorAddress(void) {
  char formatted[32];
  SetLine(s_line0, "RS485 SETTING");
  snprintf(formatted, sizeof(formatted), "ADDR :%u",
           Menu_GetSupervisorAddress());
  SetLine(s_line1, formatted);
}

static void RenderSupervisorTerm(void) {
  SetLine(s_line0, "RS485 SETTING");
  SetLine(s_line1, Menu_GetSupervisorRtermEnabled() ? "TERM:MOUNTED"
                                                    : "TERM:UNMOUNTED");
  if (Menu_HasSupervisorSaveError()) {
    SetLine(s_line1, "FLASH SAVE ERROR");
  }
}

static void RenderTimeSetting(void) {
  const RunTimeMode_t mode = Menu_GetEditTimeMode();
  const uint8_t value = Menu_GetEditTimeValue();
  char formatted[32];

  if (mode == RUN_TIME_MINUTES) {
    snprintf(formatted, sizeof(formatted), "MODE    TIME:%02uM", value);
    SetLine(s_line0, formatted);
    snprintf(formatted, sizeof(formatted), "TIME SET: %02u MIN", value);
    SetLine(s_line1, formatted);
  } else if (mode == RUN_TIME_SECONDS) {
    snprintf(formatted, sizeof(formatted), "MODE    TIME:%02uS", value);
    SetLine(s_line0, formatted);
    snprintf(formatted, sizeof(formatted), "TIME SET: %02u SEC", value);
    SetLine(s_line1, formatted);
  } else {
    SetLine(s_line0, "MODE   TIME:CONT");
    SetLine(s_line1, "TIME SET:   CONT");
  }

  if (Menu_HasTimeSaveError()) {
    SetLine(s_line1, "FLASH SAVE ERROR");
  }
}

static void RenderFrequency(void) {
  char formatted[32];
  SetLine(s_line0, "2.FREQUENCY");
  if (Menu_GetFrequencyBandIndex() == 3U) {
    snprintf(formatted, sizeof(formatted), "> %02u.%1ukHz 80k+",
             g_us_state.target_freq / 10U, g_us_state.target_freq % 10U);
  } else {
    snprintf(formatted, sizeof(formatted), "> %02u.%1u kHz  UP/DN",
             g_us_state.target_freq / 10U, g_us_state.target_freq % 10U);
  }
  SetLine(s_line1, formatted);
}

static void RenderTuneSelect(void) {
  SetLine(s_line0, "3.TUNING MODE");
  SetLine(s_line1, Menu_GetTuningMethod() == TUNE_METHOD_MANUAL
                       ? "> MANUAL  UP/DN"
                       : "> AUTO    UP/DN");
}

static void RenderManualTune(void) {
  char formatted[32];
  const uint16_t current = ADC_Control_GetCurrentCentiAmp();
  SetLine(s_line0, g_us_state.running ? "MANUAL TUNE RUN" : "MANUAL TUNE RDY");
  snprintf(formatted, sizeof(formatted), "F:%02u.%1u I:%u.%02uA",
           g_us_state.target_freq / 10U, g_us_state.target_freq % 10U,
           current / 100U, current % 100U);
  SetLine(s_line1, formatted);
}

static void RenderAutoTune(void) {
  char formatted[32];
  const ResonanceTuneState_t state = ResonanceTuning_GetState();

  if (Menu_IsAutoTuneStarting()) {
    SetLine(s_line0, "AUTO STARTING");
    snprintf(formatted, sizeof(formatted), "F:%02u.%1u WAIT",
             g_us_state.target_freq / 10U, g_us_state.target_freq % 10U);
  } else if (state == RES_TUNE_ROUGH) {
    const uint16_t current = ADC_Control_GetCurrentCentiAmp();
    SetLine(s_line0, "AUTO ROUGH PA6");
    snprintf(formatted, sizeof(formatted), "F:%02u.%1u I:%u.%02u",
             ResonanceTuning_GetScanFrequency() / 10U,
             ResonanceTuning_GetScanFrequency() % 10U, current / 100U,
             current % 100U);
  } else if (state == RES_TUNE_FINE) {
    const int16_t phase = ResonanceTuning_GetPhaseDifference();
    const uint16_t phase_abs =
        (phase < 0) ? (uint16_t)(-(int32_t)phase) : (uint16_t)phase;
    SetLine(s_line0, "AUTO FINE V/I");
    snprintf(formatted, sizeof(formatted), "F:%02u.%1u P:%c%u.%u",
             ResonanceTuning_GetScanFrequency() / 10U,
             ResonanceTuning_GetScanFrequency() % 10U,
             phase < 0 ? '-' : '+', phase_abs / 10U, phase_abs % 10U);
  } else if (state == RES_TUNE_COMPLETE) {
    SetLine(s_line0, "TUNE COMPLETE");
    snprintf(formatted, sizeof(formatted), "%02u.%1ukHz  %s",
             ResonanceTuning_GetResultFrequency() / 10U,
             ResonanceTuning_GetResultFrequency() % 10U,
             ResonanceTuning_WasPhaseRefined() ? "FINE" : "ROUGH");
  } else if (state == RES_TUNE_ERROR) {
    SetLine(s_line0, "TUNE FAILED");
    SetLine(s_line1, "CHECK CT/PHASE");
    return;
  } else {
    SetLine(s_line0, "AUTO TUNE READY");
    SetLine(s_line1, "HOLD START 3SEC");
    return;
  }
  SetLine(s_line1, formatted);
}

static void RenderSweepWidth(void) {
  char formatted[32];
  SetLine(s_line0, "4.SWEEP WIDTH");
  snprintf(formatted, sizeof(formatted), "+/- %4u Hz UP/DN",
           Menu_GetSweepWidthHz());
  SetLine(s_line1, formatted);
}

static void RenderSweepRate(void) {
  char formatted[32];
  SetLine(s_line0, "5.SWEEP SPEED");
  snprintf(formatted, sizeof(formatted), "RATE:%3u Hz UP/DN",
           Menu_GetSweepRateHz());
  SetLine(s_line1, formatted);
}

static void RenderOutputMode(void) {
  SetLine(s_line0, "6.OUTPUT MODE");
  SetLine(s_line1,
          Menu_GetOutputControlMode() == OUTPUT_CONTROL_VOLUME
              ? "> VOLUME  UP/DN"
              : "> CONST I UP/DN");
}

static void RenderCurrentSet(void) {
  char formatted[32];
  const uint16_t current = Menu_GetConstantCurrentCentiAmp();
  SetLine(s_line0, "7.CURRENT SET");
  snprintf(formatted, sizeof(formatted), "LIMIT:%u.%u A +/-", current / 100U,
           (current % 100U) / 10U);
  SetLine(s_line1, formatted);
}

static void FormatStatusLine(char *line) {
  /* PB1 가변저항을 표시 전용 히스테리시스로 안정화해 1% 경계 왕복을 막는다. */
  const uint16_t power = GetStablePowerPercent();
  /* Sweep 순간값이 아닌 작업자가 설정한 중심주파수를 항상 표시한다. */
  uint16_t frequency = g_us_state.target_freq;
  char formatted[32];
  if (frequency > FREQ_MAX) {
    frequency = FREQ_MAX;
  }
  const uint16_t full_scale_watts = Menu_GetPowerRangeWatts();
  if (full_scale_watts == 0U) {
    snprintf(formatted, sizeof(formatted), "PL:%3u%% %02u.%1ukHz", power,
             frequency / 10U, frequency % 10U);
  } else {
    const uint16_t watts =
        (uint16_t)(((uint32_t)power * full_scale_watts + 50U) / 100U);
    snprintf(formatted, sizeof(formatted), "PL:%4uW %02u.%1ukHz", watts,
             frequency / 10U, frequency % 10U);
  }
  SetLine(line, formatted);
}

static uint16_t GetStablePowerPercent(void) {
  const uint16_t normalized = ADC_Control_GetPwmVrNormalized();
  const uint16_t rounded = (normalized + 5U) / 10U;

  if (!s_power_display_initialized) {
    s_power_display_percent = rounded;
    s_power_display_initialized = true;
  } else {
    const uint16_t center = s_power_display_percent * 10U;
    const uint16_t upper = center + 5U + PL_DISPLAY_HYSTERESIS_01PCT;
    const uint16_t lower =
        (center > (5U + PL_DISPLAY_HYSTERESIS_01PCT))
            ? center - 5U - PL_DISPLAY_HYSTERESIS_01PCT
            : 0U;
    if (normalized >= upper || normalized <= lower) {
      s_power_display_percent = rounded;
    }
  }

  /* 실제 PA7 지령과 동일하게 PB1 상한에 OUTPUT_VALUE를 곱해 표시한다. */
  return (uint16_t)(((uint32_t)s_power_display_percent *
                     g_us_state.output_command + (PLC_OUTPUT_MAX / 2U)) /
                    PLC_OUTPUT_MAX);
}

static void FormatRunTime(char *text, size_t size, bool remaining) {
  if (g_us_state.run_time_mode == RUN_TIME_CONTINUOUS) {
    snprintf(text, size, "TIME:CONT");
    return;
  }

  uint32_t milliseconds =
      remaining ? g_us_state.remaining_time_ms : g_us_state.run_duration_ms;
  uint32_t seconds = (milliseconds + 999U) / 1000U;
  uint32_t value;
  char unit;
  if (g_us_state.run_time_mode == RUN_TIME_MINUTES) {
    value = (seconds + 59U) / 60U;
    unit = 'M';
  } else {
    value = seconds;
    unit = 'S';
  }
  if (value > 99U) {
    value = 99U;
  }
  snprintf(text, size, "TIME:%02lu%c", (unsigned long)value, unit);
}

static void SetLine(char *line, const char *text) {
  memset(line, ' ', LCD_COLS);
  line[LCD_COLS] = '\0';
  const size_t length = strlen(text);
  memcpy(line, text, length < LCD_COLS ? length : LCD_COLS);
}

static void WriteLines(void) {
  LCD_SetCursor(0U, 0U);
  LCD_WriteString(s_line0);
  LCD_SetCursor(1U, 0U);
  LCD_WriteString(s_line1);
}
