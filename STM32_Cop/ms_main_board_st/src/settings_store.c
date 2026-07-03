/**
 * @file  settings_store.c
 * @brief 사용자 설정 FLASH 저장/복원
 *
 * STM32G474RB (128KB, DBANK=1) flash 구조 (RM0440 Rev9 Table 7):
 *   Bank1: 0x08000000 ~ 0x0800FFFF (32 page × 2KB)
 *   Bank2: 0x08040000 ~ 0x0804FFFF (NOT 0x08010000!)
 *   -> 0x08010000 ~ 0x0803FFFF 는 실제 flash 가 아닌 alias 영역.
 *      이 영역에 erase 를 시도하면 HAL 은 HAL_OK 를 반환하지만 실제로는
 *      페이지 번호 매핑이 어긋나 silent fail 이 발생함.
 *
 * 본 구현은 **Bank1 의 마지막 2 페이지** (0x0800F000, 0x0800F800) 를 사용한다.
 *   - 두 페이지를 ping-pong 으로 번갈아 erase + program
 *   - 페이지당 endurance 10000 cycles × 2 slot = 20000+ saves
 *     (1 회/일 저장 기준 50 년 이상)
 *   - app 코드는 linker 에서 60KB (0x08000000 ~ 0x0800EFFF) 로 제한됨
 *
 * 데이터 무결성: magic + version + payload_size + sequence + CRC32
 * 로드 시 더 높은 sequence 의 valid 이미지를 채택.
 */
#include "settings_store.h"
#include "params.h"
#include "stm32g4xx_hal.h"
#include <stddef.h>
#include <string.h>

#define SETTINGS_STORE_MAGIC       0x53544734UL   /* 'STG4' */
#define SETTINGS_STORE_VERSION     0x0006U
#define SETTINGS_STORE_V5_VERSION  0x0005U
#define SETTINGS_STORE_V4_VERSION  0x0004U
#define SETTINGS_STORE_V3_VERSION  0x0003U
#define SETTINGS_STORE_V2_VERSION  0x0002U
#define SETTINGS_STORE_MAX_RETRY   3U

/* 진단용 카운터 (SWD 로 직접 읽어 실패 지점 확인).
 *   [0] attempt      [1] last_phase   [2] unlock_status
 *   [3] erase_status [4] prog_status  [5] hal_error_code
 *   [6] page_error   [7] write_addr   [8] ok_count
 *   [9] fail_count   [10] retry_used  [11] last_seq
 *   [12] verify_off  [13] read_dw0    [14] expect_dw0
 *   [15] sentinel = 0xCAFEDB6 */
__attribute__((used)) volatile uint32_t g_save_dbg[16];

#define DBG_RESET_ALL_BUT_COUNTS()  do { \
    uint32_t _ok = g_save_dbg[8]; \
    uint32_t _fail = g_save_dbg[9]; \
    uint32_t _att = g_save_dbg[0]; \
    for (uint32_t _i = 0U; _i < 16U; _i++) g_save_dbg[_i] = 0U; \
    g_save_dbg[0] = _att; \
    g_save_dbg[8] = _ok; \
    g_save_dbg[9] = _fail; \
    g_save_dbg[15] = 0xCAFEDB6UL; \
} while (0)

/* ===== 저장 영역 (Bank 1 의 마지막 2 페이지) ===== */
#define SETTINGS_SLOT_A_ADDR  0x0800F000UL  /* Bank 1, page 30 */
#define SETTINGS_SLOT_B_ADDR  0x0800F800UL  /* Bank 1, page 31 */
#define SETTINGS_SLOT_BANK    FLASH_BANK_1
#define SETTINGS_SLOT_A_PAGE  30U
#define SETTINGS_SLOT_B_PAGE  31U

