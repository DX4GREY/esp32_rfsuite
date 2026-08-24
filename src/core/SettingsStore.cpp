#include "core/AppState.h"
#include <Preferences.h>
#include "core/RfEnvironmentState.h"
#include "core/RfEnvironmentMath.h"

namespace {
constexpr uint8_t SETTINGS_SCHEMA_VERSION = 6;
constexpr unsigned long SETTINGS_SAVE_DELAY_MS = 1500;
}

void AppState::loadSettings() {
    Preferences prefs;
    if (!prefs.begin("appstate", false)) {
        setJammerTarget(jammerTarget);
        return;
    }
    const uint8_t storedSchema = prefs.getUChar("schema", 0);

    const int storedPower = prefs.getInt("power", DEFAULT_POWER);
    powerLevel = storedPower >= RF24_PA_MIN && storedPower <= RF24_PA_MAX ?
                 static_cast<rf24_pa_dbm_e>(storedPower) : DEFAULT_POWER;
    dwellTimeUs = constrain(prefs.getInt("dwell", JAMMER_DWELL_US), 10, 10000);
    const uint8_t storedTarget = prefs.getUChar("target", JAM_TARGET_WIFI);
    jammerTarget = storedTarget <= JAM_TARGET_ZIGBEE ?
                   static_cast<JammerTarget>(storedTarget) : JAM_TARGET_WIFI;

    const uint8_t storedProfile = prefs.getUChar("profile", SCAN_PROFILE_BALANCED);
    scanProfile = storedProfile <= SCAN_PROFILE_CUSTOM ?
                  static_cast<ScanProfile>(storedProfile) : SCAN_PROFILE_BALANCED;
    customSpectrumSamples = constrain(prefs.getInt("custom", 40), 10, 100);

    const uint8_t storedTheme = prefs.getUChar("theme", DISPLAY_THEME_CYBER);
    displayTheme = storedTheme < DISPLAY_THEME_COUNT ?
                   static_cast<DisplayThemeId>(storedTheme) : DISPLAY_THEME_CYBER;

    if (storedSchema >= 2) {
        const uint8_t storedTrace = prefs.getUChar("trace", ANALYZER_TRACE_LIVE);
        analyzerTraceMode = storedTrace < ANALYZER_TRACE_COUNT ?
                            static_cast<AnalyzerTraceMode>(storedTrace) :
                            ANALYZER_TRACE_LIVE;
        // A baseline describes the current RF environment and intentionally
        // lives in RAM. Never restore DELTA without a matching baseline.
        if (analyzerTraceMode == ANALYZER_TRACE_DELTA) {
            analyzerTraceMode = ANALYZER_TRACE_LIVE;
        }
        eventThreshold = constrain(prefs.getUChar("evt_thr", 60), 5, 100);
        eventHysteresis = constrain(prefs.getUChar("evt_hys", 10), 0, eventThreshold);
        eventMinSweeps = constrain(prefs.getUChar("evt_dur", 2), 1, 20);
        eventMinChannels = constrain(prefs.getUChar("evt_multi", 1), 1, 16);
        if (prefs.getBytesLength("watch") == sizeof(watchedChannels)) {
            prefs.getBytes("watch", watchedChannels, sizeof(watchedChannels));
        }
    }
    if (storedSchema >= 3) {
        const uint16_t storedWindow = prefs.getUShort("env_win", 10);
        rfEnvironmentState.config.sampleWindowSeconds =
            RfEnvironmentMath::validWindowSeconds(storedWindow) ? storedWindow : 10;
        rfEnvironmentState.config.minChannel = constrain(prefs.getUChar("env_min", 0), 0, 125);
        rfEnvironmentState.config.maxChannel = constrain(prefs.getUChar("env_max", 125), rfEnvironmentState.config.minChannel, 125);
        rfEnvironmentState.config.burstThreshold = constrain(prefs.getUChar("env_burst", 25), 5, 80);
        rfEnvironmentState.config.emaAlpha = constrain(prefs.getUChar("env_alpha", 25), 1, 100);
        rfEnvironmentState.config.historyDepth = constrain(prefs.getUChar("env_hist", RF_ENV_HISTORY_BUCKETS), 8, RF_ENV_HISTORY_BUCKETS);
        if (prefs.getBytesLength("env_cmp") == sizeof(rfEnvironmentState.config.compareChannels))
            prefs.getBytes("env_cmp", rfEnvironmentState.config.compareChannels, sizeof(rfEnvironmentState.config.compareChannels));
        rfEnvironmentState.config.compareCount = constrain(prefs.getUChar("env_cmp_n", 2), 2, RF_ENV_COMPARE_MAX);
        static const uint8_t defaultCompare[RF_ENV_COMPARE_MAX] = {6, 11, 42, 80};
        for (uint8_t i = 0; i < RF_ENV_COMPARE_MAX; ++i) {
            if (rfEnvironmentState.config.compareChannels[i] >= TOTAL_CHANNELS) {
                rfEnvironmentState.config.compareChannels[i] = defaultCompare[i];
            }
        }
#if RF_LAB_TX_ENABLED
        rfEnvironmentState.config.probeChannel = constrain(prefs.getUChar("prb_ch", 42), 0, 125);
        rfEnvironmentState.config.probeIntervalMs = constrain(prefs.getUShort("prb_int", 100), 20, 5000);
        rfEnvironmentState.config.probePacketCount = constrain(prefs.getUShort("prb_cnt", 100), 1, 1000);
        rfEnvironmentState.config.probeMaxDurationSeconds = constrain(prefs.getUShort("prb_dur", 10), 1, 60);
        rfEnvironmentState.config.probePayloadSize = constrain(prefs.getUChar("prb_size", 8), 1, 32);
        { const uint8_t rate=prefs.getUChar("prb_rate",RF24_1MBPS); rfEnvironmentState.config.probeDataRate=(rate==RF24_250KBPS||rate==RF24_1MBPS||rate==RF24_2MBPS)?rate:RF24_1MBPS; }
#endif
    }
    if (storedSchema >= 4) {
        const uint8_t layout = prefs.getUChar("menu_view", MENU_LAYOUT_GRID);
        menuLayout = layout < MENU_LAYOUT_COUNT ? static_cast<MenuLayout>(layout) : MENU_LAYOUT_GRID;
    }
    if (storedSchema >= 5) {
        saveSniffPacketsToSd = prefs.getBool("sniff_sd", false);
    }
    if (storedSchema >= 6) {
        subGhzRadioPreset = constrain(prefs.getUChar("sub_pre", 1), 0, 4);
        subGhzRegion = constrain(prefs.getUChar("sub_reg", 0), 0, 2);
        subGhzAutoTrigger = prefs.getBool("sub_trig", true);
        subGhzTriggerThreshold = constrain(static_cast<int>(prefs.getChar("sub_thr", -80)), -90, -70);
        const uint8_t repeats = prefs.getUChar("sub_rep", 1);
        subGhzReplayRepeats = repeats >= 5 ? 5 : (repeats >= 3 ? 3 : 1);
    }
    prefs.end();

    setJammerTarget(jammerTarget);
    settingsDirty = storedSchema != SETTINGS_SCHEMA_VERSION;
    settingsDirtySinceMs = millis();
}

