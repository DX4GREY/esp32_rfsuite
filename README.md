# RF Suite v2.0 — ESP32-S3 2.4 GHz and Sub-GHz Toolkit

<p align="center">
  <img src="images/banner.svg" alt="RF Suite v2.0 - ESP32-S3 2.4 GHz and Sub-GHz Toolkit">
</p>

A standalone dual-band firmware project for an ESP32-S3, one or two nRF24L01+
modules, an optional CC1101 Sub-GHz transceiver, microSD storage, and a 1.8-inch
ST7735 TFT. Version 2.0 adds a band selector, CC1101 frequency analysis, raw
recording with a live waveform, `.rfr`/Flipper `.sub` library support, packet
analysis, global hardware-missing simulation, and application-wide dirty-region
rendering. The default build remains receive-only; active RF testing is isolated
in a separate controlled-lab build profile.

The interface is designed for a 160 × 128 landscape display. It uses partial/dirty rendering: the complete screen is cleared only during page transitions, while graphs, status values, and menu cards are redrawn only where their content changes. This reduces flicker and keeps the UI responsive.

> [!CAUTION]
> RF Test can transmit traffic that disrupts 2.4 GHz communications. Use it only on equipment you own or are explicitly authorized to test, inside an RF shield box or Faraday enclosure. Never operate it against public networks, third-party devices, safety-critical services, or outside local radio regulations. You are responsible for using this project lawfully and safely.

## Features

- One- or two-radio operation with automatic degraded-mode fallback.
- Optional CC1101 support with OOK/2-FSK presets, RSSI analysis, packet
  inspection, and raw GDO0 capture.
- Sub-GHz library on `/RFSuite/SubGHz` with clean, favorite, rename, delete,
  `.rfr` replay, Flipper RAW `.sub` import/export, live replay progress, and a
  persistent replay result screen.
- Global simulation fallback when nRF24 or CC1101 hardware is missing. Every
  simulated screen is marked `SIM`; simulated data never enables RF output.
- FreeRTOS mutex protection and contention metrics for the shared nRF24 SPI bus.
- 126-channel RF24 spectrum analyzer (`0–125`, or `2400–2525 MHz`).
- Four receive modes: `FAST`, `DIV`, `R1`, and `R2`.
- `ALL`, `Wi-Fi`, and `Bluetooth` scan ranges.
- Modern spectrum graph with `LIVE`, `AVG`, `MAX`, and baseline-relative `DELTA` traces.
- Freeze, 1×/2×/4× zoom, cursor, persistent watch markers, and sampling confidence.
- 24-sweep waterfall with the newest data at the top.
- Single-channel inspector with live activity and peak readings.
- Channel survey showing the five channels with the highest average occupancy.
- RF-environment toolkit with occupancy, heatmap history, burst detection,
  channel comparison, relative interference scoring, before/after snapshots,
  and protocol-band hints.
- RF event detector with configurable threshold, hysteresis, duration, and multi-channel criteria.
- Buffered LittleFS session recorder (256 KiB cap), USB CSV summary, export, and last-sweep replay.
- Experimental passive nRF24 raw-payload sniffer with selectable channel and
  1/2 Mbps rate, live hexadecimal preview, and Serial dump.
- `FAST`, `BALANCED`, `DEEP`, and `CUSTOM` analyzer profiles.
- Independent connectivity diagnostics for both radios.
- Six-page System Status with live ESP32, nRF24, CC1101/Sub-GHz, build-profile,
  storage, UI, scan, and SPI timing data.
- Ten complete UI themes controlling palette, Grid/List layout, header/footer
  geometry, and animation: Cyber, Ocean, Amber, Matrix, Violet, Ice,
  mascot-free Flipper, Retro, Terminal, and Neon.
- Versioned NVS configuration with validation, delayed writes, migration, and confirmed factory reset.
- Native analyzer unit tests and GitHub Actions CI for both firmware profiles.
- Software restart and low-power shutdown using ESP32 deep sleep.
- Three-second watchdog for automatic recovery from an unresponsive main loop.

