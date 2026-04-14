cmake_minimum_required(VERSION 3.22)

# ---------- C 표준 ----------
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ---------- 빌드 타입 ----------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# ---------- 툴체인 ----------
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-arm-none-eabi.cmake"
    CACHE FILEPATH "ARM 툴체인 파일")

# clangd / VS Code IntelliSense 지원
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# ---------- 프로젝트 ----------
project(ms_main_board VERSION 1.0.0 LANGUAGES C ASM)
message("Build type: ${CMAKE_BUILD_TYPE}")

# ---------- CubeMX 자동생성 vs 수동 구성 선택 ----------
# cmake/stm32cubemx/ 디렉토리가 존재하면 CubeMX 생성 구성을 사용하고,
# 없으면 Drivers/ 디렉토리의 HAL/CMSIS를 직접 참조한다.
# 회로 설계 완료 후 CubeMX → CMake 프로젝트 생성 시 자동 전환됨.
set(CUBEMX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stm32cubemx")
set(DRIVERS_HAL_CORE "${CMAKE_CURRENT_SOURCE_DIR}/Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c")
set(SKIP_FIRMWARE_TARGET FALSE)

if(NOT EXISTS "${CUBEMX_DIR}/CMakeLists.txt" AND NOT EXISTS "${DRIVERS_HAL_CORE}")
    message(WARNING "-- [MODE] CubeMX/Drivers 소스 미발견: 펌웨어 타겟 생성을 건너뜁니다. (docs_html 사용 가능)")
    set(SKIP_FIRMWARE_TARGET TRUE)
endif()

if(EXISTS "${CUBEMX_DIR}/CMakeLists.txt")
    # ============================================================
    # 모드 A: STM32CubeMX 자동 생성 (HAL/CMSIS/링커/스타트업 위임)
    # ============================================================
    message("-- [MODE] CubeMX 자동 생성 구성 사용 (cmake/stm32cubemx)")
    add_subdirectory(${CUBEMX_DIR} stm32cubemx)

    add_executable(${PROJECT_NAME})

    # 애플리케이션 소스
    file(GLOB APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
    target_sources(${PROJECT_NAME} PRIVATE ${APP_SOURCES})

    target_include_directories(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    # CubeMX 라이브러리 링크 (HAL, CMSIS, 링커 스크립트 모두 포함)
    target_link_libraries(${PROJECT_NAME} stm32cubemx)

    # 잘못된 libob.a 의존성 제거 (cpp 파일 사용 시)
    list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

elseif(NOT SKIP_FIRMWARE_TARGET)
    # ============================================================
    # 모드 B: 수동 구성 (Drivers/ 직접 참조)
    # ============================================================
    message("-- [MODE] 수동 구성 (Drivers/ 직접 참조)")

    set(DRIVERS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Drivers")

    # CMSIS
    set(CMSIS_INCLUDE_DIRS
        "${DRIVERS_DIR}/CMSIS/Include"
        "${DRIVERS_DIR}/CMSIS/Device/ST/STM32G4xx/Include"
    )
    set(CMSIS_SOURCES
        "${DRIVERS_DIR}/CMSIS/Device/ST/STM32G4xx/Source/Templates/system_stm32g4xx.c"
    )

    # HAL — 사용하는 모듈만 포함 (Flash/RAM 절약)
    set(HAL_DIR "${DRIVERS_DIR}/STM32G4xx_HAL_Driver")
    set(HAL_INCLUDE_DIRS "${HAL_DIR}/Inc")
    set(HAL_SOURCES
        "${HAL_DIR}/Src/stm32g4xx_hal.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_cortex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_rcc.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_rcc_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_gpio.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_gpio_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_tim.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_tim_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_uart.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_uart_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_adc.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_adc_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_dma.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_dma_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_exti.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_flash.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_flash_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_flash_ramfunc.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_pwr.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_pwr_ex.c"
        "${HAL_DIR}/Src/stm32g4xx_hal_hrtim.c"
    )

    # 애플리케이션 소스
    set(APP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include")
    file(GLOB APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

    # 스타트업 어셈블리
    set(STARTUP_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_stm32g474xx.s")

    # 링커 스크립트
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/ldscripts/STM32G474METx_FLASH.ld")

    # 실행 파일
    add_executable(${PROJECT_NAME}
        ${APP_SOURCES}
        ${HAL_SOURCES}
        ${CMSIS_SOURCES}
        ${STARTUP_SOURCE}
    )

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${APP_INCLUDE_DIRS}
        ${HAL_INCLUDE_DIRS}
        ${CMSIS_INCLUDE_DIRS}
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        USE_HAL_DRIVER
        STM32G474xx
    )

    # 링커 스크립트 지정
    target_link_options(${PROJECT_NAME} PRIVATE
        -T${LINKER_SCRIPT}
        -Wl,-Map=${PROJECT_NAME}.map,--cref
    )

    # 링커 스크립트 변경 시 재링크
    set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_DEPENDS "${LINKER_SCRIPT}"
    )

else()
    message("-- [MODE] 문서 전용 구성 (펌웨어 타겟 생략)")
endif()

# ---------- 공통 설정 (양쪽 모드 공유) ----------
if(TARGET ${PROJECT_NAME})
    set_target_properties(${PROJECT_NAME} PROPERTIES
        SUFFIX ".elf"
    )

    # ---------- 포스트 빌드: bin / hex 생성 ----------
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
        COMMAND ${CMAKE_SIZE}    $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "=== 바이너리 생성 완료 ==="
    )

    # ---------- 커스텀 타겟: Flash 업로드 (ST-Link) ----------
    find_program(STFLASH st-flash)
    if(STFLASH)
        add_custom_target(flash
            COMMAND ${STFLASH} write ${PROJECT_NAME}.bin 0x08000000
            DEPENDS ${PROJECT_NAME}
            COMMENT "=== ST-Link으로 플래시 업로드 ==="
        )
    endif()

    find_program(OPENOCD openocd)
    if(OPENOCD)
        add_custom_target(flash_openocd
            COMMAND ${OPENOCD}
                -f interface/stlink.cfg
                -f target/stm32g4x.cfg
                -c "program ${PROJECT_NAME}.elf verify reset exit"
            DEPENDS ${PROJECT_NAME}
            COMMENT "=== OpenOCD로 플래시 업로드 ==="
        )
    endif()
endif()

# ---------- 문서 생성 타겟 (Markdown -> HTML) ----------
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    set(DOCS_MD_TO_HTML_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/docs/md_to_html.py")

    set(CIRCUIT_REF_64_MD   "${CMAKE_CURRENT_SOURCE_DIR}/docs/circuit_ref_64pin.md")
    set(CIRCUIT_REF_64_HTML "${CMAKE_CURRENT_SOURCE_DIR}/docs/circuit_ref_64pin.html")
    set(CUBEMX_64_MD        "${CMAKE_CURRENT_SOURCE_DIR}/docs/cubemx_64pin_pin_table.md")
    set(CUBEMX_64_HTML      "${CMAKE_CURRENT_SOURCE_DIR}/docs/cubemx_64pin_pin_table.html")
    set(CUBEMX_64_CHECKLIST_MD   "${CMAKE_CURRENT_SOURCE_DIR}/docs/cubemx_64pin_onepage_checklist.md")
    set(CUBEMX_64_CHECKLIST_HTML "${CMAKE_CURRENT_SOURCE_DIR}/docs/cubemx_64pin_onepage_checklist.html")

    add_custom_command(
        OUTPUT "${CIRCUIT_REF_64_HTML}"
        COMMAND ${Python3_EXECUTABLE} "${DOCS_MD_TO_HTML_SCRIPT}"
                --src "${CIRCUIT_REF_64_MD}"
                --dst "${CIRCUIT_REF_64_HTML}"
        DEPENDS "${DOCS_MD_TO_HTML_SCRIPT}" "${CIRCUIT_REF_64_MD}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "=== circuit_ref_64pin.html 생성 ==="
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${CUBEMX_64_HTML}"
        COMMAND ${Python3_EXECUTABLE} "${DOCS_MD_TO_HTML_SCRIPT}"
                --src "${CUBEMX_64_MD}"
                --dst "${CUBEMX_64_HTML}"
        DEPENDS "${DOCS_MD_TO_HTML_SCRIPT}" "${CUBEMX_64_MD}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "=== cubemx_64pin_pin_table.html 생성 ==="
        VERBATIM
    )

    add_custom_command(
        OUTPUT "${CUBEMX_64_CHECKLIST_HTML}"
        COMMAND ${Python3_EXECUTABLE} "${DOCS_MD_TO_HTML_SCRIPT}"
                --src "${CUBEMX_64_CHECKLIST_MD}"
                --dst "${CUBEMX_64_CHECKLIST_HTML}"
        DEPENDS "${DOCS_MD_TO_HTML_SCRIPT}" "${CUBEMX_64_CHECKLIST_MD}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "=== cubemx_64pin_onepage_checklist.html 생성 ==="
        VERBATIM
    )

    add_custom_target(docs_html
        DEPENDS "${CIRCUIT_REF_64_HTML}" "${CUBEMX_64_HTML}" "${CUBEMX_64_CHECKLIST_HTML}"
    )
else()
    message(STATUS "-- [DOCS] Python3 미발견: docs_html 타겟 비활성화")
endif()
