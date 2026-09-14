# RF Suite v2.1 — ESP32-S3 2.4 GHz and Sub-GHz Toolkit

<p align="center">
  <img src="images/banner.svg" alt="RF Suite v2.1 - ESP32-S3 2.4 GHz and Sub-GHz Toolkit">
</p>

A standalone dual-band firmware project for an ESP32-S3, one or two nRF24L01+
modules, an optional CC1101 Sub-GHz transceiver, microSD storage, and a 1.8-inch
ST7735 TFT. Version 2.1 introduces a standardized 3-chip UI/UX, header safety boundaries,
font rendering fixes, robust persistence validation, and expanded native testing.
The default build remains receive-only; active RF testing is isolated
in a separate controlled-lab build profile.

> **Development branch note:** the animated carousel branch adds an SD-backed
> firmware updater. When a writable SD card is mounted, **UPDATER** appears in
> the Main Menu. Firmware images are read only from `/RFSuite/binary/*.bin`.
> Selecting an image opens an explicit confirmation screen before the ESP32 OTA
> application partition is written. The image must have a valid ESP application
> header and fit the configured OTA partition. Do not remove power or the SD
> card while the flashing progress screen is active.

The interface is designed for a 160 × 128 landscape display. It uses partial/dirty rendering: the complete screen is cleared only during page transitions, while graphs, status values, and menu cards are redrawn only where their content changes. This reduces flicker and keeps the UI responsive.

For the full project documentation, hardware wiring, build profiles, controls,
RF safety boundaries, storage formats, Lua API, development workflow, and
troubleshooting guides, see the [`docs/`](docs/) directory and the repository
history for the complete v2.1 documentation set.
