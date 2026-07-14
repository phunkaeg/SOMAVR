[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Destination = (Join-Path $env:LOCALAPPDATA "SOMAVR"),
    [switch]$RemoveConfig
)

$ErrorActionPreference = "Stop"
$destinationRoot = [System.IO.Path]::GetFullPath($Destination).TrimEnd('\')
$filesystemRoot = [System.IO.Path]::GetPathRoot($destinationRoot).TrimEnd('\')
if ($destinationRoot -eq $filesystemRoot) {
    throw "Refusing to uninstall from a filesystem root: $destinationRoot"
}

function Resolve-InstalledChild([string]$RelativePath) {
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $destinationRoot $RelativePath))
    $prefix = $destinationRoot + '\'
    if (-not $candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Install manifest path escapes destination: $RelativePath"
    }
    return $candidate
}

$manifestPath = Join-Path $destinationRoot ".somavr-install.json"
if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "No SOMAVR install manifest found at $manifestPath"
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$managedFiles = @($manifest.managedFiles)

if ($PSCmdlet.ShouldProcess($destinationRoot, "Uninstall SOMAVR $($manifest.version)")) {
    foreach ($relative in $managedFiles) {
        $path = Resolve-InstalledChild (([string]$relative).Replace('/', '\'))
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            Remove-Item -LiteralPath $path -Force
        }
    }
    if ($RemoveConfig) {
        $configPath = Resolve-InstalledChild "somavr.ini"
        if (Test-Path -LiteralPath $configPath -PathType Leaf) {
            Remove-Item -LiteralPath $configPath -Force
        }
    }
    Remove-Item -LiteralPath $manifestPath -Force

    Get-ChildItem -LiteralPath $destinationRoot -Directory -Recurse -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending |
        ForEach-Object {
            if (-not (Get-ChildItem -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue)) {
                Remove-Item -LiteralPath $_.FullName -Force
            }
        }

    Write-Host "SOMAVR managed files removed from $destinationRoot"
    Write-Host $(if ($RemoveConfig) { "somavr.ini removed" } else { "somavr.ini preserved" })
}
