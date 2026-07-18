[CmdletBinding()]
param(
    [string]$GameRoot = "G:\SteamLibrary\steamapps\common\SOMA",
    [string]$Map,
    [string]$MapFolder,
    [string]$Config = "config/main_init_dev.cfg",
    [string]$DllPath
)

$ErrorActionPreference = "Stop"

$gameExecutable = Join-Path $GameRoot "Soma_NoSteam.exe"
if (-not (Test-Path -LiteralPath $gameExecutable -PathType Leaf)) {
    throw "Soma_NoSteam.exe was not found under: $GameRoot"
}
if ([string]::IsNullOrWhiteSpace($Map) -ne [string]::IsNullOrWhiteSpace($MapFolder)) {
    throw "Map and MapFolder must be supplied together."
}

$packagedInjector = Join-Path $PSScriptRoot "somavr_injector.exe"
if (Test-Path -LiteralPath $packagedInjector -PathType Leaf) {
    $injector = $packagedInjector
} else {
    $repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
    $injector = Join-Path $repositoryRoot "out\SOMAVR-latest\somavr_injector.exe"
}
if (-not (Test-Path -LiteralPath $injector -PathType Leaf)) {
    throw "The stable SOMAVR injector was not found: $injector"
}

$launchArguments = @("--launch", $gameExecutable)
if (-not [string]::IsNullOrWhiteSpace($DllPath)) {
    $launchArguments += [System.IO.Path]::GetFullPath($DllPath)
}
$launchArguments += @("--", "-user", "Dev", "-cfg", $Config)
if (-not [string]::IsNullOrWhiteSpace($Map)) {
    $launchArguments += @("-map", $Map, "-mapfolder", $MapFolder)
}

& $injector @launchArguments
exit $LASTEXITCODE
