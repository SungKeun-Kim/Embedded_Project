# STM32G474RBT6 핀맵 (LQFP64)

**적용 장치**: 초음파/메가소닉 발진기 메인보드
**패키지**: LQFP64 (10 x 10 mm)
**문서 버전**: 3.0
**최종 수정**: 2026-06-13
**기준 회로도**: U3 = STM32G474RBT6, 2026-06-13 첨부 회로도

> 파일명은 기존 `STM32G474RC_핀맵.md`를 유지한다. STM32G474RB/RC는 같은 LQFP64 패키지에서 핀맵이 같고 Flash 용량 차이가 핵심이다.
>
> 물리 핀 번호는 ST STM32G474xB/xC/xE datasheet의 LQFP64 pin definition과 현재 회로도 심볼을 기준으로 정리했다.

---

## 1. 핀 할당 전체 요약 (물리 핀 번호 순)

| 물리핀# | 포트 | AF/모드 | 회로도 Net | 기능 그룹 | 메모 |
| ------- | ---- | ------- | ---------- | --------- | ---- |
| 1 | VBAT | Power | VBAT | 전원 | 미사용 시 VDD 연결 권장 |
| 2 | PC13 | GPIO_Output | END_BZ | 부저/알림 | 종료 알림 제어 |
| 3 | PC14 | GPIO_Output | BZ_OUT | 부저/알림 | 부저 출력 |
| 4 | PC15 | - | - | 여유 | LSE 미사용 시 GPIO 가능 |
| 5 | PF0 | OSC_IN | HSE_IN | 클럭 | 외부 크리스탈 사용 시 |
| 6 | PF1 | OSC_OUT | HSE_OUT | 클럭 | 외부 크리스탈 사용 시 |
| 7 | NRST | RESET | NRST | 시스템 | ST-Link/리셋 회로 연결 |
| 8 | PC0 | GPIO_Input | START_STOP | 버튼 | 시작/정지 |
| 9 | PC1 | GPIO_Input | MODE | 버튼 | 모드 선택 |
| 10 | PC2 | GPIO_Input | UP | 버튼 | 값 증가/위로 |
| 11 | PC3 | GPIO_Input | DOWN | 버튼 | 값 감소/아래로 |
| 12 | PA0 | ADC1_IN1 | ADC1_IN1 | 아날로그 | 공진/레벨 검출 입력 1 |
| 13 | PA1 | ADC1_IN2 | ADC1_IN2 | 아날로그 | 공진/레벨 검출 입력 2, COMP1_INP 가능 |
| 14 | PA2 | USART2_TX | USART2_TX | RS485 | Modbus TXD |
| 15 | VSS | Power | GND | 전원 | Digital GND |
| 16 | VDD | Power | +3.3V | 전원 | Digital VDD |
| 17 | PA3 | USART2_RX | USART2_RX | RS485 | Modbus RXD |
| 18 | PA4 | - | - | 여유 | ADC2_IN17/DAC1_OUT1 가능 |
| 19 | PA5 | - | - | 여유 | ADC2_IN13/DAC1_OUT2 가능 |
| 20 | PA6 | ADC2_IN3 | ADC2_IN3 | 아날로그 | 추가 아날로그 입력 |
| 21 | PA7 | Analog | PWM_VR | 아날로그 | PWM/출력 설정 VR 입력, ADC2_IN4 가능 |
| 22 | PC4 | GPIO_Output | RTERM | RS485 | 종단저항 ON/OFF |
| 23 | PC5 | GPIO_Input | 485BD_DETECT | RS485 | RS485 옵션보드 감지 |
| 24 | PB0 | GPIO_Output | DE_RS485 | RS485 | Driver Enable, HIGH=송신 |
| 25 | PB1 | GPIO_Output | /RE_RS485 | RS485 | Receiver Enable, LOW=수신 |
| 26 | PB2 | - | - | 여유 | LPTIM/TIM/ADC 후보 |
| 27 | VSSA | Power | GNDA | 전원 | Analog GND |
| 28 | VREF+ | Power | VREF+ | 전원 | VDDA 또는 기준전압 |
| 29 | VDDA | Power | VDDA | 전원 | Analog VDD |
| 30 | PB10 | GPIO_Output | LCD_RS | LCD1602 | Register Select |
| 31 | VSS | Power | GND | 전원 | Digital GND |
| 32 | VDD | Power | +3.3V | 전원 | Digital VDD |
| 33 | PB11 | GPIO_Output | LCD_E | LCD1602 | Enable |
| 34 | PB12 | GPIO_Output | LCD_D4 | LCD1602 | Data 4 |
| 35 | PB13 | GPIO_Output | LCD_D5 | LCD1602 | Data 5 |
| 36 | PB14 | GPIO_Output | LCD_D6 | LCD1602 | Data 6 |
| 37 | PB15 | GPIO_Output | LCD_D7 | LCD1602 | Data 7 |
| 38 | PC6 | GPIO_Input | REMOTE | 외부 제어 | 외부 리모트 입력 |
| 39 | PC7 | GPIO_Input | RUN_SW | 외부 제어 | RUN 스위치 |
| 40 | PC8 | GPIO_Input | SWEEP_SW | 외부 제어 | Sweep 스위치 |
| 41 | PC9 | TIM3_CH4(AF2) | PWM_OUTPUT | PWM 출력 | 외부 위상/출력 제어 PWM |
| 42 | PA8 | HRTIM1_CHA1(AF13) | CHA1 / INA | 초음파 발진 | 게이트 드라이버 INA |
| 43 | PA9 | HRTIM1_CHA2(AF13) | CHA2 / INB | 초음파 발진 | 게이트 드라이버 INB, UCPD1_DBCC1 주의 |
| 44 | PA10 | GPIO_Output | GOING | 외부 제어 | 운전 상태 출력 |
| 45 | PA11 | - | - | 여유 | USB_DM/TIM/HRTIM 후보 |
| 46 | PA12 | - | - | 여유 | USB_DP/TIM/HRTIM 후보 |
| 47 | VSS | Power | GND | 전원 | Digital GND |
| 48 | VDD | Power | +3.3V | 전원 | Digital VDD |
| 49 | PA13 | SWDIO | SWDIO | 디버그 | GPIO 사용 금지 |
| 50 | PA14 | SWCLK | SWCLK | 디버그 | GPIO 사용 금지 |
| 51 | PA15 | - | - | 여유 | TIM2/USART2/HRTIM 후보 |
| 52 | PC10 | - | - | 여유 | UART4/USART3/HRTIM 후보 |
| 53 | PC11 | - | - | 여유 | UART4/USART3/HRTIM 후보 |
| 54 | PC12 | - | - | 여유 | UART5/USART3/HRTIM 후보 |
| 55 | PD2 | - | - | 여유 | TIM3_ETR/UART5_RX 후보 |
| 56 | PB3 | GPIO_Output | SONIC_ON | 초음파 제어 | 발진 Enable/드라이버 Enable |
| 57 | PB4 | - | - | 여유 | TIM16/SPI/JTRST 후보 |
| 58 | PB5 | - | - | 여유 | SPI/I2C/TIM 후보 |
| 59 | PB6 | USART1_TX(AF7) | USART1_TX | 디버그 UART | USB-UART TX |
| 60 | PB7 | USART1_RX(AF7) | USART1_RX | 디버그 UART | USB-UART RX |
| 61 | PB8 | BOOT0 / GPIO | - | 시스템/여유 | BOOT0 옵션, I2C1_SCL 후보 |
| 62 | PB9 | - | - | 여유 | I2C1_SDA/USART3_TX 후보 |
| 63 | VSS | Power | GND | 전원 | Digital GND |
| 64 | VDD | Power | +3.3V | 전원 | Digital VDD |

