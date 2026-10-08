@echo off
chcp 65001 >nul
setlocal

set "FIRMWARE=%~dp0US_MAIN_BOARD_UPDATE_V1.2.0.hex"
set "PROGRAMMER=%STM32_PROGRAMMER_CLI%"

REM Windows PATH에서 STM32CubeProgrammer CLI를 찾는다.
if defined PROGRAMMER goto programmer_found
for /f "delims=" %%F in ('where STM32_Programmer_CLI.exe 2^>nul') do set "PROGRAMMER=%%F"

REM 이 프로젝트 PC에서 사용하는 공통 경로를 확인한다.
if defined PROGRAMMER goto programmer_found
if exist "C:\ST\bin\STM32_Programmer_CLI.exe" set "PROGRAMMER=C:\ST\bin\STM32_Programmer_CLI.exe"

REM STM32CubeProgrammer 단독 설치 기본 경로를 확인한다.
if defined PROGRAMMER goto programmer_found
if exist "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" set "PROGRAMMER=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

REM C:\ST 아래의 STM32CubeCLT 설치본을 확인한다.
if defined PROGRAMMER goto programmer_found
for /d %%D in ("C:\ST\STM32CubeCLT_*") do if exist "%%~fD\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" set "PROGRAMMER=%%~fD\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

REM STM32CubeIDE 플러그인에 포함된 Programmer CLI도 마지막으로 확인한다.
if defined PROGRAMMER goto programmer_found
for /r "C:\ST" %%F in (STM32_Programmer_CLI.exe) do if not defined PROGRAMMER set "PROGRAMMER=%%~fF"

:programmer_found
if not defined PROGRAMMER goto programmer_missing
if not exist "%PROGRAMMER%" goto programmer_missing
if not exist "%FIRMWARE%" goto firmware_missing

echo ============================================================
echo  US MAIN BOARD FIELD UPDATE V1.2.0
echo  User settings are preserved. Full chip erase is NOT used.
echo ============================================================
echo Programmer: %PROGRAMMER%
echo Firmware  : %FIRMWARE%
echo.

"%PROGRAMMER%" -c port=SWD mode=UR reset=HWrst freq=100 -d "%FIRMWARE%" -v -rst -run
if errorlevel 1 goto failed

echo.
echo [PASS] Update, Verify, Reset and Run completed.
echo 기존 주파수, Sweep, 통신 설정은 유지됩니다.
pause
exit /b 0

:programmer_missing
echo.
echo [ERROR] STM32_Programmer_CLI.exe를 찾을 수 없습니다.
echo STM32CubeProgrammer를 설치하거나 STM32_PROGRAMMER_CLI 환경변수를 설정하세요.
pause
exit /b 1

:firmware_missing
echo.
echo [ERROR] 업데이트 HEX를 찾을 수 없습니다: %FIRMWARE%
pause
exit /b 1

:failed
echo.
echo [ERROR] 업데이트에 실패했습니다.
echo CubeProgrammer GUI를 종료하고 보드 전원, ST-LINK, SWDIO, SWCLK, NRST, GND를 확인하세요.
pause
exit /b 1
