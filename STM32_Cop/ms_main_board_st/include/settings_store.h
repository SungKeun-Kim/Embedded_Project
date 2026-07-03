/**
 * @file  settings_store.h
 * @brief 사용자 설정 FLASH 저장/복원 인터페이스
 */
#ifndef SETTINGS_STORE_H
#define SETTINGS_STORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "menu.h"

typedef struct {
    OperatingMode_t selected_mode;
    uint8_t selected_freq_ch;   /* 0~9 */
    uint8_t selected_power_ch;  /* 0~7 */

    FreqChannelProfile_t freq_profiles[FREQ_EDIT_CH_COUNT];
    PowerChannelProfile_t power_profiles[POWER_8STEP_COUNT];

    uint8_t modbus_addr;      /* 1~247 */
    uint8_t modbus_baud_idx;  /* 0~3 */
    uint8_t rs485_term_on;    /* 0/1 */
    uint8_t rs485_parity;     /* 0=None, 1=Even */
} SettingsStoreData_t;

uint8_t SettingsStore_Load(SettingsStoreData_t *out_data);
uint8_t SettingsStore_Save(const SettingsStoreData_t *in_data);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_STORE_H */
