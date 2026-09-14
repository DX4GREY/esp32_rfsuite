/*
 * =============================================================================
 * ESP32-S3 RF24 SUITE: DUAL-CORE JAMMER & SPECTRUM ANALYZER
 * =============================================================================
 * Organized Modules:
 *  - Config.h             : Hardware pinout, timing, frequency presets, & constants
 *  - AppState.h/.cpp      : Global state, 6 jammer target presets, & analyzer data
 *  - ButtonManager.h/.cpp : 50ms debouncing & navigation button edge detection
 *  - Watchdog.h/.cpp      : Main-loop deadline monitor (3s auto-recovery)
 *  - RadioManager.h/.cpp  : Dual-Core FreeRTOS Task (Core 0 RF Jamming) & Spectrum Scanning
 *  - DisplayManager.h/.cpp: Visual menu, real-time spectrum graph, & channel inspector
 *  - SerialCommander.h/.cpp: Interactive CLI monitor & ASCII graph visualization
 * =============================================================================
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config/Config.h"
#include "core/AppState.h"
#include "core/AppModePolicy.h"
#include "services/Watchdog.h"
#include "drivers/ButtonManager.h"
#include "drivers/RadioManager.h"
#include "drivers/Cc1101Manager.h"
#include "ui/DisplayManager.h"
#include "services/SerialCommander.h"
#include "services/SessionRecorder.h"
#include "services/StorageManager.h"
#include "services/PerformanceMonitor.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "services/RfAuthorizedProbe.h"
#include "services/LuaEngine.h"
#include "services/SubGhzRawService.h"
#include "services/EventLog.h"

// Implemented in src/ui/CarouselMenu.cpp. It returns true only when the
// animated GRID menu owns the current frame; all other screens continue through
// DisplayManager's original dirty-region renderer unchanged.
bool updateCarouselMenuUI(DisplayManager& dm);

// Implemented in src/ui/FirmwareUpdater.cpp. The input hook extends the global
// menu only while SD is usable and owns all input once the updater is open.
// The UI hook also supplies the LIST/non-animated fallback for the conditional
// menu entry.
bool processFirmwareUpdaterInput(DisplayManager& dm);
bool updateFirmwareUpdaterUI(DisplayManager& dm);

static constexpr unsigned long WAKE_HOLD_MS = 1500;

static void configureShutdownWakeSource() {
    pinMode(BTN_A, INPUT_PULLUP);
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(BTN_A));
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(BTN_A));
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(BTN_A), 0);
}

[[noreturn]] static void enterShutdownSleep() {
    configureShutdownWakeSource();
    Serial.flush();
    esp_deep_sleep_start();
    while (true) delay(1000);
}

static bool quiesceForShutdown() {
    const bool probeStopped = rfAuthorizedProbe.stopAndWait();
    const bool environmentStopped = rfEnvironmentAnalyzer.stopAndWait();
    subGhzRawService.prepareForShutdown();
    const bool radiosStopped = radioManager.stopAllAndWait();
    sessionRecorder.stop();
    watchdog.stop();
    return probeStopped && environmentStopped && radiosStopped;
}

static void validateShutdownWakePress() {
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) return;

    pinMode(BTN_A, INPUT_PULLUP);
    const unsigned long started = millis();
    while (digitalRead(BTN_A) == LOW && millis() - started < WAKE_HOLD_MS) delay(10);

    if (millis() - started < WAKE_HOLD_MS) {
        while (digitalRead(BTN_A) == LOW) delay(10);
        delay(50);
        enterShutdownSleep();
    }

    while (digitalRead(BTN_A) == LOW) delay(10);
    delay(50);
}

void yieldToUI() {
    if (!processFirmwareUpdaterInput(displayManager)) displayManager.processInput();
    if (!updateFirmwareUpdaterUI(displayManager) &&
        !updateCarouselMenuUI(displayManager)) {
        displayManager.updateUI();
    }
    watchdog.feed();
}

void setup() {
    validateShutdownWakePress();
    serialCommander.init(115200);
    appState.loadSettings();
    buttonManager.init();

    if (!sessionRecorder.begin()) {
        Serial.println("Session recorder unavailable: " + String(sessionRecorder.lastError()));
    }
    eventLog.begin();
    if (storageManager.sdDetected() && !storageManager.sdUsable())
        eventLog.warn("storage", storageManager.sdStatus());

    displayManager.init();
    displayManager.showSplash();

    if (!luaEngine.begin()) Serial.println("Lua: " + String(luaEngine.lastError()));

    pinMode(CC1101_CSN_PIN, OUTPUT);
    digitalWrite(CC1101_CSN_PIN, HIGH);
    if (!radioManager.init()) {
        Serial.println("No radio detected; continuing in diagnostics-only mode.");
        eventLog.warn("nrf24", "no radio detected");
    }

    if (!cc1101Manager.init()) {
        Serial.println("CC1101 unavailable: " + String(cc1101Manager.lastError()));
        eventLog.warn("cc1101", cc1101Manager.lastError());
    }
    cc1101Manager.setPreset(static_cast<Cc1101Preset>(appState.subGhzRadioPreset));
    subGhzRawService.setRegion(static_cast<SubGhzRegion>(appState.subGhzRegion));
    subGhzRawService.setAutoTrigger(appState.subGhzAutoTrigger);
    subGhzRawService.setTriggerThreshold(appState.subGhzTriggerThreshold);
    subGhzRawService.setReplayRepeatCount(appState.subGhzReplayRepeats);

    if (!appState.onboardingComplete) appState.appMode = APP_MODE_ONBOARDING;
    watchdog.init(WATCHDOG_TIMEOUT_US);
}

void loop() {
    performanceMonitor.tickLoop();
    subGhzRawService.service();
    rfEnvironmentAnalyzer.service();
    watchdog.feed();
    serialCommander.process();
    if (!processFirmwareUpdaterInput(displayManager)) displayManager.processInput();

    if (AppModePolicy::runsSpectrumScan(appState.appMode,
                                        appState.loggingEnabled) &&
        !appState.analyzerFrozen) {
        const uint32_t scanStartedUs = micros();
        if (appState.simulationMode && appState.radioBand == RADIO_BAND_24_GHZ) {
            static uint32_t simulatedSweep = 0;
            int minCh, maxCh; appState.getAnalyzerChannelRange(minCh, maxCh);
            uint8_t best = 0; int bestChannel = minCh;
            for (int ch = minCh; ch <= maxCh; ++ch) {
                const int beaconA = max(0, 82 - abs(ch - 42) * 7);
                const int beaconB = max(0, 63 - abs(ch - 88) * 5);
                const uint8_t level = constrain(max(beaconA, beaconB) +
                    static_cast<int>((simulatedSweep + ch * 3) % 9), 0, 100);
                appState.spectrumLevels[ch] = level;
                appState.radio1Levels[ch] = level;
                appState.radio2Levels[ch] = level > 8 ? level - 8 : 0;
                appState.peakLevels[ch] = max(appState.peakLevels[ch], level);
                if (level > best) { best = level; bestChannel = ch; }
            }
            appState.peakLevel = best; appState.peakChannel = bestChannel;
            appState.recordCompletedSweep(0); simulatedSweep++;
            delay(25);
        } else radioManager.scanSpectrum(yieldToUI);
        performanceMonitor.recordSweep(micros() - scanStartedUs);
    } else if (appState.appMode == APP_MODE_ANALYZER_CHANNEL) {
        if (appState.simulationMode) {
            const uint8_t level = 25 + ((millis() / 40 + appState.inspectedChannel * 5) % 70);
            appState.inspectedLevel = level;
            appState.inspectedPeak = max(appState.inspectedPeak, level);
        } else radioManager.inspectChannel(appState.inspectedChannel);
        delay(25);
    } else if (appState.appMode == APP_MODE_PACKET_SNIFFER) {
        radioManager.servicePacketSniffer();
        delay(2);
    } else if (appState.appMode == APP_MODE_REBOOT ||
               appState.appMode == APP_MODE_SHUTDOWN) {
        delay(10);
    } else {
        delay(10);
    }

    const uint32_t uiStartedUs = micros();
    if (!updateFirmwareUpdaterUI(displayManager) &&
        !updateCarouselMenuUI(displayManager)) {
        displayManager.updateUI();
    }
    performanceMonitor.recordUi(micros() - uiStartedUs);
    sessionRecorder.service();
    if (appState.loggingEnabled && !sessionRecorder.isRecording()) {
        appState.loggingEnabled = false;
        displayManager.requestRedraw();
    }
    appState.serviceSettingsPersistence();

    if (appState.appMode == APP_MODE_REBOOT) {
        delay(1200);
        Serial.println("REBOOTING SYSTEM...");
        sessionRecorder.stop();
        storageManager.prepareForRestart();
        Serial.flush();
        ESP.restart();
    }

    if (appState.appMode == APP_MODE_SHUTDOWN) {
        delay(900);
        while (digitalRead(BTN_A) == LOW) delay(10);
        delay(50);
        Serial.println("SYSTEM SHUTDOWN: entering deep sleep...");
        if (!quiesceForShutdown()) {
            Serial.println("SYSTEM SHUTDOWN: task stop timed out; restarting safely");
            sessionRecorder.stop();
            storageManager.prepareForRestart();
            Serial.flush();
            ESP.restart();
        }
        sessionRecorder.stop();
        storageManager.prepareForRestart();
        displayManager.prepareForShutdown();
        enterShutdownSleep();
    }

    if (watchdog.isTriggered()) {
        eventLog.error("watchdog", "main loop deadline exceeded");
        Serial.println("WATCHDOG TRIGGERED! Restarting ESP32...");
        sessionRecorder.stop();
        storageManager.prepareForRestart();
        Serial.flush();
        ESP.restart();
    }
}
