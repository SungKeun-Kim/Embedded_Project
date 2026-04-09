---
name: stm32-hal-cmake
description: >-
  Guides STM32 HAL firmware and CMake builds in this monorepo (gcc-arm-none-eabi,
  CubeMX-generated vs manual Drivers layout, clangd compile_commands). Use when
  working on STM32_Cop projects, HAL/C drivers, CMakeLists.txt, toolchain files,
  flash/debug targets, or STM32G4 (e.g. G474) peripheral setup.
---

# STM32 HAL + CMake (이 저장소)

## 먼저 할 일

- **STM32G4 메인보드(`STM32_Cop/us_main_board_st`)** 작업이면, 상세 규칙·체크리스트는 반드시 아래 스킬을 **읽고 따른다**.
  - `STM32_Cop/us_main_board_st/.github/skills/stm32g4-dev/SKILL.md`
  - 참고: `references/hal-patterns.md`, `register-quirks.md`, `code-templates.md` (같은 디렉터리)

## 이 저장소의 CMake 패턴 (`us_main_board_st` 기준)

- **툴체인**: 루트 `CMakeLists.txt`에서 `cmake/gcc-arm-none-eabi.cmake` 지정.
- **clangd/IntelliSense**: `CMAKE_EXPORT_COMPILE_COMMANDS` 사용.
- **두 가지 모드**:
  - `cmake/stm32cubemx/CMakeLists.txt`가 있으면 **CubeMX 생성 구성**을 서브디렉터리로 포함하고, 앱은 `src/*.c` + `include/`.
  - 없으면 **`Drivers/` 수동 구성** — CMSIS/HAL 소스를 CMake에서 명시적으로 나열.

## 빌드·플래시 (일반)

프로젝트 루트(해당 `CMakeLists.txt`가 있는 폴더)에서:

```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target flash
cmake --build build --target flash_openocd
```

타깃 이름은 프로젝트에 따라 다를 수 있음 — 루트 `CMakeLists.txt`의 `add_custom_target` 확인.

## 문서·안전

- 아키텍처·모듈 경계: `STM32_Cop/us_main_board_st/docs/ARCHITECTURE.md`
- 안전/동작 제약: `docs/SAFETY.md`
- 동작·프로토콜 변경 시 위 문서와 **충돌 여부**를 점검한다.

## 에이전트 행동 원칙

- HAL 초기화 순서, G4 전용 주의(ADC 캘리브, TIM1 MOE, USART 레지스터명 등)는 **stm32g4-dev 스킬**에 위임하고 중복 설명을 늘리지 않는다.
- 변경은 요청 범위 내로 제한하고, CubeMX 재생성 파일과 수동 코드 경계를 헷갈리지 않는다.
