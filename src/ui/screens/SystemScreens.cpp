#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "drivers/RadioManager.h"
#include "drivers/Cc1101Manager.h"
#include "services/PerformanceMonitor.h"
#include "services/SessionRecorder.h"
#include "services/StorageManager.h"
#include "services/SubGhzRawService.h"

using namespace DisplayUi;

namespace {
String formatStorageBytes(uint64_t bytes) {
    constexpr uint64_t KB = 1024ULL;
    constexpr uint64_t MB = KB * 1024ULL;
    constexpr uint64_t GB = MB * 1024ULL;
    if (bytes >= GB) return String(static_cast<unsigned long>(bytes / GB)) + "." +
        String(static_cast<unsigned long>((bytes % GB) * 10ULL / GB)) + " GB";
    if (bytes >= MB) return String(static_cast<unsigned long>(bytes / MB)) + "." +
        String(static_cast<unsigned long>((bytes % MB) * 10ULL / MB)) + " MB";
    if (bytes >= KB) return String(static_cast<unsigned long>(bytes / KB)) + " KB";
    return String(static_cast<unsigned long>(bytes)) + " B";
}
}

void DisplayManager::renderSettingsScreen() {
    if (!settingsLayoutDrawn) {
        drawModernHeader("APP SETTINGS", SPECTRUM_HIGH);
        drawModernFooter("U/D SEL", "A NEXT", "B BACK");
        settingsLayoutDrawn = true;
    }

    auto drawSettingRow = [&](int item, int y, const char* label,
                              const char* value, uint16_t valueColor) {
        const bool selected = settingsSelection == item;
        const uint16_t background = selected ?
                                    SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        tft.fillRect(5, y, 150, 16, ST77XX_BLACK);
        tft.fillRoundRect(5, y, 150, 16, 4, background);
        tft.drawRoundRect(5, y, 150, 16, 4,
                          selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER);
        if (selected) tft.fillRoundRect(8, y + 3, 3, 10, 1, SPECTRUM_ACCENT);
        tft.setCursor(15, y + 4);
        tft.setTextColor(selected ? SPECTRUM_ACCENT : ST77XX_GRAY,
                         background);
        tft.print(label);
        const int valueX = 148 - static_cast<int>(strlen(value)) * 6;
        tft.setCursor(valueX, y + 4);
        tft.setTextColor(valueColor, background);
        tft.print(value);
    };

    uint16_t pwrColor = ST77XX_WHITE;
    if (appState.powerLevel == RF24_PA_MAX) pwrColor = SPECTRUM_CRITICAL;
    else if (appState.powerLevel == RF24_PA_HIGH) pwrColor = SPECTRUM_HIGH;
    else if (appState.powerLevel == RF24_PA_LOW) pwrColor = SPECTRUM_MID;
    else pwrColor = SPECTRUM_LOW;
    const int theme = static_cast<int>(appState.displayTheme);
    const int layout = static_cast<int>(appState.menuLayout);
    const int sniffState = !storageManager.usingSd() ? -1 : (appState.saveSniffPacketsToSd ? 1 : 0);
    auto dirtyRow = [&](int row) {
        return previousSettingsSelection < 0 || previousSettingsTheme != theme ||
               previousSettingsSelection == row ||
               settingsSelection == row;
    };
    if (dirtyRow(0) || previousPowerLevel != static_cast<int>(appState.powerLevel))
        drawSettingRow(0, 17, "TX POWER", appState.getPowerLevelName(), pwrColor);
    if (dirtyRow(1) || previousDwellTimeUs != appState.dwellTimeUs)
        drawSettingRow(1, 34, "TX DWELL", appState.getDwellTimeName(), SPECTRUM_ACCENT);
    if (dirtyRow(2) || previousSettingsTheme != theme)
        drawSettingRow(2, 51, "DISPLAY THEME", appState.getDisplayThemeName(), SPECTRUM_ACCENT);
    if (dirtyRow(3) || previousSettingsMenuLayout != layout)
        drawSettingRow(3, 68, "MENU VIEW", appState.getMenuLayoutName(), SPECTRUM_LOW);
    const char* sniffSave = !storageManager.usingSd() ? "NO SD" :
                            (appState.saveSniffPacketsToSd ? "SD CARD" : "OFF");
    const uint16_t sniffColor = !storageManager.usingSd() ? SPECTRUM_CRITICAL :
                                  (appState.saveSniffPacketsToSd ? SPECTRUM_LOW : ST77XX_GRAY);
    if (dirtyRow(4) || previousSettingsSniffSave != sniffState)
        drawSettingRow(4, 85, "SNIFF TO SD", sniffSave, sniffColor);

    previousSettingsSelection = settingsSelection;
    previousPowerLevel = static_cast<int>(appState.powerLevel);
    previousDwellTimeUs = appState.dwellTimeUs;
    previousSettingsTheme = theme;
    previousSettingsMenuLayout = layout;
    previousSettingsSniffSave = sniffState;
}

