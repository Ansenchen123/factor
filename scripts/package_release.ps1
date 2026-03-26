param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Version = 'v1.0.0'
)

$ErrorActionPreference = 'Stop'

$buildDir = Join-Path $ProjectRoot 'build'
$exePath = Join-Path $buildDir 'factor_manager.exe'
$dllPath = Join-Path $buildDir 'raylib.dll'
$dataDir = Join-Path $ProjectRoot 'data'
$distRoot = Join-Path $ProjectRoot 'dist'
$packageRoot = Join-Path $distRoot "factor-win64-$Version"
$zipPath = Join-Path $distRoot "factor-win64-$Version.zip"

if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath"
}
if (-not (Test-Path $dllPath)) {
    throw "raylib.dll not found: $dllPath"
}
if (-not (Test-Path $dataDir)) {
    throw "Data directory not found: $dataDir"
}

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
if (Test-Path $packageRoot) {
    Remove-Item -Recurse -Force $packageRoot
}
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

Copy-Item $exePath $packageRoot
Copy-Item $dllPath $packageRoot
Copy-Item $dataDir (Join-Path $packageRoot 'data') -Recurse
Copy-Item (Join-Path $ProjectRoot 'README.md') $packageRoot
Copy-Item (Join-Path $ProjectRoot 'LICENSE') $packageRoot

$launcherNote = @"
Factor Manager Release Package

Run:
  factor_manager.exe

Required files:
  raylib.dll
  data/

This package is self-contained. Keep all files in the same folder structure.
"@
Set-Content -Path (Join-Path $packageRoot 'RUN.txt') -Value $launcherNote -Encoding UTF8

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Compress-Archive -Path "$packageRoot\\*" -DestinationPath $zipPath -Force

Write-Host "Created release folder: $packageRoot"
Write-Host "Created release zip:    $zipPath"
