# Agent Plugin Test Host 仕様書

- 文書バージョン: 0.3.0
- 作成日: 2026-08-12
- ステータス: 実装済み仕様（macOS AUv2拡張を含む）
- 仮称: **AgentPluginHost**
- 実装技術: C++17 / JUCE 8系 / CMake 3.22以上

## 1. 概要

AgentPluginHostは、AIエージェントおよび人間の開発者がVST3およびmacOS AUv2オーディオプラグインを自動・対話的に検査するためのスタンドアロンホストアプリケーションである。

本アプリケーションはコマンドラインから起動するGUIアプリとして動作し、複数プラグインの直列チェイン、決定的なテスト信号、OSC/MIDI制御、出力音声の録音と統計取得、自動終了、機械可読レポートを提供する。

中心となる利用形態は次の2つである。

1. 開発者がGUIと音声出力を利用して対話的に確認するリアルタイムモード
2. AIエージェントやCIが同じ実行ファイルをCLI/OSCから操作する自動テストモード

## 2. 対象プラットフォームとプラグイン形式

| 項目 | macOS | Windows |
| --- | --- | --- |
| CPU | arm64 / x86_64 universal 2必須 | x64必須、ARM64は将来対応 |
| OS | macOS 13以降を初期対象 | Windows 10 22H2 / Windows 11 |
| プラグイン形式 | VST3、AUv2 `.component` | VST3 |
| AudioUnit | AUv2対応、AUv3対象外 | 対象外 |
| 音声API | CoreAudio | WASAPI、ASIOは任意追加 |
| GUI | JUCEネイティブGUI | JUCEネイティブGUI |

### 2.1 形式選定

- VST3をmacOS/Windows共通形式とする。
- VST2はSDK、配布、互換性上の理由から対象外とする。
- macOSではJUCEのAudio Unitホストを有効化し、CoreAudioへ登録済みのAUv2 `.component`を対象とする。
- macOSとWindowsで同じVST3テスト定義を再利用できることを優先する。
- AUv3 `.appex`およびAudio Unit Extensionの埋め込みホストは対象外とする。

## 3. 目標

### 3.1 必須目標

- CLIで起動、構成、実行時間、出力先を指定できる。
- 複数の対応プラグインパスを指定順に直列接続できる。
- ホストGUIと各プラグインのネイティブEditorを表示できる。
- マイク、サイン波、ホワイトノイズ、無音などを入力にできる。
- GUI、OSC、CLIテストシナリオからMIDIイベントを生成できる。
- 出力を再生しながらWAV収録と統計取得ができる。
- テスト結果をJSONおよび標準出力のNDJSONで取得できる。
- 自動実行時にタイムアウト、自動終了、終了コード判定ができる。
- macOSとWindowsで共通の操作契約を持つ。

### 3.2 非目標

- DAW相当の編集、タイムライン、ミキサー機能
- VST2、AAX、AUv3、LV2の対応
- プラグインの完全なサンドボックス実行
- 商用DAWすべてとの挙動一致
- 音質の主観評価
- 著作権保護やライセンス認証の回避

## 4. 主要ユースケース

### UC-01: 単一エフェクトのスモークテスト

サイン波を対応プラグインへ入力し、5秒間処理して、出力が有限値であり無音でなく、クリップしていないことを確認する。

### UC-02: シンセサイザーのMIDIテスト

無音入力でシンセプラグインをロードし、Note On/Offを送信して、発音、リリース、無音復帰を検査する。

### UC-03: 複数プラグインのチェインテスト

シンセ、ディレイ、リミッターを指定順に接続し、チェイン全体の出力と各段の統計を取得する。

### UC-04: GUI確認

ホストGUIから入力源、MIDI、パラメータ、バイパスを操作し、各プラグインのEditorを表示する。

### UC-05: AIエージェントからの遠隔操作

エージェントがCLIで起動し、OSCでMIDIとパラメータを操作し、NDJSONイベントと最終JSONレポートを解析する。

### UC-06: CI回帰テスト

固定seed、固定サンプルレート、固定ブロックサイズでオフラインレンダリングし、統計またはゴールデン音声との差分で合否判定する。

## 5. 起動モード

### 5.1 リアルタイムGUIモード

- 物理オーディオデバイスを使用する。
- GUIを表示する。
- マイク入力と出力モニタリングを利用できる。
- OSCを受信しながら手動操作できる。

### 5.2 自動リアルタイムモード

- GUIを表示できるが、指定時間経過後に自動終了する。
- 音声デバイスを使用する。
- OSCまたは事前定義シナリオから操作する。
- 完了時にレポートを確定して終了コードを返す。

### 5.3 オフラインモード

