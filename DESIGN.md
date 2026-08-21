# AgentPluginHost UI Design

Status: integrated and visually verified for the `juce-plugin` profile.

## Aesthetic direction

The UI is a spacious harsh lab-console surface: flat monochrome, 4 px grid, square controls, dense operational readouts, and quantized meter data. It uses only `#050505`, `#2A2A2A`, `#8A8A86`, and `#F2F2F0`. There are no gradients, glow, chromatic accents, fake hardware details, decorative waveform logos, or damaged operational labels.

The surface expresses the EHL system through jagged grid construction, hard inversion states, ordered dither warnings, and reduced control travel. The wordmark is intentionally not embedded yet because the final serif logo and damage grammar are unresolved brand decisions.

## Actual-size rationale

Implemented default size: `1080 x 720` logical px. The host is resizable from `900 x 600` through `1600 x 1000`.

The larger test-console layout keeps the complete workflow visible at 100% scale:

- Left rail: source, transport, and status retain readable values and 28–32 px-high controls.
- Center chain: up to eight visible plugin rows expose metadata, latency, meter, bypass, native GUI, generic parameters, and reorder controls.
- Right rail: larger input/chain/output peak and RMS meters, OSC status, capture/report state, and pass/fail state.
- Bottom rail: a permanently visible 13-note MIDI keyboard supports both pointer and computer-key input.
- Header: session, mode, sample rate, block size, plugin count, and result state remain visible during testing.

The former `512 x 320` compact surface was retired because it hid the MIDI interaction model, constrained plug-in metadata, and produced undersized GUI/editor controls. This product prioritizes test throughput over plug-in-like compactness.

## Integration contract

Files:

- `Source/ui/AgentPluginHostUI.h`
- `Source/ui/AgentPluginHostUI.cpp`

Expected include/link dependencies:

- Include `Source/ui/AgentPluginHostUI.h` from the application layer.
- Compile `Source/ui/AgentPluginHostUI.cpp` into the JUCE app target.
- Link with `juce::juce_gui_basics`.

Primary classes:

- `agentpluginhost::ui::HostMainComponent`
- `agentpluginhost::ui::HostMainWindow`

State input:

- Build an `agentpluginhost::ui::HostUiState` snapshot on the message thread.
- Call `HostMainComponent::setState(state)` whenever source, transport, chain, meter, OSC, report, or status data changes.
- `PluginSlotState::stableId` is reserved for the host's runtime UUID or persistent chain ID. The current component IDs use visible row indices for deterministic automation; command callbacks receive the plugin `index` field.

Action output:

- Fill `agentpluginhost::ui::HostUiActions`.
- Call `HostMainComponent::setActions(actions)`.
- Callbacks are intentionally UI-to-controller only. They must enqueue app/core commands and must not touch the audio thread directly.

Callback expectations:

- `addPlugins(paths)`: append supported `.vst3` bundles, or macOS AUv2 `.component` bundles, selected from the `PLUGIN` menu or dropped onto the console. The controller owns scanning, safe realtime suspension, loading, reporting, and UI refresh.
- `play()`: request `/transport/play`.
- `stop()`: request `/transport/stop`.
- `panic()`: request `/host/panic` and all-notes-off behavior.
- `toggleRecording()`: request capture start/stop based on current capture state.
- `writeReport()`: request `/host/report/write`.
- `setSourceByDelta(delta)`: cycle the source list by `-1` or `+1`.
- `toggleBypass(index)`: request `/plugin/<index>/bypass`.
- `showNativeEditor(index)`: open or front the plugin's native editor; fall back to the generic parameter editor when no native editor exists.
- `showGenericEditor(index)`: open or front the JUCE generic parameter editor.
- `removePlugin(index)`: reserved for host chain editing; no visible button in this compact draft. Removal is intentionally kept out of the first surface because the spec allows chain changes only while stopped and the compact host needs persistent test controls more than editing throughput.
- `movePlugin(index, targetIndex)`: move a chain slot while stopped, or let the controller reject it.

Automation IDs:

- `host.main`
- `source.prev`
- `source.next`
- `transport.play`
- `transport.stop`
- `transport.panic`
- `capture.toggle`
- `report.write`
- `plugin.<row>.bypass`
- `plugin.<row>.editor.native`
- `plugin.<row>.editor.generic`
- `plugin.<row>.move.up`
- `plugin.<row>.move.down`

Human loading surfaces:

- Embedded monochrome menu bar: `PLUGIN > ADD VST3...` (stable item ID `1`) and macOS-only `PLUGIN > ADD AUDIO UNIT...` (stable item ID `2`).
- Multi-select native file choosers supporting package-style macOS VST3/AUv2 bundles and directory-style Windows VST3 bundles.
- Whole-console file drop target accepting `.vst3` paths plus `.component` paths on macOS and removing duplicate paths before dispatch.
- High-contrast `DROP PLUG-IN TO ADD TO CHAIN` overlay while an acceptable drag is over the console.
- Empty-chain guidance names both the menu and drag-and-drop paths.

The component also exposes and accepts a chromatic one-octave keyboard layout (`A W S E D F T G Y H U J K`, MIDI notes 60-72). Pointer and key press/release are translated to queued Note On/Off commands without touching the audio processor directly.

## UI coverage

The surface exposes the required operational groups:

- Source: type, level, frequency, channel layout, source cycling.
- Transport: play/stop/panic, BPM, time signature, sample position.
- Plugin chain: index, name, vendor, version, format, load state, bypass, native editor, generic editor, reorder controls, latency, per-stage meter.
- Meters: input, chain, output peak readouts with quantized bars and dither fault indication.
- OSC: bind, port, enabled implied by bind status, RX, drop/reject/overflow total, last event.
- Report/capture: recording state, JSON report status/path, pass/fail.
- Status: warning count, error count, last host status.

## Typography

Control typography requests `Departure Mono`, a bitmap-influenced mono face distributed under the SIL Open Font License. The repository does not bundle the font file; production packaging should either embed the licensed font as binary data or replace `typefaceName` with the approved embedded EHL UI font. JUCE's platform fallback remains usable when it is unavailable.

## Verification notes

The `AgentPluginHost` JUCE GUI target compiles these files and opens `HostMainWindow`. Visual QA was performed on macOS at `1080 x 720` after an actual realtime-device launch with the staged synth fixture. The host showed the complete three-column console and MIDI rail without clipping, and the row `GUI` action opened the plug-in editor in a separate window. A minimum-size regression test verifies all eight rows' automation targets remain visible, inside the component, and at least `32 x 28` logical px. Additional regression coverage verifies platform-specific VST3/AUv2 drag filtering, stable menu IDs, path de-duplication, prepared-chain append behavior, and loading the staged gain fixture after runtime preparation. The macOS integration test also proves that a registered staged AUv2 fixture scans, loads, renders offline, and reports its format as `AudioUnit`.

The design was cross-checked against the existing Canva direction `EHL / Plugins / 8-bit UI Template` (design ID `DAHSB2E1xQE`). Brand-template enumeration was unavailable under the connected Canva plan, so the implementation uses the established code-native EHL system rather than introducing an unverified logo treatment.

Release binaries are not stored inside the repository skill. The macOS arm64 and Windows x64 skill launchers resolve platform ZIPs from immutable GitHub Releases, verify `SHA256SUMS.txt`, and execute from a versioned user cache.
