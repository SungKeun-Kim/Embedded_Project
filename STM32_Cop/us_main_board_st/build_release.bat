@echo off
chcp 65001 >nul
setlocal

for %%D in ("%~dp0.") do set "PROJECT_DIR=%%~fD"
set "CLT_ROOT="

REM C:\ST에 설치된 STM32CubeCLT를 찾는다. 여러 버전이면 마지막 항목을 사용한다.
for /d %%D in ("C:\ST\STM32CubeCLT_*") do set "CLT_ROOT=%%~fD"
if not defined CLT_ROOT goto clt_missing

set "CMAKE_EXE=%CLT_ROOT%\CMake\bin\cmake.exe"
set "NINJA_EXE=%CLT_ROOT%\Ninja\bin\ninja.exe"
set "ARM_TOOLCHAIN_PATH=%CLT_ROOT%\GNU-tools-for-STM32"
set "G4_PACKAGE=%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_G4_V1.6.2"

if not exist "%CMAKE_EXE%" goto clt_missing
if not exist "%NINJA_EXE%" goto clt_missing
if not exist "%ARM_TOOLCHAIN_PATH%\bin\arm-none-eabi-gcc.exe" goto clt_missing
if not exist "%G4_PACKAGE%\Drivers\STM32G4xx_HAL_Driver" goto g4_missing

set "PYTHON_EXE="
for /f "delims=" %%F in ('where python 2^>nul') do if not defined PYTHON_EXE set "PYTHON_EXE=%%~fF"
if defined PYTHON_EXE goto python_found
if exist "%LOCALAPPDATA%\Python\bin\python.exe" set "PYTHON_EXE=%LOCALAPPDATA%\Python\bin\python.exe"
if defined PYTHON_EXE goto python_found
for /d %%D in ("%LOCALAPPDATA%\Programs\Python\Python*") do if exist "%%~fD\python.exe" set "PYTHON_EXE=%%~fD\python.exe"
if not defined PYTHON_EXE goto python_missing

:python_found
for %%D in ("%PYTHON_EXE%") do set "PATH=%%~dpD;%PATH%"

echo ============================================================
echo  US MAIN BOARD RELEASE BUILD + TEST
echo ============================================================
echo STM32CubeCLT : %CLT_ROOT%
echo STM32CubeG4  : %G4_PACKAGE%
echo Project      : %PROJECT_DIR%
echo.

"%CMAKE_EXE%" -S "%PROJECT_DIR%" -B "%PROJECT_DIR%\build" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%NINJA_EXE%" ^
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

:clt_missing
echo [ERROR] C:\ST\STM32CubeCLT_* 설치를 찾지 못했습니다.
echo STM32CubeCLT를 먼저 설치하세요.
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
