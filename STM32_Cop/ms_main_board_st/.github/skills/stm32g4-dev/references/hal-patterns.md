# STM32G4 HAL 코딩 패턴 (메가소닉 발진기)

## 주변장치 초기화 순서 (반드시 준수)

```
1. HAL_Init()                    — SysTick, NVIC 그룹 설정
2. SystemClock_Config()          — RCC PLL 설정 → 170MHz
3. GPIO 클럭 활성화              — __HAL_RCC_GPIOx_CLK_ENABLE()
4. GPIO 초기화                   — HAL_GPIO_Init()
5. 주변장치 클럭 활성화          — __HAL_RCC_HRTIMx_CLK_ENABLE() 등
6. HRTIM DLL 캘리브레이션        — HAL_HRTIM_DLLCalibrationStart() + Poll (HRTIM 전용)
7. 주변장치 초기화               — HAL_HRTIM_WaveformTimerConfig() 등
8. 인터럽트 설정                 — HAL_NVIC_SetPriority() + HAL_NVIC_EnableIRQ()
9. 주변장치 시작                 — HAL_HRTIM_WaveformOutputStart() 등
```

> **주의**: GPIO 클럭은 해당 포트를 사용하는 어떤 주변장치보다 먼저 활성화해야 함.
> **주의**: HRTIM DLL 캘리브레이션은 HRTIM 클럭 활성화 직후, Timer 설정 전에 완료해야 함.

## 클럭 설정 패턴 (HSE 8MHz → PLL → 170MHz)

```c
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSE 활성화 + PLL 설정 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;    // 8MHz / 2 = 4MHz
    RCC_OscInitStruct.PLL.PLLN = 85;                 // 4MHz × 85 = 340MHz VCO
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;     // 340MHz / 2 = 170MHz
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /* 버스 클럭 설정: AHB=170, APB1=170, APB2=170 */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
    // G4 170MHz → Flash Wait State = 4 (RM0440 Table 9 참조)
}
```

## GPIO 초기화 패턴

```c
/* 출력 핀 */
GPIO_InitTypeDef GPIO_InitStruct = {0};
GPIO_InitStruct.Pin = GPIO_PIN_x;
GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;      // 일반 GPIO
HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);

/* 입력 핀 (외부 10kΩ 풀업 저항 사용 — 택트 스위치/외부 제어용) */
GPIO_InitStruct.Pin = GPIO_PIN_x;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_NOPULL;               // 외부 10kΩ 풀업 저항 사용 (디스플레이 보드 탑재)
HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);

/* AF 핀 (HRTIM PWM 출력) — 메가소닉 핵심 */
GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;    // PA8(CHA1), PA9(CHA2)
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // HRTIM MHz 대역 필수
GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1;       // ⚠️ HRTIM은 AF13
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

/* 아날로그 핀 (ADC 입력 — RF 전력 검출기 DC 출력) */
GPIO_InitStruct.Pin = GPIO_PIN_x;
GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
GPIO_InitStruct.Pull = GPIO_NOPULL;                // 아날로그는 풀업/풀다운 금지
HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
```

## HRTIM Timer A PWM 패턴 (메가소닉 핵심)

