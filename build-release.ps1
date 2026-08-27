$ErrorActionPreference = "Stop"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake was not found. Install Visual Studio 2022 with Desktop development with C++."
}

cmake -S $PSScriptRoot -B "$PSScriptRoot\build" -A x64
cmake --build "$PSScriptRoot\build" --config Release --parallel

$exe = Join-Path $PSScriptRoot "build\Release\RareStage.exe"
$console = Join-Path $PSScriptRoot "build\Release\RareStageConsole.exe"
if (-not (Test-Path $exe)) { throw "Build completed without producing RareStage.exe." }
if (-not (Test-Path $console)) { throw "Build completed without producing RareStageConsole.exe." }
$assetDirectory = Join-Path $PSScriptRoot "build\Release\assets"
New-Item -ItemType Directory -Force $assetDirectory | Out-Null
Copy-Item (Join-Path $PSScriptRoot "assets\rare-logo.png") (Join-Path $assetDirectory "rare-logo.png") -Force
Write-Host "RareStage is ready: $exe" -ForegroundColor Cyan
Write-Host "RareStage Console is ready: $console" -ForegroundColor Cyan
