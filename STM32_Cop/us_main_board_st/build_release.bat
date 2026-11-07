@echo off
chcp 65001 >nul
setlocal

for %%D in ("%~dp0.") do set "PROJECT_DIR=%%~fD"
set "CLT_ROOT="
set "IDE_ROOT="
set "TOOLSET_NAME="
set "TOOLSET_ROOT="

REM C:\ST에 설치된 STM32CubeCLT를 찾는다. 여러 버전이면 마지막 항목을 사용한다.
for /d %%D in ("C:\ST\STM32CubeCLT_*") do set "CLT_ROOT=%%~fD"
if defined CLT_ROOT goto use_clt

REM CLT가 없으면 STM32CubeIDE에 포함된 동일 빌드 도구를 사용한다.
for /d %%D in ("C:\ST\STM32CubeIDE_*") do set "IDE_ROOT=%%~fD\STM32CubeIDE"
if not defined IDE_ROOT goto toolset_missing

for /d %%D in ("%IDE_ROOT%\plugins\com.st.stm32cube.ide.mcu.externaltools.cmake.win32_*") do set "CMAKE_EXE=%%~fD\tools\bin\cmake.exe"
for /d %%D in ("%IDE_ROOT%\plugins\com.st.stm32cube.ide.mcu.externaltools.ninja.win32_*") do set "NINJA_EXE=%%~fD\tools\bin\ninja.exe"
for /d %%D in ("%IDE_ROOT%\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.win32_*") do set "ARM_TOOLCHAIN_PATH=%%~fD\tools"
set "TOOLSET_NAME=STM32CubeIDE embedded tools"
set "TOOLSET_ROOT=%IDE_ROOT%"
goto toolset_ready

:use_clt
set "CMAKE_EXE=%CLT_ROOT%\CMake\bin\cmake.exe"
set "NINJA_EXE=%CLT_ROOT%\Ninja\bin\ninja.exe"
set "ARM_TOOLCHAIN_PATH=%CLT_ROOT%\GNU-tools-for-STM32"
set "TOOLSET_NAME=STM32CubeCLT"
set "TOOLSET_ROOT=%CLT_ROOT%"

:toolset_ready
set "G4_PACKAGE=%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_G4_V1.6.2"

if not exist "%CMAKE_EXE%" goto toolset_missing
if not exist "%NINJA_EXE%" goto toolset_missing
if not exist "%ARM_TOOLCHAIN_PATH%\bin\arm-none-eabi-gcc.exe" goto toolset_missing
if not exist "%G4_PACKAGE%\Drivers\STM32G4xx_HAL_Driver" goto g4_missing

set "PYTHON_EXE="
if exist "%LOCALAPPDATA%\Python\bin\python.exe" set "PYTHON_EXE=%LOCALAPPDATA%\Python\bin\python.exe"
if defined PYTHON_EXE goto python_found
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python*") do if exist "%%~fD\python.exe" set "PYTHON_EXE=%%~fD\python.exe"
if defined PYTHON_EXE goto python_found
for /f "delims=" %%F in ('where python 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%~fF"
if not defined PYTHON_EXE goto python_missing

:python_found
for %%D in ("%PYTHON_EXE%") do set "PATH=%%~dpD;%PATH%"

echo ============================================================
echo  US MAIN BOARD RELEASE BUILD + TEST
echo ============================================================
echo Toolset      : %TOOLSET_NAME%
echo Toolset root : %TOOLSET_ROOT%
echo STM32CubeG4  : %G4_PACKAGE%
echo Project      : %PROJECT_DIR%
echo.

"%CMAKE_EXE%" -S "%PROJECT_DIR%" -B "%PROJECT_DIR%\build" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
  -DPYTHON_EXE:FILEPATH="%PYTHON_EXE%" ^
  -DCUBE_G4_FIRMWARE_DIR="%G4_PACKAGE%"
if errorlevel 1 goto failed

"%CMAKE_EXE%" --build "%PROJECT_DIR%\build" --config Release
if errorlevel 1 goto failed

"%CMAKE_EXE%" --build "%PROJECT_DIR%\build" --target test_host
if errorlevel 1 goto failed

echo.
echo [PASS] Release Build와 전체 Host Test가 완료되었습니다.
echo Update HEX : production\US_MAIN_BOARD_UPDATE_V1.2.0.hex
echo Factory HEX: production\US_MAIN_BOARD_FACTORY_V1.2.0.hex
pause
exit /b 0

:toolset_missing
echo [ERROR] STM32 빌드 도구를 찾지 못했습니다.
echo C:\ST\STM32CubeCLT_* 또는 C:\ST\STM32CubeIDE_* 설치를 확인하세요.
pause
exit /b 1

:g4_missing
echo [ERROR] STM32Cube_FW_G4_V1.6.2 Package를 찾지 못했습니다.
echo 예상 위치: %G4_PACKAGE%
pause
exit /b 1

:python_missing
echo [ERROR] Python이 PATH에 없습니다.
echo Python 3를 설치하고 Add Python to PATH를 선택하세요.
pause
exit /b 1

:failed
echo.
echo [ERROR] Build 또는 Test에 실패했습니다. 위 Log를 확인하세요.
pause
exit /b 1
