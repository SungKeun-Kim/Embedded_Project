/**
 * @file  menu.c
 * @brief 일반 시간 메뉴와 MODE+UP 보호 설정 메뉴
 */
#include "menu.h"

#include "button.h"
#include "buzzer.h"
#include "config.h"
#include "menu_screen.h"
#include "modbus_regs.h"
#include "modbus_rtu.h"
#include "resonance_tuning.h"
#include "settings_storage.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_ctrl.h"

#define EXTERNAL_POLL_MS 10U
#define EXTERNAL_DEBOUNCE_COUNT 3U
#define FREQUENCY_CHOICE_COUNT FREQ_BAND_COUNT

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  bool stable_active;
  bool candidate_active;
  uint8_t count;
  bool changed;
  bool activated;
} ExternalInput_t;

/* 수동/자동 튜닝으로 바뀐 중심주파수를 대역별로 따로 기억한다. */
static uint16_t s_frequency_choices[FREQUENCY_CHOICE_COUNT];
static uint8_t s_frequency_fine_10hz[FREQUENCY_CHOICE_COUNT];

static MenuState_t s_state;
static uint8_t s_menu_idx;
static uint8_t s_frequency_index;
static uint8_t s_edit_time_value;
static RunTimeMode_t s_edit_time_mode;
static bool s_time_save_error;
static TuningMethod_t s_tuning_method;
static OutputControlMode_t s_output_control_mode;
static uint16_t s_sweep_width_hz;
static uint16_t s_sweep_rate_hz;
static uint16_t s_constant_current_centiamp;
static bool s_auto_starting;
static bool s_auto_result_handled;
static bool s_protected_combo_tracking;
static bool s_protected_combo_latched;
static uint32_t s_protected_combo_tick;
static bool s_supervisor_exit_combo_tracking;
static bool s_supervisor_exit_combo_latched;
static bool s_supervisor_exit_combo_armed;
static uint32_t s_supervisor_exit_combo_tick;
static uint32_t s_external_poll_tick;
static uint8_t s_power_range_index;
static uint8_t s_supervisor_baud_index;
static uint8_t s_supervisor_address;
static bool s_supervisor_rterm_enabled;
static bool s_supervisor_save_error;

static ExternalInput_t s_run_input = {RUN_SW_PORT, RUN_SW_PIN};
static ExternalInput_t s_remote_input = {REMOTE_PORT, REMOTE_PIN};
static ExternalInput_t s_sweep_input = {SWEEP_SW_PORT, SWEEP_SW_PIN};

static bool ReadActiveLow(const ExternalInput_t *input);
static void ExternalInputInit(ExternalInput_t *input);
static void ExternalInputUpdate(ExternalInput_t *input);
static void UpdateExternalInputs(void);
static void ApplySweepInput(void);
static void ApplySweepParameters(void);
static void ToggleAutomaticRun(void);
static bool SaveRunTimer(void);
static bool SaveOperatingSettings(void);
static bool SaveSupervisorSettings(void);
static bool IsSupervisorState(void);
static bool HandleSupervisorExitCombo(void);
static void HandleSupervisorMenu(ButtonEvent_t mode, ButtonEvent_t down,
                                 ButtonEvent_t up);
static void ApplyRterm(bool enabled);
static bool IsProtectedSettingState(void);
static bool HandleProtectedMenuCombo(void);
static void AdvanceMenu(void);
static void AbortTuningOutput(void);
static void HandleAutoTuneProgress(void);
static void AdjustFrequency(int8_t direction);
static void AdjustManualTune(int8_t direction);
static void ApplySelectedFrequency(void);
static void GetManualTuneLimits(uint16_t *minimum, uint16_t *maximum);
static bool IsIncrementEvent(ButtonEvent_t event);
static bool IsShortPressAllowed(ButtonId_t id);
static bool IsLongPressAllowed(ButtonId_t id);
static bool IsButtonDownAllowed(ButtonId_t id);
static void HandleButtonFeedback(ButtonEvent_t mode, ButtonEvent_t down,
                                 ButtonEvent_t up,
                                 ButtonEvent_t start_stop);

