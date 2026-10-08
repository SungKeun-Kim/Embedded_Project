/**
 * @file  settings_storage.h
 * @brief 내장 Flash를 이용한 통신/운전/Supervisor 설정 저장
 */
#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "params.h"

/** @brief Flash에 함께 저장되는 통신, 운전 및 Supervisor 설정 */
typedef struct {
  uint8_t address;    /* 1~247 */
  uint8_t baud_index; /* MODBUS_BAUD_TABLE 인덱스 */
  uint8_t parity;     /* 0=None, 1=Even, 2=Odd */
  uint8_t run_time_value; /* 1~99 */
  uint8_t run_time_mode;  /* RunTimeMode_t 값 */
  uint8_t selected_frequency_band; /* 마지막으로 선택한 주파수 대역 */
  uint16_t band_frequency[FREQ_BAND_COUNT]; /* 대역별 중심주파수, 0.1 kHz */
  uint8_t band_frequency_fine_10hz[FREQ_BAND_COUNT]; /* 0~9, 10 Hz 나머지 */
  uint16_t sweep_width_hz; /* 중심주파수 기준 Sweep 폭 */
  uint16_t sweep_rate_hz;  /* Sweep 왕복 속도 */
  uint8_t power_range_index; /* 0=PERCENT, 1~23=300~2500 W */
  uint8_t rterm_enabled;     /* 0=UNMOUNTED, 1=MOUNTED */
} SettingsStorageData_t;

/** @brief 통신/시간/주파수 대역 설정을 안전한 기본값으로 채운다. */
void SettingsStorage_SetDefaults(SettingsStorageData_t *data);

/**
 * @brief 마지막으로 저장된 유효 설정 읽기
 * @return true: 유효한 설정을 읽음, false: 저장값 없음/손상
 */
bool SettingsStorage_Load(SettingsStorageData_t *data);

/**
 * @brief 설정 저장
 *
 * 2 KB 설정 Page에 8-byte 정렬 Record를 순차 추가한다. 빈 공간이 없을
 * 때만 Page를 지워 Flash 마모를 줄인다. 기존 v1~v5 Record도 읽고
 * 새 항목은 기본값으로 보완한다.
 *
 * @return true: 저장 성공, false: Flash 오류 또는 잘못된 인자
 */
bool SettingsStorage_Save(const SettingsStorageData_t *data);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_STORAGE_H */