- 物理音声デバイスを使用しない。
- 壁時計より高速に処理できる。
- マイク入力は使用不可とする。
- GUI表示は任意だが、処理中のEditor操作をテスト結果の決定性に含めない。
- seed、入力、イベント列、サンプルレート、ブロックサイズが同じ場合、ホスト側の入力とイベント列は再現可能でなければならない。

## 6. コマンドライン仕様

### 6.1 基本形式

```text
AgentPluginHost [global-options] --plugin <path> [--plugin <path> ...]
```

macOSでは`.app`内部バイナリを直接起動する方法に加え、ラッパースクリプトまたは`open --args`で同じ引数を渡せる構成とする。Windowsでは`.exe`へ直接引数を渡す。

### 6.2 起動例

```bash
AgentPluginHost \
  --plugin "/path/to/Synth.vst3" \
  --plugin "/path/to/Delay.vst3" \
  --source sine \
  --frequency 440 \
  --level-db -18 \
  --sample-rate 48000 \
  --block-size 256 \
  --osc-port 9000 \
  --record output.wav \
  --report report.json \
  --run-seconds 10 \
  --gui
```

PowerShell:

```powershell
& .\AgentPluginHost.exe `
  --plugin "C:\VST3\Synth.vst3" `
  --plugin "C:\VST3\Delay.vst3" `
  --source sine `
  --run-seconds 10 `
  --report ".\report.json"
