# Sub-GHz Analyzer, Raw Capture, and Replay

## Scope

The Sub-GHz feature set uses an optional CC1101 transceiver. It provides a
frequency analyzer, raw pulse capture, a recording library, Flipper RAW `.sub`
interchange, packet inspection, and guarded replay. Receive-only features are
available in the default `analyzer` build. RF transmission is compiled only in
the `authorized_rf_lab` build.

The implementation is intended for owned equipment and authorized laboratory
work. It is not a calibrated receiver, protocol universalizer, or regulatory
compliance instrument.

## Hardware requirements

- A CC1101 module covering the intended band.
- An antenna matched to 315, 433, 868, or 915 MHz as appropriate.
- GDO0 connected to the configured ESP32 pin for raw pulse input/output.
- Stable 3.3 V power and a common ground.
- An SD card for raw capture and library operations.

The analyzer can operate without GDO0, but Record and Replay require it. A
433 MHz-only CC1101 module and antenna should not be expected to perform well
at 868 or 915 MHz.

## Frequency Analyzer

Analyzer samples 20 points across the supported CC1101 bands. Its live screen
shows RSSI bars, reference guides, the scanned frequency endpoints, and a white
cap on the strongest bar. The peak card reports the current peak frequency and
RSSI.

| Control | Action |
|---|---|
| `A` | Refine and tune to the strongest result, then open Record |
| `B` | Stop analysis and return to the Sub-GHz menu |

Analyzer values are RSSI estimates from the CC1101, not calibrated field
strength. Nearby transmitters, supply noise, antenna mismatch, bandwidth, and
module quality affect the result.

## Raw Record

Record captures the HIGH/LOW timing observed on CC1101 GDO0. Choose 315,
433.92, 868, or 915 MHz with `UP`/`DOWN`, then press `A` to arm or start. Press
`A` again to stop and save. Capture ends automatically at 30 seconds or 8,192
pulses.

The capture screen reports the selected frequency, `READY`/`ARMED`/`REC`
state, live waveform, pulse count, RSSI, pulse rate, buffer usage, and a
protocol/timing hint after a usable capture is decoded.

Auto-trigger is enabled by default. Record waits in `ARMED` until RSSI crosses
the configured threshold. Hold `B` while idle to toggle auto-trigger. Saved
data is cleaned by removing paired glitches shorter than 80 microseconds and
trimming excessive leading silence.

Native captures are stored as:

```text
/RFSuite/SubGHz/RAW_<frequency>_<time>.rfr
```

An `.rfr` file stores frequency, pulse count, initial level, CC1101 preset, and
raw pulse durations. It does not store an analog copy of the RF waveform.

## Modulation and presets

Modulation describes how digital data changes the radio carrier. Frequency and
modulation are separate: two devices can both use 433.92 MHz but remain
incompatible because one uses OOK and the other uses FSK.

| Preset family | Meaning | Typical use |
|---|---|---|
| OOK | Carrier is switched on and off according to pulse timing | Many simple fixed-code remotes, bells, outlets, and sensors |
| 2-FSK | Data alternates between two nearby carrier frequencies | Packet radios and some sensors/remotes |

The OOK 270/650 kHz values describe receiver bandwidth presets, not the remote
carrier frequency. A wider bandwidth tolerates more frequency error but also
admits more noise. The 2-FSK deviation presets control separation between the
two data frequencies.

Record and Replay should use a preset compatible with the original signal.
Replaying OOK timing through an FSK preset, or the reverse, normally fails even
when the carrier frequency is correct. Imported Flipper `.sub` files can select
known OOK/2-FSK presets or supply supported custom CC1101 register pairs.

## Library controls

Library lists `.rfr` and Flipper RAW `.sub` files, with favorites first.

| Control | Action |
|---|---|
| `UP` / `DOWN` | Select a file |
| `A` | Replay the selected file in the authorized RF-lab build |
| Hold `A` | Toggle favorite |
| Hold `UP` | Export `.rfr` to Flipper RAW `.sub` |
| Hold `DOWN` | Clean the selected recording again |
| Hold `UP` + press `A` | Rename to the next available `SIGNAL_N` name |
| Hold `B`, then `A` | Confirm deletion |
| `B` | Cancel deletion or return to the Sub-GHz menu |

Input is guarded across screen transitions. The press that opens a screen is
consumed, and the destination screen waits for release before accepting a new
action. One physical press therefore cannot both open a feature and activate
its primary control.

## Replay process UI

Replay opens a dedicated `SUB-GHz REPLAY` page for the entire operation. It
displays the filename, transmit frequency, current/total repeat, transmitted
pulse count, percentage, progress bar, and an animated RF indicator. `B`
requests an abort while transmission is active.

After a successful replay, the UI remains on `REPLAY COMPLETE`; it does not
automatically return to Library.

| Completion control | Action |
|---|---|
| `A REPLAY` | Replay the same selected file again |
| `B BACK` | Return to Library |

Failures and user aborts remain on `REPLAY STOPPED` with the service error.
Use `A REPLAY` to retry or `B BACK` to return.

Replay is bounded by clear-channel assessment, a one-second post-TX cooldown,
the selected repeat count, and a ten-second maximum transmission window. The
UI is refreshed from cooperative callbacks during raw transmission, keeping
animation, progress, watchdog feeding, and abort input responsive.

## TX Region

TX Region is a firmware transmission policy, not a modulation setting and not
a certification of legal operation.

| Policy | Behavior |
|---|---|
| `RX ONLY` | Allows analysis and recording; blocks Replay and RF Test |
| `ETSI` | Allows only the firmware's ETSI-oriented Sub-GHz frequency ranges |
| `FCC` | Allows only the firmware's FCC-oriented Sub-GHz frequency ranges |

If the frequency stored in a replay file is outside the selected allowlist,
Replay stops with `REGION BLOCKED`. The policy does not change the captured
data, frequency, modulation, power, duty cycle, or local legal requirements.
Select a policy only after confirming that the frequency, power, duty cycle,
equipment, and intended use are permitted at the actual location.

## Remote-control compatibility

Raw replay often works with simple fixed-code OOK/ASK remotes because their
command is represented by repeatable pulse timing. Examples can include older
wireless bells, outlets, simple sensors, and owned gate controllers.

Raw replay is not expected to reproduce every 433 MHz remote. It can fail with:

- rolling-code systems such as KeeLoq-based products;
- encrypted or challenge-response protocols;
- frequency hopping;
- unsupported modulation or custom radio parameters;
- poor capture caused by noise, overload, weak reception, or antenna mismatch;
- receivers that validate timing, counters, identifiers, or synchronization
  more strictly than a raw waveform replay provides.

"Same output" means that the firmware retransmits the recorded carrier
frequency, configured modulation/preset, initial level, and digital pulse
durations. It does not reproduce the source transmitter's exact analog output,
RF power, oscillator error, spectral shape, antenna characteristics, or
spurious emissions.

Do not repeatedly test rolling-code remotes: transmitting captured frames can
advance or desynchronize counters without operating the receiver.

## Build and safety boundary

```text
pio run -e analyzer
pio run -e authorized_rf_lab
```

The default `analyzer` artifact compiles with `RF_LAB_TX_ENABLED=0`. It can
analyze and record but cannot replay. The `authorized_rf_lab` artifact enables
guarded replay and RF Test. Use active transmission only with owned equipment,
explicit authorization, suitable RF containment, and compliance with local
spectrum rules.
