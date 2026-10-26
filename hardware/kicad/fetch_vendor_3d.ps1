# MeshCrossway vendor 3D model fetcher
# Run from PowerShell after cloning/pulling the repository.
# Downloads manufacturer 3D resources into hardware/kicad/3dmodels/vendor.

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$vendor = Join-Path $here "3dmodels\vendor"
$tmp = Join-Path $env:TEMP "MeshCrossway3D"
New-Item -ItemType Directory -Force -Path $vendor | Out-Null
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

Write-Host "MeshCrossway: downloading vendor 3D resources..."

# Seeed XIAO ESP32-S3 official 3D package
$xiaoZip = Join-Path $tmp "xiao-esp32s3-3d.zip"
Invoke-WebRequest -Uri "https://files.seeedstudio.com/wiki/SeeedStudio-XIAO-ESP32S3/res/seeed-studio-xiao-esp32s3-3d_model.zip" -OutFile $xiaoZip
$xiaoDir = Join-Path $tmp "xiao"
Remove-Item $xiaoDir -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -Path $xiaoZip -DestinationPath $xiaoDir -Force
$xiaoStep = Get-ChildItem $xiaoDir -Recurse -Include *.step,*.stp | Select-Object -First 1
if ($xiaoStep) {
    Copy-Item $xiaoStep.FullName (Join-Path $vendor "XIAO_ESP32S3.step") -Force
    Write-Host "  XIAO ESP32-S3: OK"
} else {
    Write-Warning "XIAO STEP not found in downloaded package."
}

# Seeed Wio-SX1262 official STEP from Seeed OPL KiCad library
$wioUrl = "https://raw.githubusercontent.com/Seeed-Studio/OPL_Kicad_Library/master/Seeed%20Studio%20Wio%20Series/Wio-SX1262/Wio-SX1262_for_XIAO_3D_file.step"
Invoke-WebRequest -Uri $wioUrl -OutFile (Join-Path $vendor "Wio-SX1262_for_XIAO.step")
Write-Host "  Wio-SX1262: OK"

# Waveshare official 2D/3D package
$waveZip = Join-Path $tmp "waveshare-3d.zip"
Invoke-WebRequest -Uri "https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5/ESP32-S3-Touch-LCD-3.5-2Dand3D.zip" -OutFile $waveZip
$waveDir = Join-Path $vendor "Waveshare_ESP32-S3-Touch-LCD-3.5"
Remove-Item $waveDir -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $waveDir | Out-Null
Expand-Archive -Path $waveZip -DestinationPath $waveDir -Force

$waveSteps = @(Get-ChildItem $waveDir -Recurse -File | Where-Object { $_.Extension -match '^\.(step|stp)
Write-Host ""
Write-Host "Done. Vendor files are under:"
Write-Host "  $vendor"
Write-Host ""
Write-Host "The XIAO footprint is already configured to use 3dmodels/vendor/XIAO_ESP32S3.step."
Write-Host "Wio and Waveshare models are downloaded for mechanical review; their final KiCad offsets/orientations must be validated before fabrication."
 })
if ($waveSteps.Count -eq 1) {
    Copy-Item $waveSteps[0].FullName (Join-Path $vendor "Waveshare_ESP32-S3-Touch-LCD-3.5.step") -Force
    Write-Host "  Waveshare STEP normalized: $($waveSteps[0].Name)"
} elseif ($waveSteps.Count -gt 1) {
    $preferred = $waveSteps | Where-Object { $_.Name -match 'ESP32-S3-Touch-LCD-3\.5|3\.5' } | Select-Object -First 1
    if ($preferred) {
        Copy-Item $preferred.FullName (Join-Path $vendor "Waveshare_ESP32-S3-Touch-LCD-3.5.step") -Force
        Write-Host "  Waveshare STEP normalized: $($preferred.Name)"
    } else {
        Write-Warning "Multiple Waveshare STEP files found. No unique preferred file selected."
        $waveSteps | ForEach-Object { Write-Host "    $($_.FullName)" }
    }
} else {
    Write-Warning "No STEP/STP file found in Waveshare package."
}
Write-Host "  Waveshare 2D/3D package: OK"

Write-Host ""
Write-Host "Done. Vendor files are under:"
Write-Host "  $vendor"
Write-Host ""
Write-Host "The XIAO footprint is already configured to use 3dmodels/vendor/XIAO_ESP32S3.step."
Write-Host "Wio and Waveshare models are downloaded for mechanical review; their final KiCad offsets/orientations must be validated before fabrication."
