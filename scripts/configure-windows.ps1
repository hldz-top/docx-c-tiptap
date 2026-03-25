#Requires -Version 5.1
# UTF-8 with BOM recommended for Chinese Windows + PowerShell 5.1
<#
.SYNOPSIS
  Configure (and optionally build) c-tiptap docx2tiptap on Windows: vcpkg + MSVC x64 + libxml2.

.DESCRIPTION
  - Uses vswhere to find VS or Build Tools and passes CMAKE_GENERATOR_INSTANCE to CMake
    (fixes Build-Tools-only installs that report no Visual Studio instance).
  - Set VCPKG_ROOT to your vcpkg clone; if unset, common paths are tried.
  - 若仓库根目录存在 vcpkg.json：在项目根执行 vcpkg install（清单模式，兼容 VS 自带的仅清单 vcpkg）。
  - 否则：vcpkg install libxml2:x64-windows（经典模式，需完整 vcpkg 克隆）。
  - 可用 -SkipVcpkgInstall 跳过上述步骤。

.PARAMETER NoBuild
  Configure only; do not compile.

.PARAMETER SkipVcpkgInstall
  Skip vcpkg install (use when libxml2 is already installed).

.PARAMETER CMakeGenerator
  Override CMake generator (default: from VS version, e.g. Visual Studio 18 2026 vs 17 2022).

.EXAMPLE
  $env:VCPKG_ROOT = 'D:\app\vcpkg-2026.03.18'
  .\scripts\configure-windows.ps1
#>
param(
    [switch] $NoBuild,
    [switch] $SkipVcpkgInstall,
    [string] $CMakeGenerator = ''
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
    Write-Error "Repository root not found (expected CMakeLists.txt). Script dir: $PSScriptRoot"
}

function Test-InstallHasClHostX64([string]$installRoot) {
    if (-not $installRoot) { return $false }
    $msvcRoot = Join-Path $installRoot 'VC\Tools\MSVC'
    if (-not (Test-Path $msvcRoot)) { return $false }
    foreach ($d in Get-ChildItem $msvcRoot -Directory -ErrorAction SilentlyContinue) {
        $cl = Join-Path $d.FullName 'bin\Hostx64\x64\cl.exe'
        if (Test-Path -LiteralPath $cl) { return $true }
    }
    return $false
}

function Get-CmakeVsGeneratorName([string]$installationVersion) {
    try {
        $verStr = ($installationVersion -split '\+')[0]
        $maj = ([version]$verStr).Major
        if ($maj -ge 18) { return 'Visual Studio 18 2026' }
    }
    catch {}
    return 'Visual Studio 17 2022'
}

function Find-CmakeExecutable {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmd -and $cmd.Source) { return $cmd.Source }
    foreach ($p in @(
            (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'CMake\bin\cmake.exe'),
            (Join-Path $env:LOCALAPPDATA 'Programs\CMake\bin\cmake.exe')
        )) {
        if ($p -and (Test-Path -LiteralPath $p)) { return $p }
    }
    return $null
}

function Find-VsForCmake {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }

    # VS 2026+ may not match the legacy workload component id; use JSON and prefer MSVC on disk.
    $raw = & $vswhere -products * -format json 2>$null
    if ($raw) {
        $list = $raw | ConvertFrom-Json
        if ($list -isnot [array]) { $list = @($list) }
        $sorted = $list | Sort-Object {
            try {
                $s = ($_.installationVersion -split '\+')[0]
                [version]$s
            }
            catch { [version]'0.0' }
        } -Descending

        $complete = $sorted | Where-Object { $_.installationPath -and ($_.isComplete -eq $true) }
        foreach ($inst in $complete) {
            if (-not (Test-Path -LiteralPath $inst.installationPath)) { continue }
            if (Test-InstallHasClHostX64 $inst.installationPath) {
                return @{
                    Path    = $inst.installationPath.Trim()
                    Version = $inst.installationVersion
                }
            }
        }
        # Incomplete installs (e.g. Build Tools mid-setup) may still have cl.exe on disk.
        foreach ($inst in $sorted) {
            if (-not $inst.installationPath) { continue }
            if (-not (Test-Path -LiteralPath $inst.installationPath)) { continue }
            if (Test-InstallHasClHostX64 $inst.installationPath) {
                return @{
                    Path    = $inst.installationPath.Trim()
                    Version = $inst.installationVersion
                }
            }
        }
    }

    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($path) {
        return @{ Path = $path.Trim(); Version = '17.0.0' }
    }
    $path = & $vswhere -latest -products Microsoft.VisualStudio.Product.BuildTools -property installationPath 2>$null
    if ($path) {
        return @{ Path = $path.Trim(); Version = '17.0.0' }
    }
    return $null
}

