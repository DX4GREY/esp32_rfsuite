#pragma once

#include <stddef.h>
#include <stdint.h>

namespace SettingsValidation {

inline uint8_t enumOrDefault(uint8_t value, uint8_t count, uint8_t fallback) {
    return value < count ? value : fallback;
}

inline uint8_t replayRepeats(uint8_t value) {
    return value >= 5 ? 5 : (value >= 3 ? 3 : 1);
}

inline bool validChannels(const uint8_t* channels, size_t count, uint8_t maximum) {
    if (!channels || !count) return false;
    for (size_t i = 0; i < count; ++i) if (channels[i] > maximum) return false;
    return true;
}

}  // namespace SettingsValidation
