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
    Get-ChildItem -LiteralPath $package -File -Recurse |
        Where-Object Name -ne "SHA256SUMS.txt" |
        Sort-Object Name |
        ForEach-Object {
            $relative = $_.FullName.Substring($package.Length + 1).Replace('\', '/')
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
    Set-Content -LiteralPath (Join-Path $package "somavr_dumper.exe") -Value "dumper-v1"
    New-Item -ItemType Directory -Path (Join-Path $package "docs") -Force | Out-Null
    Set-Content -LiteralPath (Join-Path $package "somavr_test_profile.txt") -Value "probe-only"
    Set-Content -LiteralPath (Join-Path $package "docs/NEXT_LIVE_EVIDENCE.md") -Value "next-test"
    Set-Content -LiteralPath (Join-Path $package "docs/HANDS_BOOTSTRAP_RE.md") -Value "hands-test"
    Set-Content -LiteralPath (Join-Path $package "unknown-payload.bin") -Value "reject-me"
    Write-PackageChecksums

    $unknownPayloadRejected = $false
    try {
        & (Join-Path $package "Install-Or-Update-SOMAVR.ps1") `
            -Destination $destination -AllowCustomDestination
    } catch {
        $unknownPayloadRejected = $true
    }
    if (-not $unknownPayloadRejected) {
        throw "Installer accepted a package file outside the literal allowlist"
    }
    Remove-Item -LiteralPath (Join-Path $package "unknown-payload.bin")
    Write-PackageChecksums

    $customDestinationRejected = $false
    try {
        & (Join-Path $package "Install-Or-Update-SOMAVR.ps1") -Destination $destination
    } catch {
        $customDestinationRejected = $true
    }
    if (-not $customDestinationRejected) {
        throw "Installer accepted a custom destination without explicit opt-in"
    }

    & (Join-Path $package "Install-Or-Update-SOMAVR.ps1") `
        -Destination $destination -AllowCustomDestination
    Assert-Text (Join-Path $destination "somavr.ini") "UserScale=1.0" "Fresh config mismatch"
    Assert-Text (Join-Path $destination "docs/NEXT_LIVE_EVIDENCE.md") "next-test" "Missing test checklist"
    Assert-Text (Join-Path $destination "docs/HANDS_BOOTSTRAP_RE.md") "hands-test" "Missing hands evidence"
    Assert-Text (Join-Path $destination "somavr_test_profile.txt") "probe-only" "Missing test profile"

    Set-Content -LiteralPath (Join-Path $destination "somavr.ini") -Value "UserScale=1.75"
    Set-Content -LiteralPath (Join-Path $destination "user-owned.txt") -Value "preserve-me"
    $manifestPath = Join-Path $destination ".somavr-install.json"
    $tamperedManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $tamperedManifest.managedFiles += "user-owned.txt"
    $tamperedManifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    Remove-Item -LiteralPath (Join-Path $package "somavr_dumper.exe")
    Set-Content -LiteralPath (Join-Path $package "somavr.dll") -Value "dll-v2"
    Set-Content -LiteralPath (Join-Path $package "somavr_build_manifest.txt") -Value "version=0.32.0-test2"
    Set-Content -LiteralPath (Join-Path $package "somavr.ini") -Value "UserScale=1.1"
    Write-PackageChecksums

    & (Join-Path $package "Install-Or-Update-SOMAVR.ps1") `
        -Destination $destination -AllowCustomDestination
    Assert-Text (Join-Path $destination "somavr.ini") "UserScale=1.75" "Update overwrote user config"
    Assert-Text (Join-Path $destination "somavr.defaults.ini") "UserScale=1.1" "Updated defaults missing"
    if (Test-Path -LiteralPath (Join-Path $destination "somavr_dumper.exe")) {
        throw "Update left a stale managed file"
    }
    Assert-Text (Join-Path $destination "user-owned.txt") "preserve-me" `
        "Update trusted a tampered manifest and removed an unknown file"

    & (Join-Path $package "Uninstall-SOMAVR.ps1") `
        -Destination $destination -AllowCustomDestination
    if (-not (Test-Path -LiteralPath (Join-Path $destination "somavr.ini") -PathType Leaf)) {
        throw "Uninstall removed user config"
    }
    $remaining = @(Get-ChildItem -LiteralPath $destination -Force)
    $remainingNames = @($remaining.Name | Sort-Object)
    if ($remaining.Count -ne 2 -or
        ($remainingNames -join ',') -ne "somavr.ini,user-owned.txt") {
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
