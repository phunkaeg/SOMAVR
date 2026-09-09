# Prove the simulated runtime without loading SOMA or somavr.dll.
[CmdletBinding()]
param(
    [string]$BuildDir,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$Dir = "$env:LOCALAPPDATA\SOMAVR\xrsim\selftest",
    [switch]$KeepFiles
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$Dir = [IO.Path]::GetFullPath($Dir)
$smoke = Join-Path (Join-Path $BuildDir $Configuration) 'somavr_xrsim_smoke.exe'
if (-not (Test-Path -LiteralPath $smoke)) {
    throw "somavr_xrsim_smoke.exe not found at $smoke"
}

$install = & (Join-Path $PSScriptRoot 'xrsim-install.ps1') `
    -BuildDir $BuildDir -Configuration $Configuration -Dir $Dir
$stdout = Join-Path $Dir 'smoke.stdout.txt'
$stderr = Join-Path $Dir 'smoke.stderr.txt'
$savedRuntime = $env:XR_RUNTIME_JSON
$savedDir = $env:SOMAVR_XRSIM_DIR
$process = $null
try {
    $env:XR_RUNTIME_JSON = $install.Manifest
    $env:SOMAVR_XRSIM_DIR = $Dir
    $process = Start-Process -FilePath $smoke `
        -ArgumentList @('--frames', '600', '--require-menu-edge') `
        -WorkingDirectory (Split-Path -Parent $smoke) -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr

    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    $statePath = Join-Path $Dir 'state.json'
    $state = $null
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($process.HasExited) { break }
        if (Test-Path -LiteralPath $statePath) {
            try { $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json }
            catch { $state = $null }
            if ($state -and $state.sessionRunning -and [int64]$state.frame -ge 5) { break }
        }
        Start-Sleep -Milliseconds 50
    }
    if (-not $state -or -not $state.sessionRunning) {
        throw "smoke client never reached a running simulated session"
    }

    & (Join-Path $PSScriptRoot 'xrsim-cmd.ps1') -Dir $Dir -Quiet 'btn menu down'
    Start-Sleep -Milliseconds 250
    & (Join-Path $PSScriptRoot 'xrsim-cmd.ps1') -Dir $Dir -Quiet 'btn menu up'

    & (Join-Path $PSScriptRoot 'xrsim-run.ps1') -Dir $Dir -Steps @(
        '@frames 2',
        'head pos 0 1.65 0',
        '@assert runtime eq somavr-xrsim',
        '@assert sessionRunning eq True'
    ) | Out-Null

    $shot = & (Join-Path $PSScriptRoot 'xrsim-shot.ps1') `
        -Out (Join-Path $Dir 'selftest') -Dir $Dir -TimeoutSec 10
    if (-not $process.WaitForExit(20000)) {
        throw "smoke client did not exit within its bounded frame run"
    }
    $out = if (Test-Path -LiteralPath $stdout) {
        Get-Content -LiteralPath $stdout -Raw
    } else { '' }
    $err = if (Test-Path -LiteralPath $stderr) {
        Get-Content -LiteralPath $stderr -Raw
    } else { '' }
    Write-Host $out
    if ($err) { Write-Host $err }
    if ($process.ExitCode -ne 0) { throw "smoke client exited $($process.ExitCode)" }
    if ($out -notmatch 'runtime:\s*somavr-xrsim') {
        throw "XR_RUNTIME_JSON did not select somavr-xrsim"
    }
    if ($out -notmatch 'frames:\s*600') { throw "smoke client did not submit all frames" }
    if ($out -notmatch 'menu-edge:\s*yes') {
        throw "OpenXR boolean action edge was not delivered through xrSyncActions"
    }
    if (-not (Test-Path -LiteralPath $shot.Sbs)) { throw "SBS capture was not written" }
    if ([double]$shot.NonBlackPctL -lt 10 -or [double]$shot.NonBlackPctR -lt 10) {
        throw "capture is unexpectedly black: L=$($shot.NonBlackPctL)% R=$($shot.NonBlackPctR)%"
    }
    if ([int]$shot.ProjViews -ne 2) {
        throw "capture reported $($shot.ProjViews) projection views, expected 2"
    }
    Write-Host "XRSIM SELFTEST PASS: runtime negotiation, WGL swapchains, sequence runner, action edges, 600 frames and stereo PNG capture." -ForegroundColor Green
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    $env:XR_RUNTIME_JSON = $savedRuntime
    $env:SOMAVR_XRSIM_DIR = $savedDir
    if (-not $KeepFiles) {
        Remove-Item -LiteralPath (Join-Path $Dir 'command.txt') -Force -ErrorAction SilentlyContinue
    }
}