---

## 2. 기능 그룹별 핀 요약

### 2.1 초음파 발진 (HRTIM1 Timer A)

| 핀 | 물리핀# | 포트/AF | 회로도 Net | 기능 |
| --- | ------- | ------- | ---------- | ---- |
| PA8 | 42 | HRTIM1_CHA1(AF13) | CHA1 / INA | 초음파 HRTIM 출력 A1 |
| PA9 | 43 | HRTIM1_CHA2(AF13) | CHA2 / INB | 초음파 HRTIM 출력 A2 |
| PB3 | 56 | GPIO_Output | SONIC_ON | 발진 Enable/게이트 드라이버 Enable |

> **선정 사유**: 20kHz~168kHz 발진 중 중심주파수 기준 ±100Hz~±1000Hz 스윕을 만들기 위해 HRTIM을 사용한다. PA8/PA9는 같은 HRTIM Timer A의 출력쌍(CHA1/CHA2)이므로 pair deadtime 설정과 주파수 가변 제어에 적합하다.

#### PA9 / UCPD1_DBCC1 주의 및 초기화

PA9는 HRTIM1_CHA2로 사용하지만, STM32G474에서 PA9는 `UCPD1_DBCC1` 기능도 겸한다. USB-C PD 기능을 사용하지 않는 보드에서는 초기화 초기에 `UCPD1_DBDIS=1`로 dead-battery pull-down 기능을 비활성화한다.

