# STM32G4 vs F1/F4 레지스터 차이점

STM32G4 코드 작성 시 기존 F1/F4 시리즈와 혼동하기 쉬운 핵심 차이점 정리.

## USART 레지스터 구조 (가장 빈번한 실수)

| 항목          | F1 (구형)            | G4 (신형)                      |
| ------------- | -------------------- | ------------------------------ |
| 상태 레지스터 | `USART->SR`          | `USART->ISR`                   |
| 데이터 수신   | `USART->DR` (공용)   | `USART->RDR` (수신 전용)       |
| 데이터 송신   | `USART->DR` (공용)   | `USART->TDR` (송신 전용)       |
| 플래그 클리어 | SR에서 읽기로 클리어 | `USART->ICR`에 1 기록          |
| IDLE 클리어   | SR 읽기 → DR 읽기    | `ICR`의 `IDLECF` 비트에 1 기록 |
| TC 클리어     | SR 읽기 → DR 쓰기    | `ICR`의 `TCCF` 비트에 1 기록   |

```c
/* ❌ F1 방식 (G4에서 컴파일 에러) */
if (USART2->SR & USART_SR_RXNE) {
    data = USART2->DR;
}

/* ✅ G4 방식 */
if (USART2->ISR & USART_ISR_RXNE_RXFNE) {
    data = (uint8_t)(USART2->RDR & 0xFF);
}

/* ❌ F1 IDLE 클리어 */
(void)USART2->SR;
(void)USART2->DR;

/* ✅ G4 IDLE 클리어 */
USART2->ICR = USART_ICR_IDLECF;
```

> HAL 함수 사용 시에는 `__HAL_UART_GET_FLAG()`, `__HAL_UART_CLEAR_FLAG()` 가 자동으로 올바른 레지스터에 접근하므로 안전. 레지스터 직접 접근 시에만 주의.

## Flash 구조

| 항목          | F1                           | G4                             |
| ------------- | ---------------------------- | ------------------------------ |
| 쓰기 단위     | 하프워드 (16비트, 2바이트)   | **더블워드 (64비트, 8바이트)** |
| 페이지 크기   | 1KB (Low/Medium), 2KB (High) | **2KB**                        |
| 프로그램 타입 | `FLASH_TYPEPROGRAM_HALFWORD` | `FLASH_TYPEPROGRAM_DOUBLEWORD` |
| 정렬 요구     | 2바이트 정렬                 | **8바이트 정렬 필수**          |
| Wait State    | 0~2 (72MHz)                  | **0~8 (170MHz → WS=4)**        |

```c
/* ❌ F1 방식 */
HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, (uint16_t)data);

/* ✅ G4 방식 */
HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr, (uint64_t)data);
```

## ADC

| 항목              | F1           | G4                                                |
| ----------------- | ------------ | ------------------------------------------------- |
| 캘리브레이션      | 선택사항     | **필수** (`HAL_ADCEx_Calibration_Start()`)        |
| 캘리브레이션 모드 | 단일         | 싱글엔드 / 디퍼렌셜 **구분**                      |
| 오버샘플링        | 없음         | **하드웨어 오버샘플링 (최대 256배, 16비트 유효)** |
| 최대 샘플링 타임  | 239.5 cycles | **640.5 cycles**                                  |
| ADC 클럭          | APB2 분주    | PLL, 비동기, 또는 AHB 분주 **선택**               |
| 입력 타입         | 싱글엔드만   | **싱글엔드 + 디퍼렌셜**                           |
| Deep Power Down   | 없음         | **있음** (사용 전 해제 필요)                      |

```c
/* ⚠️ G4 ADC: 반드시 캘리브레이션 후 사용 */
HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

/* 채널 설정 시 SingleDiff 필드 필수 */
sConfig.SingleDiff = ADC_SINGLE_ENDED;  // F1에는 없는 필드
```

## GPIO

