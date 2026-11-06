param(
    [Parameter(Mandatory = $true)]
    [string] $BackupRoot
)

$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$uploadScript = Join-Path $workspace '.vscode\upload-atmega8.ps1'

function global:Mock-Avrdude {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]] $Arguments)

    $global:mockCalls++
    $global:LASTEXITCODE = 0
    if ($Arguments -contains '-e') {
        $global:mockErases++
        for ($j = 0; $j -lt $global:mockEeprom.Length; $j++) {
            $global:mockEeprom[$j] = 255
        }
    }

    for ($i = 0; $i -lt $Arguments.Count; $i++) {
        if ($Arguments[$i] -ne '-U') { continue }
        $i++
        if ($Arguments[$i] -notmatch '^(flash|eeprom|lfuse|hfuse|lock):([rwv]):(.+):([rih])$') {
            throw "Unexpected memory operation: $($Arguments[$i])"
        }
        $memory, $operation, $path, $format = $Matches[1], $Matches[2], $Matches[3], $Matches[4]
        if ($global:mockFailVerification -eq $memory -and $operation -eq 'v') {
            $global:LASTEXITCODE = 1
            return
        }
        if ($operation -eq 'r') {
            switch ($memory) {
                'flash' { [IO.File]::WriteAllBytes($path, [byte[]]::new(8192)) }
                'eeprom' { [IO.File]::WriteAllBytes($path, $global:mockEeprom) }
                'lfuse' {
                    $value = if ($global:mockChangeFuse -and $global:mockCalls -eq 3) { '0xfe' } else { '0xff' }
                    [IO.File]::WriteAllText($path, $value)
                }
                'hfuse' { [IO.File]::WriteAllText($path, '0xc9') }
                'lock' { [IO.File]::WriteAllText($path, $global:mockLock) }
            }
        } elseif ($memory -eq 'eeprom' -and $operation -eq 'w' -and $format -eq 'r') {
            $global:mockEeprom = [IO.File]::ReadAllBytes($path)
        } elseif ($memory -eq 'eeprom' -and $operation -eq 'w' -and $format -eq 'i') {
            $record = (Get-Content -LiteralPath $path)[0]
            if ($record -notmatch '^:0801F80035C6A279D418EB5C[0-9A-F]{2}$') {
                throw "Unexpected marker record: $record"
            }
            $sum = 0
            for ($n = 1; $n -lt $record.Length; $n += 2) {
                $sum += [Convert]::ToInt32($record.Substring($n, 2), 16)
            }
            if (($sum -band 255) -ne 0) { throw 'Invalid marker checksum.' }
            $global:mockMarkerWrites++
        }
    }
}

function Reset-Mock {
    $global:mockCalls = 0
    $global:mockErases = 0
    $global:mockMarkerWrites = 0
    $global:mockFailVerification = ''
    $global:mockChangeFuse = $false
    $global:mockLock = '0xff'
    $global:mockEeprom = [byte[]]::new(512)
    for ($i = 0; $i -lt $global:mockEeprom.Length; $i++) {
        $global:mockEeprom[$i] = 255
    }
    $global:mockEeprom[2] = 123
}

function Expect-Failure {
    $failed = $false
    try {
        & $uploadScript -Avrdude Mock-Avrdude -BackupRoot $BackupRoot
    } catch {
        $failed = $true
        Write-Host "Expected failure: $_"
    }
    if (-not $failed) { throw 'Expected upload to fail.' }
}

Reset-Mock
& $uploadScript -Avrdude Mock-Avrdude -BackupRoot $BackupRoot
if ($global:mockCalls -ne 4 -or $global:mockErases -ne 1 -or
    $global:mockMarkerWrites -ne 1 -or $global:mockEeprom[2] -ne 123) {
    throw 'Successful upload did not preserve EEPROM or follow the expected sequence.'
}

Reset-Mock
$global:mockEeprom[0x1f8] = 1
Expect-Failure
if ($global:mockErases -ne 0 -or $global:mockMarkerWrites -ne 0) {
    throw 'Occupied marker bytes must prevent all writes.'
}

Reset-Mock
$global:mockLock = '0xfc'
Expect-Failure
if ($global:mockErases -ne 0 -or $global:mockMarkerWrites -ne 0) {
    throw 'Programmed lock bits must prevent all writes.'
}

Reset-Mock
$global:mockFailVerification = 'flash'
Expect-Failure
if ($global:mockErases -ne 1 -or $global:mockMarkerWrites -ne 0) {
    throw 'A failed flash verification must not issue an upload marker.'
}

Reset-Mock
$global:mockFailVerification = 'eeprom'
Expect-Failure
if ($global:mockErases -ne 1 -or $global:mockMarkerWrites -ne 0) {
    throw 'A failed EEPROM verification must not issue an upload marker.'
}

Reset-Mock
$global:mockChangeFuse = $true
Expect-Failure
if ($global:mockErases -ne 1 -or $global:mockMarkerWrites -ne 0) {
    throw 'A changed fuse must not issue an upload marker.'
}

Remove-Item Function:\Mock-Avrdude
Write-Host 'Upload script tests passed.'
