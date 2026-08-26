# Troubleshooting

## Quick diagnostic sequence

1. Open Serial at 115200 baud.
2. Run `status`.
3. Run `config`.
4. Run `session info` if recording is involved.
5. Run `perf` for timing and SPI contention.
6. Check Tools → Radio Diag and all Status pages.

## `SESSION STOPPED` on Performance

This is normally not an error. It means the LittleFS recorder is inactive.

Start it from Analyze → Logging with `A`, or:

```text
session start
```

If it immediately returns to stopped, run `session info`.

| Error | Meaning / action |
|---|---|
| `none` | Normal manual stop or recorder not started |
| `LittleFS unavailable` | Mount failed; inspect partition/flash configuration |
| `cannot create session` | File creation failed |
| `session append failed` | Append/open failure |
| `short filesystem write` | Not all buffered bytes were written |
| `session size limit reached` | Normal automatic stop near 256 KiB; export or start a new session |

## One or both radios not detected

- Confirm 3.3 V, common ground, CE/CSN wiring, and shared SPI wiring.
- Add local 10–100 µF capacitors.
- Shorten wires and reduce breadboard contact resistance.
- Check supply voltage during startup and acquisition.
- Verify R1 and R2 have distinct CE/CSN pins.
- Try one module at a time to isolate a bad module or power issue.

One working module is enough. No-radio diagnostics-only boot is expected and should not reset repeatedly.

## Radio reports connected but spectrum is flat

- `CONNECTED` verifies SPI, not the antenna or RF receive path.
- Confirm the radio has an antenna appropriate for the module.
- Try a known nearby source in a controlled setup.
- Switch between R1, R2, and DIV to compare modules.
- Use DEEP profile and Inspect on a known channel.
- Confirm the graph is not frozen.
- DELTA is zero until a baseline is captured and only shows positive change.

## Unexpected analyzer values

- Percentages are hit ratios, not dBm.
- Wideband traffic can occupy multiple sampled channels.
- FAST and DIV observe differently and need not produce identical ratios.
- AVG responds gradually; MAX does not decay.
- Zoom does not change acquisition resolution or range.
- Power supply noise and module placement can affect carrier detection.

## SPI timeout or high wait values

Run `perf` and inspect:

- contention: a caller initially found the mutex busy;
- timeout: a caller failed to acquire within its timeout;
- average/maximum wait: time spent waiting for protected radio access.

Occasional contention can be normal. Timeouts or growing maximum wait suggest an overly long lock scope, a blocked radio task, excessive status polling, or a code path that failed to release the mutex.

## Display blank, inverted, shifted, or wrong colors

- Verify TFT SCK, MOSI, DC, RST, CS, VCC, and GND.
- Confirm the controller and panel tab variant.
- Check `INITR_BLACKTAB` and rotation `3` in display initialization.
- Some ST7735 modules require different column/row offsets.
- Confirm the display is not still in sleep after a power-domain modification.

## Display flicker or slow updates

- Confirm new renderer code does not call `fillScreen()` during live updates.
- Update only dirty regions.
- Inspect Status → Performance for UI average and sweep time.
- Avoid excessive Serial output during high-rate acquisition.
- DEEP/CUSTOM profiles naturally reduce refresh rate.

## Settings do not survive shutdown

Settings are saved 1.5 seconds after the last change. Wait before selecting Shutdown. Confirm the exact value after reboot with `config`.

If values remain invalid, run:

```text
factory reset confirm
```

This resets NVS but does not remove the LittleFS session.

## Session replay fails

- Run `session info` and confirm file size is nonzero.
- Run `session export` and look for at least one complete line beginning with `S,`.
- Replay selects the last line beginning with `S,`. If that final row is truncated or malformed, replay can fail even when an earlier row was complete; export the file and remove the damaged tail before using an external parser.
- Starting a new session immediately replaces the previous file.

This section describes 2.4 GHz analyzer-session replay. For CC1101 raw replay,
use the checks below.

## Sub-GHz raw replay fails

- Flash `authorized_rf_lab`; the default `analyzer` build intentionally blocks TX.
- Confirm the CC1101 and SD card are detected and GDO0 is wired.
- Select a TX Region that permits the file frequency. `REGION BLOCKED` means
  the selected firmware allowlist rejected it.
- `CHANNEL BUSY` means clear-channel assessment observed RF energy. Wait and
  retry rather than bypassing the check.
- `TX COOLDOWN` means the required one-second interval after transmission has
  not elapsed.
- Match the CC1101 preset to the source modulation. OOK and 2-FSK are not
  interchangeable even at the same carrier frequency.
- Recapture close to the owned transmitter if pulse count is very low, RSSI is
  weak, or the waveform contains obvious noise.
- Fixed-code OOK signals are the most likely to replay. Rolling-code,
  encrypted, challenge-response, and frequency-hopping remotes are not
  expected to work from a raw capture.

The replay process page remains visible while sending. After success, press
`A` to replay the same file or `B` to return. A stopped page shows the service
error and offers the same retry/back controls.

## A button action fires twice after opening a screen

Current firmware consumes held buttons across application-mode transitions.
If an older build opens a feature and then immediately activates it again when
`A` is released, update the firmware. On current builds, release the opening
press before issuing the next action. Also inspect for a stuck switch, missing
pull-up, excessive contact bounce, or incorrect active-low wiring.

## USB Serial unavailable

- Use a data-capable cable.
- Select the correct device port.
- Use 115200 baud.
- The build enables USB CDC on boot; opening the monitor before pressing reset can help on some hosts.
- Confirm the selected firmware matches the ESP32-S3 board.

## Brownout or random reset

- Use a stronger 3.3 V regulator for PA/LNA modules.
- Add decoupling at each radio.
- Reduce cable length and shared-rail resistance.
- Check Serial reset reason and Status memory information.
- A watchdog reset can indicate a blocked loop; inspect recent changes around SPI locks, filesystem calls, and rendering.

## Device does not wake

- Hold `A` continuously for approximately 1.5 seconds.
- Confirm GPIO 9 is pulled high when idle and grounded when pressed.
- Remove external pull-downs.
- Confirm any pin remap kept an RTC-capable wake GPIO.
- Remember that a short press intentionally returns the device to deep sleep.
