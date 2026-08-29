# User Guide

## Main menu

The original RF Suite boot animation presents travelling spectrum bins, a
rotating RF locator, deterministic interference glitches, a sharp pulse trace,
radar lock, and a flashing `SIGNAL ACQUIRED` state. It borrows
the compact staged-boot concept common to ESP32 handheld firmware without
copying another project's artwork, logo, or motion design. After it completes,
the themed Main Menu provides **2.4 GHz**,
**Sub-GHz**, Settings, System Info, Lua Scripts, SD Files, and Power. Use
`UP`/`DOWN` to select and `A` to open.

From the 2.4 GHz radio menu, hold `B` to return to the Main Menu. From the
Sub-GHz menu, press `B` to return. The Sub-GHz menu contains Analyzer, Record,
Library, Presets, Packets, and RF Test.

### Sub-GHz Frequency Analyzer

If the selected radio is unavailable, the application offers a global
simulation mode: nRF24 for 2.4 GHz or CC1101 for Sub-GHz. Simulated screens are
explicitly marked `SIM` and show generated demonstration data only; they are
not real RF measurements. Capture files and every RF transmission remain
disabled for simulated radios.

Analyzer continuously samples 20 frequencies across the CC1101 hardware bands
and displays live RSSI bars, reference guides, frequency endpoints, and a
highlighted peak. Press `A` to refine the strongest result in 100 kHz steps,
lock it, and open Record. Press `B` to stop scanning.

### Sub-GHz Record Raw

Choose 315, 433.92, 868, or 915 MHz with `UP`/`DOWN`, then press `A` to start
capturing pulse timings from CC1101 GDO0. Press `A` again to stop and save.
Recording also stops automatically at 30 seconds or 8,192 pulses. Files are
stored as `/RFSuite/SubGHz/RAW_<frequency>_<time>.rfr` on the SD card.

While recording, the screen shows frequency, `READY`/`ARMED`/`REC` state, a
live HIGH/LOW waveform, pulse count, RSSI, pulse rate, and buffer usage.
Horizontal length represents pulse duration at approximately 100 microseconds
per pixel. The trace wraps inside its graph card and uses incremental drawing.

Auto-trigger is enabled by default: Record first shows `ARMED`, then starts pulse
capture when RSSI crosses the configured threshold. Hold `B` while idle to
toggle auto-trigger. The metrics panel reports pulse count, live RSSI, pulse
rate, and buffer usage. Saving removes paired glitches shorter than 80
microseconds, trims long start silence, estimates the timing element, and
reports a generic PWM/OOK candidate when the pulse pairs are consistent.

### Sub-GHz Library and Emulate

Library lists `.rfr` and Flipper `.sub` files, with favorites sorted first.
`UP`/`DOWN` selects and `A` replays in the RF-lab build. Hold `A` to toggle a
favorite, hold `UP` to export `.rfr` as Flipper RAW `.sub`, hold `DOWN` to
clean a recording again, and press `A` while holding `UP` to rename it to the
next free `SIGNAL_N` name. Hold `B`, then press `A`, to confirm deletion; tap
`B` to cancel. Only replay is TX-locked in the analyzer build.

Imported `.sub` files support standard OOK/2-FSK presets, custom CC1101
register pairs, frequency metadata, and signed `RAW_Data` timing lines.

During replay, a dedicated process screen shows frequency, repeat progress,
pulse progress, percentage, a progress bar, and RF animation. `B` aborts an
active transmission. Completion remains visible until the user presses
`A REPLAY` to send the same file again or `B BACK` to return to Library.

### Sub-GHz Presets

Choose OOK 270 kHz, OOK 650 kHz, or 2-FSK deviation 2/12/47 kHz with
`UP`/`DOWN`. Tap `A` to select `RX ONLY`, `ETSI`, or `FCC` TX policy. Hold `A`
to cycle the auto-trigger threshold from -90 through -70 dBm. Hold `B` to
select 1x, 3x, or 5x replay; tap `B` to return. Preset, region, trigger, and
replay-count configuration are persisted.

TX Region is a transmit allowlist, not a modulation or automatic statement of
local compliance. `RX ONLY` blocks replay and RF Test. `ETSI` and `FCC` enable
only their firmware-defined frequency ranges; an out-of-policy file reports
`REGION BLOCKED`.

OOK switches the carrier according to pulse timing and is common in simple
fixed-code remotes. 2-FSK represents data using two nearby frequencies. The
preset must match the source signal; sharing 433.92 MHz alone does not make two
signals compatible. See [Sub-GHz](SUB_GHZ.md) for the complete format,
compatibility, replay-flow, and safety reference.

### Sub-GHz Packet Analyzer

Packet Analyzer uses the CC1101 FIFO and shows frequency, packet count, RSSI,
LQI, CRC result, and the first eight payload bytes. `UP`/`DOWN` changes the
frequency preset and `A` clears/restarts capture. Received packets are appended
to `/RFSuite/SubGHz/packets.csv` when SD is available.

