#pragma once

#include <Arduino.h>
#include <FS.h>

class StorageManager {
public:
    bool begin();
    fs::FS& filesystem();
    const char* sessionPath() const;
    const char* scriptsPath() const { return "/RFSuite/scripts"; }
    const char* subGhzPath() const { return "/RFSuite/SubGHz"; }
    bool usingSd() const { return sdWritable; }
    bool sdDetected() const { return sdMounted; }
    bool sdUsable() const { return sdWritable; }
    bool flashUsable() const { return flashMounted; }
    const char* backendName() const { return sdWritable ? "SD" : "LittleFS"; }
    const char* sdStatus() const { return sdState; }
    const char* sdTypeName() const;
    uint64_t sdTotalBytes() const;
    uint64_t sdUsedBytes() const;
    uint64_t sdFreeBytes() const;
    uint64_t flashTotalBytes() const;
    uint64_t flashUsedBytes() const;
    uint64_t flashFreeBytes() const;
    bool retrySd();
    void prepareForRestart();
    bool benchmarkTest(String& resultSummary);
    const char* lastError() const { return lastErrorMsg; }

private:
    bool mountSd(bool resetSpiBus, bool extendedRecovery);
    bool validateMountedSd(uint32_t mountedFrequency);
    void recoverSdBus(bool resetSpiBus, uint32_t settleMs);
    bool ensureDirectory(fs::FS& fs, const char* path);
    bool sdMounted = false;
    bool sdWritable = false;
    bool flashMounted = false;
    const char* sdState = "not initialized";
    const char* lastErrorMsg = "NONE";
};

extern StorageManager storageManager;
