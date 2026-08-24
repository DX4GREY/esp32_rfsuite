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
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    SPIClass& sharedSpi = displayStorageSpi();
    sharedSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    // 4 MHz is deliberately conservative for TFT carrier boards: their SD
    // traces, level shifters, and shared SCK/MOSI wiring are often unreliable
    // at 10 MHz even though the card-identification commands still succeed.
    sdMounted = SD.begin(SD_CS_PIN, sharedSpi, SD_SPI_FREQUENCY,
                         "/sd", 8, false) &&
                SD.cardType() != CARD_NONE && SD.cardSize() > 0;
    sdState = sdMounted ? "mounted" : "not detected";
    if (sdMounted) {
        Serial.printf("Storage: SD SPI=%lu Hz, physical=%llu, volume=%llu bytes\n",
                      static_cast<unsigned long>(SD_SPI_FREQUENCY),
                      SD.cardSize(), SD.totalBytes());
        if (SD.totalBytes() == 0) {
            sdState = "filesystem error";
            Serial.println("Storage: FAT volume metadata cannot be read");
        }
        if (!ensureDirectory(SD, "/RFSuite/log") ||
            !ensureDirectory(SD, "/RFSuite/scripts") ||
            !ensureDirectory(SD, "/RFSuite/SubGHz")) {
            // Keep a successfully mounted card available for read-only tasks
            // such as File Explorer and loading existing Lua scripts. A card
            // with a damaged/read-only FAT volume may reject mkdir(), but
            // unmounting it here incorrectly turns that into "not ready" and
            // hides files that can still be read.
            sdState = "directory error";
            Serial.println("Storage: SD mounted, but RFSuite folders could not be created");
        }
    }

    // Keep flash available as a transparent recorder fallback.
    flashMounted = LittleFS.begin(true);
    Serial.printf("Storage: %s%s\n", backendName(),
                  sdMounted && strcmp(sdState, "mounted") == 0
                      ? " mounted at /RFSuite"
                      : (sdMounted ? " mounted with directory error" : " fallback"));
    return sdMounted || flashMounted;
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
    return sdMounted ? static_cast<fs::FS&>(SD) : static_cast<fs::FS&>(LittleFS);
}

const char* StorageManager::sessionPath() const {
    return sdMounted ? "/RFSuite/log/rf_session.csv" : "/rf_session.csv";
}