| 항목           | F1                                    | G4                                                 |
| -------------- | ------------------------------------- | -------------------------------------------------- |
| AF 설정 방식   | `GPIO_MODE_AF_PP` + 리맵(`AFIO_MAPR`) | `GPIO_MODE_AF_PP` + **`Alternate` 필드**(AF0~AF15) |
| 출력 속도 단계 | 2/10/50 MHz (3단계)                   | Low/Medium/High/**Very High** (4단계)              |
| JTAG 핀 해제   | `__HAL_AFIO_REMAP_SWJ_NOJTAG()` 필수  | **불필요** (SWD 기본, PA15/PB3/PB4 즉시 사용)      |

```c
/* ❌ F1: AF 설정 없음 (AFIO REMAP으로 전체 리맵) */
__HAL_AFIO_REMAP_TIM1_PARTIAL();

/* ✅ G4: 핀별 개별 AF 번호 지정 */
GPIO_InitStruct.Alternate = GPIO_AF6_TIM1;  // PA8 → TIM1_CH1용 AF6
```

## 타이머 클럭

| 항목                | F1 (72MHz)                          | G4 (170MHz)                                 |
| ------------------- | ----------------------------------- | ------------------------------------------- |
| APB1 타이머 클럭    | 72MHz (APB1 프리스케일러 관계 복잡) | **170MHz** (APB1 = AHB = 170MHz, 분주 없음) |
| APB2 타이머 클럭    | 72MHz                               | **170MHz**                                  |
| 타이머 자동 ×2 규칙 | APB 분주 시 타이머 클럭 ×2          | APB 분주 없으므로 **항상 170MHz**           |

> G4는 APB1 = APB2 = AHB = 170MHz로 통일되어, 타이머 클럭 계산이 매우 단순함.

## RCC / 클럭 트리

| 항목            | F1                    | G4                                               |
| --------------- | --------------------- | ------------------------------------------------ |
| PLL 분주기      | PLLMUL, PLLDIV (단순) | **PLLM, PLLN, PLLP, PLLQ, PLLR** (5개)           |
| 최대 SYSCLK     | 72MHz                 | **170MHz**                                       |
| Flash Latency   | 0~2 WS                | **0~8 WS** (170MHz → WS=4)                       |
| ADC 클럭 소스   | `RCC_CFGR.ADCPRE`     | `RCC_CCIPR.ADC12SEL` / `ADC345SEL`               |
| USART 클럭 소스 | APB 고정              | `RCC_CCIPR.USARTxSEL` (PCLK/SYSCLK/HSI/LSE 선택) |

## COMP (비교기)

| 항목        | F1   | G4                                  |
| ----------- | ---- | ----------------------------------- |
| 내장 비교기 | 없음 | **COMP1~7** (15ns 응답, 고속 모드)  |
| INM 소스    | –    | DAC, VREF 분압, 외부 핀 **선택**    |
| 출력 라우팅 | –    | 타이머 IC/BRK/OCREF에 **내부 연결** |
| Blanking    | –    | 타이머 OC로 **블랭킹** 가능         |

## DAC

| 항목          | F1             | G4                                           |
| ------------- | -------------- | -------------------------------------------- |
| DAC 수        | DAC1 (CH1/CH2) | **DAC1~DAC4**                                |
| 내부 전용 DAC | 없음           | **DAC3, DAC4** (외부 핀 없음, COMP INM 직결) |
| 해상도        | 12비트         | 12비트                                       |

## DMA

| 항목          | F1                            | G4                                      |
| ------------- | ----------------------------- | --------------------------------------- |
| DMA 채널 매핑 | **고정** (DMA1_CH1 = ADC1 등) | **DMAMUX** (모든 채널에 임의 소스 매핑) |
| 설정 방식     | 채널 번호로 결정              | `hdma.Init.Request = DMA_REQUEST_ADC1;` |

```c
/* ❌ F1: 채널이 소스에 고정 */
hdma.Instance = DMA1_Channel1;  // ADC1 전용

/* ✅ G4: DMAMUX로 자유 매핑 */
hdma.Instance = DMA1_Channel1;              // 아무 채널 사용 가능
hdma.Init.Request = DMA_REQUEST_ADC1;       // DMAMUX 요청 번호 지정
```
