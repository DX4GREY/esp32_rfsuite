# RF Suite Documentation

This directory is the detailed technical and operating reference for RF Suite. The repository-level [README](../README.md) is the project overview; the documents below explain installation, hardware, operation, data formats, internals, and maintenance in depth.

## Start here

| Document | Audience | Contents |
|---|---|---|
| [Getting Started](GETTING_STARTED.md) | New users | Toolchain, build profiles, upload, first boot, and initial validation |
| [Hardware Reference](HARDWARE.md) | Builders | Wiring, buses, power, buttons, display, radio fallback, and wake circuit |
| [User Guide](USER_GUIDE.md) | Device operators | Every menu, display field, button gesture, theme, status page, and shutdown flow |
| [Analyzer Concepts](ANALYZER_CONCEPTS.md) | RF users and developers | Channel mapping, carrier-hit percentages, modes, traces, confidence, events, and limitations |
| [RF Environment](RF_ENVIRONMENT.md) | RF users and test operators | Occupancy, heatmap, bursts, comparison, score, snapshots, band hints, CLI, and bounded probe |
| [Data and Storage](DATA_AND_STORAGE.md) | Data users | Serial summaries, LittleFS session layout, buffering, limits, export, and replay |
| [Sub-GHz](SUB_GHZ.md) | CC1101 users | Analyzer, raw capture, modulation, replay UI, TX-region policy, compatibility, and limits |
| [Lua Scripting](LUA_SCRIPTING.md) | Script authors | Editor library generation, SD layout, TFT loader, `rf` API, sandbox, design guidance, and examples |
| [Serial CLI](SERIAL_CLI.md) | Operators and automation | Complete command reference with validation ranges and examples |
| [v2.1.0 Release Notes](RELEASE_2.1.0.md) | Users and maintainers | UI/UX consistency, standardized footers, font rendering, and persistence fixes |
| [v2.0.0 Release Notes](RELEASE_2.0.0.md) | Users and maintainers | Dual-band, CC1101, simulation, UI, safety, and compatibility summary |

## Engineering and maintenance

| Document | Contents |
|---|---|
| [Architecture](ARCHITECTURE.md) | Runtime structure, dependency direction, module ownership, rendering, persistence, and concurrency |
| [Persistence](PERSISTENCE.md) | NVS schema, keys, defaults, deferred writes, migration, and factory reset behavior |
| [Development Guide](DEVELOPMENT.md) | Extension workflow, coding rules, build-profile gating, and compatibility requirements |
| [Testing and Release](TESTING.md) | Native tests, CI, firmware builds, hardware validation, and release checklist |
| [Troubleshooting](TROUBLESHOOTING.md) | Radio, display, filesystem, performance, reset, and shutdown diagnostics |
| [Safety and Authorized Use](SAFETY.md) | Receive-only default, RF-lab controls, legal scope, shielding, and operating checklist |

## Essential facts

- The default `analyzer` firmware is receive-only. Active RF test code is compile-time gated behind `authorized_rf_lab`.
- Analyzer percentages are carrier-detection hit ratios, not calibrated RSSI or dBm.
- One detected nRF24 is sufficient for 2.4 GHz. CC1101 is optional for Sub-GHz.
- Missing nRF24 or CC1101 hardware opens a clearly labelled simulation option;
  demo data is never presented as real RF and cannot authorize TX.
- A complete display clear occurs only on page transitions; live screens use dirty-region updates.
- `SESSION STOPPED` on the Performance page means the SD/LittleFS recorder is inactive. It is not a system fault.
- Shutdown uses ESP32 deep sleep. Hold `A` for about 1.5 seconds to boot again.

## Documentation maintenance

When behavior changes, update the most specific document and the repository README if the change affects installation, public features, controls, data formats, or safety. Commands must match `src/services/SerialCommander.cpp`, controls must match `src/ui/DisplayController.cpp`, and persisted keys must match `src/core/SettingsStore.cpp`.
