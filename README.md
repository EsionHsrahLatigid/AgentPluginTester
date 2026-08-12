# AgentPluginHost

AgentPluginHost is an EHL JUCE 8 desktop VST3 test host for deterministic manual, agent, and CI validation. It provides a serial plug-in chain, generated/file/mic input, sample-accurate MIDI, loopback OSC control, asynchronous float WAV capture, audio statistics, NDJSON progress, and final JSON reports.

## Identity

- Product: `AgentPluginHost`
- Bundle ID: `jp.ehl.agentpluginhost`
- Manufacturer: `EsionHsrahLatigid`
- Fixture manufacturer code: `EHL_`
- JUCE revision: `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2` (`8.0.13`)

## Build

```sh
cmake --preset engine-debug -DEHL_JUCE_SOURCE_DIR=/path/to/JUCE
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure

cmake --preset host-release -DEHL_JUCE_SOURCE_DIR=/path/to/JUCE
cmake --build --preset host-release
ctest --preset host-release --output-on-failure
```

When `EHL_JUCE_SOURCE_DIR` is omitted, CMake fetches the pinned JUCE revision. See [BUILD.md](BUILD.md) for the target and artifact contract.

## Stable artifacts

```text
artifacts/host-release/macos-arm64/app/agent_plugin_host.app
artifacts/host-release/macos-arm64/scanner/agent_plugin_scanner
artifacts/host-release/macos-arm64/fixtures/aph_test_gain.vst3
artifacts/host-release/macos-arm64/fixtures/aph_test_synth.vst3
artifacts/host-release/macos-arm64/ARTIFACTS.txt

artifacts/host-release/windows-x64/app/agent_plugin_host.exe
artifacts/host-release/windows-x64/scanner/agent_plugin_scanner.exe
artifacts/host-release/windows-x64/fixtures/aph_test_gain.vst3
artifacts/host-release/windows-x64/fixtures/aph_test_synth.vst3
artifacts/host-release/windows-x64/ARTIFACTS.txt
```

## Usage

```sh
AgentPluginHost --mode offline --plugin /path/to/plugin.vst3 \
  --source sine --run-seconds 1 --record result.wav --report result.json

AgentPluginHost --help
AgentPluginHost --list-devices
AgentPluginHost --inspect-plugin /path/to/plugin.vst3
```

The complete behavior contract is in [AGENT_PLUGIN_TEST_HOST_SPEC.md](AGENT_PLUGIN_TEST_HOST_SPEC.md). The EHL visual and interaction contract is in [DESIGN.md](DESIGN.md).

## Release binaries

Successful `main` CI runs produce checksum manifests and fixed-name ZIPs for:

- macOS arm64: `AgentPluginHost-macos-arm64.zip`
- Windows x64: `AgentPluginHost-windows-x64.zip`

Pushing a semantic-version tag matching the CMake project version promotes the artifacts built from that exact commit into a GitHub Release. The release workflow does not rebuild. Each release also publishes `SHA256SUMS.txt` and `VERSION.txt`; the stable latest URLs are under `https://github.com/EsionHsrahLatigid/AgentPluginTester/releases/latest/download/`.

## Codex skill

The repository includes `skills/use-agent-plugin-tester` for repeatable agent-driven GUI and offline validation. Its macOS and Windows launchers fetch the matching checksum-verified ZIP from GitHub Releases on first use and retain a versioned user cache; no application binary is tracked in Git. Install it globally for Codex from a local checkout:

```sh
npx skills add . --skill use-agent-plugin-tester -g -a codex -y
```

Install the published skill with:

```sh
npx skills add EsionHsrahLatigid/AgentPluginTester --skill use-agent-plugin-tester -g -a codex -y
```

The launchers use GitHub Latest by default. Use `--host-version v0.1.0` for an exact release, `--update` to refresh the selected version, and `--offline` to prohibit downloads. macOS caches under `~/Library/Caches/AgentPluginTester`; Windows caches under `%LOCALAPPDATA%\AgentPluginTester\Cache`.

## License

The project source is MIT licensed. JUCE and hosted plug-ins have separate licenses that must be satisfied before distribution or automated loading.
