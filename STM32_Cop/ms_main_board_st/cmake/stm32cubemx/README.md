# STM32CubeMX Generated Tree

이 디렉토리는 `ms_main_board.ioc`를 STM32CubeMX에서 코드 생성할 때 사용하는 대상 경로입니다.

## CubeMX 권장 설정

- `Project Manager > Toolchain/IDE`: `CMake`
- `Project Manager > Toolchain Folder Location`: `cmake/stm32cubemx`
- `Project Manager > Main Source Folder`: `cmake/stm32cubemx/Core/Src`

## 생성 후 기대 구조

- `cmake/stm32cubemx/Core/Inc`
- `cmake/stm32cubemx/Core/Src`
- `cmake/stm32cubemx/Drivers`
- `cmake/stm32cubemx/CMakeLists.txt`

루트 `CMakeLists.txt`는 `cmake/stm32cubemx/CMakeLists.txt`가 존재하면 CubeMX 생성 구성을 우선 사용하도록 되어 있습니다.
