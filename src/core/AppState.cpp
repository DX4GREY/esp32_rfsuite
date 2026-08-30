#include "core/AppState.h"

AppState appState;

const char* AppState::getAnimationSpeedName() const {
    static const char* names[] = {"SLOW", "NORMAL", "FAST"};
    return names[constrain(animationSpeed, static_cast<uint8_t>(0), static_cast<uint8_t>(2))];
}

uint16_t AppState::scaledAnimationDelay(uint16_t normalMs) const {
    if (animationSpeed == 0) return normalMs + normalMs / 2;
    if (animationSpeed == 2) return max<uint16_t>(1, normalMs * 2 / 3);
    return normalMs;
}

void AppState::setJammerTarget(JammerTarget target) {
    jammerTarget = target;
    switch (jammerTarget) {
        case JAM_TARGET_WIFI:
            jammerMinCh = WIFI_MIN_CH;
            jammerMaxCh = WIFI_MAX_CH;
            currentJamChannel = (WIFI_MIN_CH + WIFI_MAX_CH) / 2;
            break;
        case JAM_TARGET_BT:
            jammerMinCh = BT_MIN_CH;
            jammerMaxCh = BT_MAX_CH;
            currentJamChannel = bluetooth_even_channels[0];
            break;
        case JAM_TARGET_BLE_ADV:
            jammerMinCh = BT_MIN_CH;
            jammerMaxCh = BT_MAX_CH;
            currentJamChannel = BLE_ADV_CHANNELS[0];
            break;
        case JAM_TARGET_BLE_DATA:
            jammerMinCh = BT_MIN_CH;
            jammerMaxCh = BT_MAX_CH;
            currentJamChannel = BLE_DATA_CHANNELS[0];
            break;
        case JAM_TARGET_ALL:
            jammerMinCh = MIN_CHANNEL;
            jammerMaxCh = MAX_CHANNEL;
            currentJamChannel = (MIN_CHANNEL + MAX_CHANNEL) / 2;
            break;
        case JAM_TARGET_ZIGBEE:
            jammerMinCh = 11;
            jammerMaxCh = 26;
            currentJamChannel = 18;
            break;
    }
    currentJamChannel2 = currentJamChannel;
    markSettingsDirty();
}

bool AppState::setJammerTargetByName(const String& name) {
    String n = name;
    n.trim();
    n.toLowerCase();

    if (n == "wifi" || n == "wi-fi") {
        setJammerTarget(JAM_TARGET_WIFI);
        return true;
    } else if (n == "bt" || n == "bluetooth") {
        setJammerTarget(JAM_TARGET_BT);
        return true;
    } else if (n == "ble" || n == "bleadv" || n == "ble-adv") {
        setJammerTarget(JAM_TARGET_BLE_ADV);
        return true;
    } else if (n == "bledata" || n == "ble-data") {
        setJammerTarget(JAM_TARGET_BLE_DATA);
        return true;
    } else if (n == "all" || n == "full" || n == "drone" || n == "allband") {
        setJammerTarget(JAM_TARGET_ALL);
        return true;
    } else if (n == "zigbee") {
        setJammerTarget(JAM_TARGET_ZIGBEE);
        return true;
    }
    return false;
}

const char* AppState::getJammerTargetName() const {
    switch (jammerTarget) {
        case JAM_TARGET_WIFI:     return "Wi-Fi";
        case JAM_TARGET_BT:       return "Bluetooth";
        case JAM_TARGET_BLE_ADV:  return "BLE Advertising";
        case JAM_TARGET_BLE_DATA: return "BLE Data";
        case JAM_TARGET_ALL:      return "All Band / Drone";
        case JAM_TARGET_ZIGBEE:   return "Zigbee";
        default:                  return "Unknown";
    }
}

const char* AppState::getJammerFreqRangeStr() const {
    switch (jammerTarget) {
        case JAM_TARGET_WIFI:     return "2412-2472MHz (13 Ch)";
        case JAM_TARGET_BT:       return "2402-2480MHz (40 Ch)";
        case JAM_TARGET_BLE_ADV:  return "2401-2481MHz (9 Ch)";
        case JAM_TARGET_BLE_DATA: return "2404-2478MHz (12 Ch)";
        case JAM_TARGET_ALL:      return "2400-2483MHz (84 Ch)";
        case JAM_TARGET_ZIGBEE:   return "2405-2480MHz (16 Ch)";
        default:                  return "";
    }
}

void AppState::cycleJammerTarget(int direction) {
    int current = (int)jammerTarget;
    int total = 6;
    current = (current + direction + total) % total;
    setJammerTarget((JammerTarget)current);
}

void AppState::cyclePowerLevel(int direction) {
    int cur = (int)powerLevel;
    // RF24 PA levels: RF24_PA_MIN(0), RF24_PA_LOW(1), RF24_PA_HIGH(2), RF24_PA_MAX(3)
        cur = (cur + direction + 4) % 4;
    powerLevel = (rf24_pa_dbm_e)cur;
    markSettingsDirty();
}

