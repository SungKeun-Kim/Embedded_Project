# STM32G474RETx LQFP64 CubeMX 핀 설정표

> 목적: STM32CubeIDE .ioc 설정 시 바로 복사해 적용할 수 있는 64핀 기준 표
> 기준 소스: include/config.h, src/gpio_init.c

---

## 빠른 사용 순서

1. MCU 선택: STM32G474RETx (LQFP64)
2. SYS: Debug = Serial Wire
3. RCC: HSE = Crystal/Ceramic Resonator (PF0/PF1)
4. 아래 표대로 핀별 Peripheral/Mode/User Label 적용
5. Project Manager -> Toolchain/IDE: CMake

---

## CubeMX 복붙용 핀 설정표

| Pin  | User Label     | CubeMX 설정 | Pull    | Speed     | 비고                   |
| ---- | -------------- | ----------- | ------- | --------- | ---------------------- |
| PA0  | ADC_FWD        | ADC1_IN1    | No pull | -         | 순방향 전력 검출       |
| PA1  | ADC_REF        | ADC1_IN2    | No pull | -         | 역방향 전력 검출       |
| PA2  | MODBUS_TX      | USART2_TX   | No pull | High      | RS-485 TX              |
| PA3  | MODBUS_RX      | USART2_RX   | Pull-up | High      | RS-485 RX              |
| PA4  | BUCK_DAC       | DAC1_OUT1   | No pull | -         | BUCK FB 제어           |
| PA6  | ADC_CUR        | ADC2_IN3    | No pull | -         | 출력 전류              |
| PA7  | ADC_VOL        | ADC2_IN4    | No pull | -         | 출력 전압              |
| PA8  | MS_PWM         | HRTIM1_CHA1 | No pull | Very High | AF13                   |
| PA9  | MS_PWMN        | HRTIM1_CHA2 | No pull | Very High | AF13, 상보 출력        |
| PA10 | RELAY_LOW      | GPIO_Output | No pull | Low       | Push-Pull              |
| PA11 | RELAY_GO       | GPIO_Output | No pull | Low       | Push-Pull              |
| PA12 | RELAY_OPERATE  | GPIO_Output | No pull | Low       | Push-Pull              |
| PA13 | SWDIO          | SYS_SWDIO   | -       | -         | 디버그 전용            |
| PA14 | SWCLK          | SYS_SWCLK   | -       | -         | 디버그 전용            |
| PA15 | EXT_REMOTE     | GPIO_Input  | Pull-up | -         | 외부 REMOTE            |
| PB0  | MODBUS_DE      | GPIO_Output | No pull | High      | RS-485 DE              |
| PB1  | MODBUS_RE_N    | GPIO_Output | No pull | High      | RS-485 /RE             |
| PB3  | RELAY_HIGH     | GPIO_Output | No pull | Low       | Push-Pull              |
| PB4  | RELAY_TRANS    | GPIO_Output | No pull | Low       | Push-Pull              |
| PB6  | DEBUG_TX       | USART1_TX   | No pull | High      | 디버그 UART            |
| PB7  | DEBUG_RX       | USART1_RX   | No pull | High      | 디버그 UART            |
| PB10 | DISP_D0        | GPIO_Output | No pull | Medium    | 공유 DATA 버스         |
| PB11 | DISP_D1        | GPIO_Output | No pull | Medium    | 공유 DATA 버스         |
| PB12 | DISP_D2        | GPIO_Output | No pull | Medium    | 공유 DATA 버스         |
| PB13 | DISP_D3        | GPIO_Output | No pull | Medium    | 공유 DATA 버스         |
| PB14 | DISP_D4        | GPIO_Output | No pull | Medium    | 공유 DATA 버스         |
| PB15 | DISP_D5        | GPIO_Output | No pull | Medium    | 공유 DATA 버스         |
| PC0  | BTN_START_STOP | GPIO_Input  | Pull-up | -         | Active Low             |
| PC1  | BTN_MODE       | GPIO_Input  | Pull-up | -         | Active Low             |
| PC2  | BTN_UP         | GPIO_Input  | Pull-up | -         | Active Low             |
| PC3  | BTN_DOWN       | GPIO_Input  | Pull-up | -         | Active Low             |
| PC4  | BTN_SET        | GPIO_Input  | Pull-up | -         | Active Low             |
| PC5  | BUZZER         | GPIO_Output | No pull | Low       | Push-Pull              |
| PC6  | LC_RELAY1      | GPIO_Output | No pull | Low       | LC bit0                |
| PC7  | LC_RELAY2      | GPIO_Output | No pull | Low       | LC bit1                |
| PC8  | LC_RELAY3      | GPIO_Output | No pull | Low       | LC bit2                |
| PC9  | LC_RELAY4      | GPIO_Output | No pull | Low       | LC bit3                |
| PC10 | EXT_BCD1       | GPIO_Input  | Pull-up | -         | 8POWER BCD1            |
| PC11 | EXT_BCD2       | GPIO_Input  | Pull-up | -         | 8POWER BCD2            |
| PC12 | EXT_BCD3       | GPIO_Input  | Pull-up | -         | 8POWER BCD3            |
| PD2  | SENSOR_INPUT   | GPIO_Input  | Pull-up | -         | 외부 SENSOR            |
| PC14 | CLK_LCD        | GPIO_Output | No pull | Medium    | 74HCT574 LCD 래치 클럭 |
| PC15 | CLK_LED        | GPIO_Output | No pull | Medium    | 74HCT574 LED 래치 클럭 |
| PF0  | HSE_IN         | RCC_OSC_IN  | -       | -         | 8MHz HSE               |
| PF1  | HSE_OUT        | RCC_OSC_OUT | -       | -         | 8MHz HSE               |

---

## Spare 핀 권장 설정

| Pin  | 권장 기본 설정       | 비고                       |
| ---- | -------------------- | -------------------------- |
| PA5  | GPIO_Input (No pull) | 필요 시 ADC2_IN13로 사용   |
| PB2  | GPIO_Input (No pull) | 확장 GPIO                  |
| PB5  | GPIO_Input (No pull) | 확장 GPIO (BUZZER 이동 후) |
| PB8  | GPIO_Input (No pull) | 확장 GPIO                  |
| PB9  | GPIO_Input (No pull) | 확장 GPIO                  |
| PC13 | GPIO_Input (No pull) | 확장 GPIO (SENSOR 이동 후) |

총 여유 핀: 6

---

## CMake로 HTML 생성

아래 명령으로 64핀 문서 HTML을 생성한다.

```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target docs_html
```

생성 파일:

- docs/circuit_ref_64pin.html
- docs/cubemx_64pin_pin_table.html
- docs/cubemx_64pin_onepage_checklist.html

---

## CubeMX 최종 점검 요약 (PD2/PC5 반영)

아래 항목만 최종 확인하면 최근 핀 재배치 이슈를 대부분 차단할 수 있다.

- [ ] SENSOR_INPUT = PD2 (PC13 아님)
- [ ] BUZZER = PC5 (PB5 아님)
- [ ] RELAY_LOW = PA10 (PC15 아님)
- [ ] CLK_LCD/CLK_LED = PC14/PC15 (PB8/PB9 아님)
- [ ] PB8/PB9/PB5/PC13은 Spare로 남아 있음
- [ ] PF0/PF1은 HSE 전용으로 설정됨

---

## 다음 단계: 보드 Bring-up

전원 인가부터 GPIO/통신/HRTIM 확인까지의 실전 스모크 테스트 절차는 아래 문서를 사용한다.

- docs/bringup_64pin_smoke_test.md
