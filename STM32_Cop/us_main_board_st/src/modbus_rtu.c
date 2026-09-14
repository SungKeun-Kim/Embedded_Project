/**
 * @file  modbus_rtu.c
 * @brief Modbus RTU Server — 프레임 수신/검증/응답
 *
 * USART2 RXNE 인터럽트로 바이트를 수신하고 TIM4의 1 us Counter로
 * t1.5/t3.5를 검사한다. 지원 Function은 FC03, FC06, FC10이다.
 */
#include "modbus_rtu.h"

#include "config.h"
#include "modbus_crc.h"
#include "modbus_regs.h"
#include "params.h"
#include "settings_storage.h"

UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim4;

volatile ModbusConfig_t g_modbus_cfg;

/* ISR이 채우는 수신 Buffer와 상태 */
static volatile uint8_t s_rx_buf[MODBUS_RX_BUF_SIZE];
static volatile uint16_t s_rx_len;
static volatile uint8_t s_frame_ready;
static volatile uint8_t s_frame_invalid;

/* Main loop에서 처리하는 Snapshot과 송신 Buffer */
static uint8_t s_frame_buf[MODBUS_RX_BUF_SIZE];
static uint8_t s_tx_buf[MODBUS_TX_BUF_SIZE];
static bool s_request_is_broadcast;

/* 현재 UART 설정에 맞춘 RTU Timing */
static uint32_t s_t15_us;
static uint32_t s_t35_us;
static uint32_t s_last_valid_frame_tick;
static bool s_communication_seen;

/* ---- 내부 함수 전방 선언 ---- */
static void USART2_Init(void);
static void TIM4_Init(void);
static void UpdateRtuTiming(void);
static void TIM4_Restart(void);
static void TIM4_Stop(void);
static void ResetRxState(void);
static void SendResponse(const uint8_t *data, uint16_t len);
static void SendException(uint8_t func, uint8_t exc_code);
static void HandleFC03(const uint8_t *frame, uint16_t len);
static void HandleFC06(const uint8_t *frame, uint16_t len);
static void HandleFC10(const uint8_t *frame, uint16_t len);
static uint8_t ResultToException(ModbusRegResult_t result);

