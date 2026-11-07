# Build and inspect firmware using the tool packages copied with this project.
# Does not upload firmware, write fuses, or replace the release HEX.
[CmdletBinding()]
param(
    [switch]$Clean,
    [string]$Python = (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe')
)

$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$coreDir = Join-Path $projectRoot '.pio\core'
$avrTools = Join-Path $coreDir 'packages\toolchain-atmelavr\bin'
$previousCore = $env:PLATFORMIO_CORE_DIR

foreach ($required in @(
    $Python,
    (Join-Path $coreDir 'platforms\atmelavr\platform.json'),
    (Join-Path $avrTools 'avr-objdump.exe'),
    (Join-Path $projectRoot 'test\check_avr_timing.py')
)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file missing: $required"
    }
}

Push-Location $projectRoot
try {
    $env:PLATFORMIO_CORE_DIR = $coreDir
    if ($Clean) {
        & $Python -m platformio run -e attiny85 -t clean
        if ($LASTEXITCODE -ne 0) { throw 'PlatformIO clean failed.' }
    }
    & $Python -m platformio run -e attiny85
    if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }

    $buildDir = Join-Path $projectRoot '.pio\build\attiny85'
    $disasm = Join-Path $buildDir 'firmware.disasm'
    & (Join-Path $avrTools 'avr-objdump.exe') -d (Join-Path $buildDir 'firmware.elf') |
        Set-Content -LiteralPath $disasm -Encoding UTF8
    if ($LASTEXITCODE -ne 0) { throw 'Disassembly failed.' }

    & $Python (Join-Path $projectRoot 'test\check_avr_timing.py') $disasm
    if ($LASTEXITCODE -ne 0) { throw 'AVR timing check failed. Review before using this HEX.' }

    $hex = Join-Path $buildDir 'firmware.hex'
    $hash = (Get-FileHash -LiteralPath $hex -Algorithm SHA256).Hash
    Write-Host "Built HEX: $hex"
    Write-Host "SHA256: $hash"
    $releaseHex = Join-Path $projectRoot 'firmware.hex'
    if (Test-Path -LiteralPath $releaseHex) {
        if ($hash -eq (Get-FileHash -LiteralPath $releaseHex -Algorithm SHA256).Hash) {
            Write-Host 'MATCH: built HEX equals the root firmware.hex.' -ForegroundColor Green
        } else {
            Write-Host 'CHANGED: built HEX differs from root firmware.hex; review and test this revision.'
        }
    }
    Write-Host 'Build and instruction timing checks passed. Functional/board tests are separate.'
    Write-Host 'Release HEX and device were not modified.'
} finally {
    $env:PLATFORMIO_CORE_DIR = $previousCore
    Pop-Location
}
