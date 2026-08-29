#pragma once
#include <Arduino.h>
#include <RF24.h>
#include "config/Config.h"
#include "core/AppTypes.h"

// =============================================================================
// GLOBAL APPLICATION STATE
// =============================================================================

struct AppState {
    // Mode UI & Operasi
    AppMode appMode = APP_MODE_BAND_SELECT;
    RadioBand radioBand = RADIO_BAND_24_GHZ;
    // Runtime-only demonstration mode. Never persisted and never authorizes TX.
    bool simulationMode = false;

    // ----- JAMMER STATE -----
    volatile bool jamming = false;
    volatile JammerTarget jammerTarget = JAM_TARGET_WIFI;
    int jammerMinCh = WIFI_MIN_CH;
    int jammerMaxCh = WIFI_MAX_CH;
    // Updated by the Core 0 RF task and read by the Core 1 display task.
    volatile int currentJamChannel = 37;
    volatile int currentJamChannel2 = 37;

    // Radio Transmission Parameters (Dynamic RF Settings)
    rf24_pa_dbm_e powerLevel = DEFAULT_POWER;
    rf24_datarate_e dataRate = DEFAULT_RATE;
    volatile int dwellTimeUs = JAMMER_DWELL_US;
    DisplayThemeId displayTheme = DISPLAY_THEME_CYBER;
    // Adafruit GFX rotation: 0/2 portrait, 1/3 landscape. The default panel
    // mounting uses rotation 3.
    uint8_t displayRotation = 3;
    MenuLayout menuLayout = MENU_LAYOUT_GRID;
    bool saveSniffPacketsToSd = false;
    uint8_t subGhzRadioPreset = 1;
    uint8_t subGhzRegion = 0;
    bool subGhzAutoTrigger = true;
    int16_t subGhzTriggerThreshold = -80;
    uint8_t subGhzReplayRepeats = 1;

    // ----- ANALYZER STATE -----
    AnalyzerBand analyzerBand = SCAN_BAND_ALL;
    AnalyzerRadioMode analyzerRadioMode = ANALYZER_RADIO_FAST;
    ScanProfile scanProfile = SCAN_PROFILE_BALANCED;
    int customSpectrumSamples = 40;
    uint8_t spectrumLevels[TOTAL_CHANNELS];  // Current RF activity (0 - 100%)
    uint8_t peakLevels[TOTAL_CHANNELS];      // Peak Hold Value (0 - 100%)
    uint8_t radio1Levels[TOTAL_CHANNELS];    // Per-radio activity for diagnostics
    uint8_t radio2Levels[TOTAL_CHANNELS];
    uint8_t averageLevels[TOTAL_CHANNELS];
    uint8_t maxLevels[TOTAL_CHANNELS];
    uint8_t baselineLevels[TOTAL_CHANNELS];
    bool watchedChannels[TOTAL_CHANNELS];
    int peakChannel = 0;                     // Channel with the highest RF
    uint8_t peakLevel = 0;                   // Current highest RF value (%)
    uint8_t waterfall[WATERFALL_ROWS][TOTAL_CHANNELS];
    uint8_t waterfallHead = 0;
    uint8_t waterfallCount = 0;
    uint32_t occupancyTotal[TOTAL_CHANNELS];
    uint32_t surveySweeps = 0;
    RfEvent rfEvents[RF_EVENT_COUNT];
    uint8_t eventHead = 0;
    uint8_t eventCount = 0;
    unsigned long lastEventMs = 0;
    bool loggingEnabled = false;
    AnalyzerTraceMode analyzerTraceMode = ANALYZER_TRACE_LIVE;
    bool analyzerFrozen = false;
    bool baselineValid = false;
    bool cursorFollowsPeak = true;
    int cursorChannel = 36;
    uint8_t analyzerZoom = 1;
    uint8_t analyzerConfidence = 0;

    // Event engine configuration. Percentages represent carrier-hit ratios,
    // not calibrated RSSI or dBm.
    uint8_t eventThreshold = 60;
    uint8_t eventHysteresis = 10;
    uint8_t eventMinSweeps = 2;
    uint8_t eventMinChannels = 1;
    uint8_t eventRunLength[TOTAL_CHANNELS];
    bool eventLatched = false;

    // Single Channel Inspection
    int inspectedChannel = 36;               // Default Wi-Fi Ch 6
    uint8_t inspectedLevel = 0;              // Aktivitas real-time %
    uint8_t inspectedPeak = 0;               // Peak aktivitas %

    // ----- HELPER METHODS -----
    void setJammerTarget(JammerTarget target);
    bool setJammerTargetByName(const String& name);
    const char* getJammerTargetName() const;
    const char* getJammerFreqRangeStr() const;
    void cycleJammerTarget(int direction = 1);

    // RF Settings Helpers
    void cyclePowerLevel(int direction = 1);
    bool setPowerLevelByName(const String& name);
    const char* getPowerLevelName() const;
    const char* getPowerLevelDbmStr() const;

    void cycleDwellTime(int direction = 1);
    bool setDwellTime(int us);
    const char* getDwellTimeName() const;
    void cycleDisplayTheme(int direction = 1);
    const char* getDisplayThemeName() const;
    const char* getDisplayOrientationName() const;
    void cycleDisplayOrientation();
    void applyThemeProfile();
    const char* getMenuLayoutName() const;

    void cycleAnalyzerBand(int direction = 1);
    const char* getAnalyzerBandName() const;
    void cycleAnalyzerRadioMode(int direction = 1);
    const char* getAnalyzerRadioModeName() const;
    void cycleScanProfile(int direction = 1);
    const char* getScanProfileName() const;
    int getSpectrumSampleCount() const;
    int getInspectSampleCount() const;
    void cycleCustomSampleCount();
    void cycleAnalyzerTraceMode(int direction = 1);
    const char* getAnalyzerTraceModeName() const;
    void cycleAnalyzerZoom();
    void setCursorChannel(int channel, bool followPeak = false);
    void toggleWatchChannel(int channel);
    void captureBaseline();
    void clearAnalyzerMax();
    uint8_t getTraceLevel(int channel) const;
    void configureEventEngine(uint8_t threshold, uint8_t hysteresis,
                              uint8_t minSweeps, uint8_t minChannels);
    void recordCompletedSweep(uint8_t receiverCount = 1);
    void resetSurvey();
    void clearEvents();
    void getAnalyzerChannelRange(int &minCh, int &maxCh) const;
    void resetPeaks();
    void decayPeaks();

    // NVS Persistence
    void loadSettings();
    void saveSettings();
    void markSettingsDirty();
    void serviceSettingsPersistence();
    void factoryResetSettings();

private:
    bool settingsDirty = false;
    unsigned long settingsDirtySinceMs = 0;
};

extern AppState appState;
