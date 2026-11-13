# Fetch / stage vendor SDKs into third_party and copy runtime DLLs.
# Run from repo root:  powershell -ExecutionPolicy Bypass -File third_party\fetch_sdks.ps1

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $Root "app\imagedetect.pro"))) {
    $Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}
Set-Location $Root
Write-Host "Repo root: $Root"

# --- Snap7 ---
$SnapDir = Join-Path $Root "third_party\snap7\win64"
New-Item -ItemType Directory -Force -Path $SnapDir | Out-Null
$SnapDll = Join-Path $SnapDir "snap7.dll"
if (-not (Test-Path $SnapDll)) {
    Write-Host "Downloading Snap7 official repo (prebuilt win64)..."
    $tmp = Join-Path $env:TEMP "snap7-git"
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    try {
        git clone --depth 1 https://github.com/davenardella/snap7.git $tmp
        $built = Join-Path $tmp "build\bin\win64"
        if (Test-Path (Join-Path $built "snap7.dll")) {
            Copy-Item (Join-Path $built "snap7.dll") $SnapDll -Force
            Copy-Item (Join-Path $built "snap7.lib") (Join-Path $SnapDir "snap7.lib") -Force -ErrorAction SilentlyContinue
            Write-Host "Snap7 DLL staged: $SnapDll"
        } else {
            Write-Host "WARN: win64 prebuild not found in clone; build VS solution under build\windows"
        }
    } catch {
        Write-Host "WARN: Snap7 download failed: $($_.Exception.Message)"
        Write-Host "Manual: place snap7.dll into third_party\snap7\win64\"
    }
} else {
    Write-Host "Snap7 already present: $SnapDll"
}

# --- Hikvision MVS (already installed on this machine typically) ---
$MvsDll = "C:\Program Files (x86)\Common Files\MVS\Runtime\Win64_x64\MvCameraControl.dll"
$OutDir = Join-Path $Root "app\release"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (Test-Path $MvsDll) {
    Copy-Item $MvsDll $OutDir -Force
    Write-Host "Copied MvCameraControl.dll -> app\release"
} else {
    Write-Host "WARN: MVS runtime not found. Install Hikvision MVS, then re-run."
}

# --- Onna ---
$OnnaBin = Join-Path $Root "third_party\onna\bin"
New-Item -ItemType Directory -Force -Path $OnnaBin | Out-Null
Write-Host "Onna: place vendor OnnaCamera.dll (exports in third_party\onna\include\onna_camera_c_api.h) into:"
Write-Host "  $OnnaBin"
Write-Host "Or set ONNA_SDK_DIR to the folder containing OnnaCamera.dll"

# Copy snap7 next to exe if present
if (Test-Path $SnapDll) {
    Copy-Item $SnapDll $OutDir -Force
    Write-Host "Copied snap7.dll -> app\release"
}

Write-Host "Done."