// =============================================================================
// RENDER STATUS SCREEN (COMPACT & FIT)
// =============================================================================
void DisplayManager::renderStatusScreen() {
    static const char* pageTitles[] = {
        "DEVICE INFO", "MEMORY INFO", "RADIO / SW", "SUB-GHz STATUS",
        "PERFORMANCE", "SD CARD"
    };
    const char* labels[6];
    String values[6];
    uint16_t colors[6] = {
        ST77XX_WHITE, ST77XX_WHITE, ST77XX_WHITE,
        ST77XX_WHITE, ST77XX_WHITE, ST77XX_WHITE
    };

    if (statusPage == 0) {
        labels[0] = "CHIP";      values[0] = String(ESP.getChipModel());
        labels[1] = "REV/CORE";  values[1] = "r" + String(ESP.getChipRevision()) + " / " +
                                              String(ESP.getChipCores()) + " cores";
        labels[2] = "CPU";       values[2] = String(ESP.getCpuFreqMHz()) + " MHz";
        labels[3] = "FLASH";     values[3] = formatBytesShort(ESP.getFlashChipSize());
        labels[4] = "FLASH CLK"; values[4] = String(ESP.getFlashChipSpeed() / 1000000UL) + " MHz";
        labels[5] = "UPTIME";    values[5] = formatUptime(millis());
        colors[0] = SPECTRUM_ACCENT;
        colors[5] = SPECTRUM_LOW;
    } else if (statusPage == 1) {
        labels[0] = "HEAP TOTAL"; values[0] = formatBytesShort(ESP.getHeapSize());
        labels[1] = "HEAP FREE";  values[1] = formatBytesShort(ESP.getFreeHeap());
        labels[2] = "HEAP MIN";   values[2] = formatBytesShort(ESP.getMinFreeHeap());
        labels[3] = "MAX BLOCK";  values[3] = formatBytesShort(ESP.getMaxAllocHeap());
        labels[4] = "SKETCH";     values[4] = formatBytesShort(ESP.getSketchSize()) + " used";
        const uint32_t psramTotal = ESP.getPsramSize();
        labels[5] = "PSRAM";
        values[5] = psramTotal == 0 ? String("NOT PRESENT") :
                    formatBytesShort(ESP.getFreePsram()) + " free";
        colors[1] = SPECTRUM_LOW;
        colors[2] = SPECTRUM_HIGH;
        colors[5] = psramTotal == 0 ? ST77XX_GRAY : SPECTRUM_LOW;
    } else if (statusPage == 2) {
        const bool radio1Ok = radioManager.isRadio1Connected();
        const bool radio2Ok = radioManager.isRadio2Connected();
        labels[0] = "RADIO 1";   values[0] = appState.simulationMode ? "SIMULATED" : (radio1Ok ? "CONNECTED" : "NOT FOUND");
        labels[1] = "RADIO 2";   values[1] = appState.simulationMode ? "SIMULATED" : (radio2Ok ? "CONNECTED" : "NOT FOUND");
        labels[2] = "SCAN MODE"; values[2] = appState.getAnalyzerRadioModeName();
        labels[3] = "BUILD MODE"; values[3] = radioManager.transmitFeaturesEnabled() ? "RF LAB" : "RX ONLY";
        labels[4] = "ESP-IDF";    values[4] = ESP.getSdkVersion();
        labels[5] = "FIRMWARE";   values[5] = "v" APP_VERSION;
        colors[0] = appState.simulationMode ? SPECTRUM_HIGH : (radio1Ok ? SPECTRUM_LOW : SPECTRUM_CRITICAL);
        colors[1] = appState.simulationMode ? SPECTRUM_HIGH : (radio2Ok ? SPECTRUM_LOW : SPECTRUM_CRITICAL);
        colors[2] = SPECTRUM_ACCENT;
        colors[3] = SPECTRUM_HIGH;
    } else if (statusPage == 3) {
        const bool connected = cc1101Manager.isConnected();
        const bool simulated = subGhzRawService.simulationMode();
        labels[0] = "CC1101";
        values[0] = simulated ? "SIMULATED" : (connected ? "CONNECTED" : "NOT FOUND");
        labels[1] = "FREQUENCY";
        values[1] = String(cc1101Manager.frequencyMHz(), 2) + " MHz";
        labels[2] = "MODULATION";
        values[2] = cc1101Manager.presetName();
        labels[3] = "TX REGION";
        values[3] = subGhzRawService.regionName();
        labels[4] = "RAW RECORD";
        values[4] = subGhzRawService.isArmed() ? "ARMED" :
                    (subGhzRawService.isRecording() ?
                     String(subGhzRawService.pulseCount()) + " pulses" : "STOPPED");
        labels[5] = "ANALYZER";
        values[5] = subGhzRawService.analyzerRunning() ?
                    String(subGhzRawService.analyzerPeakFrequency(), 2) + " MHz" : "STOPPED";
        colors[0] = simulated ? SPECTRUM_HIGH :
                    (connected ? SPECTRUM_LOW : SPECTRUM_CRITICAL);
        colors[1] = SPECTRUM_ACCENT;
        colors[2] = SPECTRUM_MID;
        colors[3] = subGhzRawService.region() == SubGhzRegion::RX_ONLY ?
                    ST77XX_GRAY : SPECTRUM_HIGH;
        colors[4] = subGhzRawService.isRecording() ? SPECTRUM_CRITICAL : ST77XX_GRAY;
        colors[5] = subGhzRawService.analyzerRunning() ? SPECTRUM_LOW : ST77XX_GRAY;
    } else if (statusPage == 4) {
        const PerformanceSnapshot perf = performanceMonitor.snapshot();
        labels[0] = "SWEEP AVG"; values[0] = String(perf.averageSweepUs / 1000.0f, 1) + " ms";
        labels[1] = "SWEEP MAX"; values[1] = String(perf.maxSweepUs / 1000.0f, 1) + " ms";
        labels[2] = "UI AVG";    values[2] = String(perf.averageUiUs) + " us";
        labels[3] = "LOOP RATE"; values[3] = String(perf.loopsPerSecond) + " Hz";
        labels[4] = "SPI WAIT";  values[4] = String(radioManager.getAverageBusWaitUs()) + "/" +
                                               String(radioManager.getMaxBusWaitUs()) + " us";
        labels[5] = "SESSION";   values[5] = sessionRecorder.isRecording() ?
                                              String(sessionRecorder.recordedSweeps()) + " sweeps" : "STOPPED";
        colors[0] = SPECTRUM_ACCENT;
        colors[1] = SPECTRUM_HIGH;
        colors[2] = SPECTRUM_LOW;
        colors[4] = radioManager.getBusTimeouts() == 0 ? SPECTRUM_LOW : SPECTRUM_CRITICAL;
        colors[5] = sessionRecorder.isRecording() ? SPECTRUM_CRITICAL : ST77XX_GRAY;
    } else {
        const bool mounted = storageManager.usingSd();
        labels[0] = "STATUS";   values[0] = storageManager.sdStatus(); values[0].toUpperCase();
        labels[1] = "CARD TYPE"; values[1] = storageManager.sdTypeName();
        labels[2] = "CAPACITY"; values[2] = mounted ? formatStorageBytes(storageManager.sdTotalBytes()) : "--";
        labels[3] = "USED";     values[3] = mounted ? formatStorageBytes(storageManager.sdUsedBytes()) : "--";
        labels[4] = "FREE";     values[4] = mounted ? formatStorageBytes(storageManager.sdFreeBytes()) : "--";
        labels[5] = "RECORDER"; values[5] = storageManager.backendName();
        colors[0] = mounted ? SPECTRUM_LOW : SPECTRUM_CRITICAL;
        colors[1] = mounted ? SPECTRUM_ACCENT : ST77XX_GRAY;
        colors[2] = colors[3] = colors[4] = mounted ? ST77XX_WHITE : ST77XX_GRAY;
        colors[5] = mounted ? SPECTRUM_LOW : SPECTRUM_HIGH;
    }

    const bool layoutChanged = renderedStatusPage != statusPage;
    if (layoutChanged) {
        drawModernHeader(pageTitles[statusPage], SPECTRUM_LOW);
        tft.fillRoundRect(137, 2, 20, 10, 3, SPECTRUM_BORDER);
        tft.setCursor(139, 3);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_BORDER);
        tft.print(statusPage + 1);
        tft.print("/6");
        tft.fillRoundRect(5, 17, 150, 86, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 86, 4, SPECTRUM_BORDER);
        drawModernFooter("U/D PAGE", "A REF", "B BACK");
        for (int row = 0; row < 6; row++) previousStatusValues[row] = "";
    }

    for (int row = 0; row < 6; row++) {
        const int y = 21 + row * 13;
        if (layoutChanged) {
            if (row > 0) tft.drawFastHLine(11, y - 3, 138, SPECTRUM_GRID);
            tft.setCursor(11, y);
            tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
            tft.print(labels[row]);
        }
        if (layoutChanged || previousStatusValues[row] != values[row] ||
            previousStatusColors[row] != colors[row]) {
            tft.fillRect(76, y, 73, 8, SPECTRUM_CARD_BG);
            const int valueX = max(77, 149 - static_cast<int>(values[row].length()) * 6);
            tft.setCursor(valueX, y);
            tft.setTextColor(colors[row], SPECTRUM_CARD_BG);
            tft.print(values[row]);
            previousStatusValues[row] = values[row];
            previousStatusColors[row] = colors[row];
        }
    }

    renderedStatusPage = statusPage;
    lastStatusRenderMs = millis();
}