## Understanding analyzer results

The nRF24L01+ does not provide continuous RSSI measurements. Values displayed as `0–100%` are the percentage of samples in which carrier/RPD activity was detected during an observation window. They are not calibrated dBm readings.

The results are useful for comparing relative activity, finding busy areas, and observing changes over time, but they do not replace a calibrated spectrum analyzer.

Channels shown by the UI are nRF24 RF channels:

```text
frequency_MHz = 2400 + RF24_channel
```

These numbers are not Wi-Fi channel numbers. For example, the center of Wi-Fi channel 1 is near RF24 channel 12 (`2412 MHz`), Wi-Fi channel 6 near RF24 channel 37, and Wi-Fi channel 11 near RF24 channel 62.

## Required hardware

- ESP32-S3 DevKitC-1 or a compatible board.
- 1–2 × nRF24L01+ modules.
- Optional CC1101 module with an antenna matched to the intended Sub-GHz band.
- 1.8-inch 128 × 160 ST7735 TFT.
- 4 × normally-open push buttons.
- One 10–100 µF decoupling capacitor for each nRF24L01+.
- A stable 3.3 V supply and a USB data cable.

Power the nRF24L01+ modules from **3.3 V, never 5 V**. Connect the grounds of the ESP32, both radios, the display, and all buttons. External PA/LNA radio modules may require a dedicated 3.3 V regulator capable of supplying sufficient current.

## Wiring

### nRF24L01+ shared SPI bus

| Signal | Radio 1 | Radio 2 | ESP32-S3 GPIO |
|---|---:|---:|---:|
| VCC | VCC | VCC | 3.3 V |
| GND | GND | GND | GND |
| SCK | SCK | SCK | 12 |
| MOSI | MOSI | MOSI | 11 |
| MISO | MISO | MISO | 13 |
| CE | CE | — | 7 |
| CSN | CSN | — | 6 |
| CE | — | CE | 4 |
| CSN | — | CSN | 2 |
| IRQ | Not used | Not used | — |

Place each decoupling capacitor as close as possible to the corresponding radio's VCC and GND pins. Keep SPI wiring short to reduce communication errors.

### ST7735 TFT

| Module signal | ESP32-S3 GPIO |
|---|---:|
| SCK/CLK | 18 |
| SDA/MOSI | 17 |
| A0/DC | 16 |
| RST/RES | 15 |
| CS | 14 |
| VCC | Match the module, typically 3.3 V |
| GND | GND |

For a TFT module with a microSD slot, connect the slot's `SD_CS` to GPIO 1 and
`SD_MISO` to GPIO 21. The slot shares `SCK` (GPIO 18) and `MOSI` (GPIO 17) with
the display. Override `SD_CS_PIN` and `SD_MISO_PIN` in `build_flags` if your
board uses different wiring. GPIO 3 is intentionally avoided because it is
used by boot/JTAG-related board functions. Use a FAT16/FAT32-formatted card.

The firmware initializes the display with `INITR_BLACKTAB` and rotation `3`. If colors, offsets, or orientation are incorrect, check the panel variant in `DisplayManager::init()`.

### Navigation buttons

All buttons use `INPUT_PULLUP` and are active-low. Connect one side of each button to its GPIO and the other side to GND.

| Button | ESP32-S3 GPIO | General purpose |
|---|---:|---|
| UP | 10 | Previous item or change analyzer band |
| A | 9 | Open, confirm, or perform an action |
| DOWN | 8 | Next item or change analyzer mode |
| B | 5 | Back or switch main-menu page |

On **Tools → PKT SNIFF**, `UP/DOWN` selects nRF24 channel 0–125, `A`
switches between 1 and 2 Mbps, and `B` stops capture. The screen shows the most
recent 32-byte raw payload as hexadecimal; complete payloads are also written
to the Serial Monitor. This experimental mode uses radio 1 only and leaves
radio 2's configuration untouched.