```c
HRTIM_HandleTypeDef hhrtim;

void HRTIM_PWM_Init(void)
{
    __HAL_RCC_HRTIM1_CLK_ENABLE();

    hhrtim.Instance = HRTIM1;
    hhrtim.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
    hhrtim.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
    HAL_HRTIM_Init(&hhrtim);

    /* ⚠️ HRTIM 필수: DLL 캘리브레이션 (Timer 설정 전 완료) */
    HAL_HRTIM_DLLCalibrationStart(&hhrtim, HRTIM_CALIBRATIONRATE_3);
    HAL_HRTIM_PollForDLLCalibration(&hhrtim, 10);  // 10ms 타임아웃

    /* Timer A 설정 */
    HRTIM_TimeBaseCfgTypeDef sTimeBaseCfg = {0};
    sTimeBaseCfg.Period = 10880;                    // 500kHz 기본: 5440000000/500000
    sTimeBaseCfg.RepetitionCounter = 0;
    sTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_MUL32;  // ×32 = 5.44GHz
    sTimeBaseCfg.Mode = HRTIM_MODE_CONTINUOUS;
    HAL_HRTIM_TimeBaseConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, &sTimeBaseCfg);

    /* 비교 레지스터 (듀티비) */
    HRTIM_CompareCfgTypeDef sCompareCfg = {0};
    sCompareCfg.CompareValue = 0;                   // 초기 듀티 0%
    HAL_HRTIM_WaveformCompareConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A,
                                     HRTIM_COMPAREUNIT_1, &sCompareCfg);

    /* 데드타임 설정 (GaN FET: 5~20ns 권장) */
    HRTIM_DeadTimeCfgTypeDef sDeadTimeCfg = {0};
    sDeadTimeCfg.Prescaler = HRTIM_TIMDEADTIME_PRESCALERRATIO_MUL8;
    sDeadTimeCfg.RisingValue = 10;                  // ~15ns
    sDeadTimeCfg.RisingSign = HRTIM_TIMDEADTIME_RISINGSIGN_POSITIVE;
    sDeadTimeCfg.FallingValue = 10;                 // ~15ns
    sDeadTimeCfg.FallingSign = HRTIM_TIMDEADTIME_FALLINGSIGN_POSITIVE;
    HAL_HRTIM_DeadTimeConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A, &sDeadTimeCfg);

    /* 출력 설정 */
    HRTIM_OutputCfgTypeDef sOutputCfg = {0};
    sOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
    sOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
    sOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
    sOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
    sOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
    sOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_INACTIVE; // Fault 시 LOW
    HAL_HRTIM_WaveformOutputConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A,
                                    HRTIM_OUTPUT_TA1, &sOutputCfg);
    HAL_HRTIM_WaveformOutputConfig(&hhrtim, HRTIM_TIMERINDEX_TIMER_A,
                                    HRTIM_OUTPUT_TA2, &sOutputCfg);

    /* ⚠️ 출력 시작 (Start 호출 전까지 PWM 없음) */
    /* 실제 Start는 megasonic_ctrl.c에서 호출 */
}

/* 주파수 변경 */
static inline void HRTIM_SetFrequency(uint32_t freq_hz)
{
    // Period = HRTIM_EFF_CLK / freq = 5,440,000,000 / freq_hz
    uint32_t period = 5440000000UL / freq_hz;
    if (period < 0x0003) period = 0x0003;
    if (period > 0xFFFD) period = 0xFFFD;
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR = period;
}

/* 듀티비 변경 (0~1000 → 0.0~100.0%) */
static inline void HRTIM_SetDuty(uint16_t duty_permille)
{
    uint32_t period = HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].PERxR;
    uint32_t cmp = (period * duty_permille) / 1000;
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = cmp;
}

/* 출력 시작/정지 */
static inline void HRTIM_OutputStart(void)
{
    HAL_HRTIM_WaveformOutputStart(&hhrtim, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
    HAL_HRTIM_WaveformCountStart(&hhrtim, HRTIM_TIMERID_TIMER_A);
}

static inline void HRTIM_OutputStop(void)
{
    HAL_HRTIM_WaveformOutputStop(&hhrtim, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
    HAL_HRTIM_WaveformCountStop(&hhrtim, HRTIM_TIMERID_TIMER_A);
    HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = 0;  // 듀티 0 클리어
}
```

## ADC 패턴 (G4 필수: 캘리브레이션)

