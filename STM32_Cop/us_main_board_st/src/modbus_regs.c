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
    *value = ToProtocolBool(g_us_state.mode == MODE_SWEEP &&
                            g_us_state.running);
    break;
  case REG_ADDR_ERROR_STATUS_RESET:
    *value = ToProtocolBool(g_us_state.error_active);
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
    if (!s_local_input_locked) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    return (value >= PLC_OUTPUT_MIN && value <= PLC_OUTPUT_MAX)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_LOCAL_INPUT_LOCK:
    if (!IsProtocolBool(value)) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    return (value == PROTOCOL_BOOL_TRUE || !g_us_state.running)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_RUN_STATUS:
    if (!IsProtocolBool(value)) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    /* 정지는 Lock 여부와 관계없이 항상 허용한다. */
    if (value == PROTOCOL_BOOL_FALSE) {
      return MODBUS_REG_OK;
    }
    return (s_local_input_locked && !g_us_state.error_active)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_SWEEP_STATUS:
    if (!IsProtocolBool(value)) {
      return MODBUS_REG_ILLEGAL_VALUE;
    }
    return (value == PROTOCOL_BOOL_FALSE || s_local_input_locked)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  case REG_ADDR_ERROR_STATUS_RESET:
    return (value == PROTOCOL_BOOL_TRUE && !g_us_state.running)
               ? MODBUS_REG_OK
               : MODBUS_REG_ILLEGAL_VALUE;

  /* Board 설정값, 상태값, 통신 설정과 예비 주소는 PLC Read-only/미구현이다. */
  case REG_ADDR_FREQUENCY_VALUE:
  case REG_ADDR_DISPLAY_RANGE:
  case REG_ADDR_SWEEP_FREQUENCY:
  case REG_ADDR_EXTERNAL_INPUT:
  case REG_ADDR_RESERVED_1:
  case REG_ADDR_RESERVED_2:
  case REG_ADDR_RESERVED_3:
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
    s_local_input_locked = (value == PROTOCOL_BOOL_TRUE);
    break;
  case REG_ADDR_RUN_STATUS:
    if (value == PROTOCOL_BOOL_TRUE) {
      UltrasonicCtrl_Start();
    } else {
      UltrasonicCtrl_Stop();
    }
    break;
  case REG_ADDR_SWEEP_STATUS:
    UltrasonicCtrl_SetMode(value == PROTOCOL_BOOL_TRUE ? MODE_SWEEP
                                                       : MODE_CONTINUOUS);
    break;
  case REG_ADDR_ERROR_STATUS_RESET:
    return UltrasonicCtrl_ResetError();
  default:
    return false;
  }
  return true;
}

bool ModbusRegs_IsLocalInputLocked(void) { return s_local_input_locked; }