void Menu_Init(bool supervisor_boot) {
  SettingsStorageData_t saved;
  SettingsStorage_SetDefaults(&saved);
  const bool settings_loaded = SettingsStorage_Load(&saved);

  s_state = supervisor_boot ? MENU_SUPERVISOR : MENU_MAIN;
  s_menu_idx = 0U;
  s_time_save_error = false;
  s_frequency_index = 0U;
  s_frequency_choices[0] = FREQ_BAND_28_DEFAULT;
  s_frequency_choices[1] = FREQ_BAND_40_DEFAULT;
  s_frequency_choices[2] = FREQ_BAND_68_DEFAULT;
  s_frequency_choices[3] = FREQ_BAND_80_DEFAULT;
  for (uint8_t index = 0U; index < FREQUENCY_CHOICE_COUNT; index++) {
    s_frequency_fine_10hz[index] = 0U;
  }
  s_tuning_method = TUNE_METHOD_MANUAL;
  s_output_control_mode = OUTPUT_CONTROL_VOLUME;
  s_sweep_width_hz = SWEEP_WIDTH_DEFAULT_HZ;
  s_sweep_rate_hz = SWEEP_RATE_DEFAULT_HZ;
  s_constant_current_centiamp = CONST_CURRENT_DEFAULT_CENTIAMP;
  s_auto_starting = false;
  s_auto_result_handled = false;
  s_protected_combo_tracking = false;
  s_protected_combo_latched = false;
  s_protected_combo_tick = 0U;
  s_supervisor_exit_combo_tracking = false;
  s_supervisor_exit_combo_latched = false;
  /* 전원 투입 때 누른 MODE+DOWN을 먼저 놓은 뒤에만 종료 조합키를 받는다. */
  s_supervisor_exit_combo_armed = !supervisor_boot;
  s_supervisor_exit_combo_tick = 0U;
  s_power_range_index = saved.power_range_index;
  s_supervisor_baud_index = saved.baud_index;
  s_supervisor_address =
      (saved.address <= MODBUS_SUPERVISOR_ADDR_MAX) ? saved.address
                                                   : MODBUS_ADDR_DEFAULT;
  s_supervisor_rterm_enabled = (saved.rterm_enabled != 0U);
  s_supervisor_save_error = false;
  ApplyRterm(s_supervisor_rterm_enabled);

  if (settings_loaded) {
    UltrasonicCtrl_SetRunTimer((RunTimeMode_t)saved.run_time_mode,
                               saved.run_time_value);
    s_frequency_index = saved.selected_frequency_band;
    for (uint8_t index = 0U; index < FREQUENCY_CHOICE_COUNT; index++) {
      s_frequency_choices[index] = saved.band_frequency[index];
      s_frequency_fine_10hz[index] = saved.band_frequency_fine_10hz[index];
    }
    s_sweep_width_hz = saved.sweep_width_hz;
    s_sweep_rate_hz = saved.sweep_rate_hz;
  }

  s_edit_time_mode = g_us_state.run_time_mode;
  s_edit_time_value = g_us_state.run_time_value;
  ApplySelectedFrequency();
  ApplySweepParameters();

  ExternalInputInit(&s_run_input);
  ExternalInputInit(&s_remote_input);
  ExternalInputInit(&s_sweep_input);
  s_external_poll_tick = HAL_GetTick();
  ApplySweepInput();

  /* 수동 모드에서 REMOTE가 이미 ON이면 부팅 화면 종료 후 바로 운전한다. */
  if (!supervisor_boot && !s_run_input.stable_active &&
      s_remote_input.stable_active) {
    UltrasonicCtrl_StartUntimed();
  }
  MenuScreen_ForceRefresh();
}