Enable **Settings → SAVE SNIFF → SD CARD** to append captured packets to
`/RFSuite/log/packet_sniffer.csv`. The option can only be enabled when an SD
card was detected at boot. Records contain uptime milliseconds, RF channel,
data rate, and the 32-byte hexadecimal payload; writes are buffered to avoid a
filesystem operation for every received packet.

## Setup and build

The project uses PlatformIO with the Arduino framework.

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the PlatformIO IDE extension, or install PlatformIO Core.
2. Open this directory as a PlatformIO project.
3. Connect the ESP32-S3 using a USB data cable.
4. Build the default receive-only firmware:

   ```bash
   pio run
   ```

5. Upload it to the board:

   ```bash
   pio run --target upload
   ```

6. Open the Serial Monitor at 115200 baud:

   ```bash
   pio device monitor --baud 115200
   ```

The default environment is intentionally receive-only:

```ini
[platformio]
default_envs = analyzer
```

## Versioning and releases

The current firmware version is stored in `VERSION` and exposed in the device's
**System Status → Radio / SW** page. The current release is `v2.0.0`.

To publish a release, first update `VERSION` and `APP_VERSION` to the same
semantic version, commit the change, then push its matching tag:

```bash
git tag v2.0.0
git push origin v2.0.0
```

The release workflow validates the tag, runs native tests, builds the safe
receive-only `analyzer` profile, uploads its `.bin` file and SHA-256 checksum,
and generates release notes from commits and merged pull requests since the
previous release.

Available environments:

| Environment | Purpose | Build command |
|---|---|---|
| `analyzer` | Default receive-only analyzer; active RF Test is not compiled | `pio run -e analyzer` |
| `authorized_rf_lab` | Enables RF Test for shielded, explicitly authorized lab work | `pio run -e authorized_rf_lab` |
| `native` | Host-side unit tests for dependency-free analyzer logic | `pio test -e native` |

Upload a specific profile with `pio run -e analyzer -t upload` or, only for an isolated authorized setup, `pio run -e authorized_rf_lab -t upload`.

The current configuration selects a 4 MB upload flash size, the `default.csv` partition table, DIO flash mode, and USB CDC on boot. Adjust `board_upload.flash_size` if the physical board has a different flash capacity.

### Dependencies

PlatformIO installs these dependencies automatically:

- RF24 `^1.6.2`
- Adafruit ST7735 and ST7789 Library `^1.11.0`
- Adafruit GFX Library `^1.12.6`
- `Preferences` and `SPI` from the Arduino ESP32 framework
- `LittleFS` from the Arduino ESP32 framework

## User interface

After the splash screen, the Main Menu selects 2.4 GHz, Sub-GHz, Settings,
System Info, Lua, SD Files, or Power. The 2.4 GHz catalog retains its paged
feature menu. Choose the `GRID` or `LIST` layout in Settings; the choice applies
to the global, 2.4 GHz, and Sub-GHz menus.

For CC1101 analyzer/record screens, modulation presets, TX-region policy, raw
file format, replay progress controls, and remote compatibility, see the
[Sub-GHz guide](docs/SUB_GHZ.md).

| Control | Main-menu action |
|---|---|
| `UP` / `DOWN` | Move between feature cards |
| `A` | Open the selected feature |
| `B` | Advance to the next menu page |
| `UP` at the first item | Move to the previous page and select its last item |
| `DOWN` at the last item | Move to the next page and select its first item |

### Page 1 — Analyze

