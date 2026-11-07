/**
 * @file  modbus_regs.c
 * @brief 기존 ATmega Protocol 명령을 반영한 Modbus Holding Register
 */
#include "modbus_regs.h"

#include "config.h"
#include "menu.h"
#include "modbus_rtu.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include "ultrasonic_ctrl.h"
#include <stddef.h>

static bool s_local_input_locked;
static bool s_modbus_run_controlled;

static bool IsModbusBoardDetected(void) {
  return HAL_GPIO_ReadPin(MODBUS_DETECT_PORT, MODBUS_DETECT_PIN) ==
         GPIO_PIN_RESET;
}

static bool IsCommunicationControlReady(void) {
  return IsModbusBoardDetected() && !Menu_IsAutomaticMode() &&
         Menu_GetState() == MENU_MAIN;
}

static bool IsProtocolBool(uint16_t value) {
  return value == PROTOCOL_BOOL_FALSE || value == PROTOCOL_BOOL_TRUE;
}

static uint16_t ToProtocolBool(bool value) {
  return value ? PROTOCOL_BOOL_TRUE : PROTOCOL_BOOL_FALSE;
}

bool ModbusRegs_Read(uint16_t addr, uint16_t *value) {
  if (value == NULL) {
    return false;
  }

  switch (addr) {
  case REG_ADDR_FREQUENCY_VALUE:
    *value = g_us_state.current_freq;
    break;
  case REG_ADDR_OUTPUT_VALUE:
    *value = g_us_state.output_command;
    break;
  case REG_ADDR_DISPLAY_RANGE:
    *value = Menu_GetPowerRangeWatts();
    if (*value == 0U) {
      *value = DISPLAY_OUTPUT_RANGE;
    }
    break;
  case REG_ADDR_SWEEP_FREQUENCY:
    *value = (g_us_state.mode == MODE_SWEEP && g_us_state.running)
                 ? g_us_state.current_freq
                 : 0U;
    break;
  case REG_ADDR_LOCAL_INPUT_LOCK:
    *value = ToProtocolBool(s_local_input_locked);
    break;
  case REG_ADDR_RUN_STATUS:
    *value = ToProtocolBool(g_us_state.running);
    break;
  case REG_ADDR_EXTERNAL_INPUT:
    *value = ToProtocolBool(HAL_GPIO_ReadPin(REMOTE_PORT, REMOTE_PIN) ==
                            GPIO_PIN_RESET);
    break;
  case REG_ADDR_SWEEP_STATUS:
    /* Sweep 선택은 물리 SWEEP_SW만 담당한다. 정지 중에도 스위치 상태를 읽는다. */
    *value = ToProtocolBool(Menu_IsSweepActive());
    break;
  case REG_ADDR_ERROR_STATUS_RESET:
    *value = ToProtocolBool(g_us_state.error_active);
    break;
  case REG_ADDR_ERROR_CODE:
    /* 0=None, 1=Transducer, 2=Over current */
    *value = (uint16_t)g_us_state.error_code;
    break;
  case REG_ADDR_RUN_SWITCH_STATUS:
    /* PA11/RUN_SW는 Active LOW이며 Menu에서 디바운스한 상태를 사용한다. */
    *value = ToProtocolBool(Menu_IsAutomaticMode());
    break;
  case REG_ADDR_COMM_CONTROL_READY:
    /* Lock/RUN 명령을 받을 수 있는 보드 감지+RUN OFF+메인화면 상태 */
    *value = ToProtocolBool(IsCommunicationControlReady());
    break;
  case REG_ADDR_CENTER_FREQUENCY:
    /* Local 보호 메뉴에서 설정한 중심주파수이며 단위는 0.1 kHz다. */
    *value = Menu_GetCenterFrequency01kHz();
    break;
  case REG_ADDR_SWEEP_WIDTH:
    *value = Menu_GetSweepWidthHz();
    break;
  case REG_ADDR_SWEEP_SPEED:
    *value = Menu_GetSweepRateHz();
    break;
  case REG_ADDR_MODBUS_ADDR:
    *value = g_modbus_cfg.address;
    break;
  case REG_ADDR_MODBUS_BAUD:
    *value = Modbus_GetBaudIndex();
    break;
  case REG_ADDR_MODBUS_PARITY:
    *value = g_modbus_cfg.parity;
    break;
  default:
    return false;
  }
  return true;
}