```

### 6.3 オプション

| オプション | 値 | 既定値 | 説明 |
| --- | --- | --- | --- |
| `--plugin` | path | なし | VST3、またはmacOSでは登録済みAUv2 `.component`を指定する。複数回指定可能 |
| `--session` | JSON path | なし | セッション定義を読み込む |
| `--mode` | realtime/offline | realtime | 処理モード |
| `--gui` | flag | on | ホストGUIを表示 |
| `--no-gui` | flag | off | ホストGUIを非表示。ただしGUI必須プラグインは保証対象外 |
| `--show-editors` | flag | off | ロード後に全Editorを表示 |
| `--source` | mic/sine/white-noise/pink-noise/impulse/sweep/silence/file | silence | 入力源 |
| `--input-file` | path | なし | file入力の音声ファイル |
| `--frequency` | Hz | 440 | サイン波周波数 |
| `--level-db` | dBFS | -18 | 信号レベル |
| `--seed` | uint64 | 1 | ノイズとランダムイベントのseed |
| `--sample-rate` | Hz | device/default | サンプルレート |
| `--block-size` | samples | device/default | ブロックサイズ |
| `--input-channels` | count | 2 | 論理入力チャンネル数 |
| `--output-channels` | count | 2 | 論理出力チャンネル数 |
| `--audio-input-device` | name/id | default | リアルタイム入力デバイス |
| `--audio-output-device` | name/id | default | リアルタイム出力デバイス |
| `--midi-input` | name/id | none | 外部MIDI入力 |
| `--osc-bind` | address | 127.0.0.1 | OSC待受アドレス |
| `--osc-port` | port | 9000 | OSC待受ポート、0で無効 |
| `--osc-reply-host` | address | sender | OSC応答先 |
| `--osc-reply-port` | port | sender | OSC応答ポート |
| `--run-seconds` | seconds | 無制限 | 自動終了時間 |
| `--timeout-seconds` | seconds | 30 | 起動・ロード・終了を含む監視時間 |
| `--record` | WAV path | なし | 出力音声の保存先 |
| `--report` | JSON path | なし | 最終レポート保存先 |
| `--events` | NDJSON path | stdout | イベント出力先 |
| `--fail-on` | rule list | non-finite,load-error | 合否ルール |
| `--log-level` | error/warn/info/debug | info | ログレベル |
| `--list-devices` | flag | off | 音声/MIDIデバイスを列挙して終了 |
| `--inspect-plugin` | path | なし | メタデータとパラメータを列挙して終了 |
| `--version` | flag | off | バージョンを表示して終了 |
| `--help` | flag | off | ヘルプを表示して終了 |

### 6.4 引数の優先順位

1. 組み込み既定値
2. `--session`で読み込んだ設定
3. 個別CLI引数
4. 起動後のGUI/OSC操作

後の値が前の値を上書きする。ただし`--plugin`はCLIに1件以上存在する場合、セッション側チェインを全置換する。

### 6.5 終了コード

| コード | 意味 |
| --- | --- |
| 0 | テスト成功、または通常の対話終了 |
| 2 | CLIまたはセッション定義が不正 |
| 3 | プラグインの検出・ロード失敗 |
| 4 | 音声/MIDIデバイス初期化失敗 |
| 5 | テストアサーション失敗 |
| 6 | タイムアウトまたはハング検出 |
| 7 | 出力ファイルまたはレポート書き込み失敗 |
| 8 | プラグイン処理中の非有限値または致命的ランタイムエラー |
| 10以上 | 予約 |

複数障害がある場合は、最初の致命的障害をプロセス終了コードとし、全障害をJSONレポートへ記録する。

## 7. プラグインロードとチェイン

### 7.1 ロード

- `juce::AudioPluginFormatManager`へVST3形式を登録し、macOSではAudio Unit形式も登録する。
- 指定パスを正規化し、存在、拡張子、読取可能性を検査する。
- 対応プラグインバンドル内に複数クラスがある場合、検出したクラス一覧を返す。
- クラスが1件なら自動選択する。
- CLI/セッションで複数クラスの場合はセッション定義のclass ID指定を優先し、未指定なら明示エラーとする。
- GUIのメニュー選択またはドラッグ＆ドロップで複数クラスを含むプラグインを追加した場合は、検出順に全クラスをチェイン末尾へ追加する。
- AUv2はCoreAudioの登録情報から解決されるため、`.component`を標準のComponentsディレクトリへインストールし、AudioComponentRegistrarから認識可能にする必要がある。
- インスタンス生成はメッセージスレッドをブロックしない非同期経路を基本とする。
- ロード進捗とエラーをGUI、NDJSON、OSC応答へ同じ内容で通知する。

### 7.2 チェイン

```text
Source -> Input Tap -> Plugin 0 -> Stage Tap 0 -> ... -> Plugin N -> Output Tap -> Recorder -> Device Output
MIDI Source -----------------------------------------------------> Plugin Chain
```

- `--plugin`の出現順を処理順とする。
- 各プラグインに安定した0始まりindexと実行中UUIDを割り当てる。
- オーディオとMIDIの両方を次段へ渡す。
- MIDI出力を持つプラグインの場合、その出力を次段へ渡す。
- 各段にバイパスと任意の解析tapを置く。
- チェイン変更は停止中のみ許可する。稼働中のGUI追加は音声callbackを安全にデタッチした停止境界で実行し、既存のsource、transport、解析、capture状態を初期化せず再接続する。
- レイテンシーは各プラグインの報告値とチェイン合計を保持する。
- 初期版ではパラレル、send/return、サイドチェイン配線は対象外とする。

### 7.3 バスとチャンネル

- 初期既定はステレオ入出力とする。
- モノ入力プラグインには明示的なdownmix規則を適用する。
- ステレオ入力へモノソースを与える場合は同相信号を複製する。
- 対応不能なバス構成は暗黙変換せずロードエラーとして報告する。
- シンセなどオーディオ入力0のプラグインを許可する。
- MIDI専用プラグインは将来対応とする。

## 8. 入力ソース

### 8.1 必須ソース

| ソース | 設定 |
| --- | --- |
| `silence` | なし |
| `mic` | device、channel mapping、gain |
| `sine` | frequency、phase、level、channel mode |
| `white-noise` | seed、level |
| `pink-noise` | seed、level |
| `impulse` | sample position、amplitude、repeat interval |
| `sweep` | start Hz、end Hz、duration、linear/log |
| `file` | path、loop、start offset、gain |

### 8.2 信号生成契約

- 信号生成器はブロック境界をまたいで位相と状態を維持する。
- ノイズは固定seedで再現可能とする。
- 急激なgain変更には短いスムージングを適用する。
- 非有限または範囲外設定を音声スレッドへ渡さない。
- オフラインモードでは同一構成から同一入力サンプル列を生成する。

## 9. MIDI

### 9.1 入力経路

- GUIのオンスクリーンキーボード
- コンピューターキーボード割り当て
- 外部MIDI入力デバイス
- OSC
- セッション定義のイベント列
- Standard MIDI File。優先度P1

### 9.2 必須イベント

- Note On / Note Off
- Control Change
- Program Change
- Pitch Bend
- Channel Pressure
- Polyphonic Aftertouch
- All Notes Off
- Sustain pedal

### 9.3 タイミング

- イベントはホスト時刻または明示サンプル位置で予約できる。
- 音声ブロックへ投入するときにブロック内sample offsetを保持する。
- 同一sample offsetのイベント順は受信順で安定させる。
- 停止、シーク、テスト中断時には必要に応じてAll Notes Offを送る。
- OSCのネットワークスレッドから音声スレッドへは固定容量の非ブロッキングキューを使う。

## 10. Transport / PlayHeadエミュレーション

プラグインへ次のホスト情報を提供する。

- playing / stopped
- recording
- BPM
- time signature
- sample position
- seconds position
- PPQ position
- loop start/endとlooping
- frame rate。任意

既定値は120 BPM、4/4、先頭位置、playingとする。オフラインモードでもサンプル処理に同期して正確に進行させる。

## 11. GUI仕様

### 11.1 メインウィンドウ

- セッション状態と現在モード
- 音声/MIDIデバイス設定
- 入力ソース設定
- プラグインチェイン
- MIDIキーボードと主要MIDI操作
- Transport
- 入出力メーター
- 波形とFFT表示
- 録音・レポート状態
- OSC接続状態と直近イベント
- エラー、警告、テスト合否

### 11.2 プラグインチェインUI

- index、名称、vendor、version、formatを表示する。
- バイパス、Editor表示、汎用Editor表示、削除、並べ替えを提供する。
- ロード中、成功、失敗、無応答を視覚化する。
- 各段のpeak/RMSとレイテンシーを任意表示する。

### 11.3 Editor

- プラグインのネイティブEditorを別ウィンドウまたはタブで表示する。
- ネイティブEditorがない場合は全パラメータの汎用Editorを生成する。
- Editorの生成、表示、破棄はJUCEメッセージスレッドで行う。
- Editorを閉じてもプラグイン処理は継続する。
- プラグインごとのウィンドウ位置とサイズをセッションへ保存できる。

### 11.4 Agent向けGUI補助

- 主要コンポーネントに安定したautomation IDを付与する。
- 現在状態をJSONへ書き出せる。
- `--screenshot <directory>`を将来追加できる設計とする。
- 表示テキストだけに依存せず、index、parameter ID、UUIDで操作対象を識別する。

## 12. OSC仕様

### 12.1 基本契約

- OSC 1.0互換UDPを使用する。
- 既定bindは`127.0.0.1:9000`とする。
- 外部bindはCLIで明示した場合のみ許可する。
- 状態変更要求は`requestId`を任意の最終引数として受け付ける。
- 応答は`/reply`、エラーは`/error`、非同期イベントは`/event`へ送る。
- OSC受信コールバック内でプラグインやGUIを直接操作しない。

### 12.2 ホスト

```text
/host/ping
/host/state/get
/host/quit
/host/panic
/host/report/reset
/host/report/write <path>
```

### 12.3 入力ソース

```text
/source/type <string>
/source/level_db <float>
/source/frequency <float>
/source/seed <int64>
/source/file <path>
/source/trigger
```

### 12.4 MIDI

```text
/midi/note_on <channel:int> <note:int> <velocity:float> [sampleOffset:int]
/midi/note_off <channel:int> <note:int> <velocity:float> [sampleOffset:int]
/midi/cc <channel:int> <controller:int> <value:int> [sampleOffset:int]
/midi/pitch_bend <channel:int> <value:int> [sampleOffset:int]
/midi/program <channel:int> <program:int>
/midi/all_notes_off [channel:int]
```

MIDI channelは1から16、noteは0から127、velocityは0.0から1.0とする。

### 12.5 プラグイン

```text
/plugin/list
/plugin/<index>/info
/plugin/<index>/bypass <bool>
/plugin/<index>/editor/show <bool>
/plugin/<index>/parameters
/plugin/<index>/parameter/get <parameterId>
/plugin/<index>/parameter/set <parameterId> <normalizedValue:float>
/plugin/<index>/parameter/ramp <parameterId> <target:float> <durationMs:float>
/plugin/<index>/program <programIndex:int>
/plugin/<index>/state/save <path>
/plugin/<index>/state/load <path>
```

パラメータ操作は0.0から1.0のnormalized valueを標準とし、表示値は別フィールドで返す。

### 12.6 TransportとCapture

```text
/transport/play
/transport/stop
/transport/seek_samples <int64>
/transport/bpm <float>
/transport/time_signature <numerator:int> <denominator:int>
/capture/start <path>
/capture/stop
/capture/status
/analysis/snapshot
```

### 12.7 応答例

```text
/reply <requestId> <operation> <status> <jsonPayload>
/error <requestId> <errorCode> <message>
/event <eventName> <jsonPayload>
```

大きなパラメータ一覧や詳細レポートはOSCパケットへ直接詰めず、JSONファイルへ保存してパスと要約を返す。

## 13. 計測と録音

### 13.1 オーディオ統計

入力、各プラグイン段、最終出力について、設定されたtapごとに以下を取得できる。

- サンプル数
- channel count
- sample rate
- minimum / maximum sample
- absolute peakとpeak dBFS
- RMSとRMS dBFS
- meanとDC offset
- crest factor
- clipped sample count
- NaN count
- positive/negative infinity count
- denormal count。任意
- silent sample countと最大連続無音時間
- zero crossing count
- 周波数帯域別energy。優先度P1
- dominant frequency。優先度P1
- integrated loudness/LUFS。優先度P2

### 13.2 ホスト統計

- 処理ブロック数
- 実時間に対する処理時間
- 平均、最大、p95ブロック処理時間
- deadline overrun count
- audio device xrun/dropout count。取得可能な範囲
- プラグイン別およびチェイン合計のreported latency
- ロード時間
- Editor生成時間
- OSC受信、拒否、queue overflow件数
- レコーダーFIFO overflow件数

### 13.3 録音

- 初期形式は32-bit float WAVとする。
- サンプルレートとchannel countは実処理と一致させる。
- 音声スレッドではファイルI/Oを行わない。
- 固定容量FIFOへコピーし、専用writer threadが書き込む。
- FIFO overflowは黙って欠落させず、レポートと合否へ反映する。
- 一時ファイルへ書き、正常close後に最終パスへrenameする。

## 14. 機械可読出力

### 14.1 NDJSONイベント

標準出力はログ文と混在させず、`--events stdout`時は1行1JSONオブジェクトを出力する。人間向けログは標準エラーへ出す。

```json
{"schemaVersion":"1.0","event":"host_started","timestamp":"2026-08-12T12:00:00.000Z","pid":1234}
{"schemaVersion":"1.0","event":"plugin_loaded","index":0,"name":"Example Synth","latencySamples":128}
{"schemaVersion":"1.0","event":"measurement","tap":"output","peakDbfs":-3.2,"rmsDbfs":-18.4,"nonFinite":0}
{"schemaVersion":"1.0","event":"test_completed","passed":true,"report":"report.json"}
```

### 14.2 最終JSONレポート

必須トップレベルフィールド:

```json
{
  "schemaVersion": "1.0",
  "hostVersion": "0.1.0",
  "platform": {},
  "configuration": {},
  "plugins": [],
  "timeline": [],
  "measurements": {},
  "assertions": [],
  "artifacts": [],
  "errors": [],
  "warnings": [],
  "startedAt": "",
  "completedAt": "",
  "durationSeconds": 0.0,
  "passed": false,
  "exitCode": 0
}
```

### 14.3 合否ルール

セッション定義またはCLIから次を設定できる。

- plugin load failure
- non-finite output
- clipping count threshold
- minimum/maximum RMS
- minimum/maximum peak
- silence duration threshold
- DC offset threshold
- deadline overrun threshold
- recorder overflow
- expected latency range
- expected output duration
- golden WAVとの差分。優先度P1

## 15. セッション定義

CLIの複雑化を避けるため、JSONセッションファイルを提供する。

```json
{
  "schemaVersion": "1.0",
  "mode": "offline",
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
    { "path": "Synth.vst3", "bypass": false },
    { "path": "Delay.vst3", "bypass": false }
  ],
  "events": [
    { "atSeconds": 0.10, "type": "noteOn", "channel": 1, "note": 60, "velocity": 0.8 },
    { "atSeconds": 1.10, "type": "noteOff", "channel": 1, "note": 60, "velocity": 0.0 }
  ],
  "durationSeconds": 4.0,
  "capture": { "path": "output.wav" },
  "assertions": [
    { "type": "nonFiniteCount", "op": "eq", "value": 0 },
    { "type": "rmsDbfs", "op": "gt", "value": -80.0 }
  ],
  "report": { "path": "report.json" }
}
```

- 相対パスはセッションファイルの親ディレクトリ基準とする。
- 未知フィールドは警告とし、未知の`schemaVersion`はエラーとする。
- パス、数値範囲、イベント順序を処理開始前に検証する。

## 16. リアルタイム安全性

音声コールバックと、そのコールバックから到達する処理では次を禁止する。

- ヒープ確保、コンテナ拡張、文字列整形
- mutex、condition variable、thread join、待機
- ファイル、ネットワーク、標準入出力、一般loggerアクセス
- 例外の伝播
- GUI操作
- 時間上限のないループ

### 16.1 スレッド間通信

- GUI/OSC/CLI操作はcommand objectへ変換する。
- 固定容量SPSCまたは適切なbounded queueで音声境界へ渡す。
- MIDIイベントはsample offsetを保持する。
- メーターはatomic snapshotまたはlock-free FIFOでUIへ渡す。
- WAVデータは事前確保FIFO経由でwriter threadへ渡す。
- FFT、JSON、統計集約はワーカースレッドで行う。
- queue overflowはカウントし、操作種別ごとの方針に従ってdropまたはテスト失敗とする。

### 16.2 準備とリセット

- buffer、FIFO、FFT scratch、event storageは`prepareToPlay`相当の準備段階で確保する。
- sample rate、block size、channel layout変更時は処理を安全に停止して再準備する。
- reset、transport discontinuity、session reloadの状態遷移を明示する。
- zero channel、短いblock、無音、非有限入力を防御的に扱う。

## 17. 障害耐性

### 17.1 スキャン

- プラグインスキャンはホスト本体と分離したscanner helper processで実行することを推奨する。
- scannerには対象パス1件だけを渡す。
- タイムアウト、異常終了、検出メタデータを親へ返す。
- 直前にスキャンしていたパスをdead-man-pedalファイルへ記録する。
- 問題のあるプラグインをblacklistへ登録できる。

### 17.2 実行時クラッシュ

- MVPではプラグインDSPはホストプロセス内で実行する。
- したがってアクセス違反などのネイティブクラッシュから完全回復することは保証しない。
- 起動側エージェントがプロセス終了、timeout、レポート未確定を検出できる契約を提供する。
- 将来版ではプラグインチェインのworker process分離を検討する。

### 17.3 ハング

- 起動、スキャン、ロード、Editor生成、終了に個別timeoutを設ける。
- 音声処理のheartbeatを監視する。
- panic操作でAll Notes Off、入力停止、録音停止を要求できる。
- 強制終了が必要だった場合、未確定レポートとは別にcrash markerを残す。

## 18. セキュリティとプライバシー

- VST3およびAUv2はネイティブコードであり、ホストユーザーと同じ権限で実行されることを明示する。
- 信頼できないプラグインの実行を安全とはみなさない。
- OSCは既定でloopbackのみへbindする。
- 外部bind時は警告を表示し、将来tokenまたは許可IPを追加できる構造にする。
- 任意ファイル書き込みはユーザーが指定したcapture/report/stateパスに限定する。
- マイク使用はOS権限と明示選択を必要とする。
- レポートには絶対パスを含めるかを設定可能とし、既定では保持する。

## 19. アーキテクチャ

### 19.1 コンポーネント

| コンポーネント | 責務 |
| --- | --- |
| `Application` | 起動、CLI、終了コード、トップレベル状態 |
| `SessionController` | 設定検証、状態遷移、テスト進行 |
| `PluginScannerClient` | scanner helper processとの通信 |
| `PluginLoader` | VST3/AUv2検出、非同期生成、metadata |
| `PluginChain` | Audio/MIDI直列処理、bypass、latency |
| `SourceEngine` | マイク、生成信号、ファイル入力 |
| `MidiScheduler` | GUI/OSC/fileイベントのsample-accurate scheduling |
| `TransportModel` | PlayHead情報 |
| `AnalysisTap` | 軽量なblock統計と解析データ転送 |
| `AnalysisWorker` | FFT、集約、assertion評価 |
| `CaptureWriter` | 非同期WAV保存 |
| `OscController` | OSC受信、検証、応答 |
| `EventSink` | NDJSONイベント |
| `ReportWriter` | 最終JSONレポート |
| `MainWindow` | ホストGUI |
| `PluginEditorWindow` | ネイティブ/汎用Editor所有 |

### 19.2 状態モデル

```text
Starting -> Scanning -> Loading -> Ready -> Running -> Stopping -> Completed
     |          |          |         |         |          |
     +----------+----------+---------+---------+----------+-> Failed
