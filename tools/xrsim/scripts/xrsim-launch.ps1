# Launch the real injected SOMA build against somavr-xrsim without changing the
# machine-wide OpenXR runtime. The production DLL is used unchanged; only this
# process tree inherits the simulator and optional automation flags.
[CmdletBinding()]
param(
    [string]$GamePath = 'G:\SteamLibrary\steamapps\common\SOMA',
    [string]$Executable = 'Soma_NoSteam.exe',
    [string]$BuildDir,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$DllPath,
    [string]$InjectorPath,
    [string]$Map,
    [string]$MapFolder,
    [string]$GameConfig = 'config/main_init_dev.cfg',
    [bool]$Automation = $true,
    [switch]$NoInstall,
    [switch]$NoWaitSession,
    [int]$WaitSeconds = 120,
    [string]$Dir = "$env:LOCALAPPDATA\SOMAVR\xrsim",
    [string[]]$ExtraArgs = @()
)

$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\..'))
if (-not $BuildDir) { $BuildDir = Join-Path $repo 'build' }
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$GamePath = [IO.Path]::GetFullPath($GamePath)
$Dir = [IO.Path]::GetFullPath($Dir)
$exe = Join-Path $GamePath $Executable
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw "game exe not found: $exe" }
if ([string]::IsNullOrWhiteSpace($Map) -ne [string]::IsNullOrWhiteSpace($MapFolder)) {
    throw 'Map and MapFolder must be supplied together.'
}

if (-not $DllPath) {
    $DllPath = Join-Path $repo 'out\SOMAVR-latest\somavr.dll'
    if (-not (Test-Path -LiteralPath $DllPath)) {
        $DllPath = Join-Path (Join-Path $BuildDir $Configuration) 'somavr.dll'
    }
}
$DllPath = [IO.Path]::GetFullPath($DllPath)
if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "somavr.dll not found: $DllPath"
}
if (-not $InjectorPath) {
    $InjectorPath = Join-Path $repo 'out\SOMAVR-latest\somavr_injector.exe'
    if (-not (Test-Path -LiteralPath $InjectorPath)) {
        $InjectorPath = Join-Path (Join-Path $BuildDir $Configuration) 'somavr_injector.exe'
    }
}
$InjectorPath = [IO.Path]::GetFullPath($InjectorPath)
if (-not (Test-Path -LiteralPath $InjectorPath -PathType Leaf)) {
    throw "somavr_injector.exe not found: $InjectorPath"
}

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run xrsim from a normal PowerShell window; secure OpenXR environment selection is ignored for elevated processes.'
}

if (-not $NoInstall) {
    & (Join-Path $PSScriptRoot 'xrsim-install.ps1') -BuildDir $BuildDir `
        -Configuration $Configuration -Dir $Dir | Out-Null
}
$manifest = Join-Path $Dir 'somavr_xrsim64.json'
if (-not (Test-Path -LiteralPath $manifest)) { throw "xrsim manifest not found: $manifest" }
$statePath = Join-Path $Dir 'state.json'
$commandPath = Join-Path $Dir 'command.txt'
foreach ($stale in @($statePath, $commandPath)) {
    if (Test-Path -LiteralPath $stale) { Remove-Item -LiteralPath $stale -Force }
}

$processName = [IO.Path]::GetFileNameWithoutExtension($Executable)
$existingIds = @(
    Get-Process -Name $processName -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Id }
)
$launchArgs = @('--launch', $exe, $DllPath, '--')
if ($Map) {
    $launchArgs += @('-user', 'Dev', '-cfg', $GameConfig, '-map', $Map,
                     '-mapfolder', $MapFolder)
}
$launchArgs += $ExtraArgs

$savedRuntime = $env:XR_RUNTIME_JSON
$savedDir = $env:SOMAVR_XRSIM_DIR
$savedAutomation = $env:SOMAVR_AUTOMATION
try {
    $env:XR_RUNTIME_JSON = $manifest
    $env:SOMAVR_XRSIM_DIR = $Dir
    $env:SOMAVR_AUTOMATION = if ($Automation) { '1' } else { $null }
    Write-Host "injecting $Executable with runtime somavr-xrsim"
    if ($Map) { Write-Host "direct map: $Map ($MapFolder)" }
    $injectorOutput = @(& $InjectorPath @launchArgs 2>&1)
    $injectorExitCode = $LASTEXITCODE
    foreach ($line in $injectorOutput) { Write-Host $line }
    if ($injectorExitCode -ne 0) { throw "injector exited $injectorExitCode" }
} finally {
    $env:XR_RUNTIME_JSON = $savedRuntime
    $env:SOMAVR_XRSIM_DIR = $savedDir
    $env:SOMAVR_AUTOMATION = $savedAutomation
}

$process = $null
$processDeadline = [DateTime]::UtcNow.AddSeconds(15)
while ([DateTime]::UtcNow -lt $processDeadline -and -not $process) {
    $process = Get-Process -Name $processName -ErrorAction SilentlyContinue |
        Where-Object { $_.Id -notin $existingIds } |
        Sort-Object StartTime -Descending |
        Select-Object -First 1
    if (-not $process) { Start-Sleep -Milliseconds 100 }
}
if (-not $process) { throw "injector succeeded but no new $Executable process appeared" }

$logRoot = Split-Path -Parent $DllPath
$modLog = Join-Path $logRoot 'logs\somavr.log'
if ($NoWaitSession) {
    return [pscustomobject]@{
        Pid = $process.Id; Exe = $exe; Dll = $DllPath; Log = $modLog; Dir = $Dir
    }
}

function Read-XrSimState([string]$Path) {
    for ($attempt = 0; $attempt -lt 5; ++$attempt) {
        try { return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json }
        catch { Start-Sleep -Milliseconds 80 }
    }
    return $null
}

$state = $null
$deadline = [DateTime]::UtcNow.AddSeconds($WaitSeconds)
while ([DateTime]::UtcNow -lt $deadline) {
    $process.Refresh()
    if ($process.HasExited) { throw "SOMA exited before OpenXR bring-up (exit $($process.ExitCode))" }
    if (Test-Path -LiteralPath $statePath) {
        $state = Read-XrSimState $statePath
        if ($state -and $state.sessionRunning -and [int64]$state.frame -gt 0) { break }
    }
    Start-Sleep -Milliseconds 250
}
if (-not $state -or -not $state.sessionRunning) {
    throw "somavr-xrsim did not reach a running session within $WaitSeconds seconds. Check $modLog and $Dir\xrsim.log."
}
if ($state.runtime -ne 'somavr-xrsim') {
    throw "wrong OpenXR runtime: state reported '$($state.runtime)'"
}
$firstFrame = [int64]$state.frame
Start-Sleep -Seconds 1
$state = Read-XrSimState $statePath
if (-not $state -or [int64]$state.frame -le $firstFrame) {
    throw "OpenXR session exists but frames are not advancing from $firstFrame"
}

Write-Host "runtime somavr-xrsim, session $($state.sessionState), frame $($state.frame), pid $($process.Id)"
[pscustomobject]@{
    Pid = $process.Id
    Exe = $exe
    Dll = $DllPath
    Log = $modLog
    Dir = $Dir
    Runtime = $state.runtime
    SessionState = $state.sessionState
    Frame = [int64]$state.frame
    Map = $Map
}