void AppState::saveSettings() {
    Preferences prefs;
    if (!prefs.begin("appstate", false)) return;
    prefs.putUChar("schema", SETTINGS_SCHEMA_VERSION);
    prefs.putInt("power", static_cast<int>(powerLevel));
    prefs.putInt("dwell", dwellTimeUs);
    prefs.putUChar("target", static_cast<uint8_t>(jammerTarget));
    prefs.putUChar("profile", static_cast<uint8_t>(scanProfile));
    prefs.putInt("custom", customSpectrumSamples);
    prefs.putUChar("theme", static_cast<uint8_t>(displayTheme));
    prefs.putUChar("menu_view", static_cast<uint8_t>(menuLayout));
    prefs.putBool("sniff_sd", saveSniffPacketsToSd);
    prefs.putUChar("sub_pre", subGhzRadioPreset);
    prefs.putUChar("sub_reg", subGhzRegion);
    prefs.putBool("sub_trig", subGhzAutoTrigger);
    prefs.putChar("sub_thr", static_cast<int8_t>(subGhzTriggerThreshold));
    prefs.putUChar("sub_rep", subGhzReplayRepeats);
    prefs.putUChar("trace", static_cast<uint8_t>(analyzerTraceMode));
    prefs.putUChar("evt_thr", eventThreshold);
    prefs.putUChar("evt_hys", eventHysteresis);
    prefs.putUChar("evt_dur", eventMinSweeps);
    prefs.putUChar("evt_multi", eventMinChannels);
    prefs.putBytes("watch", watchedChannels, sizeof(watchedChannels));
    prefs.putUShort("env_win", rfEnvironmentState.config.sampleWindowSeconds);
    prefs.putUChar("env_min", rfEnvironmentState.config.minChannel);
    prefs.putUChar("env_max", rfEnvironmentState.config.maxChannel);
    prefs.putUChar("env_burst", rfEnvironmentState.config.burstThreshold);
    prefs.putUChar("env_alpha", rfEnvironmentState.config.emaAlpha);
    prefs.putUChar("env_hist", rfEnvironmentState.config.historyDepth);
    prefs.putUChar("env_cmp_n", rfEnvironmentState.config.compareCount);
    prefs.putBytes("env_cmp", rfEnvironmentState.config.compareChannels, sizeof(rfEnvironmentState.config.compareChannels));
#if RF_LAB_TX_ENABLED
    prefs.putUChar("prb_ch", rfEnvironmentState.config.probeChannel);
    prefs.putUShort("prb_int", rfEnvironmentState.config.probeIntervalMs);
    prefs.putUShort("prb_cnt", rfEnvironmentState.config.probePacketCount);
    prefs.putUShort("prb_dur", rfEnvironmentState.config.probeMaxDurationSeconds);
    prefs.putUChar("prb_size", rfEnvironmentState.config.probePayloadSize);
    prefs.putUChar("prb_rate", rfEnvironmentState.config.probeDataRate);
#endif
    prefs.end();
    settingsDirty = false;
}

