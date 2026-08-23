$ErrorActionPreference = "Stop"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found. Install Visual Studio 2022 with Desktop development with C++."
}

cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -A x64
cmake --build "$PSScriptRoot\build" --config Release --parallel

$exe = Join-Path $PSScriptRoot "build\Release\RareStage.exe"
if (-not (Test-Path $exe)) { throw "Build completed without producing RareStage.exe." }
Write-Host "RareStage is ready: $exe" -ForegroundColor Cyan

