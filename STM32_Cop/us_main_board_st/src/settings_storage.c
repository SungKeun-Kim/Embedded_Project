/**
 * @file  settings_storage.c
 * @brief STM32G474CBT6 마지막 Flash Page에 통신/운전/Supervisor 설정 저장
 */
#include "settings_storage.h"

#include "params.h"
#include "stm32g4xx_hal.h"
#include <stddef.h>
#include <string.h>

/* Linker script에서 설정 전용 마지막 Page 시작 주소를 제공한다. */
extern uint32_t __settings_flash_start__;
extern uint32_t __settings_flash_end__;

#define SETTINGS_MAGIC 0x4D424346UL /* ASCII "MBCF" */
#define SETTINGS_VERSION_LEGACY 1U
#define SETTINGS_VERSION_V2 2U
#define SETTINGS_VERSION_V3 3U
#define SETTINGS_VERSION 4U
#define SETTINGS_ERASED_WORD 0xFFFFFFFFUL

/** 기존 v1 Record. 현장 보드의 통신 설정을 계속 읽기 위해 유지한다. */
typedef struct {
  uint32_t magic;
  uint32_t sequence;
  uint8_t version;
  uint8_t address;
  uint8_t baud_index;
  uint8_t parity;
  uint32_t crc32;
} SettingsRecordV1_t;

/** v2 Record: 통신 설정과 LCD 운전 타이머 설정을 함께 보존한다. */
typedef struct {
  uint32_t magic;
  uint32_t sequence;
  uint8_t version;
  uint8_t address;
  uint8_t baud_index;
  uint8_t parity;
  uint8_t run_time_value;
  uint8_t run_time_mode;
  uint8_t reserved0;
  uint8_t reserved1;
  uint32_t reserved2;
  uint32_t crc32;
} SettingsRecordV2_t;

/** v3 Record: 대역별 중심주파수와 마지막 선택 대역을 추가한다. */
typedef struct {
  uint32_t magic;
  uint32_t sequence;
  uint8_t version;
  uint8_t address;
  uint8_t baud_index;
  uint8_t parity;
  uint8_t run_time_value;
  uint8_t run_time_mode;
  uint8_t selected_frequency_band;
  uint8_t reserved0;
  uint16_t band_frequency[FREQ_BAND_COUNT];
  uint32_t reserved1;
  uint32_t crc32;
} SettingsRecordV3_t;

/** v4 Record: Supervisor 전력 범위와 RS485 종단저항 설정을 추가한다. */
typedef struct {
  uint32_t magic;
  uint32_t sequence;
  uint8_t version;
  uint8_t address;
  uint8_t baud_index;
  uint8_t parity;
  uint8_t run_time_value;
  uint8_t run_time_mode;
  uint8_t selected_frequency_band;
  uint8_t power_range_index;
  uint16_t band_frequency[FREQ_BAND_COUNT];
  uint8_t rterm_enabled;
  uint8_t reserved[3];
  uint32_t crc32;
} SettingsRecordV4_t;

_Static_assert(sizeof(SettingsRecordV1_t) == 16U,
               "SettingsRecordV1_t must be exactly 16 bytes");
_Static_assert(sizeof(SettingsRecordV2_t) == 24U,
               "SettingsRecordV2_t must be exactly 24 bytes");
_Static_assert(sizeof(SettingsRecordV3_t) == 32U,
               "SettingsRecordV3_t must be exactly 32 bytes");
_Static_assert(sizeof(SettingsRecordV4_t) == 32U,
               "SettingsRecordV4_t must be exactly 32 bytes");

static uint32_t Crc32(const uint8_t *data, size_t length);
static bool RecordV1IsValid(const SettingsRecordV1_t *record);
static bool RecordV2IsValid(const SettingsRecordV2_t *record);
static bool RecordV3IsValid(const SettingsRecordV3_t *record);
static bool RecordV4IsValid(const SettingsRecordV4_t *record);
static bool BandFrequencyIsValid(uint8_t index, uint16_t frequency);
static uint8_t LegacyBaudIndexToCurrent(uint8_t legacy_index);
static uintptr_t SettingsStart(void);
static uintptr_t SettingsEnd(void);

void SettingsStorage_SetDefaults(SettingsStorageData_t *data) {
  if (data == NULL) {
    return;
  }

  *data = (SettingsStorageData_t){
      .address = MODBUS_ADDR_DEFAULT,
      .baud_index = MODBUS_BAUD_INDEX_DEFAULT,
      .parity = MODBUS_PARITY_DEFAULT,
      .run_time_value = RUN_TIME_VALUE_DEFAULT,
      .run_time_mode = RUN_TIME_MINUTES,
      .selected_frequency_band = 0U,
      .band_frequency = {FREQ_BAND_28_DEFAULT, FREQ_BAND_40_DEFAULT,
                         FREQ_BAND_68_DEFAULT, FREQ_BAND_80_DEFAULT},
      .power_range_index = POWER_RANGE_PERCENT_INDEX,
      .rterm_enabled = 0U,
  };
}

