#!/usr/bin/env bash
set -euo pipefail

launcher="${1:?launcher path is required}"
test_root="$(mktemp -d "${TMPDIR:-/tmp}/agent-plugin-tester-downloader.XXXXXX")"
cleanup() { rm -rf "$test_root"; }
trap cleanup EXIT

version="v0.1.0"
release_root="${test_root}/releases"
asset_name="AgentPluginHost-macos-arm64.zip"
payload="${test_root}/payload"
executable="${payload}/app/agent_plugin_host.app/Contents/MacOS/AgentPluginHost"
mkdir -p "$(dirname "$executable")" "${release_root}/latest/download" "${release_root}/download/${version}"

cat > "$executable" <<'EOF'
#!/usr/bin/env bash
printf 'fixture-host %s\n' "$*"
EOF
chmod +x "$executable"
cat > "${payload}/app/agent_plugin_host.app/Contents/Info.plist" <<'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>AgentPluginHost</string>
<key>CFBundleIdentifier</key><string>jp.ehl.agentpluginhost.downloader-fixture</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleVersion</key><string>1</string>
</dict></plist>
EOF
codesign --force --deep --sign - "${payload}/app/agent_plugin_host.app"
ditto -c -k --sequesterRsrc "$payload" "${release_root}/download/${version}/${asset_name}"
printf '%s\n' "$version" > "${release_root}/latest/download/VERSION.txt"
asset_hash="$(shasum -a 256 "${release_root}/download/${version}/${asset_name}" | awk '{print $1}')"
printf '%s  %s\n' "$asset_hash" "$asset_name" > "${release_root}/download/${version}/SHA256SUMS.txt"

cache_root="${test_root}/cache"
resolved="$(AGENT_PLUGIN_TESTER_RELEASE_BASE_URL="$release_root" "$launcher" --cache-dir "$cache_root" --print-host-path)"
[[ -x "$resolved" ]]
[[ "$resolved" == *"/releases/${version}/macos-arm64/install/app/agent_plugin_host.app/Contents/MacOS/AgentPluginHost" ]]

offline_resolved="$(AGENT_PLUGIN_TESTER_RELEASE_BASE_URL="${test_root}/missing" "$launcher" --cache-dir "$cache_root" --offline --print-host-path)"
[[ "$offline_resolved" == "$resolved" ]]
[[ "$(AGENT_PLUGIN_TESTER_RELEASE_BASE_URL="${test_root}/missing" "$launcher" --cache-dir "$cache_root" --offline -- --probe)" == "fixture-host --probe" ]]
if find "${cache_root}/releases/${version}" -maxdepth 1 \( -name 'install.*' -o -name '*.lock' \) | grep -q .; then
  echo "Downloader left temporary install or lock paths after execution." >&2
  exit 1
fi

bad_release_root="${test_root}/bad-releases"
mkdir -p "${bad_release_root}/download/${version}"
cp "${release_root}/download/${version}/${asset_name}" "${bad_release_root}/download/${version}/${asset_name}"
printf '%064d  %s\n' 0 "$asset_name" > "${bad_release_root}/download/${version}/SHA256SUMS.txt"
if AGENT_PLUGIN_TESTER_RELEASE_BASE_URL="$bad_release_root" "$launcher" \
    --cache-dir "${test_root}/bad-cache" --host-version "$version" --print-host-path >/dev/null 2>&1; then
  echo "Downloader accepted a mismatched checksum." >&2
  exit 1
fi

printf 'POSIX downloader checks passed.\n'
