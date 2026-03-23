/**
 * @file  modbus_rtu.c
 * @brief Modbus RTU 슬레이브 — 프레임 수신/파싱/응답
 *
 * USART2 RXNE 인터럽트로 바이트 수신, TIM4로 3.5T 프레임 타임아웃 감지.
 * 지원 펑션: 0x03 (Read Holding), 0x06 (Write Single), 0x10 (Write Multiple).
 */
#include "modbus_rtu.h"
#include "modbus_crc.h"
#include "modbus_regs.h"
#include "config.h"
#include "params.h"

UART_HandleTypeDef  huart2;
TIM_HandleTypeDef   htim4;

volatile ModbusConfig_t g_modbus_cfg;

/* 수신 버퍼 및 상태 */
static volatile uint8_t  s_rx_buf[MODBUS_RX_BUF_SIZE];
static volatile uint16_t s_rx_len;
static volatile uint8_t  s_frame_ready;

/* 송신 버퍼 */
static uint8_t s_tx_buf[MODBUS_TX_BUF_SIZE];

/* ---- 내부 함수 전방 선언 ---- */
static void USART2_Init(void);
static void TIM4_Init(void);
static void TIM4_Restart(void);
static void TIM4_Stop(void);
static void SendResponse(const uint8_t *data, uint16_t len);
static void SendException(uint8_t func, uint8_t exc_code);
static void HandleFC03(const uint8_t *frame, uint16_t len);
static void HandleFC06(const uint8_t *frame, uint16_t len);
static void HandleFC10(const uint8_t *frame, uint16_t len);

/* ================================================================
   공개 API
   ================================================================ */