```c
void ADC_Init(ADC_HandleTypeDef *hadc)
{
    /* ADC 클럭 활성화 */
    __HAL_RCC_ADC12_CLK_ENABLE();  // ADC1, ADC2 공용

    /* ADC 기본 설정 */
    hadc->Instance = ADC1;
    hadc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc->Init.Resolution = ADC_RESOLUTION_12B;
    hadc->Init.ScanConvMode = ADC_SCAN_ENABLE;        // 다중 채널
    hadc->Init.ContinuousConvMode = ENABLE;
    hadc->Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc->Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    HAL_ADC_Init(hadc);

    /* ⚠️ G4 필수: 캘리브레이션 (Init 후, Start 전) */
    HAL_ADCEx_Calibration_Start(hadc, ADC_SINGLE_ENDED);

    /* 채널 설정 (순방향 전력 검출) */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_1;                   // PA0 = ADC1_IN1
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;  // RF 검출기 고임피던스 출력
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    HAL_ADC_ConfigChannel(hadc, &sConfig);

    HAL_ADC_Start(hadc);  // 또는 HAL_ADC_Start_DMA()
}
```

## USART + RS-485 반이중 패턴

```c
void USART2_Init(UART_HandleTypeDef *huart, uint32_t baudrate)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart->Instance = USART2;
    huart->Init.BaudRate = baudrate;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(huart);

    /* RXNE 인터럽트 활성화 */
    HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    __HAL_UART_ENABLE_IT(huart, UART_IT_RXNE);
    __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);  // 프레임 종료 감지
}

/* RS-485 송신 패턴 (DE/RE 제어) — PC13=DE, PC14=/RE */
static void RS485_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t len)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);    // DE=HIGH (송신)
    HAL_GPIO_WritePin(RS485_RE_PORT, RS485_RE_PIN, GPIO_PIN_SET);    // /RE=HIGH (수신 비활성)

    HAL_UART_Transmit(huart, data, len, 100);

    /* ⚠️ TC(Transmission Complete) 대기 필수 — 마지막 바이트 shift 완료까지 */
    while (__HAL_UART_GET_FLAG(huart, UART_FLAG_TC) == RESET) {}

    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);  // DE=LOW (수신 모드)
    HAL_GPIO_WritePin(RS485_RE_PORT, RS485_RE_PIN, GPIO_PIN_RESET);  // /RE=LOW (수신 활성)
}
```

## DWT 마이크로초 지연 패턴 (LCD1602 제어용)

```c
/* DWT(Data Watchpoint and Trace) 카운터 기반 μs 지연 */
static inline void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);  // 170 ticks/μs @ 170MHz
    while ((DWT->CYCCNT - start) < ticks) {}
}
```

## Flash 쓰기 패턴 (G4: 더블워드 64비트 단위, 페이지 4KB)

```c
HAL_StatusTypeDef Flash_WriteData(uint32_t address, uint64_t data)
{
    HAL_StatusTypeDef status;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    /* 페이지 소거 (4KB 단위, G474 기준) */
    FLASH_EraseInitTypeDef eraseInit = {0};
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.Banks = FLASH_BANK_1;
    eraseInit.Page = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
    eraseInit.NbPages = 1;
    uint32_t pageError = 0;
    status = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (status != HAL_OK) { HAL_FLASH_Lock(); return status; }

    /* ⚠️ G4: 반드시 DOUBLEWORD(64비트) 단위, 8바이트 정렬 주소 */
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data);

    HAL_FLASH_Lock();
    return status;
}
```

## 인터럽트 핸들러 패턴 (ISR 최소화)

```c
/* ISR: 플래그만 설정 */
volatile uint8_t g_modbus_frame_ready = 0;

void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart2.Instance->RDR & 0xFF);  // G4: RDR 레지스터
        /* 바이트 버퍼에 저장 */
        rx_buffer[rx_index++] = byte;
    }
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);  // G4: ICR로 클리어
        g_modbus_frame_ready = 1;             // 메인 루프에서 처리
    }
}

/* 메인 루프: 무거운 처리 수행 */
while (1) {
    if (g_modbus_frame_ready) {
        g_modbus_frame_ready = 0;
        Modbus_ProcessFrame(rx_buffer, rx_index);
        rx_index = 0;
    }
}
```
