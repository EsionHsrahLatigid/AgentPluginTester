---
name: use-agent-plugin-tester
description: Build and operate AgentPluginHost to inspect, load, interactively test, and deterministically validate VST3 audio plugins. Use when Codex needs to test a VST3 in the EHL host GUI, open a plugin's native GUI or generic parameter editor, send MIDI notes, run offline audio checks, capture WAV output, inspect NDJSON events, evaluate the final JSON report, or diagnose host/plugin loading failures in an AgentPluginTester checkout.
---

# Use Agent Plugin Tester

Prefer the checksum-verified release host resolved by the skill launcher. Use the repository's stable CMake presets when developing the host or running on an unsupported platform. Prefer the offline lane for repeatable evidence and the realtime GUI lane for interaction and visual checks.

## Resolve the release host

Resolve paths relative to the directory containing this `SKILL.md`, then use:

```sh
<skill-directory>/scripts/agent-plugin-tester --version
```

On Windows use `scripts/agent-plugin-tester.ps1`. On the first run, the launcher downloads the matching macOS arm64 or Windows x64 ZIP from the latest public GitHub Release, verifies its SHA-256 entry, and installs it in the user cache. Later runs use the cache without network access.

Resolver controls:

```sh
# Resolve/install only
<skill-directory>/scripts/agent-plugin-tester --install-only

# Check GitHub Latest, then use the versioned cache
<skill-directory>/scripts/agent-plugin-tester --update --version

# Pin a reproducible release or require an existing offline cache
<skill-directory>/scripts/agent-plugin-tester --host-version v0.2.0 --offline --version

# Print the real executable path
<skill-directory>/scripts/agent-plugin-tester --print-host-path
```

Never bypass a checksum failure. Set `AGENT_PLUGIN_TESTER_CACHE_DIR` only when a task needs an isolated cache. Use `AGENT_PLUGIN_TESTER_RELEASE_BASE_URL` only for a trusted mirror or downloader tests.

## Build from source when needed

1. Locate the checkout containing `CMakePresets.json` and `AGENT_PLUGIN_TEST_HOST_SPEC.md`.
2. Read `BUILD.md` when artifact paths or JUCE setup are unclear.
3. Build the smallest appropriate preset:

```sh
cmake --preset engine-debug
cmake --build --preset engine-debug
ctest --preset engine-debug --output-on-failure
```

Use `host-release` when the host app, scanner, or fixture plugins are required:

```sh
cmake --preset host-release
cmake --build --preset host-release
ctest --preset host-release --output-on-failure
```

Pass `-DEHL_JUCE_SOURCE_DIR=/absolute/path/to/JUCE` at configure time to avoid fetching JUCE when a compatible checkout is available.

## Resolve a source-built executable

Use the staged artifact, not a generator-specific build path:

- macOS: `artifacts/host-release/macos-arm64/app/agent_plugin_host.app/Contents/MacOS/AgentPluginHost`
- Windows: `artifacts/host-release/windows-x64/app/agent_plugin_host.exe`
- Linux: `artifacts/host-release/linux-x64/app/agent_plugin_host`

For the commands below, set `HOST` to either the release launcher or a staged source-built executable. Treat plugin paths as untrusted input: quote them, inspect them first, and do not overwrite existing result files unless the user requested it.

## Inspect before running

```sh
"$HOST" --inspect-plugin "/absolute/path/My Plugin.vst3"
```

Stop and report the scanner error when inspection fails. If the bundle exposes multiple classes, use a session file with the desired `classId`.

## Choose a test lane

### Deterministic offline validation

Use generated input, a bounded duration, an explicit report, and `--no-gui`:

```sh
"$HOST" \
  --mode offline \
  --no-gui \
  --plugin "/absolute/path/My Plugin.vst3" \
  --source sine \
  --frequency 440 \
  --level-db -18 \
  --sample-rate 48000 \
  --block-size 256 \
  --run-seconds 2 \
  --record result.wav \
  --report result.json \
  --events events.ndjson
```

Require all of the following before calling the run successful:

- process exit code is `0`;
- report exists and parses as JSON;
- report has `passed: true` and `exitCode: 0`;
- `errors` is empty;
- requested WAV/event artifacts exist;
- repeated deterministic runs are compared only when configuration, plugin binary, platform, and host build are identical.

### Realtime GUI validation

Run with `--gui`. Use `--show-editors` to open every loaded plugin GUI after startup:

```sh
"$HOST" \
  --mode realtime \
  --gui \
  --show-editors \
  --plugin "/absolute/path/My Plugin.vst3" \
  --source sine \
  --report gui-report.json
```

In the host window:

- use `PLUGIN > ADD VST3...` to select and append one or more VST3 bundles after launch;
- drag `.vst3` bundles from Finder or Explorer onto the main console to append them to the current chain;
- expect a bundle containing multiple plug-in classes to append all detected classes in scan order when loaded from the GUI;
- use `GUI` on a plugin row to open its native editor in a separate window;
- expect `GUI` to fall back to the generic parameter editor when the plugin has no native editor;
- use `PARAMS` to explicitly open the generic JUCE parameter editor;
- expect repeat clicks to bring the existing window forward instead of creating duplicates;
- use the visible MIDI keyboard or `A W S E D F T G Y H U J K` for notes C4–C5;
- verify meters, transport, OSC counters, warnings, errors, and report state remain visible while an editor is open.

Do not use GUI interaction as deterministic offline evidence.

## Use a session for repeatable multi-step tests

Read [session-reference.md](references/session-reference.md) before creating or modifying a session JSON. Resolve relative plugin, capture, input, and report paths from the session file's directory.

## Diagnose failures

1. Re-run `--inspect-plugin` to separate scan/load failures from processing failures.
2. Confirm architecture and format match the staged host; the host loads VST3 only.
3. Run the fixture plugins from `artifacts/host-release/<platform>/fixtures/` to verify the host itself.
4. Use `--events stdout` or an NDJSON file for lifecycle evidence.
5. Check the final report before interpreting audio output.
6. For realtime failures, run `--list-devices` and verify the requested device, channel count, sample rate, and block size.
7. Preserve the failing command, exit code, event log, report, and artifact paths in the handoff.

## Completion report

Report the host build/preset, plugin identity, test lane, exact configuration, exit code, JSON result, generated artifacts, and any validation gap. Do not claim GUI success without opening the host and requested editor, and do not claim deterministic success from a realtime run.