초기화 위치는 `HAL_Init()` 이후, PA9를 HRTIM alternate function으로 설정하기 전이 좋다.

```c
/* main.c 예시 */
int main(void)
{
    HAL_Init();

    /* PA9(UCPD1_DBCC1) dead-battery 기능 비활성화 */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_DisableUCPDDeadBattery();

    SystemClock_Config();
    GPIO_Init_All();

    /* 이후 HRTIM, ADC, LCD 등 모듈 초기화 */
}
```

사용 중인 HAL 버전에 `HAL_PWREx_DisableUCPDDeadBattery()`가 없다면 같은 의미로 아래처럼 직접 설정한다.

```c
__HAL_RCC_PWR_CLK_ENABLE();
SET_BIT(PWR->CR3, PWR_CR3_UCPD1_DBDIS);
```

PA8/PA9 GPIO 설정은 HRTIM 출력으로 아래처럼 맞춘다.

```c
GPIO_InitTypeDef gpio = {0};

__HAL_RCC_GPIOA_CLK_ENABLE();

gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
gpio.Mode      = GPIO_MODE_AF_PP;
gpio.Pull      = GPIO_NOPULL;
gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
gpio.Alternate = GPIO_AF13_HRTIM1;
HAL_GPIO_Init(GPIOA, &gpio);
```

### 2.2 공진 탐색 / 아날로그 입력

| 핀 | 물리핀# | ADC/기능 | 회로도 Net | 용도 |
| --- | ------- | -------- | ---------- | ---- |
| PA0 | 12 | ADC1_IN1 | ADC1_IN1 | 공진/레벨 검출 입력 1 |
| PA1 | 13 | ADC1_IN2 / COMP1_INP | ADC1_IN2 | 공진/레벨 검출 입력 2, 전압 zero-cross 후보 |
| PA6 | 20 | ADC2_IN3 | ADC2_IN3 | 추가 아날로그 입력 |
| PA7 | 21 | ADC2_IN4 / COMP2_INP | PWM_VR | PWM/출력 설정 VR 입력 |

> **위상 검출 설계 메모**: `회로설계_참조.md`의 전압/전류 위상 검출 방식을 적용하려면 `PA1=전압 COMP1_INP`, `PA7=전류 COMP2_INP` 조합이 가장 자연스럽다. 현재 회로도에서는 PA7이 `PWM_VR`로 배정되어 있으므로, 실제 zero-cross 위상 검출 회로를 추가할 때는 `PWM_VR` 이동 또는 전류 검출 핀 재배치가 필요하다.
>
> MCU 입력은 정상 동작 중 약 `0.3V~3.0V` 안쪽으로 제한하고, 600V급 진동자 전압은 충분한 고저항 분압/절연/클램프를 거쳐야 한다.

### 2.3 RS485 / Modbus

| 핀 | 물리핀# | 포트/모드 | 회로도 Net | 기능 |
| --- | ------- | --------- | ---------- | ---- |
| PA2 | 14 | USART2_TX | USART2_TX | RS485 TXD |
| PA3 | 17 | USART2_RX | USART2_RX | RS485 RXD |
| PB0 | 24 | GPIO_Output | DE_RS485 | Driver Enable, HIGH=송신 |
| PB1 | 25 | GPIO_Output | /RE_RS485 | Receiver Enable, LOW=수신 |
| PC4 | 22 | GPIO_Output | RTERM | 종단저항 ON/OFF |
| PC5 | 23 | GPIO_Input | 485BD_DETECT | RS485 옵션보드 감지 |

### 2.4 LCD1602 (4비트 병렬)

