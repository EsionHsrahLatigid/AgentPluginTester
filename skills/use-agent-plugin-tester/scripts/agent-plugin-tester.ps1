$ErrorActionPreference = "Stop"

$repository = if ($env:AGENT_PLUGIN_TESTER_REPOSITORY) { $env:AGENT_PLUGIN_TESTER_REPOSITORY } else { "EsionHsrahLatigid/AgentPluginTester" }
$releaseBase = if ($env:AGENT_PLUGIN_TESTER_RELEASE_BASE_URL) { $env:AGENT_PLUGIN_TESTER_RELEASE_BASE_URL } else { "https://github.com/$repository/releases" }
$requestedVersion = $env:AGENT_PLUGIN_TESTER_VERSION
$cacheRoot = if ($env:AGENT_PLUGIN_TESTER_CACHE_DIR) { $env:AGENT_PLUGIN_TESTER_CACHE_DIR } else { Join-Path $env:LOCALAPPDATA "AgentPluginTester\Cache" }
$offline = $env:AGENT_PLUGIN_TESTER_OFFLINE -eq "1"
$update = $false
$reinstall = $false
$printHostPath = $false
$installOnly = $false
$hostArguments = [Collections.Generic.List[string]]::new()

function Show-ResolverHelp {
    @"
Usage: agent-plugin-tester.ps1 [resolver options] [--] [AgentPluginHost options]

Resolver options:
  --host-version <vX.Y.Z>  Use an exact release.
  --update                 Check GitHub Latest before launching.
  --reinstall              Re-download and replace the cached release.
  --offline                Never access the network.
  --cache-dir <path>       Override the user cache directory.
  --print-host-path        Print the resolved executable and exit.
  --install-only           Install/cache the host and exit.
  --resolver-help          Show this help.
"@
}

for ($index = 0; $index -lt $args.Count; $index++) {
    switch ($args[$index]) {
        "--host-version" {
            if (++$index -ge $args.Count) { throw "--host-version requires a value" }
            $requestedVersion = $args[$index]
        }
        "--update" { $update = $true }
        "--reinstall" { $reinstall = $true }
        "--offline" { $offline = $true }
        "--cache-dir" {
            if (++$index -ge $args.Count) { throw "--cache-dir requires a value" }
            $cacheRoot = $args[$index]
        }
        "--print-host-path" { $printHostPath = $true }
        "--install-only" { $installOnly = $true }
        "--resolver-help" { Show-ResolverHelp; exit 0 }
        "--" {
            for ($rest = $index + 1; $rest -lt $args.Count; $rest++) { $hostArguments.Add($args[$rest]) }
            $index = $args.Count
        }
        default {
            for ($rest = $index; $rest -lt $args.Count; $rest++) { $hostArguments.Add($args[$rest]) }
            $index = $args.Count
        }
    }
}

if (-not [Runtime.InteropServices.RuntimeInformation]::IsOSPlatform([Runtime.InteropServices.OSPlatform]::Windows)) {
    throw "The PowerShell launcher supports Windows only. Use agent-plugin-tester on macOS."
}
if (-not [Environment]::Is64BitOperatingSystem) { throw "AgentPluginHost requires 64-bit Windows." }
$platform = "windows-x64"
$assetName = "AgentPluginHost-windows-x64.zip"
$relativeExecutable = "app\agent_plugin_host.exe"
$currentVersionFile = Join-Path $cacheRoot "current-version.txt"