/* ===== 구버전 마이그레이션용 slot 들 (legacy, 읽기 전용) ===== */
static const uint32_t k_v3_slots[] = {
    0x0801F800UL,
    0x0801D000UL,
    0x0801D800UL,
};
static const uint32_t k_v2_slots[] = {
    0x0801C000UL, 0x0801C800UL,
    0x0801D000UL, 0x0801D800UL,
    0x0801E000UL, 0x0801E800UL,
    0x0801F000UL, 0x0801F800UL,
};

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    uint32_t sequence;
    uint32_t crc32;

    uint8_t selected_mode;
    uint8_t selected_freq_ch;
    uint8_t selected_power_ch;
    uint8_t modbus_addr;

    uint8_t modbus_baud_idx;
    uint8_t rs485_term_on;
    uint8_t rs485_parity;
    uint8_t reserved0;

    uint16_t freq_01khz[FREQ_EDIT_CH_COUNT];
    uint8_t  freq_l_step[FREQ_EDIT_CH_COUNT];
    uint8_t  reserved1[6];

    uint16_t power_low[POWER_8STEP_COUNT];
    uint16_t power_high[POWER_8STEP_COUNT];
    uint16_t power_def[POWER_8STEP_COUNT];
    uint16_t power_tuned_freq_01khz[POWER_8STEP_COUNT];
    uint16_t power_tuned_gate_duty_01pct[POWER_8STEP_COUNT];
    uint8_t  power_tuned_freq_ch[POWER_8STEP_COUNT];
    uint8_t  power_tuned_valid[POWER_8STEP_COUNT];
    int16_t  power_tuned_freq_offset_01khz[POWER_8STEP_COUNT][FREQ_EDIT_CH_COUNT];
    uint8_t  power_tuned_gate_duty_02pct[POWER_8STEP_COUNT][FREQ_EDIT_CH_COUNT];
    uint16_t power_tuned_valid_mask[POWER_8STEP_COUNT];

    uint16_t freq_base_voltage_01v[FREQ_EDIT_CH_COUNT];
} SettingsFlashImage_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    uint32_t sequence;
    uint32_t crc32;

    uint8_t selected_mode;
    uint8_t selected_freq_ch;
    uint8_t selected_power_ch;
    uint8_t modbus_addr;

    uint8_t modbus_baud_idx;
    uint8_t rs485_term_on;
    uint8_t rs485_parity;
    uint8_t reserved0;

    uint16_t freq_01khz[FREQ_EDIT_CH_COUNT];
    uint8_t  freq_l_step[FREQ_EDIT_CH_COUNT];
    uint8_t  reserved1[6];

    uint16_t power_low[POWER_8STEP_COUNT];
    uint16_t power_high[POWER_8STEP_COUNT];
    uint16_t power_def[POWER_8STEP_COUNT];
    uint16_t power_tuned_freq_01khz[POWER_8STEP_COUNT];
    uint16_t power_tuned_gate_duty_01pct[POWER_8STEP_COUNT];
    uint8_t  power_tuned_freq_ch[POWER_8STEP_COUNT];
    uint8_t  power_tuned_valid[POWER_8STEP_COUNT];

    uint16_t freq_base_voltage_01v[FREQ_EDIT_CH_COUNT];
} SettingsFlashImageV5_t;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    uint32_t sequence;
    uint32_t crc32;

    uint8_t selected_mode;
    uint8_t selected_freq_ch;
    uint8_t selected_power_ch;
    uint8_t modbus_addr;

    uint8_t modbus_baud_idx;
    uint8_t rs485_term_on;
    uint8_t rs485_parity;
    uint8_t reserved0;

    uint16_t freq_01khz[FREQ_EDIT_CH_COUNT];
    uint8_t  freq_l_step[FREQ_EDIT_CH_COUNT];
    uint8_t  reserved1[6];

    uint16_t power_low[POWER_8STEP_COUNT];
    uint16_t power_high[POWER_8STEP_COUNT];
    uint16_t power_def[POWER_8STEP_COUNT];

    uint16_t freq_base_voltage_01v[FREQ_EDIT_CH_COUNT];
} SettingsFlashImageV4_t;

_Static_assert((sizeof(SettingsFlashImage_t) % 8U) == 0U,
               "SettingsFlashImage_t size must be 8-byte aligned");
_Static_assert(sizeof(SettingsFlashImage_t) <= 2048U,
               "Image must fit in one 2KB flash page");
_Static_assert((sizeof(SettingsFlashImageV5_t) % 8U) == 0U,
               "SettingsFlashImageV5_t size must be 8-byte aligned");
_Static_assert((sizeof(SettingsFlashImageV4_t) % 8U) == 0U,
               "SettingsFlashImageV4_t size must be 8-byte aligned");

static uint32_t CalcCrc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    while (len-- > 0U) {
        crc ^= (uint32_t)(*data++);
        for (i = 0U; i < 8U; i++) {
            if ((crc & 1UL) != 0UL) {
                crc = (crc >> 1U) ^ 0xEDB88320UL;
            } else {
                crc >>= 1U;
            }
        }
    }
    return ~crc;
}

static uint32_t GetPayloadSize(void)
{
    return (uint32_t)(sizeof(SettingsFlashImage_t)
                      - offsetof(SettingsFlashImage_t, selected_mode));
}

static uint32_t GetPayloadSizeV4(void)
{
    return (uint32_t)(sizeof(SettingsFlashImageV4_t)
                      - offsetof(SettingsFlashImageV4_t, selected_mode));
}

static uint32_t GetPayloadSizeV5(void)
{
    return (uint32_t)(sizeof(SettingsFlashImageV5_t)
                      - offsetof(SettingsFlashImageV5_t, selected_mode));
}