```

- 1時点で状態は1つだけとする。
- `Completed`と`Failed`から新しいセッションを開始する場合は完全なresetを行う。
- 状態遷移をNDJSONとOSCイベントへ出力する。

### 19.3 推奨ディレクトリ

```text
AgentPluginHost/
  BUILD.md
  CMakeLists.txt
  CMakePresets.json
  DESIGN.md
  LICENSE
  README.md
  cmake/
  Source/
    app/
    audio/
    plugins/
    midi/
    osc/
    analysis/
    reporting/
    ui/
    scanner/
  Tests/
    unit/
    integration/
    fixtures/
  artifacts/
  schemas/
  .github/workflows/
```

## 20. JUCE/CMake構成

### 20.1 JUCE導入

- JUCEは`FetchContent`で固定tagまたはcommit SHAを指定する。
- 開発途中で追跡対象が変動する`master`/`develop`は使用しない。
- ロックされたJUCE revisionをレポートと`--version`へ含める。
- 製品配布前にJUCEおよび同梱依存関係のライセンス条件を確認する。

### 20.2 ターゲット

```text
AgentPluginHostCore        JUCE GUIに依存しない設定・解析・テスト基盤
AgentPluginHost            GUIホスト実行ファイル
AgentPluginScanner         スキャン用helper実行ファイル
AgentPluginHostTests       unit/integration tests
TestGainVST3               テスト用fixture plugin
TestSynthVST3              テスト用fixture plugin
```

### 20.3 JUCEモジュール候補

- `juce_core`
- `juce_events`
- `juce_data_structures`
- `juce_gui_basics`
- `juce_gui_extra`
- `juce_audio_basics`
- `juce_audio_devices`
- `juce_audio_formats`
- `juce_audio_processors`
- `juce_audio_utils`
- `juce_dsp`
- `juce_osc`

### 20.4 主要定義

```cmake
JUCE_PLUGINHOST_VST3=1
JUCE_PLUGINHOST_VST=0
JUCE_PLUGINHOST_AU=1  # macOSのみ。Windowsは0
JUCE_USE_CURL=0
JUCE_WEB_BROWSER=0
```

macOS/Windows固有の音声backend定義は必要最小限にする。WindowsはWASAPIを必須、ASIOはSDKおよび配布条件を確認したうえでオプション化する。

## 21. テスト戦略

### 21.1 Unit test

- CLI解析と優先順位
- JSON session schemaと範囲検証
- サイン、ノイズ、impulse、sweepの決定性
- block境界をまたぐ位相継続
- sample offset付きMIDI scheduling
- statistics、NaN/Inf、clip、silence検出
- assertion評価
- report serialization
- queue overflow処理
- transport位置計算

### 21.2 DSP/統合テスト

- silence、impulse、固定seed noise
- 32/64/128/256/512/1024 samplesのblock size
- 44.1/48/96 kHz
- mono、stereo、zero-input synth
- MIDI eventをblock先頭、中間、最終sampleへ配置
- parameter最小/最大と急速automation
- prepare/reset反復
- state save/load round trip
- bypass、latency、tail
- Editorなしでのインスタンス生成
- Editor生成と破棄を別テスト

### 21.3 Fixture plugin

- 既知gainを適用する`TestGainVST3`（VST3、macOS AUv2）
- 固定波形を発音する`TestSynthVST3`（VST3、macOS AUv2）
- latencyを報告するfixture
- MIDIを通過/変換するfixture。優先度P1
- 意図的にNaNを返すfixture。テストビルド限定
- 意図的にload timeoutを起こすscanner fixture。テストビルド限定

### 21.4 E2E

- macOSでVST3を2段ロードし、GUI表示、MIDI、録音、JSON成功を確認
- macOSでfixture AUv2を一時登録し、scan、offline render、format metadata、cleanupを確認
- Windowsで同じsessionを実行し、許容差内の統計を確認
- offlineで同一seedを2回実行し、fixture出力が一致
- 不正パスでexit code 3
- assertion不一致でexit code 5
- timeoutでexit code 6
- read-only出力先でexit code 7
- OSC操作とrequestId応答
- GUI終了、自動終了、外部SIGINT/console close時のレポート確定

### 21.5 静的/動的検査

- compiler warningsをerror扱いできる構成
- clang-format
- clang-tidy。適用範囲を段階導入
- AddressSanitizer / UndefinedBehaviorSanitizer。対応構成
- Windows Application Verifier等は任意
- callback到達コードの手動リアルタイム安全性レビュー

## 22. CI

GitHub Actionsで最低限次のmatrixを実行する。

| OS | Configuration | 内容 |
| --- | --- | --- |
| macOS | Debug | unit/integration、sanitizer可能範囲 |
| macOS | Release | app、scanner、fixture VST3/AUv2 build、offline E2E |
| Windows | Debug | unit/integration |
| Windows | Release | app、scanner、fixture VST3 build、offline E2E |

- GUIと物理デバイスを必要とするテストはCIの必須条件から分離する。
- オフラインE2Eを共通の必須gateとする。
- JUCE FetchContentとcompiler outputをcacheする。
- VST3 fixture、macOS AUv2 fixture、ホスト実行ファイルをartifactとして保存する。
- テスト結果はCTest/JUnit XML、計測結果はJSON artifactとして保存する。
- 依存revision変更時にcache keyを更新する。

## 23. パフォーマンス要件

- 音声callback内にホスト起因のファイル/ネットワークI/Oまたはmutex待機がないこと。
- 48 kHz / 256 samples / stereo / fixture plugin 3段で、ホスト自身の平均処理負荷がリアルタイム期限の10%未満を目標とする。計測環境をレポートする。
- メーターGUI更新は最大60 Hz、既定30 Hzとする。
- FFT更新は最大30 Hz、既定15 Hzとする。
- OSC command queue、MIDI queue、capture FIFOの容量を設定可能とし、overflowを観測可能にする。
- `--no-gui`オフラインモードで不要なGUI更新を行わない。

## 24. ログ

- 人間向けログは標準エラーまたはログファイルへ出力する。
- 機械向けNDJSONを標準出力へ出す場合、他の文字列を混在させない。
- timestamp、level、component、event、messageを含める。
- 音声callbackから直接ログを出さない。
- プラグインパスやユーザー名のredactionを将来設定可能にする。

## 25. 受け入れ条件

### AC-01: クロスプラットフォームビルド

- macOSとWindowsのRelease構成で`AgentPluginHost`、`AgentPluginScanner`、fixture VST3がビルド成功し、macOSではfixture AUv2もビルド成功する。

### AC-02: 複数ロード

- CLIで2つ以上のfixture VST3を指定すると、指定順にロードされる。
- JSONレポートのplugin配列とGUI表示順が一致する。

### AC-03: GUI

- ホストGUIが表示され、各fixtureのネイティブまたは汎用Editorを開閉できる。
- Editorを閉じても音声処理が継続する。
- `PLUGIN > ADD VST3...`から複数VST3を選択して現在のチェインへ追加できる。
- macOSでは`PLUGIN > ADD AUDIO UNIT...`から登録済みAUv2を選択して現在のチェインへ追加できる。
- `.vst3`およびmacOSの`.component`をホスト画面へドラッグ＆ドロップして現在のチェインへ追加でき、非対応パスは無視する。

### AC-04: 入力源

- silence、sine、white-noise、impulse、fileがmacOS/Windowsで動作する。
- micはリアルタイムモードで動作し、権限拒否を明示エラーにする。

### AC-05: MIDI

- GUIおよびOSCからNote On/Offを送信できる。
- block内sample offsetを保持した自動テストが通る。

### AC-06: 計測と録音

- peak、RMS、DC、clip、NaN、Inf、silenceをチャンネル別に取得する。
- WAV sample countが予定処理sample数と一致する。
- 録音中に音声callbackからファイルI/Oを実行しない。

### AC-07: 自動化

- `--run-seconds`で自動終了する。
- 成功時0、ロード失敗時3、assertion失敗時5を返す。
- 最終JSONレポートとNDJSON完了イベントが生成される。

### AC-08: OSC

- loopback OSCからsource、MIDI、parameter、capture、quitを操作できる。
- 不正index、範囲外値、queue overflowをエラー応答する。

### AC-09: 決定性

- fixture plugin、固定session、固定seedによる2回のオフライン実行が、float許容差内で一致する。

### AC-10: リアルタイム境界

- ホストのcallback到達コードに既知のheap allocation、mutex、file/network I/O、GUI操作がないことをレビューとテストで確認する。

## 26. 実装優先順位

### P0: MVP

- macOS/Windows VST3ホスト、macOS AUv2ホスト
- CLIとJSON session
- 複数直列チェイン
- GUIとEditor表示
- silence/sine/white-noise/impulse/file/mic
- GUI/OSC Note On/Off
- parameter get/set
- peak/RMS/DC/clip/NaN/Inf/silence
- 非同期WAV録音
- NDJSONと最終JSON
- 自動終了、timeout、終了コード
- scanner helperとdead-man-pedal
- fixture pluginsとoffline CI

### P1

- MIDI file
- CC/Pitch Bend/AftertouchのGUI補助
- sweep/pink noise
- 各段のanalysis tap
- FFTと帯域energy
- parameter ramp/automation
- state save/load
- golden WAV比較
- screenshot
- ASIOオプション

### P2

- LUFS、THD+N、周波数応答レポート
- MPE
- runtime worker process分離
- sidechain/parallel routing
- remote OSC認証
- macOS AUv3対応

## 27. 既知の制約とリスク

- ネイティブVST3/AUv2はホストプロセスをクラッシュまたはハングさせられる。
- ベンダー独自認証、GPU、WebView、OS APIを使うEditorはCIや`--no-gui`で動作しない場合がある。
- 同一VST3でもmacOS/Windows間で内部DSP、preset、parameter ID、浮動小数点結果が完全一致しない場合がある。
- AUv2はCoreAudioへ登録されていない任意パスの`.component`を直接ロードできない場合がある。
- 実時間CPU統計はOS、音声driver、電源状態に依存する。
- WASAPIとASIOではdevice、buffer、latency挙動が異なる。
- プラグインが報告するlatency/tailが正しいとは限らないため、報告値と実測値を区別する。
- `--no-gui`でもプラグイン内部がメッセージループを要求する可能性があるため、完全なconsole-onlyプロセスにはしない。

## 28. 実装開始時に確定する項目

以下は基本設計を変えずにプロジェクト初期化時に決められる。

- 製品名、会社名、bundle ID、Windows metadata
- 最低macOS deployment target
- Windows ARM64を初期対応へ含めるか
- JUCEの固定tag/commit
- JSONライブラリをJUCE `var`/`JSON`だけで実装するか
- OSC応答payloadの最大サイズ
- GUIテーマとブランド
- ASIOを初期配布物へ含めるか
- 配布方式、署名、公証、Windows code signing

## 29. 参考資料

- JUCE AudioPluginHost: <https://github.com/juce-framework/JUCE/tree/master/extras/AudioPluginHost>
- JUCE CMake API: <https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md>
- AudioPluginFormatManager: <https://docs.juce.com/master/classjuce_1_1AudioPluginFormatManager.html>
- PluginDirectoryScanner: <https://docs.juce.com/master/classjuce_1_1PluginDirectoryScanner.html>
- AudioProcessorGraph: <https://docs.juce.com/master/classjuce_1_1AudioProcessorGraph.html>
- AudioFormatWriter: <https://docs.juce.com/master/classjuce_1_1AudioFormatWriter.html>

---

本仕様では、クロスプラットフォーム性とAIエージェントによる再現可能な自動操作を優先し、macOS/Windows共通のVST3に加えてmacOSネイティブのAUv2 `.component`を同じ操作・レポート契約で扱う。AUv3は将来拡張として対象外に残す。
