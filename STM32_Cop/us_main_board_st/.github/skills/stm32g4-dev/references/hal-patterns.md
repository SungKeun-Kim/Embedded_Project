# STM32G4 HAL 코딩 패턴

## 주변장치 초기화 순서 (반드시 준수)

```
1. HAL_Init()                    — SysTick, NVIC 그룹 설정
2. SystemClock_Config()          — RCC PLL 설정 → 170MHz
3. GPIO 클럭 활성화              — __HAL_RCC_GPIOx_CLK_ENABLE()
4. GPIO 초기화                   — HAL_GPIO_Init()
5. 주변장치 클럭 활성화          — __HAL_RCC_TIMx_CLK_ENABLE() 등
6. 주변장치 초기화               — HAL_TIM_PWM_Init() 등
7. 인터럽트 설정                 — HAL_NVIC_SetPriority() + HAL_NVIC_EnableIRQ()
8. 주변장치 시작                 — HAL_TIM_PWM_Start() 등
```

> **주의**: GPIO 클럭은 해당 포트를 사용하는 어떤 주변장치보다 먼저 활성화해야 함.

## 클럭 설정 패턴 (HSI → PLL → 170MHz)

```c
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* HSI 활성화 + PLL 설정 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;    // 16MHz / 4 = 4MHz
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

/* 입력 핀 (내부 풀업) */
GPIO_InitStruct.Pin = GPIO_PIN_x;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;               // 택트 스위치용
HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);

/* AF 핀 (타이머 PWM 출력) */
GPIO_InitStruct.Pin = GPIO_PIN_x;
GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
GPIO_InitStruct.Pull = GPIO_NOPULL;
GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; // PWM 고속 출력
GPIO_InitStruct.Alternate = GPIO_AFx_TIMy;          // G4 AF 번호 확인 필수
HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);

/* 아날로그 핀 (ADC/COMP 입력) */
GPIO_InitStruct.Pin = GPIO_PIN_x;
GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
GPIO_InitStruct.Pull = GPIO_NOPULL;                // 아날로그는 풀업/풀다운 금지
HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
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

    /* 채널 설정 */
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_1;                   // PA0 = ADC1_IN1
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;  // 고임피던스는 92.5+ 권장
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    HAL_ADC_ConfigChannel(hadc, &sConfig);

    HAL_ADC_Start(hadc);  // 또는 HAL_ADC_Start_DMA()
}
```

## TIM1 고급 타이머 PWM 패턴

```c
void TIM1_PWM_Init(TIM_HandleTypeDef *htim)
{
    __HAL_RCC_TIM1_CLK_ENABLE();

    htim->Instance = TIM1;
    htim->Init.Prescaler = 0;                          // 170MHz 직결
    htim->Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;  // 대칭 PWM
    htim->Init.Period = 2124;                           // Center-aligned: 170MHz/(2×40kHz)=2125 → ARR=2124
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.RepetitionCounter = 0;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(htim);

    /* 데드타임 설정 (BDTR) */
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 34;                 // 약 200ns @ 170MHz (DTG[7:0])
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    HAL_TIMEx_ConfigBreakDeadTime(htim, &sBreakDeadTimeConfig);

    /* PWM 채널 설정 */
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;                                // 초기 듀티 0%
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    HAL_TIM_PWM_ConfigChannel(htim, &sConfigOC, TIM_CHANNEL_1);

    /* ⚠️ TIM1 필수: 주 출력 + 상보 출력 모두 시작 */
    HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1);       // PA8  (CH1)
    HAL_TIMEx_PWMN_Start(htim, TIM_CHANNEL_1);    // PB13 (CH1N)
    /* MOE 비트는 위 Start 함수들이 자동 SET */
}

/* 주파수 변경 */
static inline void TIM1_SetFrequency(TIM_HandleTypeDef *htim, uint32_t freq_hz)
{
    // Center-aligned: ARR = TIM_CLK / (2 × freq) - 1
    uint32_t arr = (170000000UL / (2UL * freq_hz)) - 1;
    __HAL_TIM_SET_AUTORELOAD(htim, arr);
}

/* 듀티비 변경 (0~1000 → 0.0~100.0%) */
static inline void TIM1_SetDuty(TIM_HandleTypeDef *htim, uint16_t duty_permille)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    uint32_t ccr = (arr * duty_permille) / 1000;
    __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, ccr);
}
```

## COMP + DAC3 + TIM2 위상 검출 패턴

```c
/* DAC3 내부 전용 — COMP 기준전압 (VDDA/2 ≈ 1.65V) */
void DAC3_Init(void)
{
    __HAL_RCC_DAC3_CLK_ENABLE();
    DAC_HandleTypeDef hdac3 = {0};
    hdac3.Instance = DAC3;
    HAL_DAC_Init(&hdac3);

    DAC_ChannelConfTypeDef sConfig = {0};
    sConfig.DAC_Trigger = DAC_TRIGGER_NONE;             // 즉시 출력
    sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_DISABLE; // 내부 전용이므로 버퍼 불필요
    HAL_DAC_ConfigChannel(&hdac3, &sConfig, DAC_CHANNEL_1);
    HAL_DAC_ConfigChannel(&hdac3, &sConfig, DAC_CHANNEL_2);

    /* VDDA/2 = 2048 (12비트 기준) */
    HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048);
    HAL_DAC_SetValue(&hdac3, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 2048);
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_1);
    HAL_DAC_Start(&hdac3, DAC_CHANNEL_2);
}

/* COMP1: PA1(+) vs DAC3_CH1(-) → TIM2_IC1 */
/* COMP2: PA7(+) vs DAC3_CH2(-) → TIM2_IC2 */
void COMP_Init(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();  // COMP 클럭

    COMP_HandleTypeDef hcomp1 = {0};
    hcomp1.Instance = COMP1;
    hcomp1.Init.InvertingInput = COMP_INPUT_MINUS_DAC3_CH1;
    hcomp1.Init.NonInvertingInput = COMP_INPUT_PLUS_IO1;  // PA1
    hcomp1.Init.OutputPol = COMP_OUTPUTPOL_NONINVERTED;
    hcomp1.Init.Hysteresis = COMP_HYSTERESIS_10MV;        // 노이즈 제거
    hcomp1.Init.BlankingSrce = COMP_BLANKINGSRC_NONE;
    hcomp1.Init.Mode = COMP_POWERMODE_HIGHSPEED;          // 15ns 응답
    HAL_COMP_Init(&hcomp1);
    HAL_COMP_Start(&hcomp1);
    /* COMP2도 동일 패턴 (DAC3_CH2, PA7) */
}

/* TIM2 듀얼 입력 캡처 — 위상차 측정 */
void TIM2_IC_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM_HandleTypeDef htim2 = {0};
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;          // 170MHz → 5.88ns/tick
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFF;    // 32비트 풀 카운트
    HAL_TIM_IC_Init(&htim2);

    TIM_IC_InitTypeDef sConfigIC = {0};
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;
    HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1);  // COMP1 출력
    HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2);  // COMP2 출력

    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
}
```

## USART + RS485 반이중 패턴

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

/* RS485 송신 패턴 (DE/RE 제어) */
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

## DWT 마이크로초 지연 패턴

```c
/* DWT(Data Watchpoint and Trace) 카운터 기반 μs 지연 — LCD 등에서 사용 */
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

## Flash 쓰기 패턴 (G4: 더블워드 64비트 단위)

```c
HAL_StatusTypeDef Flash_WriteData(uint32_t address, uint64_t data)
{
    HAL_StatusTypeDef status;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    /* 페이지 소거 (2KB 단위) */
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
