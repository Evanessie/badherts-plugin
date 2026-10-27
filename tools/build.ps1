#requires -Version 5.1
<#
    Rebuilds TapeGrain and copies the artefacts into .\dist

    为什么不是直接 cmake 当前目录：
    本项目路径里含中文（桌面 / ds工作区），MinGW 的 make 在把路径转给 cmd/sh 时
    会按 ANSI 代码页做一次编码转换，把中文路径转成乱码，编译必然失败。
    所以当路径不是纯 ASCII 时，脚本会先把工程镜像到 D:\download\vstbuild 再编译，
    最后把产物拷回来。工程如果放在纯 ASCII 路径下，就直接原地编译。
#>
param(
    [switch]$NoMirror
)

$ErrorActionPreference = 'Stop'

$root      = Split-Path -Parent $PSScriptRoot
$isAscii   = $root -match '^[\x20-\x7E]+$'
$mirror    = 'D:\download\vstbuild'
$work      = if ($isAscii -or $NoMirror) { $root } else { $mirror }

Write-Host "source : $root"
Write-Host "work   : $work"

if ($work -ne $root) {
    if (-not (Test-Path $work)) { New-Item -ItemType Directory -Path $work | Out-Null }
    $exclude = @('build', 'dist', 'docs', '.git', 'tools\_dl', 'tools\_tmp')
    $args = @($root, $work, '/E', '/NFL', '/NDL', '/NJH', '/NJS', '/NP')
    foreach ($e in $exclude) { $args += @('/XD', (Join-Path $root $e)) }
    & robocopy @args | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed ($LASTEXITCODE)" }
}

$mingw = Join-Path $work 'tools\w64devkit\w64devkit\bin'
$cmake = Join-Path $mingw 'cmake.exe'

if (-not (Test-Path $cmake)) { throw "toolchain missing: $cmake (run tools\bootstrap.ps1)" }

$env:PATH = "$mingw;$env:PATH"
$env:TMP  = Join-Path $work 'build\tmp'
$env:TEMP = $env:TMP
New-Item -ItemType Directory -Force -Path $env:TMP | Out-Null

$buildDir = Join-Path $work 'build'

& $cmake -S $work -B $buildDir -G 'MinGW Makefiles' `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ `
    '-DCMAKE_CXX_FLAGS=-include cstring -include cstdint -include cstdio -include cstdlib'

if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

& $cmake --build $buildDir -j $env:NUMBER_OF_PROCESSORS
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null

# copy every VST3 bundle and standalone player the build produced
$bundles = Get-ChildItem -Path $buildDir -Recurse -Depth 4 -Directory -Filter '*.vst3' |
           Where-Object { $_.FullName -like '*_artefacts*' }

$players = Get-ChildItem -Path $buildDir -Recurse -Depth 4 -File -Filter '*.exe' |
           Where-Object { $_.FullName -like '*_artefacts*' }

foreach ($bundle in $bundles) {
    $dest = Join-Path $dist $bundle.Name
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    Copy-Item -Recurse -Force $bundle.FullName $dist
    Write-Host "  vst3 -> $($bundle.Name)"
}

foreach ($player in $players) {
    Copy-Item -Force $player.FullName $dist
    Write-Host "  exe  -> $($player.Name)"
}

Write-Host ""
Write-Host "done ->" $dist
Write-Host "install with:  powershell -ExecutionPolicy Bypass -File tools\install.ps1"