// =============================================================================
// POWER MENU
// =============================================================================
void DisplayManager::renderPowerScreen() {
    if (!powerLayoutDrawn) {
        drawModernHeader("POWER OPTIONS", SPECTRUM_HIGH);
        drawModernFooter("U/D SEL", "A OK", "B BACK");
        powerLayoutDrawn = true;
    }

    const char* titles[2] = {"RESTART", "SHUTDOWN"};
    const char* subtitles[2] = {"Reboot firmware", "Enter deep sleep"};
    for (int item = 0; item < 2; item++) {
        if (previousPowerSelection >= 0 && item != previousPowerSelection && item != powerSelection) continue;
        const int y = 22 + item * 39;
        const bool selected = powerSelection == item;
        const uint16_t background = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        const uint16_t accent = item == 0 ? SPECTRUM_ACCENT : SPECTRUM_CRITICAL;
        tft.fillRect(10, y, 140, 32, ST77XX_BLACK);
        tft.fillRoundRect(10, y, 140, 32, 5, background);
        tft.drawRoundRect(10, y, 140, 32, 5,
                          selected ? accent : SPECTRUM_BORDER);
        if (selected) tft.fillRoundRect(13, y + 5, 3, 22, 1, accent);
        tft.setCursor(22, y + 6);
        tft.setTextColor(selected ? ST77XX_WHITE : ST77XX_GRAY, background);
        tft.print(titles[item]);
        tft.setCursor(22, y + 18);
        tft.setTextColor(selected ? accent : ST77XX_GRAY, background);
        tft.print(subtitles[item]);
    }

    previousPowerSelection = powerSelection;
}