void Modbus_Init(void)
{
    g_modbus_cfg.address  = MODBUS_ADDR_DEFAULT;
    g_modbus_cfg.baudrate = MODBUS_BAUD_DEFAULT;
    g_modbus_cfg.parity   = MODBUS_PARITY_DEFAULT;

    s_rx_len      = 0;
    s_frame_ready = 0;

    USART2_Init();
    TIM4_Init();

    /* USART2 RXNE + IDLE 인터럽트 활성화 */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

void Modbus_Process(void)
{
    if (!s_frame_ready) return;
    s_frame_ready = 0;

    uint16_t len = s_rx_len;
    if (len < 4) return;    /* 최소: addr(1) + func(1) + CRC(2) */

    /* 주소 확인 */
    if (s_rx_buf[0] != g_modbus_cfg.address) return;

    /* CRC 검증 */
    uint16_t crc_recv = (uint16_t)s_rx_buf[len - 1] << 8 | s_rx_buf[len - 2];
    uint16_t crc_calc = Modbus_CRC16((const uint8_t *)s_rx_buf, len - 2);
    if (crc_recv != crc_calc) return;

    /* 펑션 코드 분기 */
    uint8_t func = s_rx_buf[1];
    switch (func) {
    case 0x03:
        HandleFC03((const uint8_t *)s_rx_buf, len);
        break;
    case 0x06:
        HandleFC06((const uint8_t *)s_rx_buf, len);
        break;
    case 0x10:
        HandleFC10((const uint8_t *)s_rx_buf, len);
        break;
    default:
        SendException(func, 0x01);  /* Illegal Function */
        break;
    }
}

void Modbus_UART_RxCallback(uint8_t byte)
{
    if (s_rx_len < MODBUS_RX_BUF_SIZE) {
        s_rx_buf[s_rx_len++] = byte;
    }
    /* 3.5T 타이머 재시작 */
    TIM4_Restart();
}

void Modbus_FrameTimeoutCallback(void)
{
    TIM4_Stop();
    if (s_rx_len > 0) {
        s_frame_ready = 1;
    }
}

void Modbus_ReconfigUART(void)
{
    HAL_UART_DeInit(&huart2);
    USART2_Init();
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

/* ================================================================
   USART2 초기화
   ================================================================ */
static void USART2_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance        = MODBUS_USART;
    huart2.Init.BaudRate   = g_modbus_cfg.baudrate;
    huart2.Init.StopBits   = UART_STOPBITS_1;
    huart2.Init.Mode       = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    switch (g_modbus_cfg.parity) {
    case 1:  /* Even */
        huart2.Init.WordLength = UART_WORDLENGTH_9B;
        huart2.Init.Parity     = UART_PARITY_EVEN;
        break;
    case 2:  /* Odd */
        huart2.Init.WordLength = UART_WORDLENGTH_9B;
        huart2.Init.Parity     = UART_PARITY_ODD;
        break;
    default: /* None */
        huart2.Init.WordLength = UART_WORDLENGTH_8B;
        huart2.Init.Parity     = UART_PARITY_NONE;
        break;
    }

    HAL_UART_Init(&huart2);

    /* NVIC */
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

/* ================================================================
   TIM4 — 3.5 캐릭터 타임아웃
   ================================================================ */
static void TIM4_Init(void)
{
    __HAL_RCC_TIM4_CLK_ENABLE();

    /*
     * 3.5T (캐릭터 시간) = 3.5 × 11비트 / baud
     * 9600 bps → ~4 ms, 115200 bps → ~0.3 ms
     * TIM4 클럭 = 170 MHz (APB1), PSC=169 → 1 MHz (1 µs/tick)
     */
    htim4.Instance         = TIM4;
    htim4.Init.Prescaler   = 169;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period      = 4000;  /* 기본 4 ms (9600 bps용) */
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&htim4);

    HAL_NVIC_SetPriority(TIM4_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

static void TIM4_Restart(void)
{
    /* 타임아웃 기간 재계산 */
    uint32_t char_us = 11000000U / g_modbus_cfg.baudrate;  /* 1 캐릭터 µs */
    uint32_t t35_us  = (char_us * 35U) / 10U;              /* ×3.5 */
    if (t35_us < 750) t35_us = 750;   /* 최소 750 µs (고속 baud 보호) */

    __HAL_TIM_SET_AUTORELOAD(&htim4, t35_us);
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    __HAL_TIM_CLEAR_FLAG(&htim4, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
    HAL_TIM_Base_Start(&htim4);
}

static void TIM4_Stop(void)
{
    HAL_TIM_Base_Stop(&htim4);
    __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);
}

/* ================================================================
   응답 송신
   ================================================================ */
static void SendResponse(const uint8_t *data, uint16_t len)
{
    MODBUS_DE_TX();
    HAL_UART_Transmit(&huart2, (uint8_t *)data, len, 100);
    /* TC 플래그 대기 — 마지막 바이트 전송 완료 확인 */
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET) { }
    MODBUS_DE_RX();

    /* 수신 버퍼 클리어 */
    s_rx_len = 0;
}

static void SendException(uint8_t func, uint8_t exc_code)
{
    s_tx_buf[0] = g_modbus_cfg.address;
    s_tx_buf[1] = func | 0x80;
    s_tx_buf[2] = exc_code;
    uint16_t crc = Modbus_CRC16(s_tx_buf, 3);
    s_tx_buf[3] = (uint8_t)(crc & 0xFF);
    s_tx_buf[4] = (uint8_t)(crc >> 8);
    SendResponse(s_tx_buf, 5);
}

/* ================================================================
   FC 0x03: Read Holding Registers
   ================================================================ */
static void HandleFC03(const uint8_t *frame, uint16_t len)
{
    if (len < 8) { SendException(0x03, 0x03); return; }

    uint16_t start_addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t reg_count  = ((uint16_t)frame[4] << 8) | frame[5];

    if (reg_count == 0 || reg_count > 125) {
        SendException(0x03, 0x03);
        return;
    }

    s_tx_buf[0] = g_modbus_cfg.address;
    s_tx_buf[1] = 0x03;
    s_tx_buf[2] = (uint8_t)(reg_count * 2);

    uint16_t idx = 3;
    for (uint16_t i = 0; i < reg_count; i++) {
        uint16_t val;
        if (!ModbusRegs_Read(start_addr + i, &val)) {
            SendException(0x03, 0x02);  /* Illegal Data Address */
            return;
        }
        s_tx_buf[idx++] = (uint8_t)(val >> 8);
        s_tx_buf[idx++] = (uint8_t)(val & 0xFF);
    }

    uint16_t crc = Modbus_CRC16(s_tx_buf, idx);
    s_tx_buf[idx++] = (uint8_t)(crc & 0xFF);
    s_tx_buf[idx++] = (uint8_t)(crc >> 8);
    SendResponse(s_tx_buf, idx);
}

/* ================================================================
   FC 0x06: Write Single Register
   ================================================================ */
static void HandleFC06(const uint8_t *frame, uint16_t len)
{
    if (len < 8) { SendException(0x06, 0x03); return; }

    uint16_t addr  = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t value = ((uint16_t)frame[4] << 8) | frame[5];

    if (!ModbusRegs_Write(addr, value)) {
        SendException(0x06, 0x02);
        return;
    }

    /* 에코 응답 (요청과 동일) */
    for (uint16_t i = 0; i < len; i++) {
        s_tx_buf[i] = frame[i];
    }
    SendResponse(s_tx_buf, len);
}

/* ================================================================
   FC 0x10: Write Multiple Registers
   ================================================================ */
static void HandleFC10(const uint8_t *frame, uint16_t len)
{
    if (len < 9) { SendException(0x10, 0x03); return; }

    uint16_t start_addr = ((uint16_t)frame[2] << 8) | frame[3];
    uint16_t reg_count  = ((uint16_t)frame[4] << 8) | frame[5];
    uint8_t  byte_count = frame[6];

    if (reg_count == 0 || reg_count > 123 || byte_count != reg_count * 2) {
        SendException(0x10, 0x03);
        return;
    }

    for (uint16_t i = 0; i < reg_count; i++) {
        uint16_t val = ((uint16_t)frame[7 + i * 2] << 8) | frame[8 + i * 2];
        if (!ModbusRegs_Write(start_addr + i, val)) {
            SendException(0x10, 0x02);
            return;
        }
    }

    /* 응답: addr + func + start_addr + reg_count + CRC */
    s_tx_buf[0] = g_modbus_cfg.address;
    s_tx_buf[1] = 0x10;
    s_tx_buf[2] = (uint8_t)(start_addr >> 8);
    s_tx_buf[3] = (uint8_t)(start_addr & 0xFF);
    s_tx_buf[4] = (uint8_t)(reg_count >> 8);
    s_tx_buf[5] = (uint8_t)(reg_count & 0xFF);
    uint16_t crc = Modbus_CRC16(s_tx_buf, 6);
    s_tx_buf[6] = (uint8_t)(crc & 0xFF);
    s_tx_buf[7] = (uint8_t)(crc >> 8);
    SendResponse(s_tx_buf, 8);
}
