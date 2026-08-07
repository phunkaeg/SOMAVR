[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Destination = (Join-Path $env:LOCALAPPDATA "SOMAVR"),
    [switch]$RemoveConfig,
    [switch]$AllowCustomDestination
)

$ErrorActionPreference = "Stop"
$destinationRoot = [System.IO.Path]::GetFullPath($Destination).TrimEnd('\')
$filesystemRoot = [System.IO.Path]::GetPathRoot($destinationRoot).TrimEnd('\')
if ($destinationRoot -eq $filesystemRoot) {
    throw "Refusing to uninstall from a filesystem root: $destinationRoot"
}
$expectedDestination = [System.IO.Path]::GetFullPath(
    (Join-Path $env:LOCALAPPDATA "SOMAVR")).TrimEnd('\')
if (-not $destinationRoot.Equals(
        $expectedDestination, [System.StringComparison]::OrdinalIgnoreCase) -and
    -not $AllowCustomDestination) {
    throw "Custom uninstall destinations require -AllowCustomDestination: $destinationRoot"
}

function Get-ManagedFileAllowlist {
    return @(
        "somavr.dll",
        "somavr_injector.exe",
        "somavr_dumper.exe",
        "openxr_loader.dll",
        "somavr_build_flavor.txt",
        "somavr_build_manifest.txt",
        "somavr.defaults.ini",
        "README.md",
        "Install-Or-Update-SOMAVR.ps1",
        "Launch-SOMAVR-Dev.ps1",
        "Uninstall-SOMAVR.ps1",
        "docs/USER_GUIDE.md",
        "docs/CURRENT_STATE.md",
        "docs/TEST_CHECKLISTS.md",
        "docs/SMOKE_TEST_MATRIX.md",
        "SHA256SUMS.txt"
    )
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

if ($PSCmdlet.ShouldProcess($destinationRoot, "Uninstall SOMAVR $($manifest.version)")) {
    foreach ($relative in Get-ManagedFileAllowlist) {
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

    $docsPath = Join-Path $destinationRoot "docs"
    if ((Test-Path -LiteralPath $docsPath -PathType Container) -and
        -not (Get-ChildItem -LiteralPath $docsPath -Force -ErrorAction SilentlyContinue)) {
        Remove-Item -LiteralPath $docsPath -Force
    }

    Write-Host "SOMAVR managed files removed from $destinationRoot"
    Write-Host $(if ($RemoveConfig) { "somavr.ini removed" } else { "somavr.ini preserved" })
}