| Feature | Purpose | Controls |
|---|---|---|
| Spectrum | Live/average/max/delta graph, cursor, zoom, confidence, and hold | Live: tap `UP`: band, tap `DOWN`: radio mode. Frozen: tap `UP/DOWN`: cursor. Tap `A`: freeze/resume; hold `UP`: trace, hold `DOWN`: zoom, hold `A`: baseline, hold `B`: watch marker |
| Waterfall | Last 24 sweeps, newest at the top | `UP`: band, `DOWN`: radio mode, `A`: clear history |
| Inspect | Deeper observation of one RF channel | `UP/DOWN`: channel ±1, `A`: channel +10 |
| Survey | Top five average channel occupancies | `UP/DOWN`: change band and reset, `A`: reset survey |
| Events | Events that satisfy threshold, hysteresis, duration, and channel-count rules | Tap `UP/DOWN`: threshold/hysteresis; hold `UP/DOWN`: duration/channel count; `A`: clear |
| Logging | Record complete sweeps to LittleFS and summaries to USB Serial | `A`: start or stop a new session |

### Page 2 — Tools

| Feature | Purpose | Controls |
|---|---|---|
| RX Only / RF Test | RX-only notice in the default profile; controlled transmission screen in the lab profile | Lab profile: `UP/DOWN`: target, `A`: start or stop |
| Radio Diag | Check Radio 1 and Radio 2 connectivity | `A`: refresh |
| Profiles | Select analyzer sampling depth | `UP/DOWN`: profile, `A`: change CUSTOM value |
| Settings | Configure RF power, dwell time, UI theme, packet saving, and orientation | `UP/DOWN`: select field, `A`: next value |
| Status | Hardware, memory, radio/software, and performance data | `UP/DOWN`: page, `A`: refresh |
| Power | Restart or enter deep sleep | `UP/DOWN`: option, `A`: confirm |

Press `B` to return to the main menu from any feature screen.

### Page 3 — ENV TEST

| Feature | Purpose | Controls |
|---|---|---|
| Occupancy | Per-channel carrier-hit occupancy and top channels | `A`: start/stop sampling |
| Heatmap | Circular time history across channel groups | `A`: start/stop sampling |
| Bursts | Recent activity increases relative to the moving baseline | `A`: start/stop; `UP/DOWN`: browse events |
| Compare | Compare 2–4 configured RF channels | `A`: start/stop sampling |
| RF Status | Overall relative score, sample rate, cycles, and strongest channel | `A`: start/stop sampling |
| BEF/AFT | Capture two in-RAM snapshots and display their difference | `A`: capture Before, then After; `UP/DOWN`: channel |

### Page 4 — ENV MORE

| Feature | Purpose | Controls |
|---|---|---|
| Band Info | Show possible Wi-Fi, BLE/Bluetooth, and Zigbee overlap at a frequency | `UP/DOWN`: RF channel |
| RX Only / Auth Probe | RX-only notice, or bounded probe controls in the lab build | Lab build: `UP/DOWN`: field, `A`: change/action |

The environment labels are frequency-region hints, not protocol detection, and
all percentages and scores are relative carrier-detection metrics rather than
calibrated RF power. See [RF Environment](docs/RF_ENVIRONMENT.md).

## Scan ranges, radio modes, and profiles

### Scan ranges

| Range | RF24 channels | Approximate frequency |
|---|---:|---:|
| ALL | 0–125 | 2400–2525 MHz |
| Wi-Fi | 1–73 | 2401–2473 MHz |
| Bluetooth | 2–80 | 2402–2480 MHz |

### Receive modes

| Mode | Behavior |
|---|---|
| FAST | Both radios scan adjacent channels in parallel |
| DIV | Both radios observe the same channel and their results are combined |
| R1 | Use Radio 1 only |
| R2 | Use Radio 2 only |

If only one module is detected, requests that require the missing receiver transparently use the available module. With no radios detected, the firmware still boots into its UI and Serial diagnostics instead of entering a reboot loop.

### Analyzer traces and confidence

| Trace | Meaning |
|---|---|
| `LIVE` | Carrier-hit percentage from the latest sweep |
| `AVG` | Exponentially smoothed activity |
| `MAX` | Highest activity observed since boot or a clear |
| `DELTA` | Positive change relative to a RAM-only captured baseline |

`Q` on the spectrum screen is observation confidence derived from samples per channel, available receiver count, and baseline availability. It describes acquisition depth, not RF accuracy and not a dBm calibration. Baselines are intentionally not restored after reboot because the RF environment may have changed.