static uint8_t IsImageValid(const SettingsFlashImage_t *img,
                            uint32_t payload_size,
                            uint16_t expect_version)
{
    const uint8_t *payload;
    uint32_t calc_crc;

    if (img->magic != SETTINGS_STORE_MAGIC) return 0U;
    if (img->version != expect_version) return 0U;
    if (img->payload_size != (uint16_t)payload_size) return 0U;

    payload = ((const uint8_t *)img) + offsetof(SettingsFlashImage_t, selected_mode);
    calc_crc = CalcCrc32(payload, payload_size);
    return (calc_crc == img->crc32) ? 1U : 0U;
}

static uint8_t IsImageValidV4(const SettingsFlashImageV4_t *img,
                              uint32_t payload_size,
                              uint16_t expect_version)
{
    const uint8_t *payload;
    uint32_t calc_crc;

    if (img->magic != SETTINGS_STORE_MAGIC) return 0U;
    if (img->version != expect_version) return 0U;
    if (img->payload_size != (uint16_t)payload_size) return 0U;

    payload = ((const uint8_t *)img) + offsetof(SettingsFlashImageV4_t, selected_mode);
    calc_crc = CalcCrc32(payload, payload_size);
    return (calc_crc == img->crc32) ? 1U : 0U;
}

static uint8_t IsImageValidV5(const SettingsFlashImageV5_t *img,
                              uint32_t payload_size,
                              uint16_t expect_version)
{
    const uint8_t *payload;
    uint32_t calc_crc;

    if (img->magic != SETTINGS_STORE_MAGIC) return 0U;
    if (img->version != expect_version) return 0U;
    if (img->payload_size != (uint16_t)payload_size) return 0U;

    payload = ((const uint8_t *)img) + offsetof(SettingsFlashImageV5_t, selected_mode);
    calc_crc = CalcCrc32(payload, payload_size);
    return (calc_crc == img->crc32) ? 1U : 0U;
}

static const SettingsFlashImageV4_t *FindBestInSlotsV4(const uint32_t *slots,
                                                       uint32_t slot_count,
                                                       uint32_t payload_size,
                                                       uint16_t expect_version)
{
    uint32_t i;
    const SettingsFlashImageV4_t *best = NULL;
    uint32_t best_seq = 0U;

    for (i = 0U; i < slot_count; i++) {
        const SettingsFlashImageV4_t *img =
            (const SettingsFlashImageV4_t *)slots[i];
        if (IsImageValidV4(img, payload_size, expect_version) == 0U) {
            continue;
        }
        if ((best == NULL) || (img->sequence > best_seq)) {
            best = img;
            best_seq = img->sequence;
        }
    }
    return best;
}

static uint8_t PackGateDuty02Pct(uint16_t duty_01pct)
{
    if (duty_01pct < RUN_GATE_DUTY_TUNE_MIN_01PCT) {
        duty_01pct = RUN_GATE_DUTY_TUNE_MIN_01PCT;
    }
    if (duty_01pct > RUN_GATE_DUTY_TUNE_MAX_01PCT) {
        duty_01pct = RUN_GATE_DUTY_TUNE_MAX_01PCT;
    }
    return (uint8_t)((duty_01pct - RUN_GATE_DUTY_TUNE_MIN_01PCT + 1U) / 2U);
}

static uint8_t IsPageBlank(uint32_t address, uint32_t bytes)
{
    const uint32_t *p = (const uint32_t *)address;
    uint32_t n = bytes / sizeof(uint32_t);
    uint32_t i;
    for (i = 0U; i < n; i++) {
        if (p[i] != 0xFFFFFFFFUL) return 0U;
    }
    return 1U;
}

static void FlashInvalidateCaches(void)
{
    if (READ_BIT(FLASH->ACR, FLASH_ACR_ICEN) != 0U) {
        __HAL_FLASH_INSTRUCTION_CACHE_DISABLE();
        __HAL_FLASH_INSTRUCTION_CACHE_RESET();
        __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
    }
    if (READ_BIT(FLASH->ACR, FLASH_ACR_DCEN) != 0U) {
        __HAL_FLASH_DATA_CACHE_DISABLE();
        __HAL_FLASH_DATA_CACHE_RESET();
        __HAL_FLASH_DATA_CACHE_ENABLE();
    }
}

/* Bank 1 의 단일 페이지 erase.
 * Bank 1 은 실제 flash 이므로 HAL_FLASHEx_Erase 가 정상 동작한다. */
