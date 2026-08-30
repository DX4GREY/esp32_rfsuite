# Testing and Release

## Automated tests

Run:

```bash
pio test -e native
```

The current `test_analyzer_math` suite covers:

- percentage clamping;
- exponential averaging;
- non-negative baseline delta;
- confidence ordering and upper bound;
- event run retention and release through hysteresis.
- strict complete-sweep CSV parsing and malformed/overflow rejection;
- settings-value canonicalization and bounds;
- CRC32 and RFS1/RFS2 metadata validation.

Native tests intentionally avoid Arduino, RF24, TFT, FreeRTOS, Preferences, and LittleFS dependencies.

## Firmware build matrix

```bash
pio run -e analyzer
pio run -e authorized_rf_lab
```

Both environments must compile. The first verifies receive-only gating; the second verifies that the separately gated lab implementation remains buildable.

## Continuous integration

`.github/workflows/firmware.yml` runs on pushes and pull requests. It:

1. installs PlatformIO;
2. runs native unit tests;
3. builds `analyzer`;
4. builds `authorized_rf_lab`.

CI proves compilation and pure-logic behavior, not hardware correctness.

## Hardware-in-the-loop checklist

### Boot and storage

- Normal USB boot reaches the main menu.
- Existing NVS settings load and invalid values fall back safely.
- Legacy settings migrate once without repeated writes.
- LittleFS mounts and a session can be created, stopped, exported, and replayed.
- Starting a second session preserves the prior file and `session compare` reports both.
- An unusable/read-only SD falls back to LittleFS and reports its degraded state.
- A modified RFS2 payload is rejected with `CRC MISMATCH`; legacy RFS1 remains readable.
- Session automatically stops near its size limit and reports the reason.

### Radios

- Two-radio boot reports R1 and R2 connected.
- R1-only and R2-only boots reach the UI and acquire data.
- No-radio boot reaches diagnostics without reset looping.
- Missing nRF24 offers labelled 2.4 GHz simulation.
- Missing CC1101 offers labelled Sub-GHz simulation without saving fake RF captures.
- Simulation cannot start RF Test, replay, or authorized probe transmission.
- Opening any feature with a held `A` press does not trigger the destination
  screen when that same press is released.
- Sub-GHz replay shows live pass, pulse, and percentage progress; `B` can abort.
- Successful replay remains on its result page; `A` replays the same file and
  `B` returns to Library.
- `tools/generate_lua_library.py --check` confirms every registered
  `rf.subghz_*` binding is represented in the generated LuaLS library.
- `94_subghz_self_test.lua` passes without transmitting and rejects invalid
  frequency, preset, and region arguments.
- FAST, DIV, R1, and R2 behave as documented.
- Radio diagnostics do not destabilize acquisition.
- SPI timeout count remains zero during normal use.

### Analyzer

- All three band ranges update.
- FAST/BALANCED/DEEP/CUSTOM visibly change sweep timing.
- LIVE, AVG, MAX, and DELTA behave as documented.
- Freeze stops acquisition; cursor moves; resume restores peak-follow.
- Zoom changes viewport but not measured band.
- Watch markers persist after reboot.
- Waterfall ordering, survey reset, peak decay, and event latch are correct.

### Display and controls

- The RF Suite boot animation completes without clipping, long-lived SPI
  transactions, watchdog resets, or copied third-party branding.
- All 12 menu cards render without clipping.
- Ten UI themes leave no stale pixels after switching. Theme changes also
  switch their assigned Grid/List layout, header/footer geometry, and animation.
- MATRIX uses List while VIOLET and RETRO use Grid; every theme has a distinct
  menu-card silhouette or focus marker rather than a palette-only variation.
- Grid and List item navigation completes its four-frame focus transition
  without stale markers, clipping, double input, or watchdog resets.
- Dynamic graphs do not clear the full screen.
- Short and long presses do not double-trigger on Spectrum and Events.
- Status values fit their rows.

### Power

- Restart displays its transition screen and reboots.
- Shutdown stops radios and sleeps.
- Short wake press returns to sleep.
- Holding `A` for about 1.5 seconds boots without immediately opening a menu item.

### Authorized lab profile

Perform only in an RF enclosure under [Safety and Authorized Use](SAFETY.md):

- RX-only UI is replaced by the lab RF Test screen.
- Start/stop transitions do not deadlock SPI.
- Single-radio fallback stops cleanly.
- Leaving the screen powers down transmission state.

## Release gate

Before tagging or distributing firmware:

1. Run `git diff --check`.
2. Run native tests.
3. Build both ESP32 environments.
4. Complete hardware checks relevant to the change.
5. Confirm default artifacts come from `analyzer`.
6. Verify documentation matches commands, controls, schema, and data format.
7. Record RAM and flash use.
8. Review the diff for accidental credentials, generated files, or unsafe default changes.

Pushing the `v2.0.0` semantic version tag runs
`.github/workflows/release.yml`. The tag must match both `VERSION` and
`APP_VERSION`. The workflow publishes only the receive-only `analyzer` binary,
its SHA-256 checksum, and automatically generated notes covering changes since
the previous GitHub release.

## Adding tests

Put dependency-free logic behind small functions in `include/core/AnalyzerMath.h` or another pure header/module. Add focused cases under `test/<suite-name>/`. For hardware code, prefer a documented on-device test procedure unless a mock provides meaningful behavior rather than merely increasing coverage.