### CC1101 RF Test

Press `A` to start or stop a lab sweep across the four supported presets. This
screen does not offer manual frequency selection. It is available only in the
`authorized_rf_lab` build. Region allowlists, clear-channel assessment, a
10-second TX limit, and a one-second cooldown are enforced. Use it only in an
authorized, controlled RF setup.

## Display and navigation model

The TFT operates in 160 × 128 landscape orientation. Full-screen clearing occurs only when changing page layouts. Live screens update dirty graph columns, status fields, or cards to minimize flicker and SPI traffic.

The 2.4 GHz radio menu contains four pages: Analyze, Tools, ENV TEST, and
ENV MORE. The global Settings screen can render these pages as a 2 × 3 card
grid or a four-row scrolling list.

| Main-menu control | Action |
|---|---|
| `UP` / `DOWN` | Move selection between cards |
| `A` | Open selected card |
| Tap `B` | Advance to the next menu page |
| Hold `B` | Return to Main Menu |
| `UP` at the first item | Open the previous page at its last item |
| `DOWN` at the last item | Open the next page at its first item |

## Analyze page

### Spectrum

Displays the selected trace across the current band. The header reports peak channel and activity. The information row reports cursor channel/frequency, cursor value, optional watch marker `*`, and confidence `Q`.

| Control | Live acquisition | Frozen acquisition |
|---|---|---|
| Tap `UP` | Next scan band | Cursor +1 channel |
| Tap `DOWN` | Next radio mode | Cursor −1 channel |
| Tap `A` | Freeze | Resume and restore peak-follow cursor |
| Hold `UP` | Cycle `LIVE → AVG → MAX → DELTA` | Same |
| Hold `DOWN` | Cycle zoom `1× → 2× → 4×` | Same |
| Hold `A` | Capture baseline and select `DELTA` | Same |
| Hold `B` | Toggle watch marker at cursor | Same |
| Tap `B` | Stop radios and return to menu | Same |

Zoom changes only the displayed viewport around the cursor. Acquisition still scans the selected band.

### Waterfall

Shows up to 24 completed sweeps, newest at the top.

| Control | Action |
|---|---|
| `UP` | Next band |
| `DOWN` | Next radio mode |
| `A` | Clear waterfall history |
| `B` | Return to menu |

### Inspect

Observes one channel with a deeper sample count. It displays RF channel, derived frequency, live activity, peak activity, and a simple activity/clear indicator.

| Control | Action |
|---|---|
| `UP` | Channel +1 |
| `DOWN` | Channel −1 |
| `A` | Channel +10 with wraparound |
| `B` | Return to menu |

### Survey

Ranks the five channels with the highest average carrier-hit occupancy within the selected band.

| Control | Action |
|---|---|
| `UP` or `DOWN` | Change band and reset survey accumulation |
| `A` | Reset survey accumulation |
| `B` | Return to menu |

### Events

Shows recent events and the active configuration as `T`, `H`, `D`, and `M`.

- `T`: trigger threshold percentage.
- `H`: hysteresis percentage.
- `D`: required consecutive sweeps.
- `M`: required simultaneous qualifying channels.

| Control | Action |
|---|---|
| Tap `UP` | Threshold +5; wraps from 90 to 30 |
| Tap `DOWN` | Hysteresis +5; wraps from 30 to 0 |
| Hold `UP` | Duration +1; wraps from 5 to 1 in the UI |
| Hold `DOWN` | Minimum channels +1; wraps from 4 to 1 in the UI |
| `A` | Clear event history |
| `B` | Return to menu |

The Serial CLI supports the wider validated ranges documented in [Serial CLI](SERIAL_CLI.md).

### Logging

Controls the LittleFS session recorder and USB `RFLOG` summaries.

| Control | Action |
|---|---|
| `A` | Start a new session or stop the current one |
| `B` | Return to menu without automatically stopping an active session |

Starting a session replaces the previous session file. `STOPPED` is the normal inactive state. See [Data and Storage](DATA_AND_STORAGE.md).

## Tools page

### RX Only / RF Test

The default `analyzer` build shows an RX-only information page; active transmit behavior is not compiled. The `authorized_rf_lab` build shows the controlled RF Test screen.

In the lab build only:

| Control | Action |
|---|---|
| `UP` / `DOWN` | Change the predefined laboratory target group |
| `A` | Start or stop the selected test |
| `B` | Stop and return to menu |

Use only under the restrictions in [Safety and Authorized Use](SAFETY.md).

### Radio Diag

Reports SPI connectivity separately for R1 and R2. Press `A` to refresh and `B` to return.

### Profiles

