# Session reference

Use schema version `1.0`. Keep events sorted by `atSeconds`. Relative file paths resolve from the session file's directory.

```json
{
  "schemaVersion": "1.0",
  "mode": "offline",
  "gui": false,
  "showEditors": false,
  "audio": {
    "sampleRate": 48000,
    "blockSize": 256,
    "inputChannels": 2,
    "outputChannels": 2
  },
  "source": {
    "type": "silence"
  },
  "plugins": [
    { "path": "MySynth.vst3", "bypass": false }
  ],
  "events": [
    { "atSeconds": 0.1, "type": "noteOn", "channel": 1, "note": 60, "velocity": 0.8 },
    { "atSeconds": 1.1, "type": "noteOff", "channel": 1, "note": 60, "velocity": 0.0 }
  ],
  "durationSeconds": 2.0,
  "timeoutSeconds": 30.0,
  "capture": { "path": "output.wav" },
  "assertions": [
    { "type": "nonFiniteCount", "op": "eq", "value": 0 },
    { "type": "rmsDbfs", "op": "gt", "value": -80.0 }
  ],
  "report": { "path": "report.json" },
  "failOn": ["non-finite", "load-error"]
}
```

Supported top-level fields are `schemaVersion`, `mode`, `gui`, `showEditors`, `audio`, `source`, `plugins`, `events`, `durationSeconds`, `capture`, `assertions`, `report`, `failOn`, and `timeoutSeconds`. Unknown fields produce warnings; unsupported schema versions are errors.

Command-line options override session values. Repeated `--plugin` options replace the session plugin list.

For a VST3 bundle exposing multiple classes, add `classId` to the selected plugin object:

```json
{ "path": "MultiClass.vst3", "classId": "identifier returned by --inspect-plugin", "bypass": false }
```
