#include "services/StorageManager.h"
#include "config/Config.h"
#include "drivers/DisplayStorageBus.h"
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>

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

void StorageManager::recoverSdBus(bool resetSpiBus, uint32_t settleMs) {
    // The TFT and SD socket share SCK/MOSI.  Explicitly release both chip
    // selects before touching the bus. This is particularly important after
    // an upload/software reset because the SD card remains powered and may be
    // waiting for clocks from an interrupted command.
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    pinMode(SD_MISO_PIN, INPUT_PULLUP);

    SPIClass& sharedSpi = displayStorageSpi();
    if (resetSpiBus) {
        // Releasing the peripheral also clears a transaction left half-open
        // by the ROM uploader or by code running immediately before reset.
        sharedSpi.end();
        pinMode(SD_SCK_PIN, OUTPUT);
        digitalWrite(SD_SCK_PIN, LOW);
        pinMode(SD_MOSI_PIN, OUTPUT);
        digitalWrite(SD_MOSI_PIN, HIGH);
        delay(settleMs);
        sharedSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    }

    // SD SPI mode requires at least 74 clocks with CS high before CMD0. Send
    // 160 clocks so a card left mid-response by a warm reset can finish. The
    // Arduino SD driver then owns CMD0/CMD8/ACMD41 as one state machine; do not
    // inject a second CMD0 between its initialization steps.
    digitalWrite(TFT_CS, HIGH);
    digitalWrite(SD_CS_PIN, HIGH);
    sharedSpi.beginTransaction(SPISettings(400000U, MSBFIRST, SPI_MODE0));
    for (uint8_t i = 0; i < 20; ++i) sharedSpi.transfer(0xFF);

    sharedSpi.endTransaction();
    delay(2);
}

bool StorageManager::validateMountedSd(uint32_t mountedFrequency) {
    sdState = "mounted";
    sdWritable = false;
    Serial.printf("Storage: SD SPI=%lu Hz, physical=%llu, volume=%llu bytes\n",
                  static_cast<unsigned long>(mountedFrequency),
                  SD.cardSize(), SD.totalBytes());
    if (SD.totalBytes() == 0) {
        sdState = "filesystem error";
        lastErrorMsg = "SD FILESYSTEM ERROR";
        Serial.println("Storage: FAT volume metadata cannot be read");
    } else if (!ensureDirectory(SD, "/RFSuite/log") ||
               !ensureDirectory(SD, "/RFSuite/scripts") ||
               !ensureDirectory(SD, "/RFSuite/SubGHz")) {
        sdState = "directory error";
        lastErrorMsg = "SD DIRECTORY ERROR";
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
        if (!sdWritable) {
            sdState = "read only";
            lastErrorMsg = "SD READ ONLY";
        }
    }
    if (sdWritable) lastErrorMsg = "NONE";
    return sdWritable;
}

bool StorageManager::mountSd(bool resetSpiBus, bool extendedRecovery) {
    sdMounted = false;
    sdWritable = false;
    sdState = "not detected";
    SD.end();
    recoverSdBus(resetSpiBus, extendedRecovery ? 100U : 10U);

    const uint32_t mountFrequencies[] = {SD_SPI_FREQUENCY, 1000000U, 400000U};
    const size_t frequencyCount = sizeof(mountFrequencies) / sizeof(mountFrequencies[0]);
    const uint8_t rounds = extendedRecovery ? 2 : 1;
    SPIClass& sharedSpi = displayStorageSpi();

    for (uint8_t round = 0; round < rounds; ++round) {
        for (size_t attempt = 0; attempt < frequencyCount; ++attempt) {
            const uint32_t frequency = mountFrequencies[attempt];
            bool alreadyTried = false;
            for (size_t previous = 0; previous < attempt; ++previous)
                if (mountFrequencies[previous] == frequency) alreadyTried = true;
            if (alreadyTried) continue;

            Serial.printf("Storage: SD mount round=%u SPI=%lu Hz, MISO=%s\n",
                          round + 1, static_cast<unsigned long>(frequency),
                          digitalRead(SD_MISO_PIN) == HIGH ? "HIGH" : "LOW");
            digitalWrite(TFT_CS, HIGH);
            digitalWrite(SD_CS_PIN, HIGH);
            if (SD.begin(SD_CS_PIN, sharedSpi, frequency, "/sd", 8, false) &&
                SD.cardType() != CARD_NONE) {
                sdMounted = true;
                return validateMountedSd(frequency);
            }
            SD.end();
            recoverSdBus(false, 0);
            delay(20U * (attempt + 1));
        }
        if (round + 1 < rounds) {
            // A second peripheral reset is safe because both chip selects are
            // high. DisplayManager redraws the TFT after a manual retry.
            recoverSdBus(true, 150U);
        }
    }
    lastErrorMsg = digitalRead(SD_MISO_PIN) == LOW ?
                   "SD MISO STUCK LOW" : "SD MOUNT FAILED";
    return false;
}