static uint8_t EraseSettingsPage(uint8_t page_number)
{
    FLASH_EraseInitTypeDef ei;
    uint32_t page_err = 0xFFFFFFFFUL;
    HAL_StatusTypeDef st;

    memset(&ei, 0, sizeof(ei));
    ei.TypeErase = FLASH_TYPEERASE_PAGES;
    ei.Banks     = SETTINGS_SLOT_BANK;
    ei.Page      = (uint32_t)page_number;
    ei.NbPages   = 1U;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    st = HAL_FLASHEx_Erase(&ei, &page_err);

    g_save_dbg[3] = (uint32_t)st;
    g_save_dbg[6] = page_err;

    if (st != HAL_OK) return 0U;
    return 1U;
}

static uint8_t ProgramImage(uint32_t address, const SettingsFlashImage_t *img)
{
    uint32_t i;
    uint32_t dword_count = (uint32_t)(sizeof(*img) / sizeof(uint64_t));
    HAL_StatusTypeDef st;

    for (i = 0U; i < dword_count; i++) {
        uint64_t dword;
        memcpy(&dword, (const uint8_t *)img + (i * sizeof(uint64_t)),
               sizeof(uint64_t));
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                               address + (i * sizeof(uint64_t)),
                               dword);
        if (st != HAL_OK) {
            g_save_dbg[4] = (uint32_t)st;
            g_save_dbg[5] = HAL_FLASH_GetError();
            g_save_dbg[12] = i * (uint32_t)sizeof(uint64_t);
            return 0U;
        }
    }
    g_save_dbg[4] = (uint32_t)HAL_OK;
    __DSB();
    __ISB();
    return 1U;
}

static uint8_t WriteImageToSlot(uint32_t address,
                                uint8_t page_number,
                                const SettingsFlashImage_t *img,
                                uint32_t payload_size)
{
    uint32_t retry;

    g_save_dbg[7] = address;

    if (IsImageValid(img, payload_size, SETTINGS_STORE_VERSION) == 0U) {
        g_save_dbg[1] = 0x20U;
        return 0U;
    }

    for (retry = 0U; retry < SETTINGS_STORE_MAX_RETRY; retry++) {
        g_save_dbg[10] = retry;
        g_save_dbg[1]  = 0x30U;

        if (EraseSettingsPage(page_number) == 0U) {
            g_save_dbg[1] = 0x31U;
            continue;
        }
        g_save_dbg[1] = 0x40U;

        FlashInvalidateCaches();
        __DSB();
        __ISB();
        if (IsPageBlank(address, 2048U) == 0U) {
            g_save_dbg[1] = 0x32U;
            continue;
        }

        if (ProgramImage(address, img) == 0U) {
            g_save_dbg[1] = 0x41U;
            continue;
        }
        g_save_dbg[1] = 0x50U;

        FlashInvalidateCaches();
        __DSB();
        __ISB();

        if (memcmp((const void *)address, img, sizeof(*img)) != 0) {
            uint32_t off;
            const uint8_t *fl = (const uint8_t *)address;
            const uint8_t *rm = (const uint8_t *)img;
            for (off = 0U; off < sizeof(*img); off++) {
                if (fl[off] != rm[off]) {
                    g_save_dbg[12] = off;
                    g_save_dbg[13] = *(const uint32_t *)(address + (off & ~3U));
                    g_save_dbg[14] = *(const uint32_t *)(((const uint8_t *)img) + (off & ~3U));
                    break;
                }
            }
            g_save_dbg[1] = 0x51U;
            continue;
        }
        if (IsImageValid((const SettingsFlashImage_t *)address,
                         payload_size,
                         SETTINGS_STORE_VERSION) == 0U) {
            g_save_dbg[1] = 0x52U;
            continue;
        }
        g_save_dbg[1] = 0x60U;
        return 1U;
    }
    return 0U;
}