void Modbus_Init(void) {
  SettingsStorageData_t saved;

  g_modbus_cfg.address = MODBUS_ADDR_DEFAULT;
  g_modbus_cfg.baudrate = MODBUS_BAUD_DEFAULT;
  g_modbus_cfg.parity = MODBUS_PARITY_DEFAULT;

  if (SettingsStorage_Load(&saved)) {
    g_modbus_cfg.address = saved.address;
    g_modbus_cfg.baudrate = MODBUS_BAUD_TABLE[saved.baud_index];
  }

  ResetRxState();
  s_request_is_broadcast = false;
  s_last_valid_frame_tick = 0U;
  s_communication_seen = false;

  USART2_Init();
  TIM4_Init();
  UpdateRtuTiming();

  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

void Modbus_Process(void) {
  if (s_frame_ready == 0U) {
    return;
  }

  /* ISR Buffer를 짧은 Critical Section에서 Snapshot으로 옮긴다. */
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  const uint16_t len = s_rx_len;
  const bool frame_invalid = (s_frame_invalid != 0U);
  for (uint16_t i = 0U; i < len && i < MODBUS_RX_BUF_SIZE; i++) {
    s_frame_buf[i] = s_rx_buf[i];
  }
  ResetRxState();
  if (primask == 0U) {
    __enable_irq();
  }

  if (frame_invalid || len < 4U || len > MODBUS_RX_BUF_SIZE) {
    return;
  }

  const uint8_t request_address = s_frame_buf[0];
  if (request_address != g_modbus_cfg.address && request_address != 0U) {
    return;
  }

  const uint16_t crc_recv =
      ((uint16_t)s_frame_buf[len - 1U] << 8) | s_frame_buf[len - 2U];
  const uint16_t crc_calc = Modbus_CRC16(s_frame_buf, len - 2U);
  if (crc_recv != crc_calc) {
    return;
  }

  /* 주소와 CRC가 정상인 요청을 받은 시점을 LCD 통신 표시용으로 기억한다. */
  s_last_valid_frame_tick = HAL_GetTick();
  s_communication_seen = true;

  s_request_is_broadcast = (request_address == 0U);
  const uint8_t func = s_frame_buf[1];

  /* Broadcast Read는 처리하지 않고 응답도 보내지 않는다. */
  if (s_request_is_broadcast && func != 0x06U && func != 0x10U) {
    s_request_is_broadcast = false;
    return;
  }

  switch (func) {
  case 0x03U:
    HandleFC03(s_frame_buf, len);
    break;
  case 0x06U:
    HandleFC06(s_frame_buf, len);
    break;
  case 0x10U:
    HandleFC10(s_frame_buf, len);
    break;
  default:
    SendException(func, 0x01U);
    break;
  }
  s_request_is_broadcast = false;
}

void Modbus_UART_RxCallback(uint8_t byte) {
  if (s_frame_ready != 0U) {
    /* Main loop가 이전 Frame을 소비하기 전에 새 Frame이 시작됨 */
    s_frame_invalid = 1U;
    return;
  }

  if (s_rx_len > 0U) {
    const uint32_t inter_char_us = __HAL_TIM_GET_COUNTER(&htim4);
    if (inter_char_us > s_t15_us) {
      s_frame_invalid = 1U;
    }
  }

  if (s_rx_len < MODBUS_RX_BUF_SIZE) {
    s_rx_buf[s_rx_len++] = byte;
  } else {
    s_frame_invalid = 1U;
  }

  TIM4_Restart();
}

void Modbus_FrameTimeoutCallback(void) {
  TIM4_Stop();
  if (s_rx_len > 0U) {
    s_frame_ready = 1U;
  }
}

void Modbus_ReconfigUART(void) {
  TIM4_Stop();
  ResetRxState();
  HAL_UART_DeInit(&huart2);
  USART2_Init();
  UpdateRtuTiming();
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

bool Modbus_IsCommunicationActive(void) {
  return s_communication_seen &&
         (HAL_GetTick() - s_last_valid_frame_tick) <=
             MODBUS_COMM_DISPLAY_HOLD_MS;
}

uint8_t Modbus_GetBaudIndex(void) {
  for (uint8_t i = 0U; i < MODBUS_BAUD_INDEX_COUNT; i++) {
    if (MODBUS_BAUD_TABLE[i] == g_modbus_cfg.baudrate) {
      return i;
    }
  }
  return MODBUS_BAUD_INDEX_DEFAULT; /* 방어적 기본값: 9600 bps */
}

bool Modbus_ApplyLocalConfig(uint8_t address, uint8_t baud_index,
                            uint8_t parity) {
  if (address < MODBUS_ADDR_MIN || address > MODBUS_SUPERVISOR_ADDR_MAX ||
      baud_index >= MODBUS_BAUD_INDEX_COUNT ||
      parity != MODBUS_PARITY_DEFAULT) {
    return false;
  }

  const uint32_t baudrate = MODBUS_BAUD_TABLE[baud_index];
  if (address == g_modbus_cfg.address &&
      baudrate == g_modbus_cfg.baudrate && parity == g_modbus_cfg.parity) {
    return true;
  }

  SettingsStorageData_t settings = {0};
  if (!SettingsStorage_Load(&settings)) {
    SettingsStorage_SetDefaults(&settings);
  }
  settings.address = address;
  settings.baud_index = baud_index;
  settings.parity = parity;
  if (!SettingsStorage_Save(&settings)) {
    return false;
  }

  g_modbus_cfg.address = address;
  g_modbus_cfg.baudrate = baudrate;
  g_modbus_cfg.parity = parity;
  Modbus_ReconfigUART();
  return true;
}

static void USART2_Init(void) {
  __HAL_RCC_USART2_CLK_ENABLE();

  huart2.Instance = MODBUS_USART;
  huart2.Init.BaudRate = g_modbus_cfg.baudrate;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  switch (g_modbus_cfg.parity) {
  case 1U: /* Even: 8 data + parity + 1 stop */
    huart2.Init.WordLength = UART_WORDLENGTH_9B;
    huart2.Init.Parity = UART_PARITY_EVEN;
    huart2.Init.StopBits = UART_STOPBITS_1;
    break;
  case 2U: /* Odd: 8 data + parity + 1 stop */
    huart2.Init.WordLength = UART_WORDLENGTH_9B;
    huart2.Init.Parity = UART_PARITY_ODD;
    huart2.Init.StopBits = UART_STOPBITS_1;
    break;
  default: /* None: 8 data + 2 stop */
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.StopBits = UART_STOPBITS_2;
    break;
  }

  HAL_UART_Init(&huart2);
  HAL_NVIC_SetPriority(USART2_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void TIM4_Init(void) {
  __HAL_RCC_TIM4_CLK_ENABLE();

  /* TIM4 clock 170 MHz, PSC=169 → 1 MHz(1 us/tick) */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 169U;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = MODBUS_T35_FIXED_US;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

  HAL_TIM_Base_Init(&htim4);
  HAL_NVIC_SetPriority(TIM4_IRQn, 2U, 0U);
  HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

static void UpdateRtuTiming(void) {
  if (g_modbus_cfg.baudrate > 19200U) {
    s_t15_us = MODBUS_T15_FIXED_US;
    s_t35_us = MODBUS_T35_FIXED_US;
  } else {
    /* 모든 지원 형식은 Start/Data/Parity-or-extra-Stop/Stop = 11 bits */
    const uint32_t char_us =
        (11000000U + g_modbus_cfg.baudrate - 1U) / g_modbus_cfg.baudrate;
    s_t15_us = (char_us * 15U + 9U) / 10U;
    s_t35_us = (char_us * 35U + 9U) / 10U;
  }
}

static void TIM4_Restart(void) {
  __HAL_TIM_SET_AUTORELOAD(&htim4, s_t35_us);
  __HAL_TIM_SET_COUNTER(&htim4, 0U);
  __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);
  __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
  HAL_TIM_Base_Start(&htim4);
}

static void TIM4_Stop(void) {
  HAL_TIM_Base_Stop(&htim4);
  __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);
}

static void ResetRxState(void) {
  s_rx_len = 0U;
  s_frame_ready = 0U;
  s_frame_invalid = 0U;
}

static void SendResponse(const uint8_t *data, uint16_t len) {
  if (s_request_is_broadcast) {
    return;
  }

  MODBUS_DE_TX();
  HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100U);
  while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) {
  }
  MODBUS_DE_RX();
}

static void SendException(uint8_t func, uint8_t exc_code) {
  if (s_request_is_broadcast) {
    return;
  }

  s_tx_buf[0] = g_modbus_cfg.address;
  s_tx_buf[1] = func | 0x80U;
  s_tx_buf[2] = exc_code;
  const uint16_t crc = Modbus_CRC16(s_tx_buf, 3U);
  s_tx_buf[3] = (uint8_t)(crc & 0xFFU);
  s_tx_buf[4] = (uint8_t)(crc >> 8);
  SendResponse(s_tx_buf, 5U);
}

static void HandleFC03(const uint8_t *frame, uint16_t len) {
  if (len != 8U) {
    SendException(0x03U, 0x03U);
    return;
  }

  const uint16_t start_addr = ((uint16_t)frame[2] << 8) | frame[3];
  const uint16_t reg_count = ((uint16_t)frame[4] << 8) | frame[5];
  const uint16_t max_count = (MODBUS_TX_BUF_SIZE - 5U) / 2U;

  if (reg_count == 0U || reg_count > 125U || reg_count > max_count) {
    SendException(0x03U, 0x03U);
    return;
  }

  s_tx_buf[0] = g_modbus_cfg.address;
  s_tx_buf[1] = 0x03U;
  s_tx_buf[2] = (uint8_t)(reg_count * 2U);

  uint16_t idx = 3U;
  for (uint16_t i = 0U; i < reg_count; i++) {
    uint16_t value;
    if (!ModbusRegs_Read((uint16_t)(start_addr + i), &value)) {
      SendException(0x03U, 0x02U);
      return;
    }
    s_tx_buf[idx++] = (uint8_t)(value >> 8);
    s_tx_buf[idx++] = (uint8_t)(value & 0xFFU);
  }

  const uint16_t crc = Modbus_CRC16(s_tx_buf, idx);
  s_tx_buf[idx++] = (uint8_t)(crc & 0xFFU);
  s_tx_buf[idx++] = (uint8_t)(crc >> 8);
  SendResponse(s_tx_buf, idx);
}

static void HandleFC06(const uint8_t *frame, uint16_t len) {
  if (len != 8U) {
    SendException(0x06U, 0x03U);
    return;
  }

  const uint16_t addr = ((uint16_t)frame[2] << 8) | frame[3];
  const uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];
  const ModbusRegResult_t result = ModbusRegs_ValidateWrite(addr, value);
  if (result != MODBUS_REG_OK) {
    SendException(0x06U, ResultToException(result));
    return;
  }

  if (!ModbusRegs_Write(addr, value)) {
    SendException(0x06U, 0x04U);
    return;
  }

  for (uint16_t i = 0U; i < len; i++) {
    s_tx_buf[i] = frame[i];
  }
  SendResponse(s_tx_buf, len);
}