| Profile | Spectrum samples/channel | Inspector samples | Use case |
|---|---:|---:|---|
| FAST | 12 | 50 | Highest refresh rate |
| BALANCED | 30 | 100 | Default compromise |
| DEEP | 60 | 200 | More observations, slower sweep |
| CUSTOM | 10–100 | 2× spectrum, capped at 200 | User-selected depth |

Use `UP/DOWN` to select a profile. When CUSTOM is active, `A` increments the sample count by 10 and wraps after 100.

### Settings

Use `UP/DOWN` to select and `A` to advance the value.

| Setting | Values | Notes |
|---|---|---|
| TX Power | MIN, LOW, HIGH, MAX | Applied to both nRF24 and CC1101 lab transmission; CC1101 uses mapped PA-table steps |
| TX Dwell | Preset microsecond values | Applied to nRF24 hopping and the CC1101 RF Test sweep |
| UI Theme | CYBER, OCEAN, AMBER, MATRIX, VIOLET, ICE, FLIPPER, RETRO, TERMINAL, NEON | Controls colors, menu layout, geometry, and animation |
| Sniff to SD | OFF, SD CARD | Controls nRF24 packet-sniffer saving when an SD card is mounted |
| Orientation | LANDSCAPE, PORTRAIT, LANDSCAPE FLIP, PORTRAIT FLIP | Rotates the display in 90-degree steps and rotates all four button functions to match |

Menu View is no longer separate. OCEAN, MATRIX, ICE, and TERMINAL use a List
layout; the other profiles use Grid. Themes also own their card silhouette,
selection marker, header symbol, footer geometry, and motion language:

| Theme | Layout | Geometry and motion |
|---|---|---|
| CYBER | Grid | Rounded instrumentation cards and signal-bar motion |
| OCEAN | List | Pill-shaped rows, sonar rings, and a moving current dot |
| AMBER | Grid | Tight instrument corners, calibration pixels, and scan axes |
| MATRIX | List | Square bracketed rows and falling phosphor columns |
| VIOLET | Grid | Softer cards, underlined focus, and expanding energy bands |
| ICE | List | Faceted selection chevrons and snow-cross motion |
| FLIPPER | Grid | Compact orange focus rails and RF chevrons, without a mascot |
| RETRO | Grid | Square CRT cards, top scan rails, and a moving scanline |
| TERMINAL | List | Flat command rows, cursor markers, and phosphor rain |
| NEON | Grid | Double-outline cards, split headers, and orbit animation |

Theme animation is paused during timing-sensitive Sub-GHz record, replay, and
RF Test operations.

Moving between items uses a four-frame focus transition. The destination card
changes from its idle border into the theme-specific marker over roughly 48 ms;
List scrolling rebuilds the viewport once and animates only the newly focused
row. Delays yield to FreeRTOS and the transition is never used inside RF timing
loops.

### Status

Use `UP/DOWN` to change page, `A` to refresh, and `B` to return.

1. **Device Info**: chip model, revision/cores, CPU, flash size, flash clock, uptime.
2. **Memory Info**: total/free/minimum heap, largest allocation, sketch use, PSRAM.
3. **Radio / Software**: R1/R2 connection or simulation state, scan mode, build
   mode, ESP-IDF, and firmware version.
4. **Sub-GHz Status**: CC1101 connection/simulation, frequency, modulation
   preset, TX Region, raw recorder state, and analyzer state/peak frequency.
5. **Performance**: average/maximum sweep time, UI average, loop rate, SPI wait, session state.

`SESSION STOPPED` means the SD/LittleFS recorder is not currently active. It does not mean that the analyzer, filesystem, or device has failed. Start it from Analyze → Logging or with `session start`.

`SYS INFO` page 6, `SD CARD`, shows whether the card was loaded, card type,
capacity, used and free space, plus the active recorder backend. `NOT DETECTED`
with recorder `LittleFS` means no usable card was found; `DIRECTORY ERROR`
means the card mounted but the required RFSuite folders could not be prepared.
In both cases the firmware uses its flash fallback.

### Power

Choose Restart or Shutdown with `UP/DOWN`, confirm with `A`, or cancel with `B`.

- Restart calls a firmware reboot after showing the restart screen.
- Shutdown enters deep sleep after stopping radios and preparing the display.
- To wake, hold `A` for about 1.5 seconds. A short or noisy wake press returns to sleep.

## Runtime versus persistent state

Themes, profiles, event configuration, trace choice, watch markers, RF Test configuration, and CUSTOM depth are persisted. Freeze, zoom, cursor, baseline, histories, and recording state are runtime-only. See [Persistence](PERSISTENCE.md) for the exact schema.

## RF Environment pages

The ENV TEST and ENV MORE controls, metrics, limitations, Serial commands, and
record formats are documented in [RF Environment](RF_ENVIRONMENT.md). Pressing
`B` on an active environment screen requests a stop and returns to the menu.
