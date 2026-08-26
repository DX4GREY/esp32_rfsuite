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
#include "services/PerformanceMonitor.h"
#include "services/RfEnvironmentAnalyzer.h"
#include "services/RfAuthorizedProbe.h"
#include "services/LuaEngine.h"
#include "services/SubGhzRawService.h"

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
    while (true) delay(1000); // Defensive fallback; deep sleep does not return.
}

static bool quiesceForShutdown() {
    // Core 0 must have no application tasks using SPI/radios when ESP-IDF
    // stalls it as part of the deep-sleep transition.
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
    while (digitalRead(BTN_A) == LOW && millis() - started < WAKE_HOLD_MS) {
        delay(10);
    }

    if (millis() - started < WAKE_HOLD_MS) {
        // A short/noisy press must not fully boot the device.
        while (digitalRead(BTN_A) == LOW) delay(10);
        delay(50);
        enterShutdownSleep();
    }

    // Avoid treating the wake gesture as an immediate menu ENTER press.
    while (digitalRead(BTN_A) == LOW) delay(10);
    delay(50);
}

// Callback to keep buttons & UI responsive during scanning
void yieldToUI() {
    displayManager.processInput();
    displayManager.updateUI();
    watchdog.feed();
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
    // Deep-sleep wake is accepted only after a deliberate long A press.
    validateShutdownWakePress();

        // 1. Initialize Serial CLI (115200 Baud)
    serialCommander.init(115200);

    // 1b. Load persisted settings (power level, dwell time, jammer target)
    appState.loadSettings();

    // 2. Initialize Navigation Buttons (Pull-Up)
    buttonManager.init();

    // 3. Initialize TFT ST7735 1.8" Display (160x128 Compact)
    displayManager.init();

    // 3b. Show splash screen before entering the menu
    displayManager.showSplash();

    // 3c. Mount storage after TFT initialization. Both devices use the same
    // HSPI controller and shared SCK/MOSI lines with independent CS pins.
    if (!sessionRecorder.begin()) {
        Serial.println("Session recorder unavailable: " + String(sessionRecorder.lastError()));
    }
    if (!luaEngine.begin()) Serial.println("Lua: " + String(luaEngine.lastError()));

    // 4. Initialize nRF24L01+ Radio (init() retries internally before failing)
    // Deselect the optional CC1101 before starting the shared RF SPI bus so an
    // uninitialized module can never drive MISO during nRF24 discovery.
    pinMode(CC1101_CSN_PIN, OUTPUT);
    digitalWrite(CC1101_CSN_PIN, HIGH);
    if (!radioManager.init()) {
        // Keep the UI, status, storage, and Serial diagnostics available. A
        // disconnected module can then be diagnosed without a reboot loop.
        Serial.println("No radio detected; continuing in diagnostics-only mode.");
    }

    if (!cc1101Manager.init()) {
        Serial.println("CC1101 unavailable: " + String(cc1101Manager.lastError()));
    }
    cc1101Manager.setPreset(static_cast<Cc1101Preset>(appState.subGhzRadioPreset));
    subGhzRawService.setRegion(static_cast<SubGhzRegion>(appState.subGhzRegion));
    subGhzRawService.setAutoTrigger(appState.subGhzAutoTrigger);
    subGhzRawService.setTriggerThreshold(appState.subGhzTriggerThreshold);
    subGhzRawService.setReplayRepeatCount(appState.subGhzReplayRepeats);

    // 5. Initialize the main-loop deadline monitor (3.0s timeout). Avoid a
    // second Timer Group ISR alongside ESP-IDF's interrupt/task watchdogs.
    watchdog.init(WATCHDOG_TIMEOUT_US);
}

// =============================================================================
// MAIN LOOP (CORE 1: UI, SERIAL, & SPECTRUM DISPATCHER)
// =============================================================================
void loop() {
    performanceMonitor.tickLoop();
    subGhzRawService.service();
    rfEnvironmentAnalyzer.service();
    // 1. Reset Watchdog Timer (Heartbeat)
    watchdog.feed();

    // 2. Process Serial Monitor commands if any
    serialCommander.process();

    // 3. Process Physical Button Input
    displayManager.processInput();

    // 4. Execute Based on Active Mode
    if (AppModePolicy::runsSpectrumScan(appState.appMode,
                                        appState.loggingEnabled) &&
        !appState.analyzerFrozen) {
        // Radio Analyzer Mode: Scan 126 channels and update spectrum levels
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
        // Channel Inspector Mode: Deep RF monitoring on a single channel
        // (no per-loop requestRedraw => only dynamic areas are updated,
        //  eliminating flicker from repeated fillScreen)
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
        // Reboot Mode: screen is rendered in updateUI, restart is briefly delayed
        delay(10);
    } else {
        // Jammer Mode runs on Core 0 background task, Core 1 idle
        delay(10);
    }

    // 5. Render TFT screen if there are updates
    const uint32_t uiStartedUs = micros();
    displayManager.updateUI();
    performanceMonitor.recordUi(micros() - uiStartedUs);
    sessionRecorder.service();
    if (appState.loggingEnabled && !sessionRecorder.isRecording()) {
        appState.loggingEnabled = false;
        displayManager.requestRedraw();
    }
    appState.serviceSettingsPersistence();

    // 5b. Reboot System: show message then restart ESP32
    if (appState.appMode == APP_MODE_REBOOT) {
        delay(1200); // give the reboot message time to be visible on screen
        Serial.println("REBOOTING SYSTEM...");
        ESP.restart();
    }

    if (appState.appMode == APP_MODE_SHUTDOWN) {
        delay(900); // Keep the shutdown confirmation visible briefly.
        while (digitalRead(BTN_A) == LOW) delay(10);
        delay(50);
        Serial.println("SYSTEM SHUTDOWN: entering deep sleep...");
        if (!quiesceForShutdown()) {
            // A live Core 0 task makes deep sleep unsafe. A clean software
            // restart is preferable to corrupting IDLE0 and entering a panic
            // loop; the shutdown request is not persisted across reboot.
            Serial.println("SYSTEM SHUTDOWN: task stop timed out; restarting safely");
            Serial.flush();
            ESP.restart();
        }
        displayManager.prepareForShutdown();
        enterShutdownSleep();
    }

    // 6. Auto-recovery on Watchdog Timeout
    if (watchdog.isTriggered()) {
        Serial.println("WATCHDOG TRIGGERED! Restarting ESP32...");
        ESP.restart();
    }
}