bool SettingsStorage_Load(SettingsStorageData_t *data) {
  if (data == NULL) {
    return false;
  }

  const uint8_t *cursor = (const uint8_t *)SettingsStart();
  const uint8_t *end = (const uint8_t *)SettingsEnd();
  SettingsStorageData_t latest;
  SettingsStorage_SetDefaults(&latest);
  uint32_t latest_sequence = 0U;
  bool found = false;

  while ((size_t)(end - cursor) >= sizeof(SettingsRecordV1_t)) {
    const SettingsRecordV1_t *base = (const SettingsRecordV1_t *)cursor;
    if (base->magic == SETTINGS_ERASED_WORD &&
        base->sequence == SETTINGS_ERASED_WORD) {
      break;
    }

    if (base->version == SETTINGS_VERSION_LEGACY) {
      if (RecordV1IsValid(base) && (!found || base->sequence > latest_sequence)) {
        latest.address = base->address;
        latest.baud_index = LegacyBaudIndexToCurrent(base->baud_index);
        latest.parity = base->parity;
        latest_sequence = base->sequence;
        found = true;
      }
      cursor += sizeof(SettingsRecordV1_t);
      continue;
    }

    if (base->version == SETTINGS_VERSION_V2 &&
        (size_t)(end - cursor) >= sizeof(SettingsRecordV2_t)) {
      const SettingsRecordV2_t *record = (const SettingsRecordV2_t *)cursor;
      if (RecordV2IsValid(record) &&
          (!found || record->sequence > latest_sequence)) {
        latest.address = record->address;
        latest.baud_index = LegacyBaudIndexToCurrent(record->baud_index);
        latest.parity = record->parity;
        latest.run_time_value = record->run_time_value;
        latest.run_time_mode = record->run_time_mode;
        latest_sequence = record->sequence;
        found = true;
      }
      cursor += sizeof(SettingsRecordV2_t);
      continue;
    }

    if (base->version == SETTINGS_VERSION_V3 &&
        (size_t)(end - cursor) >= sizeof(SettingsRecordV3_t)) {
      const SettingsRecordV3_t *record = (const SettingsRecordV3_t *)cursor;
      if (RecordV3IsValid(record) &&
          (!found || record->sequence > latest_sequence)) {
        latest.address = record->address;
        latest.baud_index = LegacyBaudIndexToCurrent(record->baud_index);
        latest.parity = record->parity;
        latest.run_time_value = record->run_time_value;
        latest.run_time_mode = record->run_time_mode;
        latest.selected_frequency_band = record->selected_frequency_band;
        memcpy(latest.band_frequency, record->band_frequency,
               sizeof(latest.band_frequency));
        latest_sequence = record->sequence;
        found = true;
      }
      cursor += sizeof(SettingsRecordV3_t);
      continue;
    }

    if (base->version == SETTINGS_VERSION &&
        (size_t)(end - cursor) >= sizeof(SettingsRecordV4_t)) {
      const SettingsRecordV4_t *record = (const SettingsRecordV4_t *)cursor;
      if (RecordV4IsValid(record) &&
          (!found || record->sequence > latest_sequence)) {
        latest.address = record->address;
        latest.baud_index = record->baud_index;
        latest.parity = record->parity;
        latest.run_time_value = record->run_time_value;
        latest.run_time_mode = record->run_time_mode;
        latest.selected_frequency_band = record->selected_frequency_band;
        memcpy(latest.band_frequency, record->band_frequency,
               sizeof(latest.band_frequency));
        latest.power_range_index = record->power_range_index;
        latest.rterm_enabled = record->rterm_enabled;
        latest_sequence = record->sequence;
        found = true;
      }
      cursor += sizeof(SettingsRecordV4_t);
      continue;
    }

    /* 알 수 없는 Record는 길이를 판단할 수 없으므로 탐색을 끝낸다. */
    break;
  }

  if (!found) {
    return false;
  }
  *data = latest;
  return true;
}