ModbusRegResult_t ModbusRegs_ValidateWrite(uint16_t addr, uint16_t value) {
  switch (addr) {
  case REG_ADDR_OUTPUT_VALUE:
    if (!s_local_input_locked || !IsCommunicationControlReady()) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    return (value >= PLC_OUTPUT_MIN && value <= PLC_OUTPUT_MAX)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_LOCAL_INPUT_LOCK:
    if (!IsProtocolBool(value)) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    if (value == PROTOCOL_BOOL_FALSE) {
      return MODBUS_REG_OK;
    }
    return IsCommunicationControlReady() ? MODBUS_REG_OK
                                         : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_RUN_STATUS:
    if (!IsProtocolBool(value)) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    /* 정지는 Lock 여부와 관계없이 항상 허용한다. */
    if (value == PROTOCOL_BOOL_FALSE) {
      return MODBUS_REG_OK;
    }
    return (s_local_input_locked && IsCommunicationControlReady() &&
            !g_us_state.error_active)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_ERROR_STATUS_RESET:
    return (value == PROTOCOL_BOOL_TRUE && !g_us_state.running)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  /* Board 설정값, 상태값과 통신 설정은 PLC Read-only다. */
  case REG_ADDR_FREQUENCY_VALUE:
  case REG_ADDR_DISPLAY_RANGE:
  case REG_ADDR_SWEEP_FREQUENCY:
  case REG_ADDR_EXTERNAL_INPUT:
  case REG_ADDR_SWEEP_STATUS:
  case REG_ADDR_ERROR_CODE:
  case REG_ADDR_RUN_SWITCH_STATUS:
  case REG_ADDR_COMM_CONTROL_READY:
  case REG_ADDR_CENTER_FREQUENCY:
  case REG_ADDR_SWEEP_WIDTH:
  case REG_ADDR_SWEEP_SPEED:
  case REG_ADDR_MODBUS_ADDR:
  case REG_ADDR_MODBUS_BAUD:
  case REG_ADDR_MODBUS_PARITY:
  default:
    return MODBUS_REG_ILLEGAL_ADDRESS;
  }
}

bool ModbusRegs_Write(uint16_t addr, uint16_t value) {
  if (ModbusRegs_ValidateWrite(addr, value) != MODBUS_REG_OK) {
    return false;
  }

  switch (addr) {
  case REG_ADDR_OUTPUT_VALUE:
    UltrasonicCtrl_SetOutputCommand(value);
    break;
  case REG_ADDR_LOCAL_INPUT_LOCK:
    if (value == PROTOCOL_BOOL_TRUE) {
      if (!s_local_input_locked) {
        /* 통신 준비 진입은 기존 Local/REMOTE 출력을 정지한 상태에서 시작한다. */
        UltrasonicCtrl_Stop();
        s_modbus_run_controlled = false;
      }
      s_local_input_locked = true;
      UltrasonicCtrl_SetPlcOutputControl(true);
    } else {
      ModbusRegs_ReleaseLocalControl();
    }
    break;
  case REG_ADDR_RUN_STATUS:
    if (value == PROTOCOL_BOOL_TRUE) {
      /*
       * PLC가 명시적으로 Stop을 쓸 때까지 운전한다.
       * 보드의 M/S/CONT 타이머 설정은 Local 자동 운전에만 적용한다.
       */
      UltrasonicCtrl_StartUntimed();
      s_modbus_run_controlled = true;
    } else {
      UltrasonicCtrl_Stop();
      /* RUN OFF는 출력만 정지한다. 통신 준비 상태와 485 COMM 표시는 유지한다. */
      s_modbus_run_controlled = false;
    }
    break;
  case REG_ADDR_ERROR_STATUS_RESET:
    return UltrasonicCtrl_ResetError();
  default:
    return false;
  }
  return true;
}

bool ModbusRegs_IsLocalInputLocked(void) { return s_local_input_locked; }

void ModbusRegs_Update(void) {
  if (s_local_input_locked && !IsModbusBoardDetected()) {
    UltrasonicCtrl_Stop();
    ModbusRegs_ReleaseLocalControl();
  }
}

bool ModbusRegs_IsRunControlled(void) { return s_modbus_run_controlled; }

void ModbusRegs_NotifyLocalRunControl(void) {
  s_modbus_run_controlled = false;
}

void ModbusRegs_ReleaseLocalControl(void) {
  s_local_input_locked = false;
  s_modbus_run_controlled = false;
  UltrasonicCtrl_SetPlcOutputControl(false);
}