| 핀 | 물리핀# | 기능 |
| --- | ------- | ---- |
| PB10 | 30 | LCD RS |
| PB11 | 33 | LCD E |
| PB12 | 34 | LCD D4 |
| PB13 | 35 | LCD D5 |
| PB14 | 36 | LCD D6 |
| PB15 | 37 | LCD D7 |

> RW = GND 고정 (쓰기 전용). 이번 회로도에서는 LCD 핀이 PB10~PB15로 모여 있어 배선이 이전 구성보다 깔끔하다.

### 2.5 버튼 / 외부 제어 / 출력

| 핀 | 물리핀# | I/O | 회로도 Net | 기능 |
| --- | ------- | --- | ---------- | ---- |
| PC0 | 8 | Input | START_STOP | 시작/정지 버튼 |
| PC1 | 9 | Input | MODE | 모드 버튼 |
| PC2 | 10 | Input | UP | 값 증가 |
| PC3 | 11 | Input | DOWN | 값 감소 |
| PC6 | 38 | Input | REMOTE | 외부 리모트 입력 |
| PC7 | 39 | Input | RUN_SW | RUN 스위치 |
| PC8 | 40 | Input | SWEEP_SW | Sweep 스위치 |
| PC9 | 41 | TIM3_CH4(AF2) | PWM_OUTPUT | 외부 PWM 출력 |
| PA10 | 44 | Output | GOING | 운전 상태 출력 |
| PC13 | 2 | Output | END_BZ | 종료 알림 |
| PC14 | 3 | Output | BZ_OUT | 부저 출력 |

### 2.6 디버그 / 시스템

| 핀 | 물리핀# | 기능 | 비고 |
| --- | ------- | ---- | ---- |
| PA13 | 49 | SWDIO | ST-Link 필수, GPIO 사용 금지 |
| PA14 | 50 | SWCLK | ST-Link 필수, GPIO 사용 금지 |
| PB6 | 59 | USART1_TX | USB-UART 디버그 TX |
| PB7 | 60 | USART1_RX | USB-UART 디버그 RX |
| NRST | 7 | RESET | DTR/리셋 회로 연결 가능 |
| PB8 | 61 | BOOT0 / GPIO | 부트 설정 저항 확인 필요 |

### 2.7 여유(Spare) 핀

| 핀 | 물리핀# | 가능한 용도 |
| --- | ------- | ----------- |
| PC15 | 4 | GPIO 또는 LSE 미사용 시 예비 |
| PA4 | 18 | ADC2_IN17, DAC1_OUT1, GPIO |
| PA5 | 19 | ADC2_IN13, DAC1_OUT2, GPIO |
| PB2 | 26 | LPTIM1_OUT, TIM5_CH1, ADC2_IN12 |
| PA11 | 45 | HRTIM1_CHB2, USB_DM, TIM1_CH4 |
| PA12 | 46 | HRTIM1_FLT1, USB_DP, TIM16_CH1 |
| PA15 | 51 | TIM2_CH1, HRTIM1_FLT2, GPIO |
| PC10 | 52 | UART4_TX, USART3_TX, HRTIM1_FLT6 |
| PC11 | 53 | UART4_RX, USART3_RX, HRTIM1_EEV2 |
| PC12 | 54 | UART5_TX, USART3_CK, HRTIM1_EEV1 |
| PD2 | 55 | TIM3_ETR, UART5_RX |
| PB4 | 57 | TIM16_CH1, SPI1_MISO, GPIO |
| PB5 | 58 | SPI1_MOSI, I2C1_SMBA, GPIO |
| PB8 | 61 | I2C1_SCL 또는 BOOT0 옵션 |
| PB9 | 62 | I2C1_SDA, USART3_TX, GPIO |

---

## 3. 핀 사용률 통계

| 항목 | 수량 |
| ---- | ---- |
| 전체 GPIO/AF 가능 핀 | 51 |
| 사용 중 I/O 및 AF 핀 | 약 34 |
| 여유(Spare) 핀 | 약 15 |
| HSE 클럭 핀 | 2 |
| 전원/GND/VREF/VDDA 핀 | 13 |
| 디버그 전용 SWD | 2 |
| 주요 변경 | LCD=PB10~PB15, RS485 DE/RE=PB0/PB1, HRTIM=PA8/PA9 |

---

