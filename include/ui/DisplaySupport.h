#pragma once

#include <Arduino.h>
#include "core/AppTypes.h"

namespace DisplayUi {

enum ToastType : uint8_t {
    TOAST_INFO = 0,
    TOAST_SUCCESS,
    TOAST_WARN,
    TOAST_ERROR
};

// All renderer colors live in one palette so a theme change remains
// consistent across menus, graphs, status cards, and dynamic regions.
struct ThemePalette {
    uint16_t headerBg;
    uint16_t cardBg;
    uint16_t border;
    uint16_t grid;
    uint16_t accent;
    uint16_t low;
    uint16_t mid;
    uint16_t high;
    uint16_t critical;
    uint16_t footerBg;
    uint16_t activeBg;
    uint16_t barTrack;
};

const ThemePalette& palette();

inline const char* confidenceBadge(uint8_t confidence) {
    if (confidence >= 70) return "HIGH CONF";
    if (confidence >= 35) return "GOOD";
    return "LOW DATA";
}

inline uint16_t confidenceBadgeColor(uint8_t confidence) {
    if (confidence >= 70) return palette().low;
    if (confidence >= 35) return palette().accent;
    return palette().high;
}

inline const char* compactBandName(AnalyzerBand band) {
    switch (band) {
        case SCAN_BAND_WIFI: return "WIFI";
        case SCAN_BAND_BT:   return "BT";
        case SCAN_BAND_ALL:
        default:             return "ALL";
    }
}

inline String formatBytesShort(uint32_t bytes) {
    if (bytes >= 1024UL * 1024UL) {
        const uint32_t whole = bytes / (1024UL * 1024UL);
        const uint32_t decimal = ((bytes % (1024UL * 1024UL)) * 10UL) /
                                 (1024UL * 1024UL);
        return String(whole) + "." + String(decimal) + " MB";
    }
    if (bytes >= 1024UL) return String(bytes / 1024UL) + " KB";
    return String(bytes) + " B";
}

inline String formatUptime(unsigned long uptimeMs) {
    unsigned long seconds = uptimeMs / 1000UL;
    const unsigned long days = seconds / 86400UL;
    seconds %= 86400UL;
    const unsigned long hours = seconds / 3600UL;
    seconds %= 3600UL;
    const unsigned long minutes = seconds / 60UL;
    seconds %= 60UL;

    char text[20];
    if (days > 0) {
        snprintf(text, sizeof(text), "%lud %02lu:%02lu:%02lu",
                 days, hours, minutes, seconds);
    } else {
        snprintf(text, sizeof(text), "%02lu:%02lu:%02lu",
                 hours, minutes, seconds);
    }
    return String(text);
}

inline int16_t centeredTextX(const String& text, uint8_t textSize = 1,
                             int16_t screenWidth = 160) {
    return (screenWidth - text.length() * 6 * textSize) / 2;
}

}  // namespace DisplayUi

// Compatibility aliases keep screen drawing code readable while the values
// are resolved dynamically from the active palette.
#define SPECTRUM_HEADER_BG (DisplayUi::palette().headerBg)
#define SPECTRUM_CARD_BG   (DisplayUi::palette().cardBg)
#define SPECTRUM_BORDER    (DisplayUi::palette().border)
#define SPECTRUM_GRID      (DisplayUi::palette().grid)
#define SPECTRUM_ACCENT    (DisplayUi::palette().accent)
#define SPECTRUM_LOW       (DisplayUi::palette().low)
#define SPECTRUM_MID       (DisplayUi::palette().mid)
#define SPECTRUM_HIGH      (DisplayUi::palette().high)
#define SPECTRUM_CRITICAL  (DisplayUi::palette().critical)
#define DISPLAY_FOOTER_BG  (DisplayUi::palette().footerBg)
#define DISPLAY_ACTIVE_BG  (DisplayUi::palette().activeBg)
#define DISPLAY_BAR_TRACK  (DisplayUi::palette().barTrack)