void Menu_Update(void) {
  const ButtonEvent_t mode = Button_GetEvent(BTN_ID_MODE);
  const ButtonEvent_t down = Button_GetEvent(BTN_ID_DOWN);
  const ButtonEvent_t up = Button_GetEvent(BTN_ID_UP);
  const ButtonEvent_t start_stop = Button_GetEvent(BTN_ID_START_STOP);

  UpdateExternalInputs();
  HandleAutoTuneProgress();

  /* Supervisor 설정은 PLC Local Lock과 일반 운전 메뉴보다 우선한다. */
  if (IsSupervisorState()) {
    if (HandleSupervisorExitCombo()) {
      return;
    }
    HandleButtonFeedback(mode, down, up, start_stop);
    HandleSupervisorMenu(mode, down, up);
    if (mode != BTN_EVT_NONE || down != BTN_EVT_NONE || up != BTN_EVT_NONE ||
        start_stop != BTN_EVT_NONE) {
      MenuScreen_ForceRefresh();
    }
    return;
  }

  if (s_run_input.changed) {
    /* PA11/RUN_SW 토글은 출력 정지와 Local 제어권 인계를 동시에 수행한다. */
    const bool had_error = g_us_state.error_active;
    if (ModbusRegs_IsLocalInputLocked()) {
      ModbusRegs_ReleaseLocalControl();
    }
    AbortTuningOutput();
    UltrasonicCtrl_Stop();
    if (!s_run_input.stable_active && g_us_state.error_active) {
      (void)UltrasonicCtrl_ResetError();
    }
    s_state = MENU_MAIN;
    if (!had_error && !s_run_input.stable_active &&
        s_remote_input.stable_active) {
      UltrasonicCtrl_StartUntimed();
    }
    MenuScreen_ForceRefresh();
    return;
  }

  if (s_sweep_input.changed && s_state != MENU_TUNE_MANUAL &&
      s_state != MENU_TUNE_AUTO) {
    ApplySweepInput();
    MenuScreen_ForceRefresh();
  }

  /* 설정 중 REMOTE가 들어오면 설정을 닫고 REMOTE 운전을 우선한다. */
  if (!ModbusRegs_IsLocalInputLocked() && !s_run_input.stable_active &&
      s_state != MENU_MAIN &&
      s_remote_input.activated) {
    AbortTuningOutput();
    s_state = MENU_MAIN;
    ApplySweepInput();
    UltrasonicCtrl_StartUntimed();
    MenuScreen_ForceRefresh();
    return;
  }

  if (s_state == MENU_MAIN) {
    if (ModbusRegs_IsLocalInputLocked()) {
      /* 통신 제어 준비/운전 중에는 PA10 REMOTE ON/OFF만 무시한다. */
    } else if (!s_run_input.stable_active) {
      /* 수동 모드: REMOTE 현재 레벨을 그대로 출력 ON/OFF에 반영한다. */
      if (s_remote_input.stable_active && !g_us_state.running) {
        UltrasonicCtrl_StartUntimed();
        MenuScreen_ForceRefresh();
      } else if (!s_remote_input.stable_active && g_us_state.running) {
        UltrasonicCtrl_Stop();
        MenuScreen_ForceRefresh();
      }
    } else if (s_remote_input.activated) {
      ToggleAutomaticRun();
    }
  }

  /* 두 버튼을 누르는 동안 단일 MODE/UP 이벤트를 소비하지 않는다. */
  if (HandleProtectedMenuCombo()) {
    return;
  }

  HandleButtonFeedback(mode, down, up, start_stop);

  if (mode == BTN_EVT_LONG_PRESS && !IsProtectedSettingState()) {
    const bool save_frequency =
        s_state == MENU_FREQUENCY || s_state == MENU_TUNE_MANUAL ||
        (s_state == MENU_TUNE_AUTO &&
         ResonanceTuning_GetState() == RES_TUNE_COMPLETE);
    AbortTuningOutput();
    if (save_frequency) {
      s_time_save_error = !SaveOperatingSettings();
    }
    s_state = MENU_MAIN;
    ApplySweepInput();
    MenuScreen_ForceRefresh();
    return;
  }

  if (s_state == MENU_MAIN) {
    if (s_run_input.stable_active) {
      /* RUN SW ON: START/STOP으로 설정된 시간만큼 자동 운전한다. */
      if (start_stop == BTN_EVT_PRESS) {
        ToggleAutomaticRun();
      }
    } else if (!g_us_state.running && mode == BTN_EVT_PRESS) {
      s_edit_time_mode = g_us_state.run_time_mode;
      s_edit_time_value = g_us_state.run_time_value;
      s_time_save_error = false;
      s_state = MENU_TIME;
      s_menu_idx = 1U;
      MenuScreen_ForceRefresh();
    }
    return;
  }

  switch (s_state) {
  case MENU_TIME:
    if (start_stop == BTN_EVT_PRESS) {
      s_edit_time_mode = (RunTimeMode_t)(((uint8_t)s_edit_time_mode + 1U) %
                                        RUN_TIME_MODE_COUNT);
    }
    if (s_edit_time_mode != RUN_TIME_CONTINUOUS) {
      if (IsIncrementEvent(up) && s_edit_time_value < RUN_TIME_VALUE_MAX) {
        s_edit_time_value += RUN_TIME_VALUE_STEP;
      }
      if (IsIncrementEvent(down) && s_edit_time_value > RUN_TIME_VALUE_MIN) {
        s_edit_time_value -= RUN_TIME_VALUE_STEP;
      }
    }
    break;

  case MENU_FREQUENCY:
    if (IsIncrementEvent(up)) {
      AdjustFrequency(1);
    }
    if (IsIncrementEvent(down)) {
      AdjustFrequency(-1);
    }
    break;

  case MENU_TUNE_SELECT:
    if (IsIncrementEvent(up) || IsIncrementEvent(down)) {
      s_tuning_method = (s_tuning_method == TUNE_METHOD_MANUAL)
                            ? TUNE_METHOD_AUTO
                            : TUNE_METHOD_MANUAL;
    }
    break;

  case MENU_TUNE_MANUAL:
    if (IsIncrementEvent(up)) {
      AdjustManualTune(1);
    }
    if (IsIncrementEvent(down)) {
      AdjustManualTune(-1);
    }
    if (start_stop == BTN_EVT_PRESS) {
      if (g_us_state.running) {
        UltrasonicCtrl_Stop();
        s_time_save_error = !SaveOperatingSettings();
      } else {
        /* 보호 메뉴에서 정한 폭/속도로 SWEEP 시험출력을 실행한다. */
        ApplySweepParameters();
        UltrasonicCtrl_SetMode(MODE_SWEEP);
        UltrasonicCtrl_StartUntimed();
      }
    }
    break;

  case MENU_TUNE_AUTO:
    if (start_stop == BTN_EVT_HOLD_3S &&
        ResonanceTuning_GetState() != RES_TUNE_ROUGH &&
        ResonanceTuning_GetState() != RES_TUNE_FINE) {
      ResonanceTuning_Cancel();
      s_auto_result_handled = false;
      UltrasonicCtrl_SetMode(MODE_CONTINUOUS);
      UltrasonicCtrl_SetTuningDutyOverride(true, AUTO_TUNE_DUTY_01PCT);
      if (!g_us_state.running) {
        UltrasonicCtrl_StartUntimed();
      }
      s_auto_starting = true;
    } else if (start_stop == BTN_EVT_PRESS &&
               (s_auto_starting || g_us_state.running)) {
      AbortTuningOutput();
    }
    break;

  case MENU_SWEEP_WIDTH:
    if (IsIncrementEvent(up) && s_sweep_width_hz < SWEEP_WIDTH_MAX_HZ) {
      s_sweep_width_hz += SWEEP_WIDTH_STEP_HZ;
      ApplySweepParameters();
    }
    if (IsIncrementEvent(down) && s_sweep_width_hz > SWEEP_WIDTH_MIN_HZ) {
      s_sweep_width_hz -= SWEEP_WIDTH_STEP_HZ;
      ApplySweepParameters();
    }
    break;

  case MENU_SWEEP_RATE:
    if (IsIncrementEvent(up) && s_sweep_rate_hz < SWEEP_RATE_MAX_HZ) {
      s_sweep_rate_hz += SWEEP_RATE_STEP_HZ;
      ApplySweepParameters();
    }
    if (IsIncrementEvent(down) && s_sweep_rate_hz > SWEEP_RATE_MIN_HZ) {
      s_sweep_rate_hz -= SWEEP_RATE_STEP_HZ;
      ApplySweepParameters();
    }
    break;

  case MENU_OUTPUT_MODE:
    if (IsIncrementEvent(up) || IsIncrementEvent(down)) {
      s_output_control_mode =
          (s_output_control_mode == OUTPUT_CONTROL_VOLUME)
              ? OUTPUT_CONTROL_CONSTANT_CURRENT
              : OUTPUT_CONTROL_VOLUME;
    }
    break;

  case MENU_CURRENT_SET:
    if (IsIncrementEvent(up) &&
        s_constant_current_centiamp < CONST_CURRENT_MAX_CENTIAMP) {
      s_constant_current_centiamp += CONST_CURRENT_STEP_CENTIAMP;
    }
    if (IsIncrementEvent(down) &&
        s_constant_current_centiamp > CONST_CURRENT_MIN_CENTIAMP) {
      s_constant_current_centiamp -= CONST_CURRENT_STEP_CENTIAMP;
    }
    break;

  default:
    s_state = MENU_MAIN;
    break;
  }

  if (mode == BTN_EVT_PRESS) {
    AdvanceMenu();
  }
  if (up != BTN_EVT_NONE || down != BTN_EVT_NONE ||
      start_stop != BTN_EVT_NONE || mode != BTN_EVT_NONE) {
    MenuScreen_ForceRefresh();
  }
}

