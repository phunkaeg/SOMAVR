[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$Destination = (Join-Path $env:LOCALAPPDATA "SOMAVR")
)

$ErrorActionPreference = "Stop"

function Resolve-PackageRoot {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot "somavr.dll") -PathType Leaf) {
        return [System.IO.Path]::GetFullPath($PSScriptRoot)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
}

function Resolve-SafeDestination([string]$Path) {
    $resolved = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetPathRoot($resolved).TrimEnd('\')
    if ($resolved.TrimEnd('\') -eq $root) {
        throw "Refusing to install into a filesystem root: $resolved"
    }
    return $resolved.TrimEnd('\')
}

function Resolve-ChildPath([string]$Root, [string]$RelativePath) {
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $Root $RelativePath))
    $prefix = $Root.TrimEnd('\') + '\'
    if (-not $candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Package path escapes root: $RelativePath"
    }
    return $candidate
}

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

function Read-Checksums([string]$PackageRoot) {
    $checksumPath = Join-Path $PackageRoot "SHA256SUMS.txt"
    if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
        throw "Missing package checksum file: $checksumPath"
    }
    $entries = [ordered]@{}
    foreach ($line in Get-Content -LiteralPath $checksumPath) {
        if ($line -notmatch '^([0-9a-fA-F]{64})  (.+)$') {
            throw "Malformed checksum line: $line"
        }
        $relative = $matches[2].Replace('/', '\')
        if ($entries.Contains($relative)) {
            throw "Duplicate checksum entry: $relative"
        }
        $entries[$relative] = $matches[1].ToLowerInvariant()
    }
    if ($entries.Count -eq 0) {
        throw "Package checksum file is empty"
    }
    return $entries
}

$packageRoot = Resolve-PackageRoot
$destinationRoot = Resolve-SafeDestination $Destination
if ($packageRoot.TrimEnd('\').Equals($destinationRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Source package and destination must be different directories"
}

foreach ($required in @("somavr.dll", "somavr_injector.exe", "somavr_build_manifest.txt", "somavr.ini")) {
    if (-not (Test-Path -LiteralPath (Join-Path $packageRoot $required) -PathType Leaf)) {
        throw "This is not a complete SOMAVR package; missing $required in $packageRoot"
    }
}

$checksums = Read-Checksums $packageRoot
foreach ($entry in $checksums.GetEnumerator()) {
    $source = Resolve-ChildPath $packageRoot $entry.Key
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Package file is missing: $($entry.Key)"
    }
    $actual = Get-Sha256Hex $source
    if ($actual -ne $entry.Value) {
        throw "Package checksum mismatch: $($entry.Key)"
    }
}

$version = "unknown"
foreach ($line in Get-Content -LiteralPath (Join-Path $packageRoot "somavr_build_manifest.txt")) {
    if ($line -match '^version=(.+)$') {
        $version = $matches[1]
        break
    }
}

$installManifestPath = Join-Path $destinationRoot ".somavr-install.json"
$previousManaged = @()
if (Test-Path -LiteralPath $installManifestPath -PathType Leaf) {
    $previous = Get-Content -LiteralPath $installManifestPath -Raw | ConvertFrom-Json
    if ($null -ne $previous.managedFiles) {
        $previousManaged = @($previous.managedFiles)
    }
}

if ($PSCmdlet.ShouldProcess($destinationRoot, "Install or update SOMAVR $version")) {
    New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
    $managed = [System.Collections.Generic.List[string]]::new()
    $configPreserved = Test-Path -LiteralPath (Join-Path $destinationRoot "somavr.ini") -PathType Leaf

    foreach ($entry in $checksums.GetEnumerator()) {
        $relative = $entry.Key
        $source = Resolve-ChildPath $packageRoot $relative
        if ($relative.Equals("somavr.ini", [System.StringComparison]::OrdinalIgnoreCase)) {
            if ($configPreserved) {
                $relative = "somavr.defaults.ini"
            }
        }
        $target = Resolve-ChildPath $destinationRoot $relative
        $targetDirectory = Split-Path -Parent $target
        New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
        if (-not ($relative -eq "somavr.ini" -and $configPreserved)) {
            Copy-Item -LiteralPath $source -Destination $target -Force
        }
        if (-not $relative.Equals("somavr.ini", [System.StringComparison]::OrdinalIgnoreCase)) {
            $managed.Add($relative.Replace('\', '/'))
        }
    }

    Copy-Item -LiteralPath (Join-Path $packageRoot "SHA256SUMS.txt") -Destination $destinationRoot -Force
    $managed.Add("SHA256SUMS.txt")

    foreach ($relative in $previousManaged) {
        $normalized = ([string]$relative).Replace('/', '\')
        if (($normalized.Equals("somavr.ini", [System.StringComparison]::OrdinalIgnoreCase)) -or
            ($managed.Contains(([string]$relative).Replace('\', '/')))) {
            continue
        }
        $stalePath = Resolve-ChildPath $destinationRoot $normalized
        if (Test-Path -LiteralPath $stalePath -PathType Leaf) {
            Remove-Item -LiteralPath $stalePath -Force
        }
    }

    foreach ($entry in $checksums.GetEnumerator()) {
        $relative = $entry.Key
        if ($relative.Equals("somavr.ini", [System.StringComparison]::OrdinalIgnoreCase) -and $configPreserved) {
            $relative = "somavr.defaults.ini"
        }
        $target = Resolve-ChildPath $destinationRoot $relative
        $actual = Get-Sha256Hex $target
        if ($actual -ne $entry.Value) {
            throw "Installed file verification failed: $relative"
        }
    }

    $manifest = [ordered]@{
        schema = 1
        version = $version
        installedAtUtc = [DateTime]::UtcNow.ToString("o")
        configuration = $(if ($configPreserved) { "preserved; new defaults in somavr.defaults.ini" } else { "created from package defaults" })
        managedFiles = @($managed | Sort-Object -Unique)
    }
    $manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $installManifestPath -Encoding utf8

    Write-Host "SOMAVR $version installed to $destinationRoot"
    Write-Host $(if ($configPreserved) {
        "Existing somavr.ini preserved; current defaults written to somavr.defaults.ini"
    } else {
        "Created somavr.ini from package defaults"
    })
    Write-Host "Managed files: $($manifest.managedFiles.Count)"
}