void DisplayManager::renderShutdownScreen() {
    if (!needRedraw) return;

    drawModernHeader("POWER OFF", SPECTRUM_CRITICAL);
    tft.fillRoundRect(14, 23, 132, 75, 7, SPECTRUM_CARD_BG);
    tft.drawRoundRect(14, 23, 132, 75, 7, SPECTRUM_BORDER);
    tft.drawCircle(80, 44, 10, SPECTRUM_CRITICAL);
    tft.fillRect(78, 30, 5, 14, SPECTRUM_CARD_BG);
    tft.drawFastVLine(80, 29, 14, SPECTRUM_CRITICAL);
    tft.setCursor(47, 62);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.print("SHUTTING DOWN");
    tft.setCursor(29, 78);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.print("Hold A to wake");
    tft.setCursor(45, 110);
    tft.setTextColor(SPECTRUM_ACCENT, ST77XX_BLACK);
    tft.print("DEEP SLEEP");
    needRedraw = false;
}

// =============================================================================
// RENDER REBOOT SCREEN (SYSTEM RESTART)
// =============================================================================
void DisplayManager::renderRebootScreen() {
    if (needRedraw) {
        drawModernHeader("SYSTEM RESTART", SPECTRUM_CRITICAL);
        tft.fillRoundRect(18, 25, 124, 70, 7, SPECTRUM_CARD_BG);
        tft.drawRoundRect(18, 25, 124, 70, 7, SPECTRUM_BORDER);
        tft.drawCircle(80, 45, 10, SPECTRUM_CRITICAL);
        tft.drawFastVLine(80, 32, 12, SPECTRUM_CRITICAL);
        tft.setCursor(47, 62);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print("RESTARTING");
        tft.setCursor(44, 78);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("PLEASE WAIT");
        tft.setCursor(42, 109);
        tft.setTextColor(SPECTRUM_ACCENT, ST77XX_BLACK);
        tft.print("RF24 SYSTEM");

        needRedraw = false;
    }
    String dotProgReboot = "REBOOTING";
    int dots = (millis() / 250) % 4;
    for (int d = 0; d < dots; d++) {
        dotProgReboot += ".";
    }
    int16_t dotProgRebootWidth = dotProgReboot.length() * 6;
    int16_t dotX = (160 - dotProgRebootWidth) / 2;
    tft.fillRect(30, 62, 100, 8, SPECTRUM_CARD_BG);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.setCursor(dotX, 62);
    tft.print(dotProgReboot);
}

// =============================================================================
// UPDATE UI DISPATCHER
// =============================================================================
