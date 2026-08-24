# RF Suite v2.0.0

Version 2.0.0 changes the firmware from a 2.4 GHz-only analyzer into a
dual-band RF toolkit while retaining a receive-only default build.

## Highlights

- Main selector for 2.4 GHz, Sub-GHz, and shared application tools.
- Optional CC1101 driver with frequency analyzer, OOK/2-FSK presets, raw record,
  live waveform, packet analyzer, file library, replay, and bounded RF Test.
- `/RFSuite/SubGHz` storage with native `.rfr` and Flipper RAW `.sub` support.
- Global missing-radio simulation for nRF24 and CC1101, visibly marked as demo
  data and unable to transmit or save fake captures as real RF recordings.
- Global themes and grid/list layouts across all menu levels.
- Application-wide dirty-region rendering. Full-screen clearing is reserved for
  screen-layout transitions and deliberate theme rebuilds.
- Sub-GHz safety policy with RX-only default, ETSI/FCC selection, CCA, cooldown,
  replay-count selection, and a ten-second TX ceiling.
- NVS schema 6 for the expanded UI, storage, and Sub-GHz preferences.

## Compatibility

Existing nRF24 analyzer workflows remain available under the 2.4 GHz menu.
Settings from older schemas are validated and migrated. Sub-GHz features require
a correctly wired CC1101 and suitable antenna; simulation can be used to inspect
the interface without radio hardware.

## Build profiles

- `analyzer`: default receive-only firmware.
- `authorized_rf_lab`: active tests compiled only for shielded, authorized labs.
- `native`: dependency-free host tests.
