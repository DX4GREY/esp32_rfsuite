# Hardware Reference

## Supported topology

The firmware targets an ESP32-S3 with:

- one or two nRF24L01+ radios sharing one hardware SPI bus;
- one optional CC1101 Sub-GHz transceiver on that RF SPI bus;
- separate CE and CSN pins for each radio;
- an ST7735 TFT on a separate display SPI connection;
- four active-low buttons using internal pull-ups.

Two radios provide the fastest split scan and diversity operation. One radio is fully supported through automatic fallback. Zero-radio boot is supported for diagnostics, but acquisition returns no new RF measurements.

## nRF24L01+ wiring

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
| IRQ | Unused | Unused | — |

The firmware configures this shared bus for SPI mode 0, MSB first, at 16 MHz. A FreeRTOS mutex serializes access from the main loop, diagnostics, analyzer acquisition, and the optional Core 0 laboratory task.

## TFT wiring

| TFT signal | ESP32-S3 GPIO |
|---|---:|
| SCK / CLK | 18 |
| SDA / MOSI | 17 |
| A0 / DC | 16 |
| RST / RES | 15 |
| CS | 14 |
| VCC | Appropriate panel supply, normally 3.3 V |
| GND | GND |

### TFT microSD slot

| Module signal | ESP32-S3 GPIO |
|---|---:|
| SD SCK | 18 (shared with TFT) |
| SD MOSI | 17 (shared with TFT) |
| SD MISO | 21 |
| SD CS | 1 |

The SD controller uses HSPI so it does not contend with the RF24 FSPI bus.
Pins can be overridden with `SD_CS_PIN` and `SD_MISO_PIN` build flags.
GPIO 3 is intentionally avoided because it is used by boot/JTAG-related board
functions.

## CC1101 wiring

The CC1101 shares SCK, MOSI, and MISO with the nRF24 radios. Its chip-select
must remain independent and all RF modules must use 3.3 V logic and power.

| CC1101 signal | ESP32-S3 GPIO |
|---|---:|
| VCC | 3.3 V |
| GND | GND |
| SCK | 12 (shared) |
| MOSI / SI | 11 (shared) |
| MISO / SO | 13 (shared) |
| CSN / SS | 3 (default) |
| GDO0 | 38 (raw capture/replay) |
| GDO2 | Not required |

GPIO 3 is a strapping/JTAG-related pin on ESP32-S3 boards. The CC1101 CSN
line idles HIGH, but board variants can differ; override it with
`-D CC1101_CSN_PIN=<gpio>` in `platformio.ini` when another safe free GPIO is
available. Do not let CSN be held LOW while the ESP32 resets.

GPIO 38 is the default raw-data connection and can be changed with
`-D CC1101_GDO0_PIN=<gpio>`. GDO0 is required for Record and Emulate. Verify
that GPIO 38 is exposed by the selected ESP32-S3 board before wiring it.

The frequency analyzer samples the supported 300–348, 387–464, and 779–928 MHz
ranges; Record and Packet screens offer 315/433.92/868/915 MHz shortcuts. Use
an antenna and CC1101 module variant matched to the selected band. Emulate and
RF Test transmission are compiled only by the `authorized_rf_lab` environment.

The display is initialized as `INITR_BLACKTAB` with rotation `3`. Different ST7735 panel variants may require a different initialization tab, color order, offset, or rotation.

## Button wiring

| Button | GPIO | Electrical behavior |
|---|---:|---|
| `UP` | 10 | Connect to GND when pressed |
| `A` | 9 | Connect to GND when pressed |
| `DOWN` | 8 | Connect to GND when pressed |
| `B` | 5 | Connect to GND when pressed |

All button pins use `INPUT_PULLUP`; an unpressed button reads high. The firmware applies 50 ms debounce filtering. Spectrum and Events use release-aware short presses so a long press does not also execute the short action.

GPIO 9 is also configured as the active-low deep-sleep wake source. Keep this assignment on an RTC-capable GPIO if the board definition is changed.

When a button changes the active application mode, the firmware suppresses
that held button until release. This prevents an `A` press used to open a
feature from also triggering the first action on the destination screen.

## Power integrity

Power instability is the most common cause of radio detection failures and ESP32 brownouts.

- Never power an nRF24L01+ from 5 V.
- Place a 10–100 µF bulk capacitor near each radio module, plus local ceramic decoupling where practical.
- Keep SCK, MOSI, MISO, CE, CSN, power, and ground wiring short.
- Use a common ground for the ESP32, radios, display, and buttons.
- PA/LNA radio modules may need a dedicated low-noise 3.3 V regulator.
- Do not assume the development board's 3.3 V output can supply two high-current PA/LNA modules.

## Radio discovery and fallback

At startup the firmware attempts discovery up to five times with a 50 ms settle delay per attempt. Each successfully initialized module is remembered while the missing module continues to be retried.

| Detected hardware | Behavior |
|---|---|
| R1 + R2 | All receive modes available; FAST uses adjacent-channel split scanning |
| R1 only | Analyzer uses R1; requests for unavailable R2 fall back to R1 |
| R2 only | Analyzer uses R2; requests for unavailable R1 fall back to R2 |
| Neither | UI and Serial remain operational in diagnostics-only mode |

`CONNECTED` confirms SPI communication with the chip. It does not validate antenna matching, receiver sensitivity, supply noise, or RF path performance.

## Shutdown behavior

Shutdown performs a software power-down sequence:

1. Stops radio activity.
2. Prepares the TFT for shutdown.
3. Enables active-low wake on `A`/GPIO 9.
4. Enters ESP32 deep sleep.

Deep sleep does not physically disconnect the regulator, power LED, TFT board, or other peripherals. Use a load switch or power-latch circuit when true supply disconnection is required.

## Modification checklist

When changing pins or board type:

1. Update `include/config/Config.h`.
2. Confirm no boot-strap or USB conflict exists.
3. Confirm the wake button remains RTC-capable.
4. Rebuild both ESP32 environments.
5. Re-run display, radio, and deep-sleep hardware tests.
