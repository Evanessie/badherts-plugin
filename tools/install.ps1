#requires -Version 5.1
<#
    Copies every built plugin in .\dist into the system VST3 folder.
    Needs an elevated PowerShell (Program Files is admin-only).
#>
param(
    [string]$Target = 'C:\Program Files\Common Files\VST3'
)

$ErrorActionPreference = 'Stop'

$dist = Join-Path (Split-Path -Parent $PSScriptRoot) 'dist'
$bundles = Get-ChildItem -Path $dist -Directory -Filter '*.vst3'

if (-not $bundles) {
    throw "no .vst3 found in $dist - run tools\build.ps1 first"
}

if (-not (Test-Path $Target)) {
    New-Item -ItemType Directory -Path $Target -Force | Out-Null
}

foreach ($bundle in $bundles) {
    $dest = Join-Path $Target $bundle.Name

    try {
        if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
        Copy-Item -Recurse -Force $bundle.FullName $dest
        Write-Host "installed -> $dest"
    }
    catch {
        throw "copy failed for $($bundle.Name): $($_.Exception.Message). Re-run from an elevated PowerShell."
    }
}

Write-Host ""
Write-Host "now rescan plugins in your DAW (FL Studio: Options > Manage plugins > Find more plugins)"
