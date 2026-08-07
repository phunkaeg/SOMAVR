[CmdletBinding()]
param(
    [string]$BuildDirectory = "build-openxr\Release",
    [string]$OutputDirectory = "out",
    [switch]$IncludeDumper,
    [switch]$Versioned
)

$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$releaseConfigPath = Join-Path $repositoryRoot "config\somavr.release.ini"
if (-not (Test-Path -LiteralPath $releaseConfigPath -PathType Leaf)) {
    throw "Tracked release configuration is missing: $releaseConfigPath"
}
$releaseConfigText = Get-Content -LiteralPath $releaseConfigPath -Raw
$forbiddenReleaseSettings = @(
    "MatrixCapture",
    "RenderDiagnosticCapture",
    "HPLReflectionFadeControl",
    "HPLVideoLifecycleProbe",
    "HPLAudioListenerProbe",
    "HPLPostEffectResourceProbe",
    "HPLRenderStageProbe",
    "HPLDualRenderReplayProbe",
    "HPLDualRenderAutoProbe",
    "HPLPerEyePerformanceTelemetry",
    "HPLPerEyeGpuTelemetry",
    "DepthCompositionProbe"
)
foreach ($setting in $forbiddenReleaseSettings) {
    if ($releaseConfigText -match "(?m)^\s*$([regex]::Escape($setting))\s*=\s*1\s*$") {
        throw "Release configuration enables diagnostic setting $setting"
    }
}
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

$sourceVersion = $null
foreach ($line in Get-Content -LiteralPath (Join-Path $repositoryRoot "CMakeLists.txt")) {
    if ($line -match '^\s*set\(SOMAVR_BUILD_VERSION\s+"([^"]+)"\)') {
        $sourceVersion = $matches[1]
        break
    }
}
if ([string]::IsNullOrWhiteSpace($sourceVersion)) {
    throw "Unable to resolve SOMAVR_BUILD_VERSION from CMakeLists.txt"
}
if ($flavor.version -ne $sourceVersion) {
    throw "Stale build metadata: source version=$sourceVersion flavor version=$($flavor.version) build=$buildPath"
}

$manifest = @{}
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -match '^([^=]+)=(.*)$') {
        $manifest[$matches[1]] = $matches[2]
    }
}
if (($manifest.version -ne $flavor.version) -or
    ($manifest.flavor -ne $flavor.flavor) -or
    ($manifest.openxr -ne $flavor.openxr) -or
    ($manifest.artifact -ne "somavr.dll")) {
    throw "Build manifest disagrees with flavor metadata: manifestVersion=$($manifest.version) flavorVersion=$($flavor.version) manifestFlavor=$($manifest.flavor) flavor=$($flavor.flavor) manifestOpenXR=$($manifest.openxr) openxr=$($flavor.openxr) artifact=$($manifest.artifact)"
}
$buildDllPath = Join-Path $buildPath "somavr.dll"
if (-not (Test-Path -LiteralPath $buildDllPath -PathType Leaf)) {
    throw "Missing DLL referenced by build manifest: $buildDllPath"
}
$actualBuildHash = (Get-FileHash -LiteralPath $buildDllPath -Algorithm SHA256).Hash.ToLowerInvariant()
if ($manifest.sha256 -ne $actualBuildHash) {
    throw "Build manifest SHA-256 is stale: manifest=$($manifest.sha256) actual=$actualBuildHash"
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
$packageName = if ($Versioned) { "SOMAVR-$($flavor.version)" } else { "SOMAVR-latest" }
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
Copy-Item -LiteralPath $releaseConfigPath -Destination (Join-Path $stagePath "somavr.ini")
Copy-Item -LiteralPath (Join-Path $repositoryRoot "README.md") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repositoryRoot "scripts\Install-Or-Update-SOMAVR.ps1") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repositoryRoot "scripts\Launch-SOMAVR-Dev.ps1") -Destination $stagePath
Copy-Item -LiteralPath (Join-Path $repositoryRoot "scripts\Uninstall-SOMAVR.ps1") -Destination $stagePath

$packageDocs = Join-Path $stagePath "docs"
New-Item -ItemType Directory -Path $packageDocs | Out-Null
foreach ($doc in @("USER_GUIDE.md", "CURRENT_STATE.md", "TEST_CHECKLISTS.md", "SMOKE_TEST_MATRIX.md")) {
    Copy-Item -LiteralPath (Join-Path $repositoryRoot "docs\$doc") -Destination $packageDocs
}

$stagePrefix = $stagePath.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
$hashLines = Get-ChildItem -LiteralPath $stagePath -File -Recurse |
    Sort-Object FullName |
    ForEach-Object {
        if (-not $_.FullName.StartsWith($stagePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Packaged file escaped staging root: $($_.FullName)"
        }
        $relativePath = $_.FullName.Substring($stagePrefix.Length).Replace('\', '/')
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
if (-not $Versioned) {
    $preservedPaths = @(
        $stagePath,
        [System.IO.Path]::GetFullPath($archivePath)
    )
    $obsoletePackages = Get-ChildItem -LiteralPath $outputRoot -Force |
        Where-Object {
            $_.Name -like "SOMAVR-*" -and
            $_.FullName -notin $preservedPaths -and
            ($_.PSIsContainer -or $_.Extension -eq ".zip")
        }
    foreach ($obsolete in $obsoletePackages) {
        $obsoletePath = [System.IO.Path]::GetFullPath($obsolete.FullName)
        if (-not $obsoletePath.StartsWith($outputPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe obsolete package path: $obsoletePath"
        }
        if (($obsolete.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Refusing to prune reparse-point package: $obsoletePath"
        }
        if ($obsolete.PSIsContainer) {
            Remove-Item -LiteralPath $obsoletePath -Recurse -Force
        } else {
            Remove-Item -LiteralPath $obsoletePath -Force
        }
    }
    Write-Host "Pruned $($obsoletePackages.Count) obsolete SOMAVR package artifact(s)"
}

Write-Host "Packaged $packageName"
Write-Host "Staging: $stagePath"
Write-Host "Archive: $archivePath"
Write-Host "SHA-256: $archiveHash"