bool AppState::setPowerLevelByName(const String& name) {
    String n = name;
    n.trim();
    n.toLowerCase();

        if (n == "min" || n == "-18" || n == "-18dbm") {
        powerLevel = RF24_PA_MIN;
        markSettingsDirty();
        return true;
    } else if (n == "low" || n == "-12" || n == "-12dbm") {
        powerLevel = RF24_PA_LOW;
        markSettingsDirty();
        return true;
    } else if (n == "high" || n == "-6" || n == "-6dbm") {
        powerLevel = RF24_PA_HIGH;
        markSettingsDirty();
        return true;
    } else if (n == "max" || n == "0" || n == "0dbm" || n == "full") {
        powerLevel = RF24_PA_MAX;
        markSettingsDirty();
        return true;
    }
    return false;
}

const char* AppState::getPowerLevelName() const {
    switch (powerLevel) {
        case RF24_PA_MIN:  return "MIN (-18dBm)";
        case RF24_PA_LOW:  return "LOW (-12dBm)";
        case RF24_PA_HIGH: return "HIGH (-6dBm)";
        case RF24_PA_MAX:  return "MAX (0dBm)";
        default:           return "MAX (0dBm)";
    }
}

const char* AppState::getPowerLevelDbmStr() const {
    switch (powerLevel) {
        case RF24_PA_MIN:  return "-18 dBm";
        case RF24_PA_LOW:  return "-12 dBm";
        case RF24_PA_HIGH: return "-6 dBm";
        case RF24_PA_MAX:  return "0 dBm / MAX";
        default:           return "0 dBm";
    }
}

void AppState::cycleDwellTime(int direction) {
    int idx = 2; // default is 200us (index 2)
    for (int i = 0; i < DWELL_PRESETS_COUNT; i++) {
        if (dwellTimeUs == DWELL_PRESETS[i]) {
            idx = i;
            break;
        }
    }
        idx = (idx + direction + DWELL_PRESETS_COUNT) % DWELL_PRESETS_COUNT;
    dwellTimeUs = DWELL_PRESETS[idx];
    markSettingsDirty();
}

bool AppState::setDwellTime(int us) {
        if (us >= 10 && us <= 10000) {
        dwellTimeUs = us;
        markSettingsDirty();
        return true;
    }
    return false;
}

const char* AppState::getDwellTimeName() const {
    switch (dwellTimeUs) {
        case 10:   return "10 us (MAX)";
        case 50:   return "50 us (Ultra Fast)";
        case 100:  return "100 us (Fast)";
        case 200:  return "200 us (Balanced)";
        case 500:  return "500 us (Heavy)";
        case 1000: return "1000 us (1 ms)";
        default:   return "Custom";
    }
}

void AppState::cycleDisplayTheme(int direction) {
    int current = static_cast<int>(displayTheme);
    current = (current + direction + DISPLAY_THEME_COUNT) % DISPLAY_THEME_COUNT;
    displayTheme = static_cast<DisplayThemeId>(current);
    applyThemeProfile();
    markSettingsDirty();
}

const char* AppState::getDisplayThemeName() const {
    switch (displayTheme) {
        case DISPLAY_THEME_OCEAN:  return "OCEAN";
        case DISPLAY_THEME_AMBER:  return "AMBER";
        case DISPLAY_THEME_MATRIX: return "MATRIX";
        case DISPLAY_THEME_VIOLET: return "VIOLET";
        case DISPLAY_THEME_ICE:    return "ICE";
        case DISPLAY_THEME_FLIPPER:return "FLIPPER";
        case DISPLAY_THEME_RETRO:  return "RETRO";
        case DISPLAY_THEME_TERMINAL:return "TERMINAL";
        case DISPLAY_THEME_NEON:   return "NEON";
        case DISPLAY_THEME_CYBER:
        default:                   return "CYBER";
    }
}

const char* AppState::getDisplayOrientationName() const {
    switch (displayRotation) {
        case 0: return "PORTRAIT";
        case 1: return "LANDSCAPE FLIP";
        case 2: return "PORTRAIT FLIP";
        case 3:
        default: return "LANDSCAPE";
    }
}

void AppState::cycleDisplayOrientation() {
    // Present rotations clockwise starting from the board's default mounting:
    // landscape (3), portrait (0), reversed landscape (1), reversed portrait (2).
    displayRotation = (displayRotation + 1) % 4;
    markSettingsDirty();
}

void AppState::applyThemeProfile() {
    switch (displayTheme) {
        case DISPLAY_THEME_OCEAN:
        case DISPLAY_THEME_MATRIX:
        case DISPLAY_THEME_ICE:
        case DISPLAY_THEME_TERMINAL:
            menuLayout = MENU_LAYOUT_LIST;
            break;
        default:
            menuLayout = MENU_LAYOUT_GRID;
            break;
    }
}

const char* AppState::getMenuLayoutName() const {
    return menuLayout == MENU_LAYOUT_LIST ? "LIST" : "GRID";
}
