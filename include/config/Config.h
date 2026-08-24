#pragma once
#include <Arduino.h>
#include <RF24.h>

// Keep this value in sync with the root VERSION file. Release builds verify
// that the Git tag matches it before publishing any firmware.
#ifndef APP_VERSION
#define APP_VERSION "1.0.0"
#endif

// Analyzer-only is the safe default. The controlled-lab PlatformIO profile
// explicitly sets this to 1 to compile active RF test functionality.
#ifndef RF_LAB_TX_ENABLED
#define RF_LAB_TX_ENABLED 0
#endif

// =============================================================================
// HARDWARE PIN DEFINITIONS
// =============================================================================

// Radio nRF24L01+ (SPI kustom ESP32-S3)
#define CE_PIN   7
#define CSN_PIN  6
#define SCK_PIN  12
#define MOSI_PIN 11
#define MISO_PIN 13

// Radio nRF24L01+ (SPI kustom ESP32-S3, second radio)
#define CE_PIN_2  4
#define CSN_PIN_2 2

// CC1101 Sub-GHz radio. It shares the nRF24 SPI signals and uses a separate
// chip-select. Override CC1101_CSN_PIN in build_flags if GPIO 3 is unavailable
// on the target ESP32-S3 carrier.
#ifndef CC1101_CSN_PIN
#define CC1101_CSN_PIN 3
#endif
#ifndef CC1101_GDO0_PIN
#define CC1101_GDO0_PIN 38
#endif

// Display TFT ST7735 1.8" (128x160 SPI)
#define TFT_SCK   18
#define TFT_SDA   17
#define TFT_AO    16
#define TFT_RST   15
#define TFT_CS    14

// microSD slot on the TFT module. Most ST7735+SD boards share CLK/MOSI with
// the display and expose separate SD_MISO and SD_CS pins. Override these with
// PlatformIO build_flags when your carrier board is wired differently.
#ifndef SD_CS_PIN
#define SD_CS_PIN   1
#endif
#ifndef SD_MISO_PIN
#define SD_MISO_PIN 21
#endif
#define SD_SCK_PIN  TFT_SCK
#define SD_MOSI_PIN TFT_SDA
#ifndef SD_SPI_FREQUENCY
#define SD_SPI_FREQUENCY 4000000U
#endif

// Navigation Buttons (Active LOW / Internal Pull-Up)
#define BTN_UP    10
#define BTN_A     9
#define BTN_DOWN  8
#define BTN_B     5

// =============================================================================
// TFT DISPLAY COLORS (RGB565)
// =============================================================================
#ifndef ST77XX_GRAY
#define ST77XX_GRAY 0x8410
#endif
#ifndef ST77XX_DARKGRAY
#define ST77XX_DARKGRAY 0x39E7
#endif
#ifndef CUSTOM_ORANGE
#define CUSTOM_ORANGE 0xFD20
#endif

// =============================================================================
// BASIC FREQUENCY & RADIO CONFIGURATION
// =============================================================================
#define MIN_CHANNEL         0
#define MAX_CHANNEL         125
#define TOTAL_CHANNELS      126           // 0 - 125
#define DEFAULT_POWER       RF24_PA_MAX
#define DEFAULT_RATE        RF24_2MBPS    // 2Mbps = performa agresif

// Target Band Presets (Frequency = 2400 + RF24 Channel in MHz)
#define WIFI_MIN_CH         1             // 2401 MHz - awal band Wi-Fi (ch 1-13)
#define WIFI_MAX_CH         73            // 2473 MHz - akhir band Wi-Fi
#define BT_MIN_CH           2             // 2402 MHz - awal band Bluetooth
#define BT_MAX_CH           80            // 2480 MHz - akhir band Bluetooth (BR/EDR + BLE)

// =============================================================================

// CHANNEL DEFINITIONS
// =============================================================================

