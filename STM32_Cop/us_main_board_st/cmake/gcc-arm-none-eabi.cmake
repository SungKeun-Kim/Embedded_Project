# ----------------------------------------------------------------------------
# ARM GCC 크로스 컴파일 툴체인 파일 (STM32G474RC)
# ----------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m4)

# 툴체인 접두어 — 환경 변수 또는 직접 지정
if(DEFINED ENV{ARM_TOOLCHAIN_PATH})
    set(TOOLCHAIN_PREFIX "$ENV{ARM_TOOLCHAIN_PATH}/bin/arm-none-eabi-")
else()
    set(TOOLCHAIN_PREFIX "arm-none-eabi-")
endif()

if(CMAKE_HOST_WIN32)
    set(TOOLCHAIN_EXECUTABLE_SUFFIX ".exe")
else()
    set(TOOLCHAIN_EXECUTABLE_SUFFIX "")
endif()

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PREFIX}gcc${TOOLCHAIN_EXECUTABLE_SUFFIX}")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PREFIX}gcc${TOOLCHAIN_EXECUTABLE_SUFFIX}")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PREFIX}g++${TOOLCHAIN_EXECUTABLE_SUFFIX}")
set(CMAKE_OBJCOPY      "${TOOLCHAIN_PREFIX}objcopy${TOOLCHAIN_EXECUTABLE_SUFFIX}")
set(CMAKE_OBJDUMP      "${TOOLCHAIN_PREFIX}objdump${TOOLCHAIN_EXECUTABLE_SUFFIX}")
set(CMAKE_SIZE         "${TOOLCHAIN_PREFIX}size${TOOLCHAIN_EXECUTABLE_SUFFIX}")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---------- 공통 플래그 ----------
set(MCU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT
    "${MCU_FLAGS} -fdata-sections -ffunction-sections -Wall -fstack-usage")
set(CMAKE_ASM_FLAGS_INIT
    "${MCU_FLAGS} -fdata-sections -ffunction-sections")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${MCU_FLAGS} -Wl,--gc-sections -specs=nano.specs -lc -lm")

# 빌드 타입별 최적화
set(CMAKE_C_FLAGS_DEBUG          "-Og -g3 -DDEBUG" CACHE STRING "")
set(CMAKE_C_FLAGS_RELEASE        "-O2 -DNDEBUG"    CACHE STRING "")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG" CACHE STRING "")
set(CMAKE_C_FLAGS_MINSIZEREL     "-Os -DNDEBUG"    CACHE STRING "")
