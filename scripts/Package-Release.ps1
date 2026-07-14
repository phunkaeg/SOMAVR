[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-openxr\Release",
    [string]$OutputDirectory = "out",
    [switch]$IncludeDumper
)

$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildPath = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $BuildDirectory))
$outputRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputDirectory))

if (-not (Test-Path -LiteralPath $buildPath -PathType Container)) {
    throw "Build directory does not exist: $buildPath"
}

$flavorPath = Join-Path $buildPath "somavr_build_flavor.txt"
$manifestPath = Join-Path $buildPath "somavr_build_manifest.txt"
foreach ($requiredMetadata in @($flavorPath, $manifestPath)) {
    if (-not (Test-Path -LiteralPath $requiredMetadata -PathType Leaf)) {
        throw "Missing build metadata: $requiredMetadata"
    }
}

$flavor = @{}
foreach ($line in Get-Content -LiteralPath $flavorPath) {
    if ($line -match '^([^=]+)=(.*)$') {
        $flavor[$matches[1]] = $matches[2]
    }
}
if ($flavor.openxr -ne "1" -or $flavor.flavor -ne "openxr") {
    throw "Refusing to package non-OpenXR build: flavor=$($flavor.flavor) openxr=$($flavor.openxr)"
}
if ([string]::IsNullOrWhiteSpace($flavor.version)) {
    throw "Build flavor metadata has no version"
}

$runtimeFiles = @(
    "somavr.dll",
    "somavr_injector.exe",
    "openxr_loader.dll",
    "somavr_build_flavor.txt",
    "somavr_build_manifest.txt"
)
if ($IncludeDumper) {
    $runtimeFiles += "somavr_dumper.exe"
}
foreach ($file in $runtimeFiles) {
    $source = Join-Path $buildPath $file
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing release artifact: $source"
    }
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$packageName = "SOMAVR-$($flavor.version)"
$stagePath = [System.IO.Path]::GetFullPath((Join-Path $outputRoot $packageName))
$outputPrefix = $outputRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
if (-not $stagePath.StartsWith($outputPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Unsafe staging path: $stagePath"
}
if (Test-Path -LiteralPath $stagePath) {
    Remove-Item -LiteralPath $stagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath | Out-Null

foreach ($file in $runtimeFiles) {
    Copy-Item -LiteralPath (Join-Path $buildPath $file) -Destination $stagePath
}
Copy-Item -LiteralPath (Join-Path $repositoryRoot "somavr.ini") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repositoryRoot "README.md") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repositoryRoot "scripts\Install-Or-Update-SOMAVR.ps1") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repositoryRoot "scripts\Uninstall-SOMAVR.ps1") -Destination $stagePath

$packageDocs = Join-Path $stagePath "docs"
New-Item -ItemType Directory -Path $packageDocs | Out-Null
foreach ($doc in @("CURRENT_STATE.md", "TEST_CHECKLISTS.md", "SMOKE_TEST_MATRIX.md")) {
    Copy-Item -LiteralPath (Join-Path $repositoryRoot "docs\$doc") -Destination $packageDocs
}

$hashLines = Get-ChildItem -LiteralPath $stagePath -File -Recurse |
    Sort-Object FullName |
    ForEach-Object {
        $relativePath = [System.IO.Path]::GetRelativePath($stagePath, $_.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relativePath"
    }
$hashPath = Join-Path $stagePath "SHA256SUMS.txt"
$hashLines | Set-Content -LiteralPath $hashPath -Encoding ascii

$archivePath = Join-Path $outputRoot "$packageName.zip"
if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
Compress-Archive -LiteralPath $stagePath -DestinationPath $archivePath -CompressionLevel Optimal

$archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToUpperInvariant()
Write-Host "Packaged $packageName"
Write-Host "Staging: $stagePath"
Write-Host "Archive: $archivePath"
Write-Host "SHA-256: $archiveHash"
