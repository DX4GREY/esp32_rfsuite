# Lua Scripting Guide

## Editor setup and generated API library

The repository includes a generator for a LuaLS/EmmyLua-compatible definition
of the global `rf` API. Generate it from the repository root:

```bash
python3 tools/generate_lua_library.py
```

The default output is `tools/generated/rfsuite.lua`. Add that directory as a
Lua language-server library (for example, in VS Code workspace settings):

```json
{
  "Lua.workspace.library": [
    "${workspaceFolder}/tools/generated"
  ],
  "Lua.runtime.version": "Lua 5.1",
  "Lua.diagnostics.globals": ["rf"]
}
```

The generated file provides completion, parameter hints, string-value choices,
return types, and the fields of `rf.status()`. It is an editor-only stub: do not
copy or execute it on the device. Scripts themselves still go in
`/RFSuite/scripts/` on the SD card.

The generator reads function names, signatures, defaults, return types, and
descriptions directly from the `LUA_LIBRARY_METADATA` block in
`src/services/LuaEngine.cpp`. It also compares those entries with the actual
`lua_setfield()` registrations and rejects missing or obsolete metadata. After
changing the firmware API, update its adjacent metadata in `LuaEngine.cpp`,
regenerate the file, and verify it with:

```bash
python3 tools/generate_lua_library.py --check
```

Use `--output PATH` when another editor or project needs the stub elsewhere.

## Preparing the SD card

Format the card as FAT16 or FAT32. At boot the firmware creates these folders:

```text
/RFSuite/
├── log/
│   ├── rf_session.csv
│   └── lua.log
└── scripts/
    └── example.lua
```

Copy plain-text Lua 5.1 files into `/RFSuite/scripts/`. A script is limited to
32 KiB. The TFT browser displays up to 32 scripts, sorted by filename.

## Running a script from the TFT

1. Open the `SCRIPTING` menu page.
2. Select `LUA SCRIPTS`.
3. Use `UP` and `DOWN` to select a `.lua` file.
4. Press `A` to run it. Press `B` to return to the menu.

Removing or inserting a card while powered is not supported. Reboot after
changing the card. Script `print()` output is sent to USB Serial at 115200 baud;
completion or error status also appears on the TFT.

The same scripts can be managed through Serial:

```text
lua list
lua run channel_report
```

## Built-in Lua libraries

The VM provides Lua's base, `string`, `table`, and `math` libraries. For safety,
filesystem, operating-system, dynamic package loading, `dofile`, `loadfile`,
`loadstring`, `require`, and direct garbage-collector control are unavailable.
Scripts have a 200,000-instruction limit so an accidental endless loop returns
an error instead of permanently blocking the UI.

This is a synchronous, one-shot runtime: a fresh VM is created for every run,
top-level statements execute once, and all Lua variables disappear afterward.
Long loops delay normal UI and analyzer processing even when they remain under
the instruction limit. Prefer short tasks; for interactive GUI scripts, keep
delays small and leave the loop when the user requests an exit.

## RFSuite `rf` library

### Reading analyzer data

| Function | Result |
|---|---|
| `rf.millis()` | Milliseconds since boot |
| `rf.peak_channel()` | Current peak RF channel, `0..125` |
| `rf.level(channel)` | Latest relative activity percentage |
| `rf.spectrum()` | Table containing all 126 activity values; Lua index 1 is RF channel 0 |
| `rf.status()` | Table of current firmware status fields |

`rf.status()` contains `peak_channel`, `peak_level`, `confidence`, `cursor`,
`sweeps`, `radios`, `frozen`, `logging`, and `environment_running`.

`rf.spectrum()` is deliberately 1-based like a normal Lua array. Convert an
array index to an RF24 channel with `channel = index - 1`. Activity values and
status fields are snapshots; call the function again to obtain newer data.

### Independent nRF24 access

| Function | Behavior |
|---|---|
| `rf.radio_test()` | Return per-module detection and loopback results |
| `rf.radio_sample(radio, channel[, samples])` | Atomically sample only Radio 1 or 2; returns activity percentage or `nil,error` when busy |
| `rf.radio_transmit(radio, channel, payload[, power[, rate]])` | Send one 1–32 byte packet through only the selected radio in `authorized_rf_lab` |

The per-radio calls serialize access to the shared SPI bus, leave the other
module untouched, and restore the selected receiver's previous channel. They
return a busy error instead of taking a module currently used by spectrum scan
or packet sniffing. `power` is `0..3`; `rate` is `250k`, `1m`, or `2m`.

### Analyzer control

| Function | Behavior |
|---|---|
| `rf.set_cursor(channel)` | Move cursor to channel `0..125` |
| `rf.freeze(boolean)` | Freeze or resume acquisition |
| `rf.set_band("all"|"wifi"|"bt")` | Select analyzer band |
| `rf.set_trace("live"|"avg"|"max"|"delta")` | Select trace |
| `rf.capture_baseline()` | Capture current analyzer baseline |
| `rf.clear_max()` | Clear maximum trace history |
| `rf.toggle_watch(channel)` | Toggle a watched-channel marker |

