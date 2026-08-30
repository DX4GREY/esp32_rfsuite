#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

namespace SessionFormat {

constexpr size_t CHANNEL_COUNT = 126;

struct Sweep {
    uint32_t timestampMs = 0;
    uint32_t sequence = 0;
    uint8_t peakChannel = 0;
    uint8_t peakLevel = 0;
    uint8_t confidence = 0;
    uint8_t levels[CHANNEL_COUNT] = {};
};

inline bool parseUnsigned(const char* begin, const char* end, uint32_t& value) {
    if (!begin || begin == end) return false;
    uint32_t result = 0;
    for (const char* p = begin; p < end; ++p) {
        if (*p < '0' || *p > '9') return false;
        const uint8_t digit = static_cast<uint8_t>(*p - '0');
        if (result > (UINT32_MAX - digit) / 10U) return false;
        result = result * 10U + digit;
    }
    value = result;
    return true;
}

inline bool parseSweepLine(const char* line, Sweep& output) {
    if (!line || line[0] != 'S' || line[1] != ',') return false;
    Sweep parsed{};
    const char* fieldStart = line + 2;
    size_t field = 1;
    size_t channel = 0;
    while (true) {
        const char* end = fieldStart;
        while (*end && *end != ',' && *end != '\r' && *end != '\n') ++end;
        uint32_t value = 0;
        if (!parseUnsigned(fieldStart, end, value)) return false;
        if (field == 1) parsed.timestampMs = value;
        else if (field == 2) parsed.sequence = value;
        else if (field == 3) { if (value >= CHANNEL_COUNT) return false; parsed.peakChannel = value; }
        else if (field == 4) { if (value > 100) return false; parsed.peakLevel = value; }
        else if (field == 5) { if (value > 100) return false; parsed.confidence = value; }
        else if (field >= 9) {
            if (channel >= CHANNEL_COUNT || value > 100) return false;
            parsed.levels[channel++] = static_cast<uint8_t>(value);
        }
        ++field;
        if (*end != ',') break;
        fieldStart = end + 1;
    }
    if (field != 9 + CHANNEL_COUNT || channel != CHANNEL_COUNT) return false;
    output = parsed;
    return true;
}

}  // namespace SessionFormat
