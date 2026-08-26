# Firmware Architecture

This document describes the module boundaries used by RF24 Suite and the expected workflow for extending it. The main rule is that hardware access, application state, UI rendering, and user input must remain separate.

Return to the [documentation index](README.md), or see [Development Guide](DEVELOPMENT.md) for the implementation checklist.

## Runtime overview

```text
Boot / Core 1
  wake validation → Serial/NVS/storage → buttons/TFT → nRF24/CC1101 probe → watchdog
                                      │
                                      ▼
Arduino loop / Core 1                 Optional RF-lab task / Core 0
  watchdog                            channel schedule
  Serial commands                     short SPI transaction groups
  button routing                              │
  analyzer acquisition ───── radio SPI mutex ─┘
  dirty-region display updates
  recorder/settings service
```

The TFT uses a separate SPI connection and is not protected by the radio mutex. Analyzer scans hold the radio mutex for a sweep, while the optional Core 0 task acquires it around short radio transaction groups. Status probes use bounded waits and fall back to the boot-time availability state if the bus is temporarily busy.

## Directory layout

```text
include/
├── config/       Hardware pins and compile-time constants
├── core/         Application types, analyzer math/state, and mode policies
├── drivers/      Buttons, dual nRF24, and CC1101 interfaces
├── services/     Serial, recorders, Sub-GHz processing, metrics, and watchdog
└── ui/           Display API, shared theme, and menu metadata

src/
├── config/       Channel tables and static assets
├── core/         RF settings, analyzer state, and NVS persistence
├── drivers/      Radio lifecycle, RF task, and analyzer acquisition
├── services/     Serial, recorder, performance, and watchdog implementations
├── ui/           Display core, controller, and feature catalog
│   └── screens/  Renderers grouped by feature domain
└── main.cpp      Boot, main-loop scheduling, shutdown, and wake-up
```

## Dependency direction

```text
config ───────────────┐
                     ▼
core types/state ◄── drivers
       ▲              ▲
       │              │
services          UI controller/screens
       ▲              ▲
       └────── main ──┘
```

- `config` must not depend on application or UI modules.
- `core/AppTypes.h` stays lightweight because menus and policies use it.
- Drivers may update application state but must not render the display.
- Screen renderers may read state and driver status, but RF acquisition remains in drivers.
- `main.cpp` schedules modules; feature-specific rendering does not belong there.

## Main-loop scheduling

`AppModePolicy` decides whether the current screen needs continuous sweeps. Spectrum, Waterfall, Survey, and Events scan continuously. Logging scans only while recording is enabled. Frozen analyzer state suppresses continuous acquisition.

Each loop iteration:

1. records a loop heartbeat and feeds the watchdog;
2. processes one pending Serial line;
3. routes button input;
4. performs the mode-specific acquisition or idle delay;
5. renders changed display regions and records UI duration;
6. services buffered session writes and deferred NVS writes;
7. handles restart, shutdown, and watchdog recovery.

Completed sweeps update all shared analyzer products once: waterfall, occupancy, AVG, MAX, event counters, confidence, cursor peak-follow, USB summary, and optional LittleFS session row.

## Module responsibilities

### Core

- `AppTypes.h`: shared enums and small data structures.
- `AppState.h`: the public state contract used by the application.
- `AppState.cpp`: RF Test target, power, and dwell configuration.
- `AnalyzerMath.h`: dependency-free averaging, delta, confidence, and event-run primitives used by native tests.
- `AnalyzerState.cpp`: analyzer traces, baseline, confidence, watchlist, configurable events, survey, and peaks.
- `SettingsStore.cpp`: schema-versioned NVS validation, migration, delayed writes, and factory reset.
- `AppModePolicy.cpp`: decides which screens require continuous spectrum sweeps.

### Drivers

- `RadioManager.cpp`: one/two-radio discovery, shared-SPI mutex and metrics, RX/TX lifecycle, and the optional Core 0 RF Test task.
- `RadioAnalyzer.cpp`: 2.4 GHz spectrum and single-channel acquisition.
- `Cc1101Manager.cpp`: CC1101 SPI lifecycle, presets, RSSI, raw RX/TX, and packets.
- `ButtonManager.cpp`: debounced edge and long-press detection.

### UI

- `DisplayManager.cpp`: TFT initialization, shared primitives, splash, and caches.
- `DisplayTheme.cpp`: selectable RGB565 palettes resolved from the active theme.
- `DisplayController.cpp`: mode dispatch and physical-button input routing.
- `MenuCatalog.cpp`: labels, icons, destination modes, and open actions for every menu card.
- `DisplaySupport.h`: shared theme colors and formatting helpers.
- `screens/MenuScreens.cpp`: main menu and RF Test renderers.
- `screens/AnalyzerScreens.cpp`: spectrum, waterfall, survey, event, logging, diagnostics, profile, and inspector renderers.
- `screens/SystemScreens.cpp`: settings, status, restart, shutdown, and power renderers.

### Services

- `SessionRecorder.cpp`: buffered LittleFS sessions, size limiting, CSV export, and last-sweep replay.
- `SubGhzRawService.cpp`: Sub-GHz analyzer, waveform capture, protocol hints,
  library/import/export, packet logging, simulation, guarded replay/RF test,
  and live replay pulse/pass progress exposed to the UI.

