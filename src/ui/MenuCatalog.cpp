#include "ui/MenuCatalog.h"

namespace {

constexpr MenuFeature FEATURES[MenuCatalog::FEATURE_COUNT] = {
    {"SPECTRUM",  APP_MODE_ANALYZER_SPECTRUM, 0,
     MENU_OPEN_STOP_RADIOS | MENU_OPEN_RESET_PEAKS},
    {"WATERFALL", APP_MODE_WATERFALL,          1, MENU_OPEN_STOP_RADIOS},
    {"INSPECT",   APP_MODE_ANALYZER_CHANNEL,   2,
     MENU_OPEN_STOP_RADIOS | MENU_OPEN_RESET_INSPECTOR},
    {"SURVEY",    APP_MODE_SURVEY,             3, MENU_OPEN_STOP_RADIOS},
    {"EVENTS",    APP_MODE_EVENTS,             4, MENU_OPEN_STOP_RADIOS},
    {"LOGGING",   APP_MODE_LOGGING,            5, MENU_OPEN_STOP_RADIOS},
#if RF_LAB_TX_ENABLED
    {"RF TEST",   APP_MODE_JAMMER,             6, MENU_OPEN_NONE},
#else
    {"RX ONLY",   APP_MODE_JAMMER,             6, MENU_OPEN_NONE},
#endif
    {"RADIO DIAG",APP_MODE_RADIO_DIAG,         7, MENU_OPEN_NONE},
    {"PROFILES",  APP_MODE_PROFILES,           8, MENU_OPEN_NONE},
    {"PKT SNIFF", APP_MODE_PACKET_SNIFFER,     7, MENU_OPEN_NONE}
    ,{"OCCUPANCY",APP_MODE_ENV_OCCUPANCY,      0, MENU_OPEN_STOP_RADIOS}
    ,{"HEATMAP",  APP_MODE_ENV_HEATMAP,        1, MENU_OPEN_STOP_RADIOS}
    ,{"BURSTS",   APP_MODE_ENV_BURSTS,         4, MENU_OPEN_STOP_RADIOS}
    ,{"COMPARE",  APP_MODE_ENV_COMPARE,        2, MENU_OPEN_STOP_RADIOS}
    ,{"RF STATUS",APP_MODE_ENV_STATUS,         3, MENU_OPEN_STOP_RADIOS}
    ,{"BEF/AFT",  APP_MODE_ENV_BEFORE_AFTER,   5, MENU_OPEN_STOP_RADIOS}
    ,{"BAND INFO",APP_MODE_ENV_BAND_INFO,      7, MENU_OPEN_STOP_RADIOS}
#if RF_LAB_TX_ENABLED
    ,{"AUTH PROBE",APP_MODE_ENV_PROBE,         6, MENU_OPEN_STOP_RADIOS}
#else
    ,{"RX ONLY",  APP_MODE_ENV_PROBE,          6, MENU_OPEN_STOP_RADIOS}
#endif
};

constexpr const char* PAGE_TITLES[MenuCatalog::PAGE_COUNT] = {
    "ANALYZE",
    "TOOLS",
    "ENV TEST",
    "ENV MORE"
};

constexpr uint8_t PAGE_ITEM_COUNTS[MenuCatalog::PAGE_COUNT] = {6, 4, 6, 2};

}  // namespace

namespace MenuCatalog {

int featureIndex(int page, int slot) {
    page = constrain(page, 0, PAGE_COUNT - 1);
    int index = slot;
    for (int i = 0; i < page; ++i) index += PAGE_ITEM_COUNTS[i];
    return index;
}

const MenuFeature& featureAt(int page, int slot) {
    return FEATURES[featureIndex(page, slot)];
}

const char* pageTitle(int page) {
    return PAGE_TITLES[page];
}

int pageItemCount(int page) {
    return PAGE_ITEM_COUNTS[constrain(page, 0, PAGE_COUNT - 1)];
}

}  // namespace MenuCatalog