### Sampling profiles

| Profile | Spectrum | Channel Inspector | Characteristics |
|---|---:|---:|---|
| FAST | 12 samples/channel | 50 samples | Fastest refresh |
| BALANCED | 30 samples/channel | 100 samples | Default |
| DEEP | 60 samples/channel | 200 samples | More stable, slower sweeps |
| CUSTOM | 10–100 samples/channel | 2× spectrum value, capped at 200 | Adjustable with `A` |

The selected profile and CUSTOM sample count are stored in NVS.

## Session recording, SD card, and Lua scripting

At boot, the firmware mounts the TFT microSD slot and creates `/RFSuite/log/`
and `/RFSuite/scripts/`. Open `Analyze → Logging` to start a new session. On SD,
the previous `/RFSuite/log/rf_session.csv` is replaced. If no usable card is
present, recording transparently falls back to LittleFS `/rf_session.csv`.

Lua 5.1 scripts (maximum 32 KiB) can be placed in `/RFSuite/scripts/`, listed
with `lua list`, and executed with `lua run NAME`. The sandbox exposes 2.4 GHz
analysis/control, custom TFT drawing, button input, and integrated Sub-GHz
status, analyzer, raw record, Library, and guarded replay APIs. Sub-GHz Lua
replay uses the native progress/result UI and remains unavailable outside the
`authorized_rf_lab` build. `rf.log(message)` appends to
`/RFSuite/log/lua.log`. File/OS/package loading is disabled and each run has an
instruction limit. See `examples/lua/` for ready-to-copy examples and
self-tests.
The complete API and TFT loading workflow are documented in
[`docs/LUA_SCRIPTING.md`](docs/LUA_SCRIPTING.md).

```text
RFLOG,<timestamp_ms>,<sweep>,<peak_channel>,<peak_percent>,<band>,<radio_mode>,<trace>,<confidence>
```

Example:

```text
RFLOG,18240,57,37,73,Wi-Fi (1-73),FAST,LIVE,65
```

The recorder flushes in batches to reduce flash churn and stops at approximately 256 KiB. Use `session export` to stream the stored CSV or `session replay` to load and freeze its most recent complete sweep. Recording state is not restored after reboot.

## System Status

The Status screen reads live runtime values instead of displaying hard-coded hardware information:

1. **Device Info** — chip model, revision and core count, CPU frequency, flash size and clock, and uptime.
2. **Memory Info** — total, free, and minimum heap, largest allocation block, sketch size, and PSRAM status.
3. **Radio / Software** — each nRF24 connection or simulation state, scan mode,
   receive-only/lab build, ESP-IDF, and firmware version.
4. **Sub-GHz Status** — CC1101 connection, frequency, modulation, TX Region,
   raw recorder, and analyzer state.
5. **Performance** — average/maximum sweep time, UI render time, loop rate, SPI mutex wait, and recorder state.

A radio status of `CONNECTED` confirms SPI communication with the chip. It does not prove that the antenna, RF matching, or receiver sensitivity is working correctly.

## NVS persistence

The current settings use schema version 8, validation, migration, and a
1.5-second deferred write. It also stores menu layout, packet-sniffer SD
logging, CC1101 preset, TX-region policy, Sub-GHz trigger configuration, and
replay count.

- RF power.
- Dwell time.
- Last RF Test target.
- Analyzer profile.
- CUSTOM profile sample count.
- Display theme.
- Analyzer trace (except `DELTA`, because its environmental baseline is RAM-only).
- Event detector configuration.
- Channel watch markers.

Use the exact Serial command `factory reset confirm` to clear the namespace, write validated defaults, and reboot. The explicit confirmation suffix prevents accidental resets.

Waterfall history, survey results, RF events, receive mode, analyzer range, and logging state exist only in RAM and are cleared by a restart or shutdown.

## Shutdown and wake-up

Select `Tools → Power → Shutdown`. The firmware will:

