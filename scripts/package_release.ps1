param(
    [string]$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path,
    [string]$Version = 'v1.0.1'
)

$ErrorActionPreference = 'Stop'

function Get-CompilerBinDir {
    if ($env:CXX) {
        $candidate = Get-Command $env:CXX -ErrorAction SilentlyContinue
        if ($candidate) {
            return Split-Path -Parent $candidate.Source
        }
    }

    $cxx = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($cxx) {
        return Split-Path -Parent $cxx.Source
    }

    throw 'Unable to locate g++.exe for MinGW runtime DLL resolution.'
}

function Copy-RequiredFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path $Source)) {
        throw "Required file not found: $Source"
    }

    Copy-Item $Source $Destination -Force
}

$buildDir = Join-Path $ProjectRoot 'build'
$exePath = Join-Path $buildDir 'factor_manager.exe'
$raylibDllPath = Join-Path $buildDir 'raylib.dll'
$dataDir = Join-Path $ProjectRoot 'data'
$distRoot = Join-Path $ProjectRoot 'dist'
$packageRoot = Join-Path $distRoot "factor-win64-$Version"
$zipPath = Join-Path $distRoot "factor-win64-$Version.zip"
$compilerBinDir = Get-CompilerBinDir

$runtimeDllMap = @{
    'raylib.dll' = $raylibDllPath
    'libstdc++-6.dll' = Join-Path $compilerBinDir 'libstdc++-6.dll'
    'libgcc_s_seh-1.dll' = Join-Path $compilerBinDir 'libgcc_s_seh-1.dll'
    'libwinpthread-1.dll' = Join-Path $compilerBinDir 'libwinpthread-1.dll'
}

if (-not (Test-Path $exePath)) {
    throw "Executable not found: $exePath"
}
if (-not (Test-Path $dataDir)) {
    throw "Data directory not found: $dataDir"
}

New-Item -ItemType Directory -Force -Path $distRoot | Out-Null
if (Test-Path $packageRoot) {
    Remove-Item -Recurse -Force $packageRoot
}
New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null

Copy-RequiredFile -Source $exePath -Destination $packageRoot
foreach ($entry in $runtimeDllMap.GetEnumerator()) {
    Copy-RequiredFile -Source $entry.Value -Destination (Join-Path $packageRoot $entry.Key)
}

Copy-Item $dataDir (Join-Path $packageRoot 'data') -Recurse
Copy-Item (Join-Path $ProjectRoot 'README.md') $packageRoot
Copy-Item (Join-Path $ProjectRoot 'LICENSE') $packageRoot

$releaseDatabasePath = Join-Path $packageRoot 'data\maintenance\database.json'
$trackedDatabase = git -C $ProjectRoot show 'HEAD:data/maintenance/database.json'
if ($LASTEXITCODE -eq 0 -and $trackedDatabase) {
    Set-Content -Path $releaseDatabasePath -Value $trackedDatabase -Encoding UTF8
}

$launcherNote = @"
Factor Manager Release Package

Run:
  1. Extract the zip to a normal folder first
  factor_manager.exe

Bundled runtime files:
  raylib.dll
  libstdc++-6.dll
  libgcc_s_seh-1.dll
  libwinpthread-1.dll
  data/

This package is self-contained. Keep all files in the same folder structure.
If the app does not open, check factor_startup.log next to the EXE.
"@
Set-Content -Path (Join-Path $packageRoot 'RUN.txt') -Value $launcherNote -Encoding UTF8

if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Compress-Archive -Path "$packageRoot\*" -DestinationPath $zipPath -Force

Write-Host "Created release folder: $packageRoot"
Write-Host "Created release zip:    $zipPath"