$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot) {
    foreach ($c in @(
            'D:\app\vcpkg-2026.03.18',
            'D:\vcpkg',
            'C:\vcpkg',
            (Join-Path $env:USERPROFILE 'vcpkg')
        )) {
        if ($c -and (Test-Path (Join-Path $c 'scripts\buildsystems\vcpkg.cmake'))) {
            $vcpkgRoot = $c
            Write-Host "[configure-windows] VCPKG_ROOT not set; using: $vcpkgRoot" -ForegroundColor Yellow
            break
        }
    }
}
if (-not $vcpkgRoot -or -not (Test-Path (Join-Path $vcpkgRoot 'scripts\buildsystems\vcpkg.cmake'))) {
    Write-Error "vcpkg not found. Set VCPKG_ROOT to your vcpkg root, e.g. `$env:VCPKG_ROOT = 'D:\app\vcpkg-2026.03.18'"
}

$env:VCPKG_ROOT = $vcpkgRoot

$vs = Find-VsForCmake
if (-not $vs) {
    Write-Error 'MSVC not found. Install Visual Studio or Build Tools with Desktop development with C++ (MSVC + Windows SDK).'
}
$vsPath = $vs.Path
$vsGen = if ($CMakeGenerator) { $CMakeGenerator } else { Get-CmakeVsGeneratorName $vs.Version }
# Portable VS: "PATH,version=BUILD" (path then comma-separated key=value; BUILD = vswhere installationVersion).
$buildNumber = ($vs.Version -split '\+')[0]
$loc = $vsPath -replace '\\', '/'
$vsInstanceArg = "${loc},version=${buildNumber}"
Write-Host "[configure-windows] VS instance: $vsPath" -ForegroundColor DarkGray
Write-Host "[configure-windows] CMAKE_GENERATOR_INSTANCE: $vsInstanceArg" -ForegroundColor DarkGray
Write-Host "[configure-windows] CMake generator: $vsGen" -ForegroundColor DarkGray

$vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
if (-not $SkipVcpkgInstall -and (Test-Path $vcpkgExe)) {
    $manifest = Join-Path $Root 'vcpkg.json'
    if (Test-Path -LiteralPath $manifest) {
        Write-Host '[configure-windows] vcpkg manifest install (from repo root) ...' -ForegroundColor Cyan
        Push-Location $Root
        try {
            & $vcpkgExe install --triplet x64-windows
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        }
        finally {
            Pop-Location
        }
    }
    else {
        Write-Host '[configure-windows] vcpkg install libxml2:x64-windows ...' -ForegroundColor Cyan
        & $vcpkgExe install libxml2:x64-windows
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
}

$buildDir = Join-Path $Root 'build'
$cmakeCache = Join-Path $buildDir 'CMakeCache.txt'
if (Test-Path $cmakeCache) {
    $cache = Get-Content $cmakeCache -Raw -ErrorAction SilentlyContinue
    if ($cache -match 'CMAKE_GENERATOR:INTERNAL=NMake Makefiles' -or $cache -match 'CMAKE_GENERATOR:INTERNAL=Unix Makefiles') {
        Write-Host "[configure-windows] Removing stale build dir: $buildDir" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $buildDir
    }
}

$toolchain = ($vcpkgRoot -replace '\\', '/') + '/scripts/buildsystems/vcpkg.cmake'
$cmakeArgs = @(
    '-S', $Root,
    '-B', $buildDir,
    '-G', $vsGen,
    '-A', 'x64',
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    '-DVCPKG_TARGET_TRIPLET=x64-windows',
    "-DCMAKE_GENERATOR_INSTANCE=$vsInstanceArg"
)

$cmakeExe = Find-CmakeExecutable
if (-not $cmakeExe) {
    Write-Error 'cmake not found. Add CMake to PATH or install from https://cmake.org/download/'
}
Write-Host "[configure-windows] Using cmake: $cmakeExe" -ForegroundColor DarkGray
Write-Host "[configure-windows] cmake $($cmakeArgs -join ' ')" -ForegroundColor Cyan
# Use call operator (not Start-Process -ArgumentList): PS 5.1 splits -G "Visual Studio 18 2026" incorrectly otherwise.
& $cmakeExe @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $NoBuild) {
    Write-Host '[configure-windows] cmake --build (Release) ...' -ForegroundColor Cyan
    & $cmakeExe --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $cliExe = Join-Path $buildDir 'Release\docx2tiptap_cli.exe'
    Write-Host "[configure-windows] OK. CLI: $cliExe" -ForegroundColor Green
}