static void BuildImageFromData(const SettingsStoreData_t *in_data,
                               SettingsFlashImage_t *img,
                               uint32_t payload_size,
                               uint32_t sequence)
{
    uint32_t payload_offset;
    uint32_t i;
    uint32_t j;

    payload_offset = (uint32_t)offsetof(SettingsFlashImage_t, selected_mode);

    memset(img, 0xFF, sizeof(*img));
    img->magic        = SETTINGS_STORE_MAGIC;
    img->version      = SETTINGS_STORE_VERSION;
    img->payload_size = (uint16_t)payload_size;
    img->sequence     = sequence;

    img->selected_mode     = (uint8_t)in_data->selected_mode;
    img->selected_freq_ch  = in_data->selected_freq_ch;
    img->selected_power_ch = in_data->selected_power_ch;
    img->modbus_addr       = in_data->modbus_addr;
    img->modbus_baud_idx   = in_data->modbus_baud_idx;
    img->rs485_term_on     = in_data->rs485_term_on;
    img->rs485_parity      = in_data->rs485_parity;

    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        img->freq_01khz[i]  = in_data->freq_profiles[i].freq_01khz;
        img->freq_l_step[i] = in_data->freq_profiles[i].l_step;
        img->freq_base_voltage_01v[i] = in_data->freq_profiles[i].base_voltage_01v;
    }
    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        img->power_low[i]  = in_data->power_profiles[i].low_01w;
        img->power_high[i] = in_data->power_profiles[i].high_01w;
        img->power_def[i]  = in_data->power_profiles[i].def_01w;
        img->power_tuned_freq_01khz[i] =
            in_data->power_profiles[i].tuned_freq_01khz;
        img->power_tuned_gate_duty_01pct[i] =
            in_data->power_profiles[i].tuned_gate_duty_01pct;
        img->power_tuned_freq_ch[i] =
            in_data->power_profiles[i].tuned_freq_ch;
        img->power_tuned_valid[i] =
            in_data->power_profiles[i].tuned_valid;
        img->power_tuned_valid_mask[i] =
            in_data->power_profiles[i].tuned_valid_mask;
        for (j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            img->power_tuned_freq_offset_01khz[i][j] =
                in_data->power_profiles[i].tuned_freq_offset_01khz[j];
            img->power_tuned_gate_duty_02pct[i][j] =
                in_data->power_profiles[i].tuned_gate_duty_02pct[j];
        }
    }

    img->crc32 = CalcCrc32(((const uint8_t *)img) + payload_offset, payload_size);
}

static void CopyImageToStoreData(const SettingsFlashImage_t *img,
                                 SettingsStoreData_t *out_data)
{
    uint8_t i;
    uint8_t j;
    out_data->selected_mode     = (OperatingMode_t)img->selected_mode;
    out_data->selected_freq_ch  = img->selected_freq_ch;
    out_data->selected_power_ch = img->selected_power_ch;
    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        out_data->freq_profiles[i].freq_01khz = img->freq_01khz[i];
        out_data->freq_profiles[i].l_step     = img->freq_l_step[i];
        out_data->freq_profiles[i].base_voltage_01v = img->freq_base_voltage_01v[i];
    }
    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        out_data->power_profiles[i].low_01w  = img->power_low[i];
        out_data->power_profiles[i].high_01w = img->power_high[i];
        out_data->power_profiles[i].def_01w  = img->power_def[i];
        out_data->power_profiles[i].tuned_freq_01khz =
            img->power_tuned_freq_01khz[i];
        out_data->power_profiles[i].tuned_gate_duty_01pct =
            img->power_tuned_gate_duty_01pct[i];
        out_data->power_profiles[i].tuned_freq_ch =
            img->power_tuned_freq_ch[i];
        out_data->power_profiles[i].tuned_valid =
            img->power_tuned_valid[i];
        out_data->power_profiles[i].tuned_valid_mask =
            (uint16_t)(img->power_tuned_valid_mask[i] & ((1U << FREQ_EDIT_CH_COUNT) - 1U));
        for (j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            out_data->power_profiles[i].tuned_freq_offset_01khz[j] =
                img->power_tuned_freq_offset_01khz[i][j];
            out_data->power_profiles[i].tuned_gate_duty_02pct[j] =
                img->power_tuned_gate_duty_02pct[i][j];
        }
    }
    out_data->modbus_addr     = img->modbus_addr;
    out_data->modbus_baud_idx = img->modbus_baud_idx;
    out_data->rs485_term_on   = img->rs485_term_on;
    out_data->rs485_parity    = img->rs485_parity;
}

