param(
    [Parameter(Mandatory = $true)]
    [string]$Launcher
)

$ErrorActionPreference = "Stop"
$testRoot = Join-Path ([IO.Path]::GetTempPath()) "agent-plugin-tester-downloader.$PID.$([Guid]::NewGuid().ToString('N'))"

try {
    $version = "v0.1.0"
    $releaseRoot = Join-Path $testRoot "releases"
    $assetName = "AgentPluginHost-windows-x64.zip"
    $payload = Join-Path $testRoot "payload"
    $executable = Join-Path $payload "app\agent_plugin_host.exe"
    New-Item -ItemType Directory -Path (Split-Path $executable -Parent), (Join-Path $releaseRoot "latest\download"), (Join-Path $releaseRoot "download\$version") -Force | Out-Null
    Set-Content $executable "downloader fixture" -Encoding Ascii

    $asset = Join-Path $releaseRoot "download\$version\$assetName"
    Compress-Archive -Path (Join-Path $payload "*") -DestinationPath $asset
    Set-Content (Join-Path $releaseRoot "latest\download\VERSION.txt") $version -Encoding Ascii
    $assetHash = (Get-FileHash $asset -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content (Join-Path $releaseRoot "download\$version\SHA256SUMS.txt") "$assetHash  $assetName" -Encoding Ascii

    $cacheRoot = Join-Path $testRoot "cache"
    $env:AGENT_PLUGIN_TESTER_RELEASE_BASE_URL = $releaseRoot
    $resolved = (& $Launcher --cache-dir $cacheRoot --print-host-path).Trim()
    if (-not (Test-Path $resolved -PathType Leaf)) { throw "Resolved executable is missing: $resolved" }
    if (-not $resolved.EndsWith("releases\$version\windows-x64\install\app\agent_plugin_host.exe")) { throw "Unexpected resolved path: $resolved" }

    $env:AGENT_PLUGIN_TESTER_RELEASE_BASE_URL = Join-Path $testRoot "missing"
    $offlineResolved = (& $Launcher --cache-dir $cacheRoot --offline --print-host-path).Trim()
    if ($offlineResolved -ne $resolved) { throw "Offline cache resolved a different executable." }

    $badRoot = Join-Path $testRoot "bad-releases"
    New-Item -ItemType Directory -Path (Join-Path $badRoot "download\$version") -Force | Out-Null
    Copy-Item $asset (Join-Path $badRoot "download\$version\$assetName")
    Set-Content (Join-Path $badRoot "download\$version\SHA256SUMS.txt") "$('0' * 64)  $assetName" -Encoding Ascii
    $env:AGENT_PLUGIN_TESTER_RELEASE_BASE_URL = $badRoot
    $failed = $false
    try {
        & $Launcher --cache-dir (Join-Path $testRoot "bad-cache") --host-version $version --print-host-path | Out-Null
    } catch {
        $failed = $true
    }
    if (-not $failed) { throw "Downloader accepted a mismatched checksum." }

    Write-Output "Windows downloader checks passed."
} finally {
    Remove-Item Env:AGENT_PLUGIN_TESTER_RELEASE_BASE_URL -ErrorAction SilentlyContinue
    if (Test-Path $testRoot) { Remove-Item $testRoot -Recurse -Force }
}