bool SettingsStorage_Save(const SettingsStorageData_t *data) {
  if (data == NULL || data->address < MODBUS_ADDR_MIN ||
      data->address > MODBUS_ADDR_MAX ||
      data->baud_index >= MODBUS_BAUD_INDEX_COUNT || data->parity > 2U ||
      data->run_time_value < RUN_TIME_VALUE_MIN ||
      data->run_time_value > RUN_TIME_VALUE_MAX ||
      data->run_time_mode >= RUN_TIME_MODE_COUNT ||
      data->selected_frequency_band >= FREQ_BAND_COUNT ||
      data->power_range_index > POWER_RANGE_INDEX_MAX ||
      data->rterm_enabled > 1U) {
    return false;
  }
  for (uint8_t index = 0U; index < FREQ_BAND_COUNT; index++) {
    if (!BandFrequencyIsValid(index, data->band_frequency[index])) {
      return false;
    }
  }

  const uint8_t *cursor = (const uint8_t *)SettingsStart();
  const uint8_t *end = (const uint8_t *)SettingsEnd();
  uint32_t next_sequence = 1U;

  while ((size_t)(end - cursor) >= sizeof(SettingsRecordV1_t)) {
    const SettingsRecordV1_t *base = (const SettingsRecordV1_t *)cursor;
    if (base->magic == SETTINGS_ERASED_WORD &&
        base->sequence == SETTINGS_ERASED_WORD) {
      break;
    }
    if (base->version == SETTINGS_VERSION_LEGACY) {
      if (RecordV1IsValid(base) && base->sequence >= next_sequence) {
        next_sequence = base->sequence + 1U;
      }
      cursor += sizeof(SettingsRecordV1_t);
    } else if (base->version == SETTINGS_VERSION_V2 &&
               (size_t)(end - cursor) >= sizeof(SettingsRecordV2_t)) {
      const SettingsRecordV2_t *record = (const SettingsRecordV2_t *)cursor;
      if (RecordV2IsValid(record) && record->sequence >= next_sequence) {
        next_sequence = record->sequence + 1U;
      }
      cursor += sizeof(SettingsRecordV2_t);
    } else if (base->version == SETTINGS_VERSION_V3 &&
               (size_t)(end - cursor) >= sizeof(SettingsRecordV3_t)) {
      const SettingsRecordV3_t *record = (const SettingsRecordV3_t *)cursor;
      if (RecordV3IsValid(record) && record->sequence >= next_sequence) {
        next_sequence = record->sequence + 1U;
      }
      cursor += sizeof(SettingsRecordV3_t);
    } else if (base->version == SETTINGS_VERSION &&
               (size_t)(end - cursor) >= sizeof(SettingsRecordV4_t)) {
      const SettingsRecordV4_t *record = (const SettingsRecordV4_t *)cursor;
      if (RecordV4IsValid(record) && record->sequence >= next_sequence) {
        next_sequence = record->sequence + 1U;
      }
      cursor += sizeof(SettingsRecordV4_t);
    } else {
      cursor = end;
      break;
    }
  }

  if (HAL_FLASH_Unlock() != HAL_OK) {
    return false;
  }

  bool success = true;
  uintptr_t target = (uintptr_t)cursor;
  if ((size_t)(end - cursor) < sizeof(SettingsRecordV4_t)) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    const uint32_t page_address = (uint32_t)SettingsStart();

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.Page = (page_address - FLASH_BASE) / FLASH_PAGE_SIZE;
    erase.NbPages = 1U;

    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) {
      success = false;
    } else {
      target = SettingsStart();
    }
  }

  SettingsRecordV4_t record = {
      .magic = SETTINGS_MAGIC,
      .sequence = next_sequence,
      .version = SETTINGS_VERSION,
      .address = data->address,
      .baud_index = data->baud_index,
      .parity = data->parity,
      .run_time_value = data->run_time_value,
      .run_time_mode = data->run_time_mode,
      .selected_frequency_band = data->selected_frequency_band,
      .power_range_index = data->power_range_index,
      .band_frequency = {data->band_frequency[0], data->band_frequency[1],
                         data->band_frequency[2], data->band_frequency[3]},
      .rterm_enabled = data->rterm_enabled,
      .reserved = {0U, 0U, 0U},
      .crc32 = 0U,
  };
  record.crc32 = Crc32((const uint8_t *)&record,
                        offsetof(SettingsRecordV4_t, crc32));

  if (success) {
    const uint8_t *bytes = (const uint8_t *)&record;
    for (uint32_t offset = 0U; offset < sizeof(record); offset += 8U) {
      uint64_t doubleword = 0U;
      memcpy(&doubleword, &bytes[offset], sizeof(doubleword));
      if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                            (uint32_t)(target + offset),
                            doubleword) != HAL_OK) {
        success = false;
        break;
      }
    }
  }

  if (HAL_FLASH_Lock() != HAL_OK) {
    success = false;
  }
  return success;
}

