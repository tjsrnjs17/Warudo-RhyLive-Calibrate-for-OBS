param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [string]$BuildDirectory = 'build',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$build = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) { $BuildDirectory } else { Join-Path $root $BuildDirectory }
$qt = (Resolve-Path -LiteralPath $QtRoot).Path
$testing = if ($SkipTests) { 'OFF' } else { 'ON' }
& cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$qt" "-DBUILD_TESTING=$testing"
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& cmake --build $build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
if (-not $SkipTests) {
    $savedPath = $env:PATH
    $savedPlatform = $env:QT_QPA_PLATFORM
    try {
        $env:PATH = (Join-Path $qt 'bin') + ';' + $env:PATH
        $env:QT_QPA_PLATFORM = 'minimal'
        & ctest --test-dir $build -C Release --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    } finally {
        $env:PATH = $savedPath
        $env:QT_QPA_PLATFORM = $savedPlatform
    }
}
Write-Output (Join-Path $build 'Release\warudo-calibrate.dll')