## 4. PCB 아트웍 배선 가이드

```text
좌측 핀: PC0~PC3 버튼, PC4~PC9 외부 제어/RS485/PWM, PC13~PC14 부저
우측 핀: PA0~PA7 아날로그/USART2, PA8~PA10 HRTIM/GOING, PA13~PA14 SWD
하측 핀: PB10~PB15 LCD 6핀 묶음
상측 핀: PB3 SONIC_ON, PB6~PB7 USART1, PB8~PB9 예비/I2C 후보
```

### PCB 부품 배치 권장

| PCB 영역 | 배치 부품 | MCU 핀 |
| -------- | --------- | ------ |
| 조작부 | START/STOP, MODE, UP, DOWN | PC0~PC3 |
| RS485 커넥터/트랜시버 | USART2, DE, /RE, RTERM, Board Detect | PA2, PA3, PB0, PB1, PC4, PC5 |
| 아날로그 검출부 | 공진/레벨 검출, PWM_VR | PA0, PA1, PA6, PA7 |
| 게이트 드라이버 근처 | HRTIM 출력, SONIC_ON | PA8, PA9, PB3 |
| LCD 커넥터 | LCD1602 4bit | PB10~PB15 |
| 디버그 커넥터 | SWD, USART1, NRST | PA13, PA14, PB6, PB7, NRST |

### 아트웍 주의사항

1. **PA8/PA9 HRTIM 고속 출력**: 게이트 드라이버 INA/INB까지 짧고 나란히 배선한다. 고전압 스위칭 루프와 거리를 두고 연속 GND 플레인 위로 라우팅한다.

2. **PA9 UCPD1_DBCC1 겸용 주의**: USB-C PD를 쓰지 않으면 펌웨어에서 `UCPD1_DBDIS=1` 설정을 누락하지 않는다.

3. **PA0/PA1/PA6/PA7 아날로그 입력 보호**: MCU 핀에는 정상 동작 중 0.3V~3.0V 안쪽의 신호만 들어오도록 분압, bias, 직렬저항, 저용량 클램프를 배치한다.

4. **LCD PB10~PB15 묶음 배선**: LCD 데이터/제어선은 고전압 스위칭 노드와 평행하게 길게 달리지 않도록 한다.

5. **PB8/BOOT0 처리**: PB8을 GPIO/I2C로 쓸지 BOOT0 옵션으로 둘지 확정하고, 부트모드가 떠 있지 않도록 pull-down/pull-up 정책을 명확히 한다.

---

## 5. 이전 문서 대비 변경 이력

| 변경 내용 | 이전 핀/기능 | 현재 핀/기능 | 변경 사유 |
| --------- | ------------ | ------------ | --------- |
| MCU 표기 | STM32G474RCT6 | STM32G474RBT6 | 현재 회로도 U3 표기 반영 |
| 물리 핀 번호 | 예전 문서 번호 | ST LQFP64 번호 | PA8=42, PA9=43 등 실제 핀 번호로 정정 |
| 초음파 HRTIM 출력 | PA8/PA9 유지 | PA8(42)/PA9(43) | 물리 핀 번호 정정 및 CHA1/CHA2 명확화 |
| LCD 핀 | PB12, PB14, PB15, PC6~PC8 | PB10~PB15 | 회로도 기준 LCD 6핀 묶음 반영 |
| RS485 DE/RE | PA5/PA6 | PB0/PB1 | 회로도 기준 변경 |
| 버튼 | BTN_MENU/UP/DOWN/OK | START_STOP/MODE/UP/DOWN | 회로도 Net 이름 반영 |
| 외부 입력 | PB1/PB2/PB10 등 | PC6/PC7/PC8 | REMOTE/RUN_SW/SWEEP_SW 반영 |
| PWM_OUTPUT | PB0 TIM3_CH3 | PC9 TIM3_CH4 후보 | 회로도 Net 반영 |
| 디버그 UART | PA9/PA10 | PB6/PB7 | PA9 HRTIM 사용으로 USART1 이동 |
| SONIC_ON | 미정 | PB3 | 발진 Enable 출력 추가 |
| GOING | PA11 등 혼재 | PA10 | 회로도 Net 반영 |
| 부저 | PC4/PC5 등 | PC13/PC14 | END_BZ/BZ_OUT 반영 |