MenuState_t Menu_GetState(void) { return s_state; }
uint8_t Menu_GetSelectedIndex(void) { return s_menu_idx; }

/* 기존 통신 메뉴 API 호환 */
uint8_t Menu_GetModbusEditAddress(void) { return g_modbus_cfg.address; }
uint32_t Menu_GetModbusEditBaudrate(void) { return g_modbus_cfg.baudrate; }
uint8_t Menu_GetModbusEditParity(void) { return g_modbus_cfg.parity; }
bool Menu_HasModbusSaveError(void) { return false; }

bool Menu_IsAutomaticMode(void) { return s_run_input.stable_active; }
bool Menu_IsRemoteActive(void) { return s_remote_input.stable_active; }
bool Menu_IsSweepActive(void) { return s_sweep_input.stable_active; }
RunTimeMode_t Menu_GetEditTimeMode(void) { return s_edit_time_mode; }
uint8_t Menu_GetEditTimeValue(void) { return s_edit_time_value; }
bool Menu_HasTimeSaveError(void) { return s_time_save_error; }
TuningMethod_t Menu_GetTuningMethod(void) { return s_tuning_method; }
OutputControlMode_t Menu_GetOutputControlMode(void) {
  return s_output_control_mode;
}
uint16_t Menu_GetSweepWidthHz(void) { return s_sweep_width_hz; }
uint16_t Menu_GetSweepRateHz(void) { return s_sweep_rate_hz; }
uint16_t Menu_GetCenterFrequency01kHz(void) {
  const uint32_t center_hz =
      (uint32_t)s_frequency_choices[s_frequency_index] * 100U +
      (uint32_t)s_frequency_fine_10hz[s_frequency_index] * 10U;
  return (uint16_t)((center_hz + 50U) / 100U);
}
uint16_t Menu_GetConstantCurrentCentiAmp(void) {
  return s_constant_current_centiamp;
}
bool Menu_IsAutoTuneStarting(void) { return s_auto_starting; }
uint8_t Menu_GetFrequencyBandIndex(void) { return s_frequency_index; }
uint16_t Menu_GetPowerRangeWatts(void) {
  if (s_power_range_index == POWER_RANGE_PERCENT_INDEX) {
    return 0U;
  }
  return (uint16_t)(POWER_RANGE_WATT_MIN +
                    ((uint16_t)s_power_range_index - 1U) *
                        POWER_RANGE_WATT_STEP);
}
uint8_t Menu_GetSupervisorBaudIndex(void) {
  return s_supervisor_baud_index;
}
uint8_t Menu_GetSupervisorAddress(void) { return s_supervisor_address; }
bool Menu_GetSupervisorRtermEnabled(void) {
  return s_supervisor_rterm_enabled;
}
bool Menu_HasSupervisorSaveError(void) { return s_supervisor_save_error; }

static bool IsSupervisorState(void) {
  return s_state == MENU_SUPERVISOR || s_state == MENU_SUPERVISOR_PL ||
         s_state == MENU_SUPERVISOR_BAUD ||
         s_state == MENU_SUPERVISOR_ADDR ||
         s_state == MENU_SUPERVISOR_TERM;
}