static void CopyImageV5ToStoreData(const SettingsFlashImageV5_t *img,
                                   SettingsStoreData_t *out_data)
{
    uint8_t i;
    uint8_t j;

    out_data->selected_mode     = (OperatingMode_t)img->selected_mode;
    out_data->selected_freq_ch  = img->selected_freq_ch;
    out_data->selected_power_ch = img->selected_power_ch;
    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        out_data->freq_profiles[i].freq_01khz = img->freq_01khz[i];
        out_data->freq_profiles[i].l_step     = img->freq_l_step[i];
        out_data->freq_profiles[i].base_voltage_01v = img->freq_base_voltage_01v[i];
    }
    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        out_data->power_profiles[i].low_01w  = img->power_low[i];
        out_data->power_profiles[i].high_01w = img->power_high[i];
        out_data->power_profiles[i].def_01w  = img->power_def[i];
        out_data->power_profiles[i].tuned_freq_01khz =
            img->power_tuned_freq_01khz[i];
        out_data->power_profiles[i].tuned_gate_duty_01pct =
            img->power_tuned_gate_duty_01pct[i];
        out_data->power_profiles[i].tuned_freq_ch =
            img->power_tuned_freq_ch[i];
        out_data->power_profiles[i].tuned_valid =
            img->power_tuned_valid[i];
        out_data->power_profiles[i].tuned_valid_mask = 0U;
        for (j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            out_data->power_profiles[i].tuned_freq_offset_01khz[j] = 0;
            out_data->power_profiles[i].tuned_gate_duty_02pct[j] = 0U;
        }
        if ((img->power_tuned_valid[i] != 0U)
            && (img->power_tuned_freq_ch[i] < FREQ_EDIT_CH_COUNT)
            && (img->power_tuned_gate_duty_01pct[i] >= RUN_GATE_DUTY_TUNE_MIN_01PCT)
            && (img->power_tuned_gate_duty_01pct[i] <= RUN_GATE_DUTY_TUNE_MAX_01PCT)) {
            uint8_t ch = img->power_tuned_freq_ch[i];
            int32_t offset = (int32_t)img->power_tuned_freq_01khz[i]
                           - (int32_t)img->freq_01khz[ch];

            if (offset < -32768L) offset = -32768L;
            if (offset > 32767L) offset = 32767L;
            out_data->power_profiles[i].tuned_freq_offset_01khz[ch] = (int16_t)offset;
            out_data->power_profiles[i].tuned_gate_duty_02pct[ch] =
                PackGateDuty02Pct(img->power_tuned_gate_duty_01pct[i]);
            out_data->power_profiles[i].tuned_valid_mask = (uint16_t)(1U << ch);
        }
    }
    out_data->modbus_addr     = img->modbus_addr;
    out_data->modbus_baud_idx = img->modbus_baud_idx;
    out_data->rs485_term_on   = img->rs485_term_on;
    out_data->rs485_parity    = img->rs485_parity;
}

static void CopyImageV4ToStoreData(const SettingsFlashImageV4_t *img,
                                   SettingsStoreData_t *out_data)
{
    uint8_t i;
    uint8_t j;
    out_data->selected_mode     = (OperatingMode_t)img->selected_mode;
    out_data->selected_freq_ch  = img->selected_freq_ch;
    out_data->selected_power_ch = img->selected_power_ch;
    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        out_data->freq_profiles[i].freq_01khz = img->freq_01khz[i];
        out_data->freq_profiles[i].l_step     = img->freq_l_step[i];
        out_data->freq_profiles[i].base_voltage_01v = img->freq_base_voltage_01v[i];
    }
    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        out_data->power_profiles[i].low_01w  = img->power_low[i];
        out_data->power_profiles[i].high_01w = img->power_high[i];
        out_data->power_profiles[i].def_01w  = img->power_def[i];
        out_data->power_profiles[i].tuned_freq_01khz = 0U;
        out_data->power_profiles[i].tuned_gate_duty_01pct = 0U;
        out_data->power_profiles[i].tuned_freq_ch = 0U;
        out_data->power_profiles[i].tuned_valid = 0U;
        out_data->power_profiles[i].tuned_valid_mask = 0U;
        for (j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            out_data->power_profiles[i].tuned_freq_offset_01khz[j] = 0;
            out_data->power_profiles[i].tuned_gate_duty_02pct[j] = 0U;
        }
    }
    out_data->modbus_addr     = img->modbus_addr;
    out_data->modbus_baud_idx = img->modbus_baud_idx;
    out_data->rs485_term_on   = img->rs485_term_on;
    out_data->rs485_parity    = img->rs485_parity;
}

static uint8_t IsSameStoreData(const SettingsStoreData_t *a,
                               const SettingsStoreData_t *b)
{
    uint8_t i;
    uint8_t j;
    if (a->selected_mode != b->selected_mode) return 0U;
    if (a->selected_freq_ch != b->selected_freq_ch) return 0U;
    if (a->selected_power_ch != b->selected_power_ch) return 0U;
    for (i = 0U; i < FREQ_EDIT_CH_COUNT; i++) {
        if (a->freq_profiles[i].freq_01khz != b->freq_profiles[i].freq_01khz) return 0U;
        if (a->freq_profiles[i].l_step != b->freq_profiles[i].l_step) return 0U;
        if (a->freq_profiles[i].base_voltage_01v != b->freq_profiles[i].base_voltage_01v) return 0U;
    }
    for (i = 0U; i < POWER_8STEP_COUNT; i++) {
        if (a->power_profiles[i].low_01w  != b->power_profiles[i].low_01w)  return 0U;
        if (a->power_profiles[i].high_01w != b->power_profiles[i].high_01w) return 0U;
        if (a->power_profiles[i].def_01w  != b->power_profiles[i].def_01w)  return 0U;
        if (a->power_profiles[i].tuned_freq_01khz
            != b->power_profiles[i].tuned_freq_01khz) return 0U;
        if (a->power_profiles[i].tuned_gate_duty_01pct
            != b->power_profiles[i].tuned_gate_duty_01pct) return 0U;
        if (a->power_profiles[i].tuned_freq_ch
            != b->power_profiles[i].tuned_freq_ch) return 0U;
        if (a->power_profiles[i].tuned_valid
            != b->power_profiles[i].tuned_valid) return 0U;
        if (a->power_profiles[i].tuned_valid_mask
            != b->power_profiles[i].tuned_valid_mask) return 0U;
        for (j = 0U; j < FREQ_EDIT_CH_COUNT; j++) {
            if (a->power_profiles[i].tuned_freq_offset_01khz[j]
                != b->power_profiles[i].tuned_freq_offset_01khz[j]) return 0U;
            if (a->power_profiles[i].tuned_gate_duty_02pct[j]
                != b->power_profiles[i].tuned_gate_duty_02pct[j]) return 0U;
        }
    }
    if (a->modbus_addr     != b->modbus_addr)     return 0U;
    if (a->modbus_baud_idx != b->modbus_baud_idx) return 0U;
    if (a->rs485_term_on   != b->rs485_term_on)   return 0U;
    if (a->rs485_parity    != b->rs485_parity)    return 0U;
    return 1U;
}

