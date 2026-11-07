/**
 * @file  menu_screen.c
 * @brief 기본조작 매뉴얼의 LCD1602 화면 렌더링
 */
#include "menu_screen.h"

#include "adc_control.h"
#include "lcd1602.h"
#include "modbus_regs.h"
#include "params.h"
#include "resonance_tuning.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_ctrl.h"
#include <stdio.h>
#include <string.h>

#define LOCAL_VR_PREVIEW_HOLD_MS 1500U

static uint8_t s_need_refresh;
static uint32_t s_last_refresh_tick;
static bool s_power_display_initialized;
static uint16_t s_power_display_percent;
static uint16_t s_power_display_candidate;
static uint8_t s_power_display_candidate_count;
static uint32_t s_local_vr_display_until_tick;
static uint32_t s_last_pot_override_counter;
static char s_line0[LCD_COLS + 1U];
static char s_line1[LCD_COLS + 1U];

static void SetLine(char *line, const char *text);
static void SetAlignedFields(char *line, const char *left, const char *right);
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
static uint16_t MapCommandToOutputPercent(uint16_t command_percent);
static bool IsLocalVrPreviewActive(void);

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

  if (g_us_state.error_active) {
    SetLine(s_line0,
            g_us_state.error_code == ULTRASONIC_ERROR_OVER_CURRENT
                ? "OVER CURRENT"
                : "TRANSDUCER ERROR");
    SetLine(s_line1, "RUN OFF TO RESET");
    WriteLines();
    return;
  }

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
  const bool rs485_control = ModbusRegs_IsLocalInputLocked();
  const bool rs485_run = ModbusRegs_IsRunControlled();

  if (rs485_control && !g_us_state.running) {
    SetLine(s_line0, sweep ? "RDY/SWP 485 COMM" : "RDY/    485 COMM");
    /* 통신 준비/정지 중에는 출력 Level과 주파수를 숨긴다. */
    SetLine(s_line1, "    K-SONICS");
    return;
  }

  if (!automatic && !g_us_state.running && !rs485_control) {
    FormatRunTime(time_text, sizeof(time_text), false);
    snprintf(formatted, sizeof(formatted), "STANDBY %s", time_text);
    SetLine(s_line0, formatted);
    SetLine(s_line1, "    K-SONICS");
    return;
  }

  if (rs485_control || !automatic) {
    FormatRunPrefix(prefix, g_us_state.running, sweep);
    const char *status = "RemoteON";
    if (rs485_run && g_us_state.running) {
      status = "485 ON";
    } else if (rs485_control) {
      status = "485 COMM";
    }
    /* 상태 문자열 길이에 관계없이 LCD의 마지막 칸에 맞춘다. */
    SetAlignedFields(s_line0, prefix, status);
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
  if (running) {
    /* 사용자 정의 역대각선으로 | / - 역대각선의 4단계 회전을 만든다. */
    static const char spinner[] = {
        '|', '/', '-', (char)LCD_CHAR_SPINNER_BACKSLASH};
    const char motion =
        spinner[(HAL_GetTick() / LCD_MAIN_REFRESH_MS) & 0x03U];
    if (sweep) {
      snprintf(prefix, 8U, "RUN%cSWP", motion);
    } else {
      snprintf(prefix, 8U, "RUN%c", motion);
    }
  } else {
    snprintf(prefix, 8U, "%s", sweep ? "RDY/SWP" : "RDY/");
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
  SetLine(s_line0, "1.FREQUENCY");
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
  SetLine(s_line0, "4.TUNING MODE");
  SetLine(s_line1, Menu_GetTuningMethod() == TUNE_METHOD_MANUAL
                       ? "> MANUAL  UP/DN"
                       : "> AUTO    UP/DN");
}

static void RenderManualTune(void) {
  char formatted[32];
  const uint16_t current = ADC_Control_GetCurrentCentiAmp();
  SetLine(s_line0,
          g_us_state.running ? "MANUAL SWEEP RUN" : "MANUAL SWEEP RDY");
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
    const uint32_t frequency_hz = ResonanceTuning_GetScanFrequencyHz();
    SetLine(s_line0, "AUTO TUNE 20SEC");
    snprintf(formatted, sizeof(formatted), "F:%02lu.%02lu I:%u.%02u",
             (unsigned long)(frequency_hz / 1000U),
             (unsigned long)((frequency_hz % 1000U) / 10U), current / 100U,
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
    const uint32_t result_hz = ResonanceTuning_GetResultFrequencyHz();
    SetLine(s_line0, "TUNE COMPLETE");
    snprintf(formatted, sizeof(formatted), "%02lu.%02lukHz MODE=OK",
             (unsigned long)(result_hz / 1000U),
             (unsigned long)((result_hz % 1000U) / 10U));
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
  SetLine(s_line0, "2.SWEEP WIDTH");
  snprintf(formatted, sizeof(formatted), "+/- %4u Hz UP/DN",
           Menu_GetSweepWidthHz());
  SetLine(s_line1, formatted);
}

static void RenderSweepRate(void) {
  char formatted[32];
  SetLine(s_line0, "3.SWEEP SPEED");
  snprintf(formatted, sizeof(formatted), "RATE:%3u Hz UP/DN",
           Menu_GetSweepRateHz());
  SetLine(s_line1, formatted);
}

static void RenderOutputMode(void) {
  SetLine(s_line0, "5.OUTPUT MODE");
  SetLine(s_line1,
          Menu_GetOutputControlMode() == OUTPUT_CONTROL_VOLUME
              ? "> VOLUME  UP/DN"
              : "> CONST I UP/DN");
}

static void RenderCurrentSet(void) {
  char formatted[32];
  const uint16_t current = Menu_GetConstantCurrentCentiAmp();
  SetLine(s_line0, "6.CURRENT SET");
  snprintf(formatted, sizeof(formatted), "LIMIT:%u.%u A +/-", current / 100U,
           (current % 100U) / 10U);
  SetLine(s_line1, formatted);
}

static void FormatStatusLine(char *line) {
  /* PB1 가변저항을 표시 전용 히스테리시스로 안정화해 1% 경계 왕복을 막는다. */
  const uint16_t power = GetStablePowerPercent();
  if (IsLocalVrPreviewActive()) {
    char vr_formatted[24];
    snprintf(vr_formatted, sizeof(vr_formatted), "VR:%3u%% MANUAL",
             s_power_display_percent);
    SetLine(line, vr_formatted);
    return;
  }
  /* 정지 중에는 00.0 kHz, 출력 시작 후에는 설정 중심주파수를 표시한다. */
  uint16_t frequency = g_us_state.running ? g_us_state.target_freq : 0U;
  char power_text[16];
  char frequency_text[12];
  if (frequency > FREQ_MAX) {
    frequency = FREQ_MAX;
  }
  const uint16_t full_scale_watts = Menu_GetPowerRangeWatts();
  if (full_scale_watts == 0U) {
    snprintf(power_text, sizeof(power_text), "PL:%3u%%", power);
  } else {
    const uint16_t watts =
        (uint16_t)(((uint32_t)power * full_scale_watts + 50U) / 100U);
    snprintf(power_text, sizeof(power_text), "PL:%4uW", watts);
  }
  snprintf(frequency_text, sizeof(frequency_text), "%02u.%1ukHz",
           frequency / 10U, frequency % 10U);
  /* Local/통신 운전 모두 주파수의 마지막 글자를 LCD 끝 칸에 맞춘다. */
  SetAlignedFields(line, power_text, frequency_text);
}

static uint16_t GetStablePowerPercent(void) {
  const uint16_t normalized = ADC_Control_GetPwmVrNormalized();
  const uint16_t rounded = (normalized + 5U) / 10U;

  if (!s_power_display_initialized) {
    s_power_display_percent = rounded;
    s_power_display_candidate = rounded;
    s_power_display_candidate_count = 0U;
    s_power_display_initialized = true;
  } else {
    const uint16_t center = s_power_display_percent * 10U;
    const uint16_t upper = center + 5U + PL_DISPLAY_HYSTERESIS_01PCT;
    const uint16_t lower =
        (center > (5U + PL_DISPLAY_HYSTERESIS_01PCT))
            ? center - 5U - PL_DISPLAY_HYSTERESIS_01PCT
            : 0U;
    if (normalized >= upper || normalized <= lower) {
      if (rounded != s_power_display_candidate) {
        s_power_display_candidate = rounded;
        s_power_display_candidate_count = 1U;
      } else if (s_power_display_candidate_count <
                 PL_DISPLAY_CONFIRM_REFRESHES) {
        s_power_display_candidate_count++;
      }
      if (s_power_display_candidate_count >= PL_DISPLAY_CONFIRM_REFRESHES) {
        s_power_display_percent = s_power_display_candidate;
        s_power_display_candidate_count = 0U;
      }
    } else {
      /* 현재 표시값 주변으로 돌아온 순간 변동은 후보에서 제거한다. */
      s_power_display_candidate = s_power_display_percent;
      s_power_display_candidate_count = 0U;
    }
  }

  if (UltrasonicCtrl_IsPlcOutputControl()) {
    /* 통신/가변저항 중 마지막으로 입력된 실제 출력 지령을 표시한다. */
    const uint16_t command_percent =
        (uint16_t)(((uint32_t)g_us_state.output_command * 100U +
                    (PLC_OUTPUT_MAX / 2U)) /
                   PLC_OUTPUT_MAX);
    return MapCommandToOutputPercent(command_percent);
  }
  return MapCommandToOutputPercent(s_power_display_percent);
}

static uint16_t MapCommandToOutputPercent(uint16_t command_percent) {
  if (command_percent > 100U) {
    command_percent = 100U;
  }
  const uint32_t output_01pct =
      PWM_OUTPUT_DUTY_MIN +
      ((uint32_t)command_percent *
       (PWM_OUTPUT_DUTY_MAX - PWM_OUTPUT_DUTY_MIN) + 50U) /
          100U;
  return (uint16_t)((output_01pct + 5U) / 10U);
}

static bool IsLocalVrPreviewActive(void) {
  const uint32_t override_counter =
      UltrasonicCtrl_GetPotOverrideCounter();
  if (override_counter != s_last_pot_override_counter) {
    s_last_pot_override_counter = override_counter;
    s_local_vr_display_until_tick =
        HAL_GetTick() + LOCAL_VR_PREVIEW_HOLD_MS;
  }
  return UltrasonicCtrl_IsPlcOutputControl() &&
         s_local_vr_display_until_tick != 0U &&
         (int32_t)(HAL_GetTick() - s_local_vr_display_until_tick) < 0;
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

static void SetAlignedFields(char *line, const char *left, const char *right) {
  memset(line, ' ', LCD_COLS);
  line[LCD_COLS] = '\0';

  size_t left_length = strlen(left);
  while (left_length > 0U && left[left_length - 1U] == ' ') {
    left_length--;
  }

  size_t right_length = strlen(right);
  if (right_length > LCD_COLS) {
    right += right_length - LCD_COLS;
    right_length = LCD_COLS;
  }
  const size_t right_column = LCD_COLS - right_length;
  if (left_length > right_column) {
    left_length = right_column;
  }

  memcpy(line, left, left_length);
  memcpy(&line[right_column], right, right_length);
}

static void WriteLines(void) {
  LCD_SetCursor(0U, 0U);
  LCD_WriteString(s_line0);
  LCD_SetCursor(1U, 0U);
  LCD_WriteString(s_line1);
}