static bool HandleSupervisorExitCombo(void) {
  const bool combo_pressed = Button_IsPressed(BTN_ID_MODE) &&
                             Button_IsPressed(BTN_ID_DOWN);

  if (!combo_pressed) {
    s_supervisor_exit_combo_armed = true;
    s_supervisor_exit_combo_tracking = false;
    s_supervisor_exit_combo_latched = false;
    return false;
  }

  if (!s_supervisor_exit_combo_armed) {
    return true;
  }

  if (s_supervisor_exit_combo_latched) {
    return true;
  }

  const uint32_t now = HAL_GetTick();
  if (!s_supervisor_exit_combo_tracking) {
    s_supervisor_exit_combo_tracking = true;
    s_supervisor_exit_combo_tick = now;
    Buzzer_RequestButtonClick();
    return true;
  }

  if ((now - s_supervisor_exit_combo_tick) >= BTN_PROTECTED_MENU_HOLD_MS) {
    s_supervisor_exit_combo_latched = true;
    if (SaveSupervisorSettings()) {
      Buzzer_RequestComplete();
      s_state = MENU_MAIN;
      s_menu_idx = 0U;
      ApplySweepInput();
    } else {
      s_supervisor_save_error = true;
      Buzzer_RequestInvalidButton();
    }
    MenuScreen_ForceRefresh();
  }
  return true;
}

static void HandleSupervisorMenu(ButtonEvent_t mode, ButtonEvent_t down,
                                 ButtonEvent_t up) {
  if (s_state == MENU_SUPERVISOR) {
    if (mode == BTN_EVT_PRESS) {
      s_state = MENU_SUPERVISOR_PL;
    }
    return;
  }

  if (s_state == MENU_SUPERVISOR_PL) {
    if (IsIncrementEvent(up) &&
        s_power_range_index < POWER_RANGE_INDEX_MAX) {
      s_power_range_index++;
    }
    if (IsIncrementEvent(down) &&
        s_power_range_index > POWER_RANGE_PERCENT_INDEX) {
      s_power_range_index--;
    }
    if (mode == BTN_EVT_PRESS) {
      s_state = MENU_SUPERVISOR_BAUD;
    }
    return;
  }

  if (s_state == MENU_SUPERVISOR_BAUD) {
    if (IsIncrementEvent(up) &&
        s_supervisor_baud_index + 1U < MODBUS_BAUD_INDEX_COUNT) {
      s_supervisor_baud_index++;
    }
    if (IsIncrementEvent(down) && s_supervisor_baud_index > 0U) {
      s_supervisor_baud_index--;
    }
    if (mode == BTN_EVT_PRESS) {
      s_state = MENU_SUPERVISOR_ADDR;
    }
    return;
  }

  if (s_state == MENU_SUPERVISOR_ADDR) {
    if (IsIncrementEvent(up) &&
        s_supervisor_address < MODBUS_SUPERVISOR_ADDR_MAX) {
      s_supervisor_address++;
    }
    if (IsIncrementEvent(down) && s_supervisor_address > MODBUS_ADDR_MIN) {
      s_supervisor_address--;
    }
    if (mode == BTN_EVT_PRESS) {
      s_state = MENU_SUPERVISOR_TERM;
    }
    return;
  }

  if (s_state == MENU_SUPERVISOR_TERM) {
    if (IsIncrementEvent(up) || IsIncrementEvent(down)) {
      s_supervisor_rterm_enabled = !s_supervisor_rterm_enabled;
      ApplyRterm(s_supervisor_rterm_enabled);
    }
    if (mode == BTN_EVT_PRESS) {
      if (SaveSupervisorSettings()) {
        Buzzer_RequestComplete();
        s_state = MENU_SUPERVISOR_PL;
      } else {
        s_supervisor_save_error = true;
      }
    }
  }
}

static bool SaveSupervisorSettings(void) {
  SettingsStorageData_t settings;
  if (!SettingsStorage_Load(&settings)) {
    SettingsStorage_SetDefaults(&settings);
  }

  settings.address = s_supervisor_address;
  settings.baud_index = s_supervisor_baud_index;
  settings.parity = MODBUS_PARITY_DEFAULT; /* Supervisor 사양은 8-E-1 고정 */
  settings.power_range_index = s_power_range_index;
  settings.rterm_enabled = s_supervisor_rterm_enabled ? 1U : 0U;

  if (!SettingsStorage_Save(&settings)) {
    return false;
  }

  const bool uart_changed =
      g_modbus_cfg.address != settings.address ||
      g_modbus_cfg.baudrate != MODBUS_BAUD_TABLE[settings.baud_index] ||
      g_modbus_cfg.parity != settings.parity;
  g_modbus_cfg.address = settings.address;
  g_modbus_cfg.baudrate = MODBUS_BAUD_TABLE[settings.baud_index];
  g_modbus_cfg.parity = settings.parity;
  if (uart_changed) {
    Modbus_ReconfigUART();
  }
  ApplyRterm(s_supervisor_rterm_enabled);
  s_supervisor_save_error = false;
  return true;
}