/* 두 ping-pong slot 중 더 높은 sequence 의 valid 이미지 선택.
 * out_other_addr 에는 (있으면) 사용되지 않은 slot 주소, 없으면 SLOT_A 반환. */
static const SettingsFlashImage_t *
FindBestPingPong(uint32_t payload_size,
                 uint32_t *out_best_addr,
                 uint32_t *out_other_addr,
                 uint32_t *out_best_seq)
{
    const SettingsFlashImage_t *img_a = (const SettingsFlashImage_t *)SETTINGS_SLOT_A_ADDR;
    const SettingsFlashImage_t *img_b = (const SettingsFlashImage_t *)SETTINGS_SLOT_B_ADDR;
    uint8_t va = IsImageValid(img_a, payload_size, SETTINGS_STORE_VERSION);
    uint8_t vb = IsImageValid(img_b, payload_size, SETTINGS_STORE_VERSION);

    if (va && vb) {
        if (img_a->sequence >= img_b->sequence) {
            if (out_best_addr)  *out_best_addr  = SETTINGS_SLOT_A_ADDR;
            if (out_other_addr) *out_other_addr = SETTINGS_SLOT_B_ADDR;
            if (out_best_seq)   *out_best_seq   = img_a->sequence;
            return img_a;
        } else {
            if (out_best_addr)  *out_best_addr  = SETTINGS_SLOT_B_ADDR;
            if (out_other_addr) *out_other_addr = SETTINGS_SLOT_A_ADDR;
            if (out_best_seq)   *out_best_seq   = img_b->sequence;
            return img_b;
        }
    } else if (va) {
        if (out_best_addr)  *out_best_addr  = SETTINGS_SLOT_A_ADDR;
        if (out_other_addr) *out_other_addr = SETTINGS_SLOT_B_ADDR;
        if (out_best_seq)   *out_best_seq   = img_a->sequence;
        return img_a;
    } else if (vb) {
        if (out_best_addr)  *out_best_addr  = SETTINGS_SLOT_B_ADDR;
        if (out_other_addr) *out_other_addr = SETTINGS_SLOT_A_ADDR;
        if (out_best_seq)   *out_best_seq   = img_b->sequence;
        return img_b;
    }
    if (out_best_addr)  *out_best_addr  = 0U;
    if (out_other_addr) *out_other_addr = SETTINGS_SLOT_A_ADDR;
    if (out_best_seq)   *out_best_seq   = 0U;
    return NULL;
}

static const SettingsFlashImageV4_t *
FindBestPingPongV4(uint32_t payload_size)
{
    const SettingsFlashImageV4_t *img_a = (const SettingsFlashImageV4_t *)SETTINGS_SLOT_A_ADDR;
    const SettingsFlashImageV4_t *img_b = (const SettingsFlashImageV4_t *)SETTINGS_SLOT_B_ADDR;
    uint8_t va = IsImageValidV4(img_a, payload_size, SETTINGS_STORE_V4_VERSION);
    uint8_t vb = IsImageValidV4(img_b, payload_size, SETTINGS_STORE_V4_VERSION);

    if (va && vb) {
        return (img_a->sequence >= img_b->sequence) ? img_a : img_b;
    }
    if (va) return img_a;
    if (vb) return img_b;
    return NULL;
}

