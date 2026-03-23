/**
 * @file  modbus_regs.c
 * @brief Modbus 레지스터 맵 읽기/쓰기 핸들러
 */
#include "modbus_regs.h"
#include "megasonic_ctrl.h"
#include "modbus_rtu.h"
#include "params.h"
#include "stm32g4xx_hal.h"

bool ModbusRegs_Read(uint16_t addr, uint16_t *value)
{
    switch (addr) {
    case REG_ADDR_ON_OFF:
        *value = g_us_state.running ? 1 : 0;
        break;
    case REG_ADDR_FREQ_SET:
        *value = g_us_state.target_freq;
        break;
    case REG_ADDR_DUTY_SET:
        *value = g_us_state.target_duty;
        break;
    case REG_ADDR_FREQ_ACTUAL:
        *value = g_us_state.target_freq;  /* 실제 출력 주파수 */
        break;
    case REG_ADDR_DUTY_ACTUAL:
        *value = g_us_state.current_duty;
        break;
    case REG_ADDR_STATUS_FLAGS:
        *value = MegasonicCtrl_GetStatusFlags();
        break;
    case REG_ADDR_OPER_MODE:
        *value = (uint16_t)g_us_state.mode;
        break;
    case REG_ADDR_PULSE_ON:
        *value = g_us_state.pulse_on_ms;
        break;
    case REG_ADDR_PULSE_OFF:
        *value = g_us_state.pulse_off_ms;
        break;
    case REG_ADDR_SWEEP_START:
        *value = g_us_state.sweep_start_freq;
        break;
    case REG_ADDR_SWEEP_END:
        *value = g_us_state.sweep_end_freq;
        break;
    case REG_ADDR_SWEEP_TIME:
        *value = g_us_state.sweep_time_ms;
        break;
    case REG_ADDR_MODBUS_ADDR:
        *value = g_modbus_cfg.address;
        break;
    case REG_ADDR_MODBUS_BAUD: {
        /* baud → 인덱스 */
        uint16_t idx = 0;
        for (uint16_t i = 0; i < MODBUS_BAUD_INDEX_COUNT; i++) {
            if (MODBUS_BAUD_TABLE[i] == g_modbus_cfg.baudrate) { idx = i; break; }
        }
        *value = idx;
        break;
    }
    case REG_ADDR_MODBUS_PARITY:
        *value = g_modbus_cfg.parity;
        break;
    default:
        return false;  /* 잘못된 주소 */
    }
    return true;
}

bool ModbusRegs_Write(uint16_t addr, uint16_t value)
{
    switch (addr) {
    case REG_ADDR_ON_OFF:
        if (value == 1) MegasonicCtrl_Start();
        else if (value == 0) MegasonicCtrl_Stop();
        else return false;
        break;
    case REG_ADDR_FREQ_SET:
        if (value < FREQ_MIN || value > FREQ_MAX) return false;
        MegasonicCtrl_SetFrequency(value);
        break;
    case REG_ADDR_DUTY_SET:
        if (value > DUTY_MAX) return false;
        MegasonicCtrl_SetDuty(value);
        break;
    case REG_ADDR_OPER_MODE:
        if (value >= MODE_COUNT) return false;
        MegasonicCtrl_SetMode((OperatingMode_t)value);
        break;
    case REG_ADDR_PULSE_ON:
        if (value < PULSE_ON_MIN || value > PULSE_ON_MAX) return false;
        g_us_state.pulse_on_ms = value;
        break;
    case REG_ADDR_PULSE_OFF:
        if (value < PULSE_OFF_MIN || value > PULSE_OFF_MAX) return false;
        g_us_state.pulse_off_ms = value;
        break;
    case REG_ADDR_SWEEP_START:
        if (value < FREQ_MIN || value > FREQ_MAX) return false;
        g_us_state.sweep_start_freq = value;
        break;
    case REG_ADDR_SWEEP_END:
        if (value < FREQ_MIN || value > FREQ_MAX) return false;
        g_us_state.sweep_end_freq = value;
        break;
    case REG_ADDR_SWEEP_TIME:
        if (value < SWEEP_TIME_MIN || value > SWEEP_TIME_MAX) return false;
        g_us_state.sweep_time_ms = value;
        break;
    case REG_ADDR_MODBUS_ADDR:
        if (value < MODBUS_ADDR_MIN || value > MODBUS_ADDR_MAX) return false;
        g_modbus_cfg.address = (uint8_t)value;
        break;
    case REG_ADDR_MODBUS_BAUD:
        if (value >= MODBUS_BAUD_INDEX_COUNT) return false;
        g_modbus_cfg.baudrate = MODBUS_BAUD_TABLE[value];
        Modbus_ReconfigUART();
        break;
    case REG_ADDR_MODBUS_PARITY:
        if (value > 2) return false;
        g_modbus_cfg.parity = (uint8_t)value;
        Modbus_ReconfigUART();
        break;
    case REG_ADDR_SYSTEM_RESET:
        if (value == 0x1234U) {
            NVIC_SystemReset();
        }
        return false;  /* 잘못된 값이면 무시 */
    default:
        return false;
    }
    return true;
}
