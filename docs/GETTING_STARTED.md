# Getting Started

## Requirements

- ESP32-S3 DevKitC-1 or compatible board.
- One or two nRF24L01+ modules.
- ST7735 128 × 160 TFT configured for 160 × 128 landscape use.
- Four normally-open buttons.
- A stable 3.3 V radio supply and local decoupling capacitors.
- USB data cable.
- Visual Studio Code with PlatformIO IDE, or PlatformIO Core.

Read [Hardware Reference](HARDWARE.md) before applying power. nRF24L01+ modules are 3.3 V devices.

## Build profiles

The profiles are declared in `platformio.ini`.

| Environment | Active RF test | Purpose |
|---|---:|---|
| `analyzer` | No | Default receive-only analyzer and diagnostics firmware |
| `authorized_rf_lab` | Yes | Controlled, shielded, explicitly authorized RF laboratory work |
| `native` | Not applicable | Host-side unit tests for dependency-free analyzer math |

Running `pio run` selects `analyzer` because it is the configured default.

## Build and upload

From the project root:

```bash
pio run -e analyzer
pio run -e analyzer -t upload
pio device monitor --baud 115200
```

For an isolated and authorized RF laboratory only:

```bash
pio run -e authorized_rf_lab
pio run -e authorized_rf_lab -t upload
```

Do not use the lab profile as the general-purpose firmware. See [Safety and Authorized Use](SAFETY.md).

## Run tests

```bash
pio test -e native
```

The native suite does not require an ESP32. Hardware behavior still needs the checks in [Testing and Release](TESTING.md).

## Boot sequence

On a normal boot the firmware performs these operations:

1. Validates a deep-sleep wake gesture if the reset originated from the `A` button wake source.
2. Starts USB Serial at 115200 baud.
3. Loads and validates versioned NVS settings.
4. Mounts LittleFS for session recording. Mount failure triggers the framework's format-on-failure behavior.
5. Initializes buttons and the TFT, then shows the splash screen.
6. Creates the radio SPI mutex and probes both nRF24 modules up to five times.
7. Starts the three-second main-loop watchdog.

The boot continues if no radio is detected. This intentional diagnostics-only mode lets the display, Serial CLI, storage, and status pages remain available for troubleshooting.

## First-boot validation

1. Open `Tools → Radio Diag` and confirm the expected modules show `CONNECTED`.
2. Open `System Info` and review all five status pages.
3. Open `Analyze → Spectrum`; verify that the graph updates without visible full-screen flicker.
4. Change band, radio mode, trace, and zoom.
5. Open `Analyze → Logging`, press `A`, and verify `RECORDING`.
6. Return to Status → Performance and verify the session row reports a sweep count.
7. Stop recording and run `session info` over Serial.
8. Test Shutdown, then hold `A` for approximately 1.5 seconds to wake.

## Build outputs

PlatformIO stores generated files under `.pio/build/<environment>/`. These files are build artifacts and should not be edited or committed.

## Next steps

- Device controls: [User Guide](USER_GUIDE.md)
- Measurement interpretation: [Analyzer Concepts](ANALYZER_CONCEPTS.md)
- Serial automation: [Serial CLI](SERIAL_CLI.md)
- Common failures: [Troubleshooting](TROUBLESHOOTING.md)
