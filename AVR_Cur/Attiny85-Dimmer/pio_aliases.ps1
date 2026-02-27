# PlatformIO Aliases (optional - terminal profile already has build/upload)
$env:PIO = "C:\Users\USER\.platformio\penv\Scripts\pio.exe"

function pio { & $env:PIO $args }
function build { & $env:PIO run }
function upload { & $env:PIO run -t upload }
function clean { & $env:PIO run -t clean }
function monitor { & $env:PIO device monitor }

Write-Host "PlatformIO Ready! Commands: build, upload, clean, monitor" -ForegroundColor Green
