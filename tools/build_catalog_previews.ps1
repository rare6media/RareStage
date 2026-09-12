$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    $process = Start-Process build/Release/RareStage.exe -ArgumentList '--play-test --catalog-capture' -WindowStyle Hidden -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Catalog rendering failed: $($process.ExitCode)" }
    Add-Type -AssemblyName System.Drawing
    $images = @(Get-ChildItem -LiteralPath RareStage-sequence-validation/catalog -Filter '*.png')
    if ($images.Count -ne 204) { throw "Expected 188 program previews and 16 continuous scene previews" }
    New-Item -ItemType Directory -Force assets/catalog,build/Release/assets/catalog | Out-Null
    foreach ($file in $images) {
        $bitmap = [System.Drawing.Image]::FromFile($file.FullName)
        try { $bitmap.Save((Join-Path $repo "assets/catalog/$($file.BaseName).bmp"), [System.Drawing.Imaging.ImageFormat]::Bmp) }
        finally { $bitmap.Dispose() }
        Copy-Item -LiteralPath "assets/catalog/$($file.BaseName).bmp" -Destination build/Release/assets/catalog/
    }
} finally { Pop-Location }
