@echo off
setlocal

set "FIRMWARE=%~dp0US_MAIN_BOARD_FACTORY_V1.2.0.hex"
set "PROGRAMMER=%STM32_PROGRAMMER_CLI%"

REM Find STM32CubeProgrammer CLI from Windows PATH.
if defined PROGRAMMER goto programmer_found
for /f "delims=" %%F in ('where STM32_Programmer_CLI.exe 2^>nul') do set "PROGRAMMER=%%F"

REM Check the standalone STM32CubeProgrammer default folder.
if defined PROGRAMMER goto programmer_found
if exist "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" set "PROGRAMMER=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

REM Check all STM32CubeCLT versions installed under C:\ST.
if defined PROGRAMMER goto programmer_found
for /d %%D in ("C:\ST\STM32CubeCLT_*") do if exist "%%~fD\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" set "PROGRAMMER=%%~fD\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

:programmer_found
if not defined PROGRAMMER goto programmer_missing
if not exist "%PROGRAMMER%" goto programmer_missing
if not exist "%FIRMWARE%" goto firmware_missing

echo [1/2] Configure BOOT option bytes...
echo Programmer: %PROGRAMMER%
"%PROGRAMMER%" -c port=SWD mode=UR reset=HWrst freq=100 -ob nSWBOOT0=0 nBOOT0=1
if errorlevel 1 goto failed

echo [2/2] Program and verify factory firmware...
"%PROGRAMMER%" -c port=SWD mode=UR reset=HWrst freq=100 -d "%FIRMWARE%" -v -rst -run
if errorlevel 1 goto failed

echo [PASS] Factory programming completed.
exit /b 0

:programmer_missing
echo [ERROR] STM32_Programmer_CLI.exe not found.
echo Install STM32CubeProgrammer or set STM32_PROGRAMMER_CLI.
exit /b 1

:firmware_missing
echo [ERROR] Factory HEX not found: %FIRMWARE%
exit /b 1

:failed
echo [ERROR] Factory programming failed.
exit /b 1
