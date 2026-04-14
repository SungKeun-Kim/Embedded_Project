# STM32G474RETx LQFP64 핀맵 (메가소닉 메인보드)

> 64pin 전환 기준 마스터 핀맵 문서
> 레거시 80pin 기준은 docs/STM32G474MET6*LQFP80*핀맵.md 참고

---

## 핵심 재배치

| 기능          | 80pin | 64pin |
| ------------- | ----- | ----- |
| LCD 래치 클럭 | PD8   | PC14  |
| LED 래치 클럭 | PD9   | PC15  |
| SENSOR 입력   | PD0   | PD2   |
| LOW_ALARM     | PC15  | PA10  |
| BUZZER        | PB5   | PC5   |

---

## 기능별 핀 할당

### HRTIM

| 핀  | 기능        |
| --- | ----------- |
| PA8 | HRTIM1_CHA1 |
| PA9 | HRTIM1_CHA2 |

### LCD/LED 버스 (74HCT574 x2)

| 핀        | 기능       |
| --------- | ---------- |
| PB10~PB15 | DATA[0..5] |
| PC14      | CLK_LCD    |
| PC15      | CLK_LED    |

### 버튼 (5개)

| 핀  | 기능           |
| --- | -------------- |
| PC0 | BTN_START_STOP |
| PC1 | BTN_MODE       |
| PC2 | BTN_UP         |
| PC3 | BTN_DOWN       |
| PC4 | BTN_SET        |

### RS-485

| 핀  | 기능      |
| --- | --------- |
| PA2 | USART2_TX |
| PA3 | USART2_RX |
| PB0 | DE        |
| PB1 | /RE       |

### ADC

| 핀  | 기능     |
| --- | -------- |
| PA0 | ADC1_IN1 |
| PA1 | ADC1_IN2 |
| PA6 | ADC2_IN3 |
| PA7 | ADC2_IN4 |

### 외부 입력

| 핀   | 기능   |
| ---- | ------ |
| PA15 | REMOTE |
| PC10 | BCD1   |
| PC11 | BCD2   |
| PC12 | BCD3   |
| PD2  | SENSOR |

### 알람 릴레이

| 핀   | 기능             |
| ---- | ---------------- |
| PA11 | RELAY_GO         |
| PA12 | RELAY_OPERATE    |
| PA10 | RELAY_LOW        |
| PB3  | RELAY_HIGH       |
| PB4  | RELAY_TRANSDUCER |

### LC 릴레이

| 핀  | 기능      |
| --- | --------- |
| PC6 | LC_RELAY1 |
| PC7 | LC_RELAY2 |
| PC8 | LC_RELAY3 |
| PC9 | LC_RELAY4 |

### 기타

| 핀   | 기능      |
| ---- | --------- |
| PA4  | DAC1_OUT1 |
| PC5  | BUZZER    |
| PB6  | USART1_TX |
| PB7  | USART1_RX |
| PA13 | SWDIO     |
| PA14 | SWCLK     |
| PF0  | OSC_IN    |
| PF1  | OSC_OUT   |

---

## 여유 핀 (64pin 기준)

| 핀   | 비고           |
| ---- | -------------- |
| PA5  | ADC2_IN13 겸용 |
| PB2  | 확장 GPIO      |
| PB5  | 확장 GPIO      |
| PB8  | 확장 GPIO      |
| PB9  | 확장 GPIO      |
| PC13 | 확장 GPIO      |

총 여유: 6핀