// --- Bluetooth Classic (BR/EDR) ---
// All even channels 2..80 (2402..2480 MHz)
extern const byte bluetooth_even_channels[];
extern const int BLUETOOTH_EVEN_CHANNELS_COUNT;
// All odd channels 1..79 (2401..2479 MHz)
extern const byte bluetooth_odd_channels[];
extern const int BLUETOOTH_ODD_CHANNELS_COUNT;

// --- Wi-Fi 2.4 GHz (50 programmed channels, 22 MHz band) ---
extern const byte wifi_channels[];
extern const int WIFI_CHANNELS_COUNT;

// --- BLE Advertising (Ch 37/38/39 = RF 2/26/80 + adjacent channels) ---
extern const byte ble_channels[];
extern const int BLE_CHANNELS_COUNT;

// --- USB / Video / RC presets ---
extern const byte usb_channels[];
extern const int USB_CHANNELS_COUNT;
extern const byte video_channels[];
extern const int VIDEO_CHANNELS_COUNT;
extern const byte rc_channels[];
extern const int RC_CHANNELS_COUNT;

// --- Full 2.4 GHz band (channels 1..100) ---
extern const byte full_channels[];
extern const int FULL_CHANNELS_COUNT;

// 3 Main BLE Advertising Channels (2402, 2426, 2480 MHz)
extern const int BLE_ADV_CHANNELS[3];

// BLE Data Channel Groups (3 groups, each near a BLE advertising channel neighborhood)
constexpr int BLE_DATA_GROUPS_COUNT = 3;
constexpr int BLE_DATA_CHANNELS_PER_GROUP = 4;
extern const byte channelGroup1[4];
extern const byte channelGroup2[4];
extern const byte channelGroup3[4];
constexpr int BLE_DATA_CHANNELS_COUNT = 12;
extern const byte BLE_DATA_CHANNELS[12];

// Fast payload for MAX-AGGRESSION Packet Storm (1-byte ultra low overhead)
// On-air packet @2Mbps: preamble(1) + addr(3) + payload(1) + CRC(off)
//                       = 5 bytes ~= 20 us -> maximum repetition rate.
#define FAST_PAYLOAD_SIZE   1
#define FAST_ADDRESS_WIDTH  3
extern const uint8_t FAST_JAM_PAYLOAD[1];

// JAMMER AGGRESSION: continuous-transmission dwell per channel (microseconds).
// Every visited channel is hammered at 100% duty for this long (REUSE mode)
// before hopping. Higher = harder bang per channel, lower = quicker revisit.
// Recommended: 100-400 us. Wi-Fi/ALL sweeps: 200 us is a good default.
#define JAMMER_DWELL_US     200

// Preset options for Dwell Time (microseconds)
extern const int DWELL_PRESETS[];
extern const int DWELL_PRESETS_COUNT;

// =============================================================================
// ANALYZER & GRAPH CONFIGURATION
// =============================================================================
#define SPECTRUM_SAMPLES_PER_CH 30        // Carrier hit sample count per channel
#define INSPECT_SAMPLES         100       // Deep channel inspector sample count
#define PEAK_DECAY_RATE         1         // Kecepatan penurunan peak hold

// Spectrum Graph Area Dimensions (160x128 Compact Safe Layout Screen)
#define GRAPH_X_START       18            // Left margin for Y labels
#define GRAPH_WIDTH         126           // 1 pixel per channel (0..125)
#define GRAPH_Y_TOP         16
#define GRAPH_HEIGHT        64
#define GRAPH_Y_BASELINE    80

// =============================================================================
// TIMING & BUFFER
// =============================================================================
#define BUTTON_DEBOUNCE_MS  50
#define WATCHDOG_TIMEOUT_US 3000000       // 3 seconds (3,000,000 µs)
#define PAYLOAD_SIZE        32

// Dummy nRF24 pipe address
extern const uint8_t DEFAULT_PIPE_ADDRESS[5];

// splashLogo, 64x64px
extern const unsigned long splashLogo[] PROGMEM;