bool StorageManager::begin() {
    const esp_reset_reason_t reason = esp_reset_reason();
    const bool warmReset = reason != ESP_RST_POWERON;
    Serial.printf("Storage: reset_reason=%d, recovery=%s, pins SCK=%d MOSI=%d MISO=%d CS=%d\n",
                  static_cast<int>(reason), warmReset ? "extended" : "normal",
                  SD_SCK_PIN, SD_MOSI_PIN, SD_MISO_PIN, SD_CS_PIN);

    // A regulator/level shifter on combo boards can rise slowly at cold boot.
    // On warm reset the card never lost power, so bus recovery matters more
    // than a long power stabilization delay.
    delay(warmReset ? 50U : 250U);
    mountSd(true, warmReset);

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

uint64_t StorageManager::flashTotalBytes() const { return flashMounted ? LittleFS.totalBytes() : 0; }
uint64_t StorageManager::flashUsedBytes() const { return flashMounted ? LittleFS.usedBytes() : 0; }
uint64_t StorageManager::flashFreeBytes() const {
    const uint64_t total = flashTotalBytes();
    const uint64_t used = flashUsedBytes();
    return total > used ? total - used : 0;
}

bool StorageManager::retrySd() {
    // Retry only the SD backend. Re-running begin() here used to remount
    // LittleFS and reset the shared bus without a proper SD idle sequence.
    const bool mounted = mountSd(false, true);
    Serial.printf("Storage: manual retry -> %s\n", mounted ? "SD ready" : "failed");
    return mounted;
}

void StorageManager::prepareForRestart() {
    // Finish buffered filesystem work while the driver and bus are healthy.
    // Afterwards leave the still-powered card deselected with extra clocks so
    // the next firmware instance never inherits a partial command/data block.
    if (sdMounted) SD.end();
    sdMounted = false;
    sdWritable = false;
    pinMode(TFT_CS, OUTPUT);
    digitalWrite(TFT_CS, HIGH);
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    SPIClass& sharedSpi = displayStorageSpi();
    sharedSpi.beginTransaction(SPISettings(400000U, MSBFIRST, SPI_MODE0));
    for (uint8_t i = 0; i < 32; ++i) sharedSpi.transfer(0xFF);
    sharedSpi.endTransaction();
    delay(20);
    Serial.println("Storage: SD quiesced for restart");
}

bool StorageManager::benchmarkTest(String& resultSummary) {
    // Storage Health is specifically an SD diagnostic. Never report a
    // successful LittleFS fallback as an SD benchmark.
    if (!usingSd() && !retrySd()) {
        resultSummary = "SD NO MOUNT";
        return false;
    }
    fs::FS& fs = SD;
    const char* testPath = "/RFSuite/log/.bench_test";
    fs.remove(testPath);

    uint8_t buffer[512];
    for (size_t i = 0; i < sizeof(buffer); ++i) buffer[i] = static_cast<uint8_t>(i ^ 0x5A);

    const unsigned long startWrite = micros();
    File file = fs.open(testPath, FILE_WRITE);
    if (!file) {
        lastErrorMsg = "WRITE OPEN FAIL";
        resultSummary = "SD OPEN FAIL";
        return false;
    }

    // Write 4KB in 512B chunks
    for (int chunk = 0; chunk < 8; ++chunk) {
        if (file.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
            file.close();
            fs.remove(testPath);
            lastErrorMsg = "WRITE SHORT";
            resultSummary = "SD WRITE FAIL";
            return false;
        }
    }
    file.close();
    const unsigned long writeDurationUs = micros() - startWrite;

    // Read and verify
    const unsigned long startRead = micros();
    file = fs.open(testPath, FILE_READ);
    if (!file) {
        fs.remove(testPath);
        lastErrorMsg = "READ OPEN FAIL";
        resultSummary = "SD READ FAIL";
        return false;
    }

    uint8_t readBuf[512];
    for (int chunk = 0; chunk < 8; ++chunk) {
        const size_t bytesRead = file.read(readBuf, sizeof(readBuf));
        if (bytesRead != sizeof(readBuf) || memcmp(buffer, readBuf, sizeof(readBuf)) != 0) {
            file.close();
            fs.remove(testPath);
            lastErrorMsg = "CRC/VERIFY FAIL";
            resultSummary = "SD CRC FAIL";
            return false;
        }
    }
    file.close();
    const unsigned long readDurationUs = micros() - startRead;
    fs.remove(testPath);

    const uint32_t writeSpeedKBps = writeDurationUs > 0 ? (4096UL * 1000UL) / writeDurationUs : 0;
    const uint32_t readSpeedKBps = readDurationUs > 0 ? (4096UL * 1000UL) / readDurationUs : 0;
    lastErrorMsg = "NONE";
    resultSummary = "SD W" + String(writeSpeedKBps) + " R" + String(readSpeedKBps);
    return true;
}

fs::FS& StorageManager::filesystem() {
    return sdWritable ? static_cast<fs::FS&>(SD) : static_cast<fs::FS&>(LittleFS);
}

const char* StorageManager::sessionPath() const {
    return sdWritable ? "/RFSuite/log/rf_session.csv" : "/rf_session.csv";
}