static void ApplyRterm(bool enabled) {
  HAL_GPIO_WritePin(MODBUS_RTERM_PORT, MODBUS_RTERM_PIN,
                    enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool IsProtectedSettingState(void) {
  return s_state == MENU_FREQUENCY || s_state == MENU_SWEEP_WIDTH ||
         s_state == MENU_SWEEP_RATE || s_state == MENU_TUNE_SELECT ||
         s_state == MENU_TUNE_MANUAL || s_state == MENU_TUNE_AUTO ||
         s_state == MENU_OUTPUT_MODE || s_state == MENU_CURRENT_SET;
}

static bool HandleProtectedMenuCombo(void) {
  const bool combo_pressed = Button_IsPressed(BTN_ID_MODE) &&
                             Button_IsPressed(BTN_ID_UP);

  if (!combo_pressed) {
    s_protected_combo_tracking = false;
    s_protected_combo_latched = false;
    return false;
  }

  if (s_protected_combo_latched) {
    return true;
  }

  const bool entering = s_state == MENU_MAIN &&
                        !s_run_input.stable_active && !g_us_state.running;
  const bool exiting = IsProtectedSettingState();
  if (!entering && !exiting) {
    if (!s_protected_combo_tracking) {
      s_protected_combo_tracking = true;
      Buzzer_RequestInvalidButton();
    }
    return true;
  }

  const uint32_t now = HAL_GetTick();
  if (!s_protected_combo_tracking) {
    s_protected_combo_tracking = true;
    s_protected_combo_tick = now;
    Buzzer_RequestButtonClick();
    return true;
  }

  if ((now - s_protected_combo_tick) >= BTN_PROTECTED_MENU_HOLD_MS) {
    s_protected_combo_latched = true;
    if (exiting) {
      AbortTuningOutput();
      s_time_save_error = !SaveOperatingSettings();
      if (!s_time_save_error) {
        Buzzer_RequestComplete();
      }
      s_state = MENU_MAIN;
      s_menu_idx = 0U;
      ApplySweepInput();
    } else {
      s_time_save_error = false;
      s_state = MENU_FREQUENCY;
      s_menu_idx = 1U;
      ApplySelectedFrequency();
      ApplySweepParameters();
    }
    MenuScreen_ForceRefresh();
  }
  return true;
}

static void AdvanceMenu(void) {
  switch (s_state) {
  case MENU_TIME:
    UltrasonicCtrl_SetRunTimer(s_edit_time_mode, s_edit_time_value);
    s_time_save_error = !SaveRunTimer();
    if (!s_time_save_error) {
      Buzzer_RequestComplete();
    }
    s_state = MENU_MAIN;
    s_menu_idx = 0U;
    break;
  case MENU_FREQUENCY:
    s_time_save_error = !SaveOperatingSettings();
    if (!s_time_save_error) {
      Buzzer_RequestComplete();
    }
    s_state = MENU_SWEEP_WIDTH;
    s_menu_idx = 2U;
    break;
  case MENU_SWEEP_WIDTH:
    s_time_save_error = !SaveOperatingSettings();
    if (!s_time_save_error) {
      Buzzer_RequestComplete();
    }
    s_state = MENU_SWEEP_RATE;
    s_menu_idx = 3U;
    break;
  case MENU_SWEEP_RATE:
    s_time_save_error = !SaveOperatingSettings();
    if (!s_time_save_error) {
      Buzzer_RequestComplete();
    }
    s_state = MENU_TUNE_SELECT;
    s_menu_idx = 4U;
    break;
  case MENU_TUNE_SELECT:
    ResonanceTuning_Cancel();
    s_auto_starting = false;
    s_auto_result_handled = false;
    UltrasonicCtrl_SetMode(MODE_CONTINUOUS);
    s_state = (s_tuning_method == TUNE_METHOD_MANUAL) ? MENU_TUNE_MANUAL
                                                       : MENU_TUNE_AUTO;
    break;
  case MENU_TUNE_MANUAL:
    AbortTuningOutput();
    s_time_save_error = !SaveOperatingSettings();
    if (!s_time_save_error) {
      Buzzer_RequestComplete();
    }
    ApplySweepInput();
    s_state = MENU_OUTPUT_MODE;
    s_menu_idx = 5U;
    break;
  case MENU_TUNE_AUTO:
    if (ResonanceTuning_GetState() == RES_TUNE_COMPLETE) {
      const uint32_t result_hz = ResonanceTuning_GetResultFrequencyHz();
      s_frequency_choices[s_frequency_index] =
          (uint16_t)(result_hz / 100U);
      s_frequency_fine_10hz[s_frequency_index] =
          (uint8_t)((result_hz % 100U) / 10U);
      ApplySelectedFrequency();
    }
    AbortTuningOutput();
    s_time_save_error = !SaveOperatingSettings();
    if (!s_time_save_error) {
      Buzzer_RequestComplete();
    }
    ApplySweepInput();
    s_state = MENU_OUTPUT_MODE;
    s_menu_idx = 5U;
    break;
  case MENU_OUTPUT_MODE:
    if (s_output_control_mode == OUTPUT_CONTROL_CONSTANT_CURRENT) {
      s_state = MENU_CURRENT_SET;
      s_menu_idx = 6U;
    } else {
      s_state = MENU_FREQUENCY;
      s_menu_idx = 1U;
    }
    break;
  case MENU_CURRENT_SET:
    s_state = MENU_FREQUENCY;
    s_menu_idx = 1U;
    break;
  default:
    s_state = MENU_MAIN;
    s_menu_idx = 0U;
    break;
  }
}

static void AbortTuningOutput(void) {
  s_auto_starting = false;
  if (ResonanceTuning_GetState() == RES_TUNE_ROUGH ||
      ResonanceTuning_GetState() == RES_TUNE_FINE) {
    ResonanceTuning_Cancel();
  }
  if (g_us_state.running &&
      (s_state == MENU_TUNE_MANUAL || s_state == MENU_TUNE_AUTO)) {
    UltrasonicCtrl_Stop();
  }
  UltrasonicCtrl_SetTuningDutyOverride(false, AUTO_TUNE_DUTY_01PCT);
}

static void HandleAutoTuneProgress(void) {
  if (s_state != MENU_TUNE_AUTO) {
    return;
  }

  if (s_auto_starting && g_us_state.running && !g_us_state.soft_starting) {
    if (ResonanceTuning_Start(g_us_state.target_freq_hz)) {
      s_auto_starting = false;
      MenuScreen_ForceRefresh();
    }
  }

  const ResonanceTuneState_t state = ResonanceTuning_GetState();
  if (!s_auto_result_handled &&
      (state == RES_TUNE_COMPLETE || state == RES_TUNE_ERROR)) {
    s_auto_starting = false;
    if (g_us_state.running) {
      UltrasonicCtrl_Stop();
    }
    UltrasonicCtrl_SetTuningDutyOverride(false, AUTO_TUNE_DUTY_01PCT);
    if (state == RES_TUNE_COMPLETE) {
      Buzzer_RequestComplete();
    }
    s_auto_result_handled = true;
    MenuScreen_ForceRefresh();
  }
}

static void AdjustFrequency(int8_t direction) {
  if (direction > 0 && s_frequency_index + 1U < FREQUENCY_CHOICE_COUNT) {
    s_frequency_index++;
  } else if (direction < 0 && s_frequency_index > 0U) {
    s_frequency_index--;
  }
  ApplySelectedFrequency();
  ApplySweepParameters();
}

static void AdjustManualTune(int8_t direction) {
  uint16_t minimum;
  uint16_t maximum;
  GetManualTuneLimits(&minimum, &maximum);
  uint16_t frequency = g_us_state.target_freq;
  if (direction > 0 && frequency < maximum) {
    frequency += FREQ_STEP;
  } else if (direction < 0 && frequency > minimum) {
    frequency -= FREQ_STEP;
  }
  UltrasonicCtrl_SetFrequency(frequency);
  s_frequency_choices[s_frequency_index] = frequency;
  s_frequency_fine_10hz[s_frequency_index] = 0U;
  ApplySweepParameters();
}

static void ApplySelectedFrequency(void) {
  const uint32_t frequency_hz =
      (uint32_t)s_frequency_choices[s_frequency_index] * 100U +
      (uint32_t)s_frequency_fine_10hz[s_frequency_index] * 10U;
  UltrasonicCtrl_SetFrequencyHz(frequency_hz);
}

static void GetManualTuneLimits(uint16_t *minimum, uint16_t *maximum) {
  static const uint16_t low[FREQUENCY_CHOICE_COUNT] = {
      FREQ_BAND_28_MIN, FREQ_BAND_40_MIN, FREQ_BAND_68_MIN,
      FREQ_BAND_80_MIN};
  static const uint16_t high[FREQUENCY_CHOICE_COUNT] = {
      FREQ_BAND_28_MAX, FREQ_BAND_40_MAX, FREQ_BAND_68_MAX,
      FREQ_BAND_80_MAX};
  *minimum = low[s_frequency_index];
  *maximum = high[s_frequency_index];
}

static bool ReadActiveLow(const ExternalInput_t *input) {
  return HAL_GPIO_ReadPin(input->port, input->pin) == GPIO_PIN_RESET;
}

static void ExternalInputInit(ExternalInput_t *input) {
  const bool active = ReadActiveLow(input);
  input->stable_active = active;
  input->candidate_active = active;
  input->count = 0U;
  input->changed = false;
  input->activated = false;
}

static void ExternalInputUpdate(ExternalInput_t *input) {
  const bool active = ReadActiveLow(input);
  input->changed = false;
  input->activated = false;

  if (active != input->candidate_active) {
    input->candidate_active = active;
    input->count = 1U;
    return;
  }
  if (input->count < EXTERNAL_DEBOUNCE_COUNT) {
    input->count++;
  }
  if (input->count >= EXTERNAL_DEBOUNCE_COUNT &&
      input->stable_active != input->candidate_active) {
    input->stable_active = input->candidate_active;
    input->changed = true;
    input->activated = input->stable_active;
  }
}

static void UpdateExternalInputs(void) {
  const uint32_t now = HAL_GetTick();
  if ((now - s_external_poll_tick) < EXTERNAL_POLL_MS) {
    s_run_input.changed = false;
    s_remote_input.changed = false;
    s_remote_input.activated = false;
    s_sweep_input.changed = false;
    return;
  }
  s_external_poll_tick = now;
  ExternalInputUpdate(&s_run_input);
  ExternalInputUpdate(&s_remote_input);
  ExternalInputUpdate(&s_sweep_input);
}

static void ApplySweepInput(void) {
  UltrasonicCtrl_SetMode(s_sweep_input.stable_active ? MODE_SWEEP
                                                     : MODE_CONTINUOUS);
}

static void ApplySweepParameters(void) {
  UltrasonicCtrl_SetSweepParameters(g_us_state.target_freq, s_sweep_width_hz,
                                    s_sweep_rate_hz);
}

static void ToggleAutomaticRun(void) {
  /* PB9/START_STOP 또는 자동 REMOTE edge가 실행한 Local 운전이다. */
  ModbusRegs_NotifyLocalRunControl();
  if (g_us_state.running) {
    UltrasonicCtrl_Stop();
  } else {
    UltrasonicCtrl_Start();
  }
  MenuScreen_ForceRefresh();
}

static bool SaveRunTimer(void) {
  return SaveOperatingSettings();
}

static bool SaveOperatingSettings(void) {
  SettingsStorageData_t settings;
  SettingsStorageData_t previous;
  const bool loaded = SettingsStorage_Load(&previous);
  if (loaded) {
    settings = previous;
  } else {
    SettingsStorage_SetDefaults(&settings);
  }

  settings.address = g_modbus_cfg.address;
  settings.baud_index = Modbus_GetBaudIndex();
  settings.parity = g_modbus_cfg.parity;
  settings.run_time_value = s_edit_time_value;
  settings.run_time_mode = (uint8_t)s_edit_time_mode;
  settings.selected_frequency_band = s_frequency_index;
  settings.sweep_width_hz = s_sweep_width_hz;
  settings.sweep_rate_hz = s_sweep_rate_hz;
  for (uint8_t index = 0U; index < FREQUENCY_CHOICE_COUNT; index++) {
    settings.band_frequency[index] = s_frequency_choices[index];
    settings.band_frequency_fine_10hz[index] = s_frequency_fine_10hz[index];
  }

  if (loaded && settings.address == previous.address &&
      settings.baud_index == previous.baud_index &&
      settings.parity == previous.parity &&
      settings.run_time_value == previous.run_time_value &&
      settings.run_time_mode == previous.run_time_mode &&
      settings.selected_frequency_band == previous.selected_frequency_band &&
      settings.sweep_width_hz == previous.sweep_width_hz &&
      settings.sweep_rate_hz == previous.sweep_rate_hz) {
    bool frequencies_equal = true;
    for (uint8_t index = 0U; index < FREQUENCY_CHOICE_COUNT; index++) {
      if (settings.band_frequency[index] != previous.band_frequency[index] ||
          settings.band_frequency_fine_10hz[index] !=
              previous.band_frequency_fine_10hz[index]) {
        frequencies_equal = false;
      }
    }
    if (frequencies_equal) {
      return true;
    }
  }
  return SettingsStorage_Save(&settings);
}

static bool IsIncrementEvent(ButtonEvent_t event) {
  return event == BTN_EVT_PRESS || event == BTN_EVT_LONG_PRESS ||
         event == BTN_EVT_REPEAT;
}

static bool IsShortPressAllowed(ButtonId_t id) {
  if (IsSupervisorState()) {
    if (id == BTN_ID_MODE) {
      return true;
    }
    return (id == BTN_ID_UP || id == BTN_ID_DOWN) &&
           s_state != MENU_SUPERVISOR;
  }
  if (id == BTN_ID_MODE) {
    return s_state != MENU_MAIN ||
           (!s_run_input.stable_active && !g_us_state.running);
  }

  if (id == BTN_ID_START_STOP) {
    switch (s_state) {
    case MENU_MAIN:
      return s_run_input.stable_active;
    case MENU_TIME:
    case MENU_TUNE_MANUAL:
      return true;
    case MENU_TUNE_AUTO:
      return s_auto_starting || g_us_state.running;
    default:
      return false;
    }
  }

  if (id != BTN_ID_UP && id != BTN_ID_DOWN) {
    return false;
  }

  switch (s_state) {
  case MENU_TIME:
    return s_edit_time_mode != RUN_TIME_CONTINUOUS;
  case MENU_FREQUENCY:
  case MENU_TUNE_SELECT:
  case MENU_TUNE_MANUAL:
  case MENU_SWEEP_WIDTH:
  case MENU_SWEEP_RATE:
  case MENU_OUTPUT_MODE:
  case MENU_CURRENT_SET:
    return true;
  default:
    return false;
  }
}

static bool IsLongPressAllowed(ButtonId_t id) {
  if (IsSupervisorState()) {
    return IsShortPressAllowed(id);
  }
  if (id == BTN_ID_MODE) {
    return s_state != MENU_MAIN;
  }
  if (id == BTN_ID_START_STOP) {
    return s_state == MENU_TUNE_AUTO;
  }
  return IsShortPressAllowed(id);
}

static bool IsButtonDownAllowed(ButtonId_t id) {
  return IsShortPressAllowed(id) || IsLongPressAllowed(id);
}

static void HandleButtonFeedback(ButtonEvent_t mode, ButtonEvent_t down,
                                 ButtonEvent_t up,
                                 ButtonEvent_t start_stop) {
  /* 조합키가 거의 동시에 눌릴 때 첫 버튼을 단독 입력으로 울리지 않는다. */
  if (Button_IsPhysicallyPressed(BTN_ID_MODE) &&
      Button_IsPhysicallyPressed(BTN_ID_UP)) {
    return;
  }

  const ButtonEvent_t events[BTN_ID_COUNT] = {mode, down, up, start_stop};
  bool valid_press = false;
  bool invalid_press = false;

  for (uint8_t id = 0U; id < BTN_ID_COUNT; id++) {
    if (events[id] == BTN_EVT_DOWN) {
      if (IsButtonDownAllowed((ButtonId_t)id)) {
        valid_press = true;
      } else {
        invalid_press = true;
      }
    } else if ((id == BTN_ID_UP || id == BTN_ID_DOWN) &&
               (events[id] == BTN_EVT_LONG_PRESS ||
                events[id] == BTN_EVT_REPEAT) &&
               IsLongPressAllowed((ButtonId_t)id)) {
      Buzzer_RequestAdjustTick();
    }
  }

  /* 동시에 여러 버튼이 들어오면 잘못된 입력 경고음을 우선한다. */
  if (invalid_press) {
    Buzzer_RequestInvalidButton();
  } else if (valid_press) {
    Buzzer_RequestButtonClick();
  }
}
