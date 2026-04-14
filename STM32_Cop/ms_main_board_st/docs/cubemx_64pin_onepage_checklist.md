# STM32G474RETx LQFP64 CubeMX 한 장 점검표

> 목적: CubeMX(.ioc) 핀 설정을 1회에 맞추기 위한 최종 체크리스트
> 기준: include/config.h, docs/cubemx_64pin_pin_table.md

---

## 1) 전역 설정

- [ ] MCU: STM32G474RETx (LQFP64)
- [ ] SYS Debug: Serial Wire
- [ ] RCC HSE: Crystal/Ceramic Resonator (PF0/PF1)
- [ ] Project Manager Toolchain: CMake
- [ ] BOOT0(핀 61): GPIO로 사용하지 않음

---

## 2) 필수 핀 매핑 (핵심만)

### HRTIM / 고속

- [ ] PA8 = HRTIM1_CHA1 (AF13, Very High)
- [ ] PA9 = HRTIM1_CHA2 (AF13, Very High)

### LCD/LED 래치 (74HCT574 x2)

- [ ] PB10~PB15 = DISP_D0~D5 (GPIO_Output, Medium)
- [ ] PC14 = CLK_LCD (GPIO_Output, Medium)
- [ ] PC15 = CLK_LED (GPIO_Output, Medium)

### 릴레이 / 부저

- [ ] PA10 = RELAY_LOW (GPIO_Output)
- [ ] PA11 = RELAY_GO (GPIO_Output)
- [ ] PA12 = RELAY_OPERATE (GPIO_Output)
- [ ] PB3 = RELAY_HIGH (GPIO_Output)
- [ ] PB4 = RELAY_TRANS (GPIO_Output)
- [ ] PC5 = BUZZER (GPIO_Output)

### LC 릴레이

- [ ] PC6 = LC_RELAY1
- [ ] PC7 = LC_RELAY2
- [ ] PC8 = LC_RELAY3
- [ ] PC9 = LC_RELAY4

### 통신 / 외부입력

- [ ] PA2 = USART2_TX, PA3 = USART2_RX
- [ ] PB0 = MODBUS_DE, PB1 = MODBUS_RE_N
- [ ] PA15 = EXT_REMOTE
- [ ] PC10 = EXT_BCD1, PC11 = EXT_BCD2, PC12 = EXT_BCD3
- [ ] PD2 = SENSOR_INPUT

### 버튼 / ADC / 디버그

- [ ] PC0~PC4 = BTN_START_STOP/MODE/UP/DOWN/SET (Pull-up)
- [ ] PA0 = ADC1_IN1, PA1 = ADC1_IN2, PA6 = ADC2_IN3, PA7 = ADC2_IN4
- [ ] PB6 = USART1_TX, PB7 = USART1_RX
- [ ] PA13/PA14 = SWD 전용

---

## 3) 충돌 방지 체크

- [ ] RELAY_LOW가 PC15가 아니라 PA10으로 설정되어 있음
- [ ] CLK_LCD/CLK_LED가 PB8/PB9가 아니라 PC14/PC15로 설정되어 있음
- [ ] BUZZER가 PB5가 아니라 PC5로 설정되어 있음
- [ ] SENSOR_INPUT이 PC13이 아니라 PD2로 설정되어 있음
- [ ] PB8/PB9는 Spare(GPIO_Input No pull)로 남겨둠
- [ ] PF0/PF1은 HSE 전용으로 유지

---

## 4) CubeMX 생성 후 코드 확인

- [ ] include/config.h의 핀 정의와 .ioc가 동일
- [ ] src/gpio_init.c의 초기 출력값이 모두 안전 상태(LOW)
- [ ] HRTIM 핀 속도 Very High 적용 확인
- [ ] 버튼 입력 Pull-up 적용 확인

---

## 5) 빌드/문서 생성

```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target docs_html
```

생성 HTML:

- docs/circuit_ref_64pin.html
- docs/cubemx_64pin_pin_table.html
- docs/cubemx_64pin_onepage_checklist.html