### Storage and environment test

| Function | Behavior |
|---|---|
| `rf.log(message)` | Append `milliseconds,message` to `/RFSuite/log/lua.log` |
| `rf.recording(true|false)` | Start a new session or stop recording; returns success |
| `rf.environment(true|false)` | Start or stop passive occupancy analysis; returns success |

### Sub-GHz and CC1101

The Sub-GHz Lua API uses the same `Cc1101Manager`, `SubGhzRawService`, storage,
TX-region checks, cooldown, clear-channel assessment, and replay UI as the
native menus. It does not provide a bypass around firmware safety controls.

| Function | Behavior |
|---|---|
| `rf.subghz_status()` | Return CC1101, analyzer, recorder, peak, simulation, TX-build, region, and error fields |
| `rf.subghz_set_frequency(mhz)` | Tune within `300..348`, `387..464`, or `779..928` MHz |
| `rf.subghz_set_preset(name)` | Select `ook270`, `ook650`, `fsk2`, `fsk12`, or `fsk47` and persist it |
| `rf.subghz_set_region(name)` | Select and persist `rx_only`, `etsi`, or `fcc`; this does not transmit |
| `rf.subghz_analyzer(true|false)` | Start or stop the 20-point CC1101 analyzer |
| `rf.subghz_record(true[,mhz])` | Start raw capture at an optional frequency, or pass `false` to stop/save |
| `rf.subghz_files()` | Return up to 32 `.rfr`/`.sub` Library filenames |
| `rf.subghz_replay(filename)` | Replay through the native progress/result UI in `authorized_rf_lab` |

`rf.subghz_status()` returns `connected`, `simulation`, `tx_enabled`,
`frequency`, `preset`, `region`, `recording`, `armed`, `pulses`, `rssi`,
`analyzer_running`, `analyzer_peak_frequency`, `analyzer_peak_rssi`, and
`last_error`.

Raw recording requires CC1101 GDO0 and SD storage. Starting a recording from
Lua leaves it active after the one-shot script exits, allowing the script to
open the native Record screen. Replay accepts safe Library basenames only and
requires `.rfr` or `.sub`. In the normal `analyzer` build it raises a Lua error
before any TX attempt. In `authorized_rf_lab`, it opens the same animated live
progress page used by Library, honors `B` abort, and remains on the completion
or stopped result page after the Lua call returns.

Setting `etsi` or `fcc` only changes the firmware allowlist. It does not grant
authorization or guarantee compliance. See [Sub-GHz](SUB_GHZ.md) and
[Safety](SAFETY.md).

### TFT navigation

`rf.open_screen(name)` changes to one of `spectrum`, `waterfall`, `inspect`,
`survey`, `events`, `logging`, `status`, `menu`, `subghz`,
`subghz_analyzer`, or `subghz_record` after the script finishes. Opening
`subghz_analyzer` also starts its analyzer service.

### Custom TFT GUI

Lua can draw inside a protected 152×86 pixel canvas. The firmware retains its
header, border, and button footer. Canvas coordinates start at `(0, 0)` in the
top-left content area and are clipped so scripts cannot overwrite the firmware
frame.

| Function | Behavior |
|---|---|
| `rf.gui_begin(title)` | Open and clear a firmware-framed Lua GUI |
| `rf.gui_footer(left,middle,right)` | Set three firmware footer labels |
| `rf.gui_clear()` | Clear only the Lua canvas |
| `rf.gui_text(x,y,text,color)` | Draw clipped single-line text |
| `rf.gui_pixel(x,y,color)` | Draw one pixel |
| `rf.gui_line(x0,y0,x1,y1,color)` | Draw a clipped line |
| `rf.gui_rect(x,y,w,h,color,filled)` | Draw an outline or filled rectangle |
| `rf.gui_circle(x,y,r,color,filled)` | Draw an outline or filled circle |
| `rf.button("up"|"down"|"a"|"b")` | Read an active-low button during a script |
| `rf.delay(milliseconds)` | Yield for `0..1000` ms and feed the watchdog |

Colors are `white`, `black`, `gray`, `accent`/`cyan`, `green`, `yellow`,
`orange`, and `red`. Color arguments are optional and default to white. On a
Lua GUI, press `A` to rerun the script and `B` to return to the script list.

Optional parameters are positional. To fill a rectangle with the default white
color, pass the default color explicitly: `rf.gui_rect(0, 0, 20, 10, "white",
true)`. Canvas drawing is not automatically refreshed by a callback; the script
must draw each new frame itself.

### Controlled-lab RF functions

`rf.lab_start(target)` and `rf.lab_stop()` mirror the firmware's controlled-lab
feature. `target` can use the same target names accepted by the firmware CLI.
`rf.lab_start()` is compiled out of the normal `analyzer` profile and returns a
Lua error there. It is available only in `authorized_rf_lab`, preserving the
same build-time safety boundary as the native firmware feature.

