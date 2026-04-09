@echo off
REM ═══════════════════════════════════════════
REM  호스트 PC 단위 테스트 빌드 및 실행
REM  하드웨어(보드) 없이 로직만 검증
REM ═══════════════════════════════════════════

echo [BUILD] 단위 테스트 컴파일 중...
gcc -DUNIT_TEST -Wall -Wextra -o test_logic.exe test_logic.c -lm

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] 컴파일 실패
    exit /b 1
)

echo [RUN] 테스트 실행 중...
echo.
test_logic.exe

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [RESULT] 일부 테스트 실패 — 위 FAIL 항목 확인
    exit /b 1
) else (
    echo.
    echo [RESULT] 모든 테스트 통과
)
