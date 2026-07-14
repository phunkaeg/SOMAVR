param(
    [Parameter(Mandatory = $true)]
    [string]$Installer,
    [Parameter(Mandatory = $true)]
    [string]$Uninstaller
)

$ErrorActionPreference = "Stop"
$root = Join-Path $env:TEMP ("somavr-installer-test-" + [Guid]::NewGuid().ToString("N"))
$package = Join-Path $root "package"
$destination = Join-Path $root "installed"

function Get-Sha256Hex([string]$Path) {
    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $algorithm = [System.Security.Cryptography.SHA256]::Create()
        try {
            return ([System.BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
        } finally {
            $algorithm.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Write-PackageChecksums {
    Get-ChildItem -LiteralPath $package -File |
        Where-Object Name -ne "SHA256SUMS.txt" |
        Sort-Object Name |
        ForEach-Object {
            $relative = $_.Name
            $hash = Get-Sha256Hex $_.FullName
            "$hash  $relative"
        } |
        Set-Content -LiteralPath (Join-Path $package "SHA256SUMS.txt") -Encoding ascii
}

function Assert-Text([string]$Path, [string]$Expected, [string]$Message) {
    $actual = (Get-Content -LiteralPath $Path -Raw).Trim()
    if ($actual -ne $Expected) {
        throw "$Message (expected '$Expected', got '$actual')"
    }
}

try {
    New-Item -ItemType Directory -Path $package -Force | Out-Null
    Copy-Item -LiteralPath $Installer -Destination (Join-Path $package "Install-Or-Update-SOMAVR.ps1")
    Copy-Item -LiteralPath $Uninstaller -Destination (Join-Path $package "Uninstall-SOMAVR.ps1")
    Set-Content -LiteralPath (Join-Path $package "somavr.dll") -Value "dll-v1"
    Set-Content -LiteralPath (Join-Path $package "somavr_injector.exe") -Value "injector-v1"
    Set-Content -LiteralPath (Join-Path $package "somavr_build_manifest.txt") -Value "version=0.32.0-test1"
    Set-Content -LiteralPath (Join-Path $package "somavr.ini") -Value "UserScale=1.0"
    Set-Content -LiteralPath (Join-Path $package "legacy.txt") -Value "legacy"
    Write-PackageChecksums

    & (Join-Path $package "Install-Or-Update-SOMAVR.ps1") -Destination $destination
    Assert-Text (Join-Path $destination "somavr.ini") "UserScale=1.0" "Fresh config mismatch"

    Set-Content -LiteralPath (Join-Path $destination "somavr.ini") -Value "UserScale=1.75"
    Remove-Item -LiteralPath (Join-Path $package "legacy.txt")
    Set-Content -LiteralPath (Join-Path $package "somavr.dll") -Value "dll-v2"
    Set-Content -LiteralPath (Join-Path $package "somavr_build_manifest.txt") -Value "version=0.32.0-test2"
    Set-Content -LiteralPath (Join-Path $package "somavr.ini") -Value "UserScale=1.1"
    Write-PackageChecksums

    & (Join-Path $package "Install-Or-Update-SOMAVR.ps1") -Destination $destination
    Assert-Text (Join-Path $destination "somavr.ini") "UserScale=1.75" "Update overwrote user config"
    Assert-Text (Join-Path $destination "somavr.defaults.ini") "UserScale=1.1" "Updated defaults missing"
    if (Test-Path -LiteralPath (Join-Path $destination "legacy.txt")) {
        throw "Update left a stale managed file"
    }

    & (Join-Path $package "Uninstall-SOMAVR.ps1") -Destination $destination
    if (-not (Test-Path -LiteralPath (Join-Path $destination "somavr.ini") -PathType Leaf)) {
        throw "Uninstall removed user config"
    }
    $remaining = @(Get-ChildItem -LiteralPath $destination -Force)
    if ($remaining.Count -ne 1 -or $remaining[0].Name -ne "somavr.ini") {
        throw "Unexpected uninstall residue: $($remaining.Name -join ', ')"
    }
    Write-Host "Installer lifecycle tests passed"
} finally {
    $resolvedRoot = [IO.Path]::GetFullPath($root)
    $tempPrefix = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    if ($resolvedRoot.StartsWith($tempPrefix, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedRoot)) {
        Remove-Item -LiteralPath $resolvedRoot -Recurse -Force
    }
}