## Examples

Ready-to-copy scripts are available in `examples/lua/`:

| Script | Purpose |
|---|---|
| `01_status_report.lua` | Print radio, sweep, peak, and recorder status |
| `02_top_channels.lua` | Rank the ten busiest channels |
| `03_wifi_report.lua` | Summarize and log activity in channels 1–73 |
| `04_threshold_alert.lua` | Print and log channels above a threshold |
| `05_watch_busy.lua` | Mark the five busiest channels for watching |
| `06_delta_setup.lua` | Capture a baseline and open the DELTA trace |
| `07_start_recording.lua` | Start CSV recording and open Logging |
| `08_stop_recording.lua` | Stop CSV recording |
| `09_environment_start.lua` | Start passive environment sampling |
| `10_channel_inspect.lua` | Freeze and inspect the current peak channel |
| `11_subghz_status.lua` | Print CC1101, recorder, analyzer, and TX-policy status |
| `12_subghz_record_433.lua` | Configure OOK 433.92 MHz, start raw capture, and open Record |
| `13_subghz_library.lua` | List `.rfr` and `.sub` Library files |
| `14_subghz_replay.lua` | Replay one owned Library file with the native UI in the RF-lab build |
| `30_custom_gui.lua` | Render a custom RF dashboard inside the firmware frame |
| `40_snake_game.lua` | Interactive Snake using all four hardware buttons |
| `90_api_self_test.lua` | Validate status, spectrum, levels, and range checks |
| `91_control_self_test.lua` | Test cursor/freeze controls and invalid arguments |
| `92_integration_self_test.lua` | Test SD logging and environment start/stop |
| `93_rf_lab_self_test.lua` | Test authorized RF start/stop and target validation |
| `94_subghz_self_test.lua` | Non-transmitting Sub-GHz status, Library, and validation tests |

Copy the desired files to `/RFSuite/scripts/` on the SD card.

### Running the Lua self-tests

Copy the `9x_*_self_test.lua` files to `/RFSuite/scripts/`, then run them
from the TFT or Serial:

```text
lua run 90_api_self_test
lua run 91_control_self_test
lua run 92_integration_self_test
lua run 94_subghz_self_test
```

Each test prints individual `PASS`/`FAIL` lines followed by a summary. The API
test is read-only. The control test temporarily changes cursor/freeze state and
restores it. The integration test appends a marker to `lua.log` and briefly
starts/stops passive environment sampling when it was not already running.

The RF lab self-test must only be run in a shielded, explicitly authorized RF
setup with the `authorized_rf_lab` firmware. It starts the Wi-Fi lab target and
stops it immediately, then verifies that an invalid target is rejected. On the
normal `analyzer` build it prints `SKIP`; active RF remains compiled out.

The Sub-GHz self-test does not transmit or start recording. It verifies status
and Library return types plus invalid frequency, preset, and region rejection.

Read a full spectrum and log channels above 70%:

```lua
local levels = rf.spectrum()
for index, level in ipairs(levels) do
    local channel = index - 1
    if level >= 70 then
        rf.log(string.format("high activity ch=%d level=%d", channel, level))
    end
end
```

Configure the analyzer and open its TFT screen:

```lua
rf.set_band("wifi")
rf.set_trace("avg")
rf.freeze(false)
rf.recording(true)
rf.open_screen("spectrum")
```

Check status before starting a passive environment test:

```lua
local status = rf.status()
print("radios", status.radios, "peak", status.peak_channel)
if status.radios > 0 then
    assert(rf.environment(true), "environment analyzer could not start")
end
```

## Error handling

Use `assert`, `pcall`, and explicit validation like normal Lua:

```lua
local ok, message = pcall(function()
    rf.set_band("invalid")
end)
if not ok then
    rf.log("script error: " .. tostring(message))
end
```

Keep scripts short and event-oriented. A script runs synchronously, so regular
button processing and scanning resume after it exits.

## Script design checklist

- Keep the file below 32 KiB and use a safe filename containing letters,
  numbers, `_`, `-`, or `.`.
- Validate channels before calling channel functions; valid RF24 channels are
  `0..125`.
- Remember that displayed activity is a relative carrier-hit percentage, not
  calibrated RSSI or dBm.
- Check boolean results from `rf.recording()`, `rf.environment()`, and
  Sub-GHz operations instead of assuming the operation started.
- Check `rf.subghz_status().tx_enabled` before presenting a replay action, but
  still handle errors from `rf.subghz_replay()` because region, hardware,
  storage, cooldown, and clear-channel checks can reject it.
- Wrap expected argument failures with `pcall`; an unhandled Lua error stops the
  whole script and is reported on Serial/TFT.
- Use `rf.delay()` rather than a busy-wait loop. Each call is limited to 1000 ms.
- Stop controlled-lab activity with `rf.lab_stop()` before leaving a lab script,
  including its error path. Active RF remains unavailable in normal builds.
