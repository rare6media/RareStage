param(
    [string]$Version,
    [string]$Configuration = "Release"
)
# Assembles a runnable RareStage folder next to the source tree (..\RareStage-<version>) from an existing build:
# the renderer, both consoles, the logo asset, prepared shader bytecode (so the first launch does not recompile the
# main shader), the notices and release notes, and a SHA-256 list. Run build-release.ps1 first.
$ErrorActionPreference = "Stop"

if (-not $Version) {
    $match = Select-String -Path (Join-Path $PSScriptRoot "CMakeLists.txt") -Pattern 'project\(RareStage VERSION ([0-9]+\.[0-9]+)\.[0-9]+'
    if (-not $match) { throw "Could not read the version from CMakeLists.txt." }
    $Version = $match.Matches[0].Groups[1].Value
}
$build = Join-Path $PSScriptRoot "build\$Configuration"
$out = Join-Path (Split-Path $PSScriptRoot -Parent) "RareStage-$Version"
foreach ($name in "RareStage.exe", "RareStageConsole.exe", "RareStageConsoleClassic.exe") {
    if (-not (Test-Path (Join-Path $build $name))) { throw "$name is missing from $build. Build first." }
}
if (Test-Path $out) { throw "$out already exists. Remove it or pass a different -Version." }

New-Item -ItemType Directory -Force (Join-Path $out "assets") | Out-Null
foreach ($name in "RareStage.exe", "RareStageConsole.exe", "RareStageConsoleClassic.exe") {
    Copy-Item (Join-Path $build $name) $out
}
Copy-Item (Join-Path $PSScriptRoot "assets\rare-logo.png") (Join-Path $out "assets")
Copy-Item (Join-Path $PSScriptRoot "README.md") $out
Copy-Item (Join-Path $PSScriptRoot "THIRD_PARTY_NOTICES.md") $out
Copy-Item (Join-Path $PSScriptRoot "SECURITY.md") $out
Copy-Item (Join-Path $PSScriptRoot "docs\RELEASE_0_31.md") (Join-Path $out "RELEASE-NOTES.md")
Copy-Item (Join-Path $PSScriptRoot "docs\GENERATIVE_DIRECTOR.md") $out

# Prepared bytecode for the stage shaders; the renderer loads it from assets\shaders when the source hash matches.
Push-Location $out
try {
    $process = Start-Process (Join-Path $out "RareStage.exe") -ArgumentList "--prepare-shaders" -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) { throw "--prepare-shaders failed with exit code $($process.ExitCode)." }
} finally { Pop-Location }
# The preparation run leaves its own logs behind; they are not part of the package.
Get-ChildItem $out -File | Where-Object { $_.Extension -eq ".log" -or $_.Name -like "RareStage-adapter*" } | Remove-Item -Force
if (-not (Get-ChildItem (Join-Path $out "assets\shaders") -Filter *.rsc -ErrorAction SilentlyContinue)) { throw "No prepared shader bytecode was written." }

Get-ChildItem $out -Recurse -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" } | ForEach-Object {
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower()
    "$hash  $($_.FullName.Substring($out.Length + 1))"
} | Set-Content (Join-Path $out "SHA256SUMS.txt") -Encoding ascii
Write-Host "Packaged RareStage $Version at $out" -ForegroundColor Cyan
