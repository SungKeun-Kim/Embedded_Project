@echo off
chcp 65001 >nul
setlocal EnableExtensions DisableDelayedExpansion

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
for /r "C:\ST" %%F in (STM32_Programmer_CLI.exe) do if exist "%%~fF" if not defined PROGRAMMER set "PROGRAMMER=%%~fF"

:programmer_found
if not defined PROGRAMMER goto programmer_missing
if not exist "%PROGRAMMER%" goto programmer_missing
if not exist "%FIRMWARE%" goto firmware_missing
set "EXPECTED_SHA256=a1aee6be79bcb4f09e025a531b798208c120e0dc1c08c041fa70dbb0d66eb5a8"
powershell.exe -NoProfile -NonInteractive -Command "try { if ((Get-FileHash -LiteralPath $env:FIRMWARE -Algorithm SHA256).Hash -ine $env:EXPECTED_SHA256) { exit 1 } } catch { exit 1 }"
if errorlevel 1 goto firmware_mismatch

echo ============================================================
echo  US MAIN BOARD FIELD UPDATE V1.2.0_LCD40003_CURRENT40004_20261016
echo  User settings are preserved. Full chip erase is NOT used.
echo ============================================================
echo Programmer: "%PROGRAMMER%"
echo Firmware  : "%FIRMWARE%"
echo.

REM Writing 전에 초음파 출력을 정지하고 외부 시작 입력을 OFF로 둔다.
echo 초음파 출력을 STOP하고 REMOTE 및 PLC RUN 명령을 OFF로 설정하세요.
echo 업데이트 후 Reset/Run으로 MCU가 다시 실행됩니다.
choice /C YN /N /M "Output stopped and external RUN off? [Y/N]: "
if errorlevel 2 goto cancelled
if errorlevel 1 goto output_stopped
goto cancelled

:output_stopped

"%PROGRAMMER%" -c port=SWD mode=UR reset=HWrst freq=100 -d "%FIRMWARE%" -v -rst -run
if errorlevel 1 goto failed

echo.
echo [PASS] Update, Verify, Reset and Run completed.
echo 기존 주파수, Sweep, 통신 설정은 유지됩니다.
pause
exit /b 0

:cancelled
echo [CANCEL] 출력을 정지한 뒤 다시 실행하세요. Writing은 실행하지 않았습니다.
pause
exit /b 1

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

:firmware_mismatch
echo [ERROR] Firmware SHA-256 mismatch. Use the HEX supplied with this version.
pause
exit /b 1
