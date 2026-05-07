# CubeMX `.ioc` 핀 감사 결과

기준 파일:

- `include/config.h`
- `ms_main_board.ioc`

검사 목적:

- 펌웨어에서 실제 사용하는 핀이 `.ioc`에 누락되지 않았는지 확인
- 동일 핀에 다중 Signal 할당 충돌 여부 확인

## 결과 요약

- `config.h` 사용 핀(주요 35개) 대비 `.ioc` Signal 누락: **0건**
- `.ioc` Signal 라인 수: **45**
- 동일 베이스 핀 중복 할당(충돌): **0건**

## 검증 대상 주요 기능군

- HRTIM: `PA8`, `PA9`
- ADC/DAC: `PA0`, `PA1`, `PA4`, `PA6`, `PA7`
- RS-485/USART: `PA2`, `PA3`, `PB0`, `PB1`, `PB6`, `PB7`
- 릴레이 출력: `PA10`, `PA11`, `PA12`, `PB3`, `PB4`, `PC6~PC9`
- 디스플레이 버스: `PB10~PB15`, `PC14`, `PC15`
- 외부 입력/버튼: `PC0~PC4`, `PA15`, `PC10~PC12`, `PD2`
- 부저: `PC5`

## 결론

현재 `.ioc`는 프로젝트 코드 기준 핀맵과 정합성이 맞는 상태입니다.
