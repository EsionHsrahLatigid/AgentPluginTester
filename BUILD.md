# Build Notes

Requirements: CMake 3.22+, Ninja, a C++17 compiler, and JUCE `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.

`engine-debug` builds the host core and unit test runner without the GUI app or scanner. `host-release` builds the app, scanner, unit tests, and two fixture VST3s, then stages stable products through `ehl_stage_products`.

## CMake options

- `AGENTPLUGINHOST_BUILD_APP`: build the GUI host and scanner.
- `AGENTPLUGINHOST_BUILD_TESTS`: build the test runner and fixture VST3s.
- `AGENTPLUGINHOST_VERIFY_ARTIFACTS`: add the stable artifact contract to CTest.
- `AGENTPLUGINHOST_ARTIFACT_ROOT`: override the default `artifacts/` staging root.
- `EHL_JUCE_SOURCE_DIR`: use an existing JUCE checkout instead of FetchContent.

## Fixed targets

- `AgentPluginHost`
- `AgentPluginScanner`
- `AgentPluginHostTests`
- `TestGainVST3_VST3`
- `TestSynthVST3_VST3`
- `ehl_stage_products`

The release preset stages platform-normalized products under `artifacts/host-release/<platform>/`. CTest verifies that every required app, scanner, fixture, manifest, and macOS signature is present.

## CI and releases

The GitHub Actions CI follows the fail-closed artifact pattern used by `EsionHsrahLatigid/juce-ci`: macOS arm64 and Windows x64 build, test, stage, package, checksum, and upload independently. The release workflow accepts only a semantic-version tag that matches `project(AgentPluginHost VERSION ...)`, resolves exactly one successful `main` CI run for the tagged commit, verifies both candidate checksums, and promotes those exact artifacts without rebuilding.

A published release contains exactly:

- `AgentPluginHost-macos-arm64.zip`
- `AgentPluginHost-windows-x64.zip`
- `SHA256SUMS.txt`
- `VERSION.txt`

To publish a release, update the CMake project version, land the commit on `main`, wait for CI to pass, then push the matching tag (for example `v0.2.0`).