1. Stop both radios.
2. Disable the TFT and place its controller into sleep mode.
3. Put the ESP32-S3 into deep sleep.

To wake the device, hold `A` for approximately 1.5 seconds. A short press returns the device to deep sleep, preventing accidental boots caused by contact noise.

Deep sleep is a very-low-power software shutdown, not physical power disconnection. The board regulator, power LED, and external peripherals may still draw current. A hardware power latch or load switch is required to disconnect the supply completely.

## Serial CLI

The command interface runs at 115200 baud.

| Command | Description |
|---|---|
| `help` | Show the available commands |
| `status` | Show system and radio status |
| `config` / `settings` | Show the current RF configuration |
| `scan` / `spectrum` | Run one sweep and print an ASCII graph |
| `inspect <0-125>` | Measure activity on one RF channel |
| `trace <live\|avg\|max\|delta>` | Select the spectrum trace |
| `freeze` / `resume` | Hold or resume acquisition |
| `zoom <1\|2\|4>` | Set graph zoom around the cursor |
| `cursor <0-125>` | Place the cursor and disable peak-follow |
| `watch <0-125>` | Toggle a persistent channel marker |
| `baseline` | Capture the current average as baseline and select `DELTA` |
| `max clear` | Clear maximum and peak history |
| `event threshold <5-100>` | Set event trigger percentage |
| `event hysteresis <0-threshold>` | Set the release margin |
| `event duration <1-20>` | Set minimum consecutive sweeps |
| `event channels <1-16>` | Require simultaneous qualifying channels |
| `session start\|stop\|info` | Control or inspect LittleFS recording |
| `session export` | Stream the stored full-sweep CSV |
| `session replay` | Load and freeze the last stored sweep |
| `perf` | Print scan, UI, loop, and SPI mutex timings |
| `factory reset confirm` | Restore NVS defaults and reboot |
| `power` / `pwr` | Show the current RF power |
| `power <min\|low\|high\|max>` | Change RF power |
| `dwell` | Show the current dwell time |
| `dwell <10-10000>` | Set dwell time in microseconds |
| `jam <wifi\|bt\|ble\|bledata\|all\|zigbee>` | Select a target and start RF Test; shielded lab use only |
| `start` | Start RF Test with the active target |
| `stop` | Stop all transmission |

The `scan` and `inspect` commands stop RF Test first so that available radios enter the correct receive mode. Transmit commands print an unavailable message in the default `analyzer` build.

## Firmware architecture

```text
Core 0                          Core 1 / Arduino loop
┌─────────────────────┐         ┌────────────────────────────┐
│ RF Test FreeRTOS    │         │ Buttons + Serial CLI       │
│ task (when active)  │         │ Analyzer dispatcher        │
│                     │         │ Partial display renderer   │
└──────────┬──────────┘         └─────────────┬──────────────┘
           └──── mutex-protected nRF24 SPI ───┘
```

- `AppState` owns UI, analyzer, event, survey, and NVS state.
- `RadioManager` controls RX/TX transitions and access to both nRF24 modules.
- `SessionRecorder` buffers full sweeps to LittleFS and supports export/replay.
- `PerformanceMonitor` captures sweep, UI, loop, and SPI wait behavior.
- `DisplayManager` implements the menu grid and dirty-region rendering.
- `MenuCatalog` is the single source of truth for menu labels, destination modes, icons, and open actions.
- `AppModePolicy` centralizes which screens run continuous spectrum acquisition.
- `ButtonManager` provides 50 ms debouncing and long-press detection.
- `SerialCommander` handles the CLI and logging output.
- `Watchdog` monitors the main loop with a three-second timeout.

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for module boundaries and the feature-extension checklist.

## Detailed documentation

The complete manual is indexed in [docs/README.md](docs/README.md). It includes getting started, hardware, every display control, analyzer interpretation, session formats, Serial commands, persistence, development rules, testing, troubleshooting, and RF-lab safety.

## Project structure

