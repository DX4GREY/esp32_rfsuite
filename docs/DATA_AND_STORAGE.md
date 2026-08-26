# Data and Storage

## Storage overview

| Data | Location | Survives reboot | Notes |
|---|---|---:|---|
| User configuration | NVS namespace `appstate` | Yes | Versioned schema and deferred writes |
| Latest recorded session | SD `/RFSuite/log/rf_session.csv`, LittleFS fallback `/rf_session.csv` | Yes | Replaced when a new session starts |
| Sub-GHz raw captures | SD `/RFSuite/SubGHz/*.rfr` | Yes | One file per completed CC1101 raw capture |
| Flipper RAW interchange | SD `/RFSuite/SubGHz/*.sub` | Yes | Imported directly or exported from `.rfr` |
| Lua application log | SD `/RFSuite/log/lua.log` | Yes | Appended by `rf.log()` |
| Live, AVG, MAX, baseline | RAM | No | DELTA baseline is deliberately temporary |
| Waterfall, survey, analyzer events | RAM | No | Rebuilt from new sweeps |
| RF-environment statistics, heatmap, bursts, snapshots | RAM | No | Configuration is persisted, measurements are not |
| Recorder active state | RAM | No | Starts as stopped after boot |

See [Persistence](PERSISTENCE.md) for NVS details.

## SD media playback

File Explorer supports lightweight RFSuite media formats inside the protected
152×86 display area:

- `.rfv`: uncompressed RGB565 video, 1–30 FPS. `A` skips ten seconds and `B`
  closes the player.
- `.rfi`: one RGB565 photo. `UP` opens the next photo in the current folder,
  `DOWN` opens the previous photo, and `B` closes the viewer.

Convert regular media using FFmpeg and the included tools:

```text
python3 tools/rfv_convert.py input.mp4 output.rfv --fps 10
python3 tools/rfi_convert.py input.jpg output.rfi
```

Copy the converted files anywhere on the SD card and open them with `A` from
`SD FILES`. Aspect ratio is preserved with black padding. Audio, MP4, JPEG,
and PNG decoding are intentionally performed on the computer rather than the
ESP32 to keep firmware RAM and flash use low.

## Starting and stopping recording

Start from Analyze → Logging with `A`, or over Serial:

```text
session start
```

Starting recording removes the previous session CSV on the active storage backend and creates a new v1 header. Stop with `A` on the Logging page or:

```text
session stop
```

Stopping flushes pending data. Leaving the Logging page does not stop an active recorder, but only screens registered for continuous acquisition produce new sweeps.

## Buffering and limits

- Pending text reserves approximately 4 KiB of heap.
- A flush occurs at 3,072 pending bytes or after about two seconds.
- The session stops when persisted plus pending data reaches approximately 256 KiB.
- A storage error stops recording and is available through `session info`.
- The main loop synchronizes the Logging indicator if the recorder stops automatically.

The recorder calls `LittleFS.begin(true)`. If mounting fails, the Arduino LittleFS implementation is allowed to format the filesystem. Do not store unrelated irreplaceable data in this filesystem partition.

## USB summary format

While `loggingEnabled` is true, each completed sweep prints:

```text
RFLOG,<ms>,<sweep>,<peak_channel>,<peak_percent>,<band_name>,<radio_mode>,<trace>,<confidence>
```

Example:

```text
RFLOG,18240,57,37,73,Wi-Fi (1-73),FAST,LIVE,65
```

Fields:

| Field | Meaning |
|---|---|
| `ms` | `millis()` since boot |
| `sweep` | Completed survey sweep count |
| `peak_channel` | Strongest RF channel in the sweep |
| `peak_percent` | Carrier-hit percentage at that channel |
| `band_name` | Human-readable selected scan band |
| `radio_mode` | `FAST`, `DIV`, `R1`, or `R2` |
| `trace` | Selected display trace |
| `confidence` | Observation-depth heuristic |

## LittleFS session format

The file begins with metadata comments and a compact sweep header:

```text
# RF24 analyzer session v1
# ACTIVITY values are carrier-hit percentages, not dBm
# firmware_build=<profile> compiled=<date> <time>
# E: type,ms,test,start_ms,duration_ms,radios,min_ch,max_ch,window_s,avg,peak_ch,peak_pct,score,bursts,top5
# P: type,ms,channel,pa,data_rate,payload_size,packets,interval_ms,duration_ms
type,ms,sweep,peak_ch,peak_pct,confidence,band,mode,trace,ch0..ch125
```

Each data row starts with `S` and contains 135 fields: nine metadata fields followed by 126 activity values.

```text
S,<ms>,<sweep>,<peak_ch>,<peak_pct>,<confidence>,<band>,<mode>,<trace>,<ch0>,...,<ch125>
```

The header uses `ch0..ch125` as compact notation; consumers must expand it to 126 columns.

### Numeric enums

| Field | Values |
|---|---|
| `band` | `0=ALL`, `1=Wi-Fi`, `2=Bluetooth` |
| `mode` | `0=FAST`, `1=DIV`, `2=R1`, `3=R2` |
| `trace` | `0=LIVE`, `1=AVG`, `2=MAX`, `3=DELTA` |

The 126 channel values always store the live `spectrumLevels`, regardless of the selected display trace.

### Environment and probe rows

When recording is active, stopping an environment run or capturing a snapshot
adds an `E` row:

```text
E,<ms>,<test>,<start_ms>,<duration_ms>,<radios>,<min_ch>,<max_ch>,<window_s>,<avg>,<peak_ch>,<peak_pct>,<score>,<bursts>,<ch:pct>...
```

`test` is currently `occupancy`, `compare`, `before`, or `after`. Five trailing
`channel:moving_average` values form the top-five list. In the lab build, a
completed bounded probe adds a `P` row:

```text
P,<ms>,<channel>,<pa>,<data_rate>,<payload_size>,<packets>,<interval_ms>,<duration_ms>
```

`P` is compiled only into `authorized_rf_lab`. These rows share the same 256 KiB
file limit and buffer as `S` rows.

## Export

```text
session export
```

The command flushes pending data and prints the file between markers:

```text
--- RF SESSION CSV BEGIN ---
...
--- RF SESSION CSV END ---
```

Automation should discard the marker lines and retain the comments/header if metadata is useful.

## Replay

```text
session replay
```

Replay stops radio activity and recording, finds the last complete `S` row, loads its peak, confidence, and 126 live levels, opens Spectrum, and freezes acquisition.

Replay does not animate the entire session and does not restore the recorded band, radio mode, or trace enum. It is a last-sweep inspection feature.

## Meaning of `SESSION STOPPED`

Status → Performance reads `sessionRecorder.isRecording()`:

- `STOPPED`: recorder inactive, not yet started, manually stopped, size limit reached, or stopped by an error.
- `<n> sweeps`: recorder active and `n` sweeps have been accepted in the current runtime session.

Use `session info` to distinguish a normal stop from an error.
