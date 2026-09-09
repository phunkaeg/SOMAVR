# xrsim-install.ps1 - write the simulated runtime's manifest and prepare its
# control directory.
#
# NEVER touches the registry. The sim is selected per-process through
# XR_RUNTIME_JSON (the OpenXR loader checks that env var before the registry),
# so the machine's real ActiveRuntime stays on VDXR and putting the Quest 3 on
# still works with no switching.
#
# Usage:
#   .\tools\xrsim\scripts\xrsim-install.ps1
#   .\tools\xrsim\scripts\xrsim-install.ps1 -BuildDir .\build -Configuration Release
[CmdletBinding()]
param(
    [string]$BuildDir,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$Dir = "$env:LOCALAPPDATA\SOMAVR\xrsim",
    [switch]$KeepControlFiles
)

$ErrorActionPreference = 'Stop'

$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$Dir = [IO.Path]::GetFullPath($Dir)
$outDir = Join-Path $BuildDir $Configuration
$dll = Join-Path $outDir 'somavr_xrsim64.dll'

if (-not (Test-Path $dll)) {
    throw "somavr_xrsim64.dll not found in $outDir - configure with SOMAVR_ENABLE_OPENXR=ON and SOMAVR_BUILD_XRSIM=ON, then build it."
}

# --- bitness guard -----------------------------------------------------------
# A wrong-bitness DLL is silently skipped by the loader, which can make a test
# run against the machine's real runtime while the transcript says "sim".
$fs = [System.IO.File]::OpenRead($dll)
try {
    $br = New-Object System.IO.BinaryReader($fs)
    $fs.Seek(0x3C, 'Begin') | Out-Null
    $peOffset = $br.ReadInt32()
    $fs.Seek($peOffset, 'Begin') | Out-Null
    if ($br.ReadUInt32() -ne 0x00004550) { throw "$dll is not a PE image (no PE\0\0 signature)." }
    $machine = $br.ReadUInt16()
} finally { $fs.Close() }

if ($machine -ne 0x8664) {
    throw ("$dll reports PE machine 0x{0:X4}, not 0x8664 (x64). SOMA " -f $machine) +
          "cannot load it, and the loader could quietly fall back to the real runtime."
}

# --- directories -------------------------------------------------------------
New-Item -ItemType Directory -Force -Path $Dir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Dir "capture") | Out-Null

# --- manifest ----------------------------------------------------------------
# ABSOLUTE library_path. A relative one resolves against the loading process's
# working directory, not the manifest's - which is why the game (launched with
# its own cwd) failed to load a relative manifest that somavr_xrsim_smoke accepted.
$manifest = Join-Path $Dir 'somavr_xrsim64.json'
$escaped  = $dll.Replace('\', '\\')
$json     = '{"file_format_version":"1.0.0","runtime":{"name":"somavr-xrsim","library_path":"' + $escaped + '"}}'
# No BOM: the loader's JSON parser chokes on one.
[System.IO.File]::WriteAllText($manifest, $json, [System.Text.UTF8Encoding]::new($false))

# --- stale control files -----------------------------------------------------
if (-not $KeepControlFiles) {
    foreach ($leaf in @("command.txt", "state.json", "ack.txt")) {
        $p = Join-Path $Dir $leaf
        if (Test-Path $p) {
            if ($leaf -eq "command.txt") {
                $stale = (Get-Content $p -Raw).Trim() -replace "`r?`n", " | "
                if ($stale) { Write-Host "cleared a stale command.txt (would have re-applied): $stale" }
            }
            Remove-Item $p -Force
        }
    }
}

Write-Host "manifest: $manifest"
Write-Host "dll:      $dll ($Configuration, x64)"
Write-Host "dir:      $Dir"
Write-Host ""
Write-Host "To use it, launch the game with:  `$env:XR_RUNTIME_JSON = '$manifest'"
Write-Host "(xrsim-launch.ps1 does that for you, and restores it afterwards.)"

[pscustomobject]@{
    Dir      = $Dir
    Manifest = $manifest
    Dll      = $dll
    Config   = $Configuration
    BuiltUtc = (Get-Item $dll).LastWriteTimeUtc
}
