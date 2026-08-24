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
    bool usingSd() const { return sdMounted; }
    const char* backendName() const { return sdMounted ? "SD" : "LittleFS"; }
    const char* sdStatus() const { return sdState; }
    const char* sdTypeName() const;
    uint64_t sdTotalBytes() const;
    uint64_t sdUsedBytes() const;
    uint64_t sdFreeBytes() const;

private:
    bool ensureDirectory(fs::FS& fs, const char* path);
    bool sdMounted = false;
    bool flashMounted = false;
    const char* sdState = "not initialized";
};

extern StorageManager storageManager;