static void HandleFC10(const uint8_t *frame, uint16_t len) {
  if (len < 9U) {
    SendException(0x10U, 0x03U);
    return;
  }

  const uint16_t start_addr = ((uint16_t)frame[2] << 8) | frame[3];
  const uint16_t reg_count = ((uint16_t)frame[4] << 8) | frame[5];
  const uint8_t byte_count = frame[6];

  if (reg_count == 0U || reg_count > 123U ||
      byte_count != (uint16_t)(reg_count * 2U) ||
      len != (uint16_t)(9U + byte_count)) {
    SendException(0x10U, 0x03U);
    return;
  }

  /* 1차 Pass: 전체 검증. 하나라도 실패하면 아무 값도 변경하지 않는다. */
  for (uint16_t i = 0U; i < reg_count; i++) {
    const uint16_t value =
        ((uint16_t)frame[7U + i * 2U] << 8) | frame[8U + i * 2U];
    const ModbusRegResult_t result =
        ModbusRegs_ValidateWrite((uint16_t)(start_addr + i), value);
    if (result != MODBUS_REG_OK) {
      SendException(0x10U, ResultToException(result));
      return;
    }
  }

  /* 2차 Pass: 검증된 값 일괄 적용 */
  for (uint16_t i = 0U; i < reg_count; i++) {
    const uint16_t value =
        ((uint16_t)frame[7U + i * 2U] << 8) | frame[8U + i * 2U];
    if (!ModbusRegs_Write((uint16_t)(start_addr + i), value)) {
      SendException(0x10U, 0x04U);
      return;
    }
  }

  s_tx_buf[0] = g_modbus_cfg.address;
  s_tx_buf[1] = 0x10U;
  s_tx_buf[2] = (uint8_t)(start_addr >> 8);
  s_tx_buf[3] = (uint8_t)(start_addr & 0xFFU);
  s_tx_buf[4] = (uint8_t)(reg_count >> 8);
  s_tx_buf[5] = (uint8_t)(reg_count & 0xFFU);
  const uint16_t crc = Modbus_CRC16(s_tx_buf, 6U);
  s_tx_buf[6] = (uint8_t)(crc & 0xFFU);
  s_tx_buf[7] = (uint8_t)(crc >> 8);
  SendResponse(s_tx_buf, 8U);
}

static uint8_t ResultToException(ModbusRegResult_t result) {
  return (result == MODBUS_REG_ILLEGAL_VALUE) ? 0x03U : 0x02U;
}
