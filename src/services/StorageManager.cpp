#include "services/StorageManager.h"
#include "config/Config.h"
#include "drivers/DisplayStorageBus.h"
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>

StorageManager storageManager;

bool StorageManager::ensureDirectory(fs::FS& fs, const char* path) {
    if (fs.exists(path)) return true;
    String current;
    const String full(path);
    for (size_t i = 1; i <= full.length(); ++i) {
        if (i == full.length() || full[i] == '/') {
            current = full.substring(0, i);
            if (current.length() && !fs.exists(current) && !fs.mkdir(current)) return false;
        }
    }
    return fs.exists(path);
}

bool StorageManager::begin() {
    // The TFT and SD socket share SCK/MOSI.  Explicitly release both chip
    // selects before touching the bus; some ST7735 breakout boards otherwise
    // leave MISO polluted while the card is answering CMD0/CMD8.
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    // SD cards release DO/MISO while CS is high. Some inexpensive combo
    // boards omit the required pull-up, leaving ESP32-S3 to read random LOW
    // bits before CMD0. The internal pull-up is sufficient for this idle line
    // and is automatically overridden whenever the card drives a response.
    pinMode(SD_MISO_PIN, INPUT_PULLUP);

    // A regulator and level shifter are commonly fitted between the combo
    // board supply and its microSD socket. Give that rail enough time to
    // stabilize before the first 400 kHz initialization clocks.
    delay(250);
    Serial.printf("Storage: SPI pins SCK=%d MOSI=%d MISO=%d CS=%d, MISO idle=%s\n",
                  SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_CS_PIN,
                  digitalRead(SD_MISO_PIN) == HIGH ? "HIGH" : "LOW");

    SPIClass& sharedSpi = displayStorageSpi();
    sharedSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    // Long breakout traces and resistor level shifters can make a card fail at
    // 4 MHz even though the same wiring is reliable at 1 MHz. Retry cleanly at
    // progressively safer clocks instead of permanently reporting no card.
    // cardSize() is intentionally not a detection condition: it may be zero
    // for an unreadable FAT volume even when the physical card was detected.
    const uint32_t mountFrequencies[] = {SD_SPI_FREQUENCY, 1000000U, 400000U};
    uint32_t mountedFrequency = 0;
    sdMounted = false;
    delay(10);
    for (size_t attempt = 0;
         attempt < sizeof(mountFrequencies) / sizeof(mountFrequencies[0]);
         ++attempt) {
        const uint32_t frequency = mountFrequencies[attempt];
        bool alreadyTried = false;
        for (size_t previous = 0; previous < attempt; ++previous) {
            if (mountFrequencies[previous] == frequency) alreadyTried = true;
        }
        if (alreadyTried) continue;
        digitalWrite(TFT_CS, HIGH);
        digitalWrite(SD_CS_PIN, HIGH);
        if (SD.begin(SD_CS_PIN, sharedSpi, frequency, "/sd", 8, false)) {
            const uint8_t cardType = SD.cardType();
            if (cardType != CARD_NONE) {
                sdMounted = true;
                mountedFrequency = frequency;
                break;
            }
        }
        SD.end();
        digitalWrite(SD_CS_PIN, HIGH);
        delay(10);
    }
    sdState = sdMounted ? "mounted" : "not detected";
    sdWritable = false;
    if (sdMounted) {
        Serial.printf("Storage: SD SPI=%lu Hz, physical=%llu, volume=%llu bytes\n",
                      static_cast<unsigned long>(mountedFrequency),
                      SD.cardSize(), SD.totalBytes());
        if (SD.totalBytes() == 0) {
            sdState = "filesystem error";
            Serial.println("Storage: FAT volume metadata cannot be read");
        } else if (!ensureDirectory(SD, "/RFSuite/log") ||
            !ensureDirectory(SD, "/RFSuite/scripts") ||
            !ensureDirectory(SD, "/RFSuite/SubGHz")) {
            // Preserve physical-card diagnostics, but route application writes
            // to LittleFS when the expected directory tree is unavailable.
            sdState = "directory error";
            Serial.println("Storage: SD mounted, but RFSuite folders could not be created");
        } else {
            const char* probePath = "/RFSuite/log/.write_test";
            SD.remove(probePath);
            File probe = SD.open(probePath, FILE_WRITE);
            if (probe && probe.write(static_cast<uint8_t>(0xA5)) == 1) {
                probe.close();
                sdWritable = SD.remove(probePath);
            } else if (probe) {
                probe.close();
            }
            if (!sdWritable) sdState = "read only";
        }
    }

    // Keep flash available as a transparent recorder fallback.
    flashMounted = LittleFS.begin(true);
    Serial.printf("Storage: %s%s\n", backendName(),
                  sdWritable
                      ? " mounted at /RFSuite"
                      : (sdMounted ? " fallback (SD degraded)" : " fallback"));
    return sdWritable || flashMounted;
}

const char* StorageManager::sdTypeName() const {
    if (!sdMounted) return "NONE";
    switch (SD.cardType()) {
        case CARD_MMC: return "MMC";
        case CARD_SD: return "SDSC";
        case CARD_SDHC: return "SDHC/SDXC";
        default: return "UNKNOWN";
    }
}

uint64_t StorageManager::sdTotalBytes() const { return sdMounted ? SD.totalBytes() : 0; }
uint64_t StorageManager::sdUsedBytes() const { return sdMounted ? SD.usedBytes() : 0; }
uint64_t StorageManager::sdFreeBytes() const {
    const uint64_t total = sdTotalBytes();
    const uint64_t used = sdUsedBytes();
    return total > used ? total - used : 0;
}

fs::FS& StorageManager::filesystem() {
    return sdWritable ? static_cast<fs::FS&>(SD) : static_cast<fs::FS&>(LittleFS);
}

const char* StorageManager::sessionPath() const {
    return sdWritable ? "/RFSuite/log/rf_session.csv" : "/rf_session.csv";
}