| Path | Purpose |
|---|---|
| `platformio.ini` | Board environment, dependencies, flash, and USB CDC settings |
| `include/config`, `src/config` | Pin assignments, constants, channel tables, and static assets |
| `include/core`, `src/core` | Shared types, application state, analyzer aggregation, policies, and NVS |
| `include/drivers`, `src/drivers` | Buttons, dual-radio lifecycle, RF acquisition, and RF Test task |
| `include/services`, `src/services` | Serial CLI, watchdog, session recording, and performance metrics |
| `include/ui`, `src/ui` | Display API, theme, menu catalog, controller, and screen modules |
| `src/ui/screens` | Renderers grouped into menu, analyzer, and system domains |
| `src/main.cpp` | Setup, main loop, shutdown, and wake validation |
| `docs/ARCHITECTURE.md` | Dependency rules and guide for adding features |

## Troubleshooting

### One or both nRF24 modules are not detected

- Confirm that VCC is 3.3 V and all grounds are connected.
- Verify the separate CE/CSN pins and the shared SCK/MOSI/MISO lines.
- Install a 10–100 µF capacitor close to each module.
- Use short wires and a supply that does not sag when both radios are active.
- The firmware makes up to five initialization attempts. It runs with either radio, and stays in diagnostics-only mode if neither responds.

### The ESP32 resets or reports a brownout

- Do not power high-current PA/LNA modules from an undersized on-board regulator.
- Use a dedicated 3.3 V regulator with a common ground.
- Add rail decoupling and shorten the power wiring.

### The display is blank, inverted, or has incorrect colors

- Verify every TFT pin and the common ground.
- Confirm that the panel controller is an ST7735.
- Adjust `INITR_BLACKTAB` and `setRotation(3)` for a different panel variant.

### The Serial Monitor does not appear

- Use a USB data cable and select the correct port.
- Set the monitor to 115200 baud.
- This project enables `ARDUINO_USB_MODE=1` and `ARDUINO_USB_CDC_ON_BOOT=1`; pressing reset after opening the monitor may help on some hosts.

### The device does not wake from shutdown

- Hold `A`/GPIO 9 for at least approximately 1.5 seconds.
- Confirm that the button connects GPIO 9 to GND and has no external pull-down.
- Deep-sleep wake depends on an RTC-capable GPIO, so preserve this pin assignment when changing the wiring.

## Current limitations

- The analyzer measures carrier-detection probability, not absolute RSSI or protocol identity.
- It does not decode Wi-Fi, Bluetooth, BLE, or Zigbee packets.
- Waterfall, survey, event history, and environmental baselines are RAM-only; recorded sweeps use LittleFS.
- The ST7735 renderer does not use a full framebuffer; partial rendering is used to save RAM.
- Software shutdown does not physically disconnect board power.
- Native tests cover dependency-free analyzer math, but radio timing, display offsets, wake behavior, and RF behavior still require hardware-in-the-loop verification.

## Contributing

Before submitting changes, run:

```bash
pio run --target clean
pio test -e native
pio run -e analyzer
pio run -e authorized_rf_lab
```

Keep pin definitions in `include/config/Config.h`, avoid full-screen redraws for dynamic updates, and document changes to Serial or NVS formats to preserve user compatibility.

This repository currently has no separate license file. Add a `LICENSE` before distributing it under specific license terms.
# RF Environment Test

Firmware now includes a passive RF Environment Test subsystem: full/ranged nRF24 carrier-hit occupancy, 32-bucket heatmap, burst events, relative interference scoring, 2–4 channel comparison, before/after snapshots, and frequency-band overlap hints. Results are relative activity observations, not RSSI, dBm, calibrated RF power, or protocol detection.

Use the `analyzer` profile for the default RX-only firmware. The optional `authorized_rf_lab` profile compiles a single-channel, bounded, low-duty probe (LOW power, 100 ms interval, 8-byte payload, 10-second default limit). It never uses continuous carrier or retransmit reuse.
