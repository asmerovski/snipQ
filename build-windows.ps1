<#
.SYNOPSIS
    Build snipQ for Windows.
.DESCRIPTION
    Builds snipQ using MSVC or MinGW, then runs windeployqt to create
    a self-contained folder that can be zipped and distributed.
.PARAMETER QtPath
    Path to Qt6 installation, e.g. C:\Qt\6.7.0\msvc2022_64
.PARAMETER Generator
    CMake generator: "MSVC" (default) or "MinGW"
.EXAMPLE
    .\build-windows.ps1 -QtPath "C:\Qt\6.7.0\msvc2022_64"
    .\build-windows.ps1 -QtPath "C:\Qt\6.7.0\mingw_64" -Generator MinGW
#>
param(
    [string]$QtPath = "",
    [ValidateSet("MSVC","MinGW")]
    [string]$Generator = "MSVC",
    [string]$Config = "Release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Auto-detect Qt path ───────────────────────────────────────────────────────
if (-not $QtPath) {
    $candidates = @(
        "C:\Qt\6.7.0\msvc2022_64",
        "C:\Qt\6.6.0\msvc2022_64",
        "C:\Qt\6.5.0\msvc2022_64",
        "C:\Qt\6.7.0\mingw_64",
        "$env:USERPROFILE\Qt\6.7.0\msvc2022_64"
    )
    foreach ($c in $candidates) {
        if (Test-Path "$c\lib\cmake\Qt6") { $QtPath = $c; break }
    }
    if (-not $QtPath) {
        Write-Error "Qt6 not found. Pass -QtPath 'C:\Qt\6.x.x\<arch>'"
    }
}
Write-Host "Using Qt6 at: $QtPath" -ForegroundColor Cyan

# ── Configure ────────────────────────────────────────────────────────────────
$buildDir = "build-win"
Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $buildDir | Out-Null

if ($Generator -eq "MSVC") {
    # Find MSVC vcvars
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -property installationPath
        $vcvars = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
        Write-Host "Using MSVC: $vcvars" -ForegroundColor Cyan
    }
    cmake -B $buildDir -G "Visual Studio 17 2022" -A x64 `
          -DCMAKE_PREFIX_PATH="$QtPath" `
          -DCMAKE_BUILD_TYPE=$Config
    cmake --build $buildDir --config $Config --parallel
    $exePath = "$buildDir\$Config\snipQ.exe"
} else {
    # MinGW — assumes mingw32-make in PATH
    cmake -B $buildDir -G "MinGW Makefiles" `
          -DCMAKE_PREFIX_PATH="$QtPath" `
          -DCMAKE_BUILD_TYPE=$Config
    cmake --build $buildDir --parallel
    $exePath = "$buildDir\snipQ.exe"
}

if (-not (Test-Path $exePath)) {
    Write-Error "Build failed — $exePath not found"
}
Write-Host "Built: $exePath" -ForegroundColor Green

# ── Deploy ───────────────────────────────────────────────────────────────────
$distDir = "dist\snipQ-windows"
Remove-Item -Recurse -Force $distDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $distDir | Out-Null

Copy-Item $exePath $distDir
Copy-Item README.md $distDir -ErrorAction SilentlyContinue

$windeployqt = "$QtPath\bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    Write-Warning "windeployqt not found at $windeployqt — skipping Qt deployment"
} else {
    & $windeployqt --release --no-translations `
        --no-system-d3d-compiler --no-opengl-sw `
        "$distDir\snipQ.exe"
    Write-Host "windeployqt done" -ForegroundColor Green
}

# ── Zip ──────────────────────────────────────────────────────────────────────
$zipName = "snipQ-windows-x64.zip"
Remove-Item $zipName -ErrorAction SilentlyContinue
Compress-Archive -Path "$distDir\*" -DestinationPath $zipName
Write-Host ""
Write-Host "✓ Distribution package: $zipName" -ForegroundColor Green
Write-Host "  Contents: $(Get-ChildItem $distDir | Measure-Object | Select-Object -ExpandProperty Count) files"
Write-Host ""
Write-Host "To run: .\dist\snipQ-windows\snipQ.exe"