void AppState::markSettingsDirty() {
    settingsDirty = true;
    settingsDirtySinceMs = millis();
}

void AppState::serviceSettingsPersistence() {
    if (settingsDirty && millis() - settingsDirtySinceMs >= SETTINGS_SAVE_DELAY_MS) {
        saveSettings();
    }
}

void AppState::factoryResetSettings() {
    Preferences prefs;
    if (prefs.begin("appstate", false)) {
        prefs.clear();
        prefs.end();
    }
    powerLevel = DEFAULT_POWER;
    dwellTimeUs = JAMMER_DWELL_US;
    displayTheme = DISPLAY_THEME_CYBER;
    menuLayout = MENU_LAYOUT_GRID;
    saveSniffPacketsToSd = false;
    subGhzRadioPreset = 1;
    subGhzRegion = 0;
    subGhzAutoTrigger = true;
    subGhzTriggerThreshold = -80;
    scanProfile = SCAN_PROFILE_BALANCED;
    customSpectrumSamples = 40;
    analyzerTraceMode = ANALYZER_TRACE_LIVE;
    configureEventEngine(60, 10, 2, 1);
    memset(watchedChannels, 0, sizeof(watchedChannels));
    rfEnvironmentState.config = RfEnvironmentConfig();
    setJammerTarget(JAM_TARGET_WIFI);
    saveSettings();
}