static uint32_t Crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFFUL;

  while (length-- > 0U) {
    crc ^= *data++;
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      crc = ((crc & 1U) != 0U) ? ((crc >> 1) ^ 0xEDB88320UL)
                               : (crc >> 1);
    }
  }
  return crc ^ 0xFFFFFFFFUL;
}

static bool RecordV1IsValid(const SettingsRecordV1_t *record) {
  return record->magic == SETTINGS_MAGIC &&
         record->version == SETTINGS_VERSION_LEGACY &&
         record->address >= MODBUS_ADDR_MIN &&
         record->address <= MODBUS_ADDR_MAX && record->baud_index < 4U &&
         record->parity <= 2U &&
         record->crc32 ==
             Crc32((const uint8_t *)record,
                   offsetof(SettingsRecordV1_t, crc32));
}

static bool RecordV2IsValid(const SettingsRecordV2_t *record) {
  return record->magic == SETTINGS_MAGIC &&
         record->version == SETTINGS_VERSION_V2 &&
         record->address >= MODBUS_ADDR_MIN &&
         record->address <= MODBUS_ADDR_MAX && record->baud_index < 4U &&
         record->parity <= 2U &&
         record->run_time_value >= RUN_TIME_VALUE_MIN &&
         record->run_time_value <= RUN_TIME_VALUE_MAX &&
         record->run_time_mode < RUN_TIME_MODE_COUNT &&
         record->crc32 ==
             Crc32((const uint8_t *)record,
                   offsetof(SettingsRecordV2_t, crc32));
}

static bool RecordV3IsValid(const SettingsRecordV3_t *record) {
  if (record->magic != SETTINGS_MAGIC ||
      record->version != SETTINGS_VERSION_V3 ||
      record->address < MODBUS_ADDR_MIN ||
      record->address > MODBUS_ADDR_MAX ||
      record->baud_index >= 4U || record->parity > 2U ||
      record->run_time_value < RUN_TIME_VALUE_MIN ||
      record->run_time_value > RUN_TIME_VALUE_MAX ||
      record->run_time_mode >= RUN_TIME_MODE_COUNT ||
      record->selected_frequency_band >= FREQ_BAND_COUNT ||
      record->crc32 != Crc32((const uint8_t *)record,
                             offsetof(SettingsRecordV3_t, crc32))) {
    return false;
  }
  for (uint8_t index = 0U; index < FREQ_BAND_COUNT; index++) {
    if (!BandFrequencyIsValid(index, record->band_frequency[index])) {
      return false;
    }
  }
  return true;
}

static bool RecordV4IsValid(const SettingsRecordV4_t *record) {
  if (record->magic != SETTINGS_MAGIC || record->version != SETTINGS_VERSION ||
      record->address < MODBUS_ADDR_MIN ||
      record->address > MODBUS_ADDR_MAX ||
      record->baud_index >= MODBUS_BAUD_INDEX_COUNT || record->parity > 2U ||
      record->run_time_value < RUN_TIME_VALUE_MIN ||
      record->run_time_value > RUN_TIME_VALUE_MAX ||
      record->run_time_mode >= RUN_TIME_MODE_COUNT ||
      record->selected_frequency_band >= FREQ_BAND_COUNT ||
      record->power_range_index > POWER_RANGE_INDEX_MAX ||
      record->rterm_enabled > 1U ||
      record->crc32 != Crc32((const uint8_t *)record,
                             offsetof(SettingsRecordV4_t, crc32))) {
    return false;
  }
  for (uint8_t index = 0U; index < FREQ_BAND_COUNT; index++) {
    if (!BandFrequencyIsValid(index, record->band_frequency[index])) {
      return false;
    }
  }
  return true;
}

static uint8_t LegacyBaudIndexToCurrent(uint8_t legacy_index) {
  /* v1~v3의 index 3은 115200이었다. v4부터 57600이 index 3에 추가됐다. */
  return (legacy_index == 3U) ? 4U : legacy_index;
}

static bool BandFrequencyIsValid(uint8_t index, uint16_t frequency) {
  static const uint16_t minimum[FREQ_BAND_COUNT] = {
      FREQ_BAND_28_MIN, FREQ_BAND_40_MIN, FREQ_BAND_68_MIN,
      FREQ_BAND_80_MIN};
  static const uint16_t maximum[FREQ_BAND_COUNT] = {
      FREQ_BAND_28_MAX, FREQ_BAND_40_MAX, FREQ_BAND_68_MAX,
      FREQ_BAND_80_MAX};
  return index < FREQ_BAND_COUNT && frequency >= minimum[index] &&
         frequency <= maximum[index];
}

static uintptr_t SettingsStart(void) {
  return (uintptr_t)&__settings_flash_start__;
}

static uintptr_t SettingsEnd(void) {
  return (uintptr_t)&__settings_flash_end__;
}