function Get-ReleaseFile([string]$Source, [string]$Destination) {
    if (Test-Path $Source -PathType Leaf) {
        Copy-Item $Source $Destination -Force
    } elseif ($Source.StartsWith("file://")) {
        Copy-Item ([Uri]$Source).LocalPath $Destination -Force
    } else {
        Invoke-WebRequest -UseBasicParsing -Uri $Source -OutFile $Destination
    }
}

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha256 = [Security.Cryptography.SHA256]::Create()
        try {
            return ([BitConverter]::ToString($sha256.ComputeHash($stream))).Replace("-", "").ToLowerInvariant()
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
if (-not $requestedVersion) {
    if (-not $update -and (Test-Path $currentVersionFile -PathType Leaf)) {
        $requestedVersion = (Get-Content $currentVersionFile -Raw).Trim()
    } elseif ($offline) {
        throw "No cached AgentPluginHost version is selected; --offline cannot resolve GitHub Latest."
    } else {
        $versionTemp = Join-Path $cacheRoot "version.$PID.tmp"
        Get-ReleaseFile "$releaseBase/latest/download/VERSION.txt" $versionTemp
        $requestedVersion = (Get-Content $versionTemp -Raw).Trim()
        Remove-Item $versionTemp -Force
    }
}

if ($requestedVersion -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+$') { throw "Invalid AgentPluginHost release version: $requestedVersion" }

$releaseDir = Join-Path $cacheRoot "releases\$requestedVersion\$platform"
$installDir = Join-Path $releaseDir "install"
$hostExecutable = Join-Path $installDir $relativeExecutable
$installMarker = Join-Path $releaseDir "INSTALL.txt"
$cacheIsValid = (Test-Path $hostExecutable -PathType Leaf) -and (Test-Path $installMarker -PathType Leaf)
if ($cacheIsValid) {
    $marker = @{}
    Get-Content $installMarker | ForEach-Object {
        $parts = $_ -split '=', 2
        if ($parts.Count -eq 2) { $marker[$parts[0]] = $parts[1] }
    }
    $cacheIsValid = $marker.version -eq $requestedVersion -and $marker.platform -eq $platform
}

if (-not $cacheIsValid -or $reinstall) {
    if ($offline) { throw "AgentPluginHost $requestedVersion for $platform is not available in the offline cache." }
    New-Item -ItemType Directory -Path (Split-Path $releaseDir -Parent) -Force | Out-Null
    $lockDir = "$releaseDir.lock"
    $locked = $false
    for ($attempt = 0; $attempt -lt 100; $attempt++) {
        try {
            New-Item -ItemType Directory -Path $lockDir -ErrorAction Stop | Out-Null
            $locked = $true
            break
        } catch {
            Start-Sleep -Milliseconds 100
        }
    }
    if (-not $locked) { throw "Timed out waiting for the AgentPluginHost cache lock." }

    $workDir = Join-Path (Split-Path $releaseDir -Parent) "install.$PID.$([Guid]::NewGuid().ToString('N'))"
    try {
        New-Item -ItemType Directory -Path $workDir -Force | Out-Null
        $releaseUrl = "$releaseBase/download/$requestedVersion"
        $checksumsFile = Join-Path $workDir "SHA256SUMS.txt"
        $archiveFile = Join-Path $workDir $assetName
        Get-ReleaseFile "$releaseUrl/SHA256SUMS.txt" $checksumsFile
        $checksumLine = Get-Content $checksumsFile | Where-Object { $_ -match "^[0-9a-fA-F]{64}\s+$([Regex]::Escape($assetName))$" }
        if (@($checksumLine).Count -ne 1) { throw "Release checksum manifest has no unique valid entry for $assetName." }
        $expectedHash = ($checksumLine -split '\s+')[0].ToLowerInvariant()
        Get-ReleaseFile "$releaseUrl/$assetName" $archiveFile
        $actualHash = Get-Sha256 $archiveFile
        if ($actualHash -ne $expectedHash) { throw "SHA-256 mismatch for $assetName. Expected $expectedHash, actual $actualHash." }

        $candidateInstall = Join-Path $workDir "install"
        Expand-Archive -Path $archiveFile -DestinationPath $candidateInstall -Force
        $candidateExecutable = Join-Path $candidateInstall $relativeExecutable
        if (-not (Test-Path $candidateExecutable -PathType Leaf)) { throw "Release archive is missing $relativeExecutable." }

        New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
        $oldInstall = Join-Path $releaseDir "install.old.$PID"
        if (Test-Path $oldInstall) { Remove-Item $oldInstall -Recurse -Force }
        if (Test-Path $installDir) { Move-Item $installDir $oldInstall }
        Move-Item $candidateInstall $installDir
        @(
            "version=$requestedVersion"
            "platform=$platform"
            "asset=$assetName"
            "sha256=$actualHash"
        ) | Set-Content "$installMarker.tmp" -Encoding Ascii
        Move-Item "$installMarker.tmp" $installMarker -Force
        if (Test-Path $oldInstall) { Remove-Item $oldInstall -Recurse -Force }
    } finally {
        if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
        if (Test-Path $lockDir) { Remove-Item $lockDir -Force }
    }
}

Set-Content "$currentVersionFile.tmp" $requestedVersion -Encoding Ascii
Move-Item "$currentVersionFile.tmp" $currentVersionFile -Force

if ($printHostPath) { Write-Output $hostExecutable; exit 0 }
if ($installOnly) { Write-Output "AgentPluginHost $requestedVersion cached for $platform"; exit 0 }

& $hostExecutable @hostArguments
exit $LASTEXITCODE