static const SettingsFlashImageV5_t *
FindBestPingPongV5(uint32_t payload_size)
{
    const SettingsFlashImageV5_t *img_a = (const SettingsFlashImageV5_t *)SETTINGS_SLOT_A_ADDR;
    const SettingsFlashImageV5_t *img_b = (const SettingsFlashImageV5_t *)SETTINGS_SLOT_B_ADDR;
    uint8_t va = IsImageValidV5(img_a, payload_size, SETTINGS_STORE_V5_VERSION);
    uint8_t vb = IsImageValidV5(img_b, payload_size, SETTINGS_STORE_V5_VERSION);

    if (va && vb) {
        return (img_a->sequence >= img_b->sequence) ? img_a : img_b;
    }
    if (va) return img_a;
    if (vb) return img_b;
    return NULL;
}

uint8_t SettingsStore_Load(SettingsStoreData_t *out_data)
{
    const SettingsFlashImage_t *img;
    const SettingsFlashImageV5_t *img_v5;
    const SettingsFlashImageV4_t *img_v4;
    uint32_t payload_size;
    uint32_t payload_size_v5;
    uint32_t payload_size_v4;

    if (out_data == NULL) return 0U;
    payload_size = GetPayloadSize();
    payload_size_v5 = GetPayloadSizeV5();
    payload_size_v4 = GetPayloadSizeV4();

    img = FindBestPingPong(payload_size, NULL, NULL, NULL);
    if (img != NULL) {
        CopyImageToStoreData(img, out_data);
        return 1U;
    }

    img_v5 = FindBestPingPongV5(payload_size_v5);
    if (img_v5 != NULL) {
        CopyImageV5ToStoreData(img_v5, out_data);
        return 1U;
    }

    img_v4 = FindBestPingPongV4(payload_size_v4);
    if (img_v4 != NULL) {
        CopyImageV4ToStoreData(img_v4, out_data);
        return 1U;
    }

    /* fallback : 구 v3 / v2 슬롯 1 회 마이그레이션 로드 */
    img_v4 = FindBestInSlotsV4(k_v3_slots,
                               (uint32_t)(sizeof(k_v3_slots) / sizeof(k_v3_slots[0])),
                               payload_size_v4,
                               SETTINGS_STORE_V3_VERSION);
    if (img_v4 != NULL) {
        CopyImageV4ToStoreData(img_v4, out_data);
        return 1U;
    }

    img_v4 = FindBestInSlotsV4(k_v2_slots,
                               (uint32_t)(sizeof(k_v2_slots) / sizeof(k_v2_slots[0])),
                               payload_size_v4,
                               SETTINGS_STORE_V2_VERSION);
    if (img_v4 != NULL) {
        CopyImageV4ToStoreData(img_v4, out_data);
        return 1U;
    }

    return 0U;
}

uint8_t SettingsStore_Save(const SettingsStoreData_t *in_data)
{
    SettingsFlashImage_t img __attribute__((aligned(8)));
    SettingsStoreData_t prev_data;
    const SettingsFlashImage_t *prev_img;
    uint32_t payload_size;
    uint32_t cur_addr   = 0U;
    uint32_t write_addr = 0U;
    uint32_t cur_seq    = 0U;
    uint32_t next_seq;
    uint8_t  write_page;
    uint8_t  save_ok = 0U;

    if (in_data == NULL) return 0U;
    payload_size = GetPayloadSize();

    prev_img = FindBestPingPong(payload_size, &cur_addr, &write_addr, &cur_seq);
    if (prev_img != NULL) {
        CopyImageToStoreData(prev_img, &prev_data);
        if (IsSameStoreData(in_data, &prev_data) != 0U) {
            return 1U;
        }
    } else {
        write_addr = SETTINGS_SLOT_A_ADDR;
    }

    next_seq   = cur_seq + 1UL;
    write_page = (write_addr == SETTINGS_SLOT_A_ADDR) ?
                 SETTINGS_SLOT_A_PAGE : SETTINGS_SLOT_B_PAGE;

    DBG_RESET_ALL_BUT_COUNTS();
    g_save_dbg[0]++;
    g_save_dbg[11] = next_seq;
    g_save_dbg[7]  = write_addr;
    g_save_dbg[1]  = 0x10U;

    BuildImageFromData(in_data, &img, payload_size, next_seq);

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    {
        HAL_StatusTypeDef ust = HAL_FLASH_Unlock();
        g_save_dbg[2] = (uint32_t)ust;
        if (ust != HAL_OK) {
            g_save_dbg[1] = 0x11U;
            g_save_dbg[5] = HAL_FLASH_GetError();
            g_save_dbg[9]++;
            return 0U;
        }
    }
    g_save_dbg[1] = 0x12U;

    if (WriteImageToSlot(write_addr, write_page, &img, payload_size) != 0U) {
        save_ok = 1U;
    }

    (void)HAL_FLASH_Lock();

    if (save_ok != 0U) {
        g_save_dbg[8]++;
    } else {
        g_save_dbg[9]++;
    }
    return save_ok;
}
