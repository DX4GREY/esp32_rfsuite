#pragma once

#include <stdint.h>
#include <string.h>

namespace RfrFormat {

struct __attribute__((packed)) HeaderV1 {
    char magic[4];
    uint32_t frequencyHz;
    uint32_t pulseCount;
    uint8_t firstLevel;
    uint8_t reserved[3];
};

struct __attribute__((packed)) HeaderV2 {
    char magic[4];
    uint32_t frequencyHz;
    uint32_t pulseCount;
    uint8_t firstLevel;
    uint8_t preset;
    uint8_t region;
    uint8_t flags;
    uint32_t payloadCrc32;
};

inline bool isV1(const char magic[4]) { return memcmp(magic, "RFS1", 4) == 0; }
inline bool isV2(const char magic[4]) { return memcmp(magic, "RFS2", 4) == 0; }
inline bool validMetadata(uint32_t frequencyHz, uint32_t pulseCount,
                          uint32_t maxPulses) {
    return frequencyHz >= 250000000UL && frequencyHz <= 960000000UL &&
           pulseCount >= 2 && pulseCount <= maxPulses;
}

}  // namespace RfrFormat