Raw replay remains synchronous to preserve microsecond pulse timing. A
cooperative callback services input, rendering, and the watchdog between pulse
batches and long pulse segments. The replay renderer consumes pass/pulse
progress, keeps abort responsive, and enters a persistent completion/result
state after the service returns.
- `PerformanceMonitor.cpp`: smoothed scan/UI duration, maxima, and loop-rate tracking.
- `SerialCommander.cpp`: CLI routing and machine-readable/diagnostic output.
- `Watchdog.cpp`: main-loop liveness monitoring.

## State ownership and lifetime

`AppState` is the shared application model. Its data has three lifetimes:

- high-rate acquisition and histories live only in RAM;
- compact preferences are validated and deferred to NVS;
- complete recorded sweeps are buffered to SD, with LittleFS as fallback.

The Core 0 lab task reads volatile test state and updates volatile active-channel fields. The Core 1 UI snapshots those fields before deriving displayed frequencies so a channel/frequency pair remains internally consistent.

## Radio lifecycle

`RadioManager` owns both RF24 objects and never exposes mutable references. Boot creates one mutex for their shared bus, probes each module independently, and enters receive mode in the default build. A missing module is retried while an already discovered module is retained. No-radio boot returns control to the application for diagnostics.

Transitions between transmit, receive, stopped, and powered-down states stay inside the driver. Any new code that accesses an RF24 object must follow the concurrency rules in [Development Guide](DEVELOPMENT.md).

## Display lifecycle

`DisplayController` compares the current `AppMode` with the last rendered mode. A mode change clears the screen once, resets dynamic caches, and builds the new layout. Within the same mode, renderers update only dirty cards, fields, or graph columns. Theme changes intentionally force one clean rebuild so colors from the previous palette cannot remain.

## Simulation lifecycle

Simulation is runtime-only and selected after a missing-radio popup. The active
band generates nRF24-style carrier activity or CC1101-style RSSI, pulse, and
packet demonstrations. Headers display `SIM`, fake Sub-GHz recordings are not
written as captures, and TX entry points remain blocked in every build profile.

## Adding a new display feature

1. Add a new `AppMode` to `include/core/AppTypes.h`.
2. Add any runtime data and focused helper methods to `AppState`.
3. Implement state behavior in the closest core source file. Create a new focused file if the feature does not belong to an existing domain.
4. Declare the renderer in `include/ui/DisplayManager.h`.
5. Implement it in the matching `src/ui/screens` file or create a new screen group.
6. Register its label, mode, icon, and open flags in `src/ui/MenuCatalog.cpp`.
7. Add the render dispatch and button handling to `DisplayController.cpp`.
8. If it needs continuous scanning, register the mode in `AppModePolicy.cpp`.
9. Update the README controls table and run a clean build.

Menu labels and mode routing must not be duplicated inside the renderer. `MenuCatalog` is the single source of truth for menu metadata.

## Adding analyzer data processing

- Put RF sampling in `RadioAnalyzer.cpp`.
- Put aggregation, event generation, or history management in `AnalyzerState.cpp`.
- Keep screen code read-only with respect to raw acquisition whenever possible.
- Document whether the result is instantaneous, averaged, or persistent.
- Avoid presenting carrier-detection ratios as calibrated RSSI/dBm.

## Rendering rules

- A full-screen clear is allowed only when switching to a different page layout.
- Dynamic screens should redraw dirty values, columns, cards, or graph regions only.
- Keep common colors and formatting in `DisplaySupport.h`.
- Add new palettes to `DisplayTheme.cpp`, then extend `DisplayThemeId` and its display name together.
- Cache previously rendered values in `DisplayManager` when updates are frequent.
- Throttle high-frequency rendering independently from RF acquisition.

## Persistence rules

- Only compact user configuration belongs in NVS; writes must be marked dirty and deferred.
- Full sweep sessions belong in LittleFS, while waterfall rows, events, and temporary UI state stay in RAM.
- Add all new NVS keys in `SettingsStore.cpp` and provide a safe default.
- Validate stored enum and numeric values before using them.
- Keep keys short because ESP32 Preferences/NVS key length is limited.
- Increment the schema when persisted structure semantics change and preserve migration from earlier keys.

## Verification

Run a clean build after structural or dependency changes:

```bash
pio test -e native
pio run -e analyzer
pio run -e authorized_rf_lab
```

Before testing on hardware, also verify:

- Both radios are detected independently.
- Spectrum, waterfall, survey, and event screens continue updating.
- Button navigation returns safely to the menu.
- RF Test stops before entering receive-based tools.
- NVS settings survive restart.
- Shutdown returns to deep sleep after a short wake press and boots after a long press.

The full automated, hardware, and release matrices are in [Testing and Release](TESTING.md).
# RF Environment subsystem

`RfEnvironmentAnalyzer` owns the Core 0 passive sampling task. It requests short carrier-sample batches through `RadioManager`, which retains ownership of RF24 objects and the shared SPI mutex. `RfEnvironmentState` contains fixed-size channel statistics, 32×126 history, 32 burst events, comparison configuration, and volatile snapshots. Pure calculations and mappings live in `RfEnvironmentMath`; UI only renders state and dispatches input. Settings use the existing delayed NVS service and session summaries use the recorder's buffered LittleFS path.

The task runs at priority 2 with periodic one-tick yields. SPI is locked only for a 32-sample channel batch, and stop is checked between channels. Dual receivers observe the same channel for diversity; shared-SPI benchmarking should be done on hardware before changing to concurrent SPI transactions, which cannot run in parallel on this bus.
