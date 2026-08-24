#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "ui/MenuCatalog.h"
#include "drivers/RadioManager.h"
#include "drivers/Cc1101Manager.h"
#include "services/SubGhzRawService.h"
#include "services/StorageManager.h"

using namespace DisplayUi;

namespace {
constexpr const char* MAIN_LABELS[] = {
    "2.4 GHz", "SUB-GHz", "SETTINGS", "SYS INFO", "LUA", "SD FILES", "POWER"
};
constexpr uint8_t MAIN_ICONS[] = {6, 6, 9, 10, 9, 12, 11};
constexpr const char* SUB_LABELS[] = {
    "ANALYZER", "RECORD", "LIBRARY", "PRESETS", "PACKETS", "RF TEST"
};
constexpr uint8_t SUB_ICONS[] = {0, 5, 12, 8, 7, 6};
}

void DisplayManager::drawMainMenuItem(int featureIndex, int slot, bool selected) {
    const bool list = appState.menuLayout == MENU_LAYOUT_LIST;
    const int visibleRow = slot - mainMenuScrollOffset;
    if (list && (visibleRow < 0 || visibleRow >= 4)) return;
    const int x = list ? 3 : 4 + (slot % 2) * 78;
    const int y = list ? 16 + visibleRow * 22 : 16 + (slot / 2) * 29;
    const int width = list ? 154 : 74;
    const int height = list ? 20 : 26;
    const uint16_t bg = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
    const uint16_t edge = selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER;
    tft.fillRect(x, y, width, height, ST77XX_BLACK);
    tft.fillRoundRect(x, y, width, height, 4, bg);
    tft.drawRoundRect(x, y, width, height, 4, edge);
    if (selected) tft.fillRoundRect(x + 2, y + (list ? 3 : 5), 3, list ? 14 : 17, 1,
                                    SPECTRUM_ACCENT);
    drawMenuIcon(MAIN_ICONS[featureIndex], list ? x + 20 : x + width / 2,
                 list ? y + 10 : y + 8, selected ? SPECTRUM_ACCENT : ST77XX_GRAY, bg);
    const int labelX = list ? x + 39 : x + (width - static_cast<int>(strlen(MAIN_LABELS[featureIndex])) * 6) / 2;
    tft.setCursor(labelX, list ? y + 7 : y + 17);
    tft.setTextColor(selected ? ST77XX_WHITE : ST77XX_GRAY, bg);
    tft.print(MAIN_LABELS[featureIndex]);
}

void DisplayManager::renderBandSelector() {
    const int page = bandSelection / 6;
    drawModernHeader("MAIN MENU", SPECTRUM_ACCENT);
    tft.fillRoundRect(137, 2, 20, 10, 3, SPECTRUM_BORDER);
    tft.setCursor(139, 3); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_BORDER);
    tft.print(page + 1); tft.print("/2");
    tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
    const int count = page == 0 ? 6 : 1;
    const int firstSlot = appState.menuLayout == MENU_LAYOUT_LIST ? mainMenuScrollOffset : 0;
    const int lastSlot = appState.menuLayout == MENU_LAYOUT_LIST ? min(count, firstSlot + 4) : count;
    for (int slot = firstSlot; slot < lastSlot; ++slot) {
        const int feature = page * 6 + slot;
        drawMainMenuItem(feature, slot, feature == bandSelection);
    }
    drawModernFooter("U/D MOVE", "B PAGE", "A OPEN");
}

void DisplayManager::redrawMainMenuItems() {
    const int page = bandSelection / 6;
    if (appState.menuLayout == MENU_LAYOUT_LIST &&
        previousMainMenuScrollOffset != mainMenuScrollOffset) {
        tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        const int count = page == 0 ? 6 : 1;
        for (int slot = mainMenuScrollOffset; slot < min(count, mainMenuScrollOffset + 4); ++slot)
            drawMainMenuItem(page * 6 + slot, slot, page * 6 + slot == bandSelection);
    } else {
        drawMainMenuItem(previousBandSelection, previousBandSelection % 6, false);
        drawMainMenuItem(bandSelection, bandSelection % 6, true);
    }
}

void DisplayManager::drawSubGhzMenuItem(int index, bool selected) {
    const bool list = appState.menuLayout == MENU_LAYOUT_LIST;
    const int visibleRow = index - subGhzMenuScrollOffset;
    if (list && (visibleRow < 0 || visibleRow >= 4)) return;
    const int x = list ? 3 : 4 + (index % 2) * 78;
    const int y = list ? 16 + visibleRow * 22 : 16 + (index / 2) * 29;
    const int width = list ? 154 : 74;
    const int height = list ? 20 : 26;
    const uint16_t bg = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
    const uint16_t edge = selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER;
    tft.fillRect(x, y, width, height, ST77XX_BLACK);
    tft.fillRoundRect(x, y, width, height, 4, bg);
    tft.drawRoundRect(x, y, width, height, 4, edge);
    if (selected) tft.fillRoundRect(x + 2, y + (list ? 3 : 5), 3, list ? 14 : 17, 1,
                                    SPECTRUM_ACCENT);
    drawMenuIcon(SUB_ICONS[index], list ? x + 20 : x + width / 2,
                 list ? y + 10 : y + 8, selected ? SPECTRUM_ACCENT : ST77XX_GRAY, bg);
    const int labelX = list ? x + 39 : x + (width - static_cast<int>(strlen(SUB_LABELS[index])) * 6) / 2;
    tft.setCursor(labelX, list ? y + 7 : y + 17);
    tft.setTextColor(selected ? ST77XX_WHITE : ST77XX_GRAY, bg);
    tft.print(SUB_LABELS[index]);
}

void DisplayManager::renderSubGhzScreen() {
    drawModernHeader(subGhzRawService.simulationMode() ? "SUB-GHz [SIM]" : "SUB-GHz", SPECTRUM_HIGH);
    tft.fillRoundRect(137, 2, 20, 10, 3, SPECTRUM_BORDER);
    tft.setCursor(139, 3); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_BORDER); tft.print("1/1");
    tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
    for (int i = 0; i < 6; ++i) drawSubGhzMenuItem(i, i == subGhzMenuSelection);
    drawModernFooter("U/D MOVE", "B MAIN", "A OPEN");
}

void DisplayManager::renderSubGhzOfflinePopup() {
    drawModernHeader("CC1101 OFFLINE", SPECTRUM_CRITICAL);
    tft.fillRoundRect(6, 20, 148, 70, 5, SPECTRUM_CARD_BG);
    tft.drawRoundRect(6, 20, 148, 70, 5, SPECTRUM_CRITICAL);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.setCursor(14, 29); tft.print("CC1101 NOT");
    tft.setCursor(14, 41); tft.print("DETECTED");
    tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
    tft.setCursor(14, 58); tft.print("Use simulation?");
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.setCursor(14, 73); tft.print("NOT REAL RF DATA");
    drawModernFooter("", "A SIMULATE", "B CANCEL");
}

void DisplayManager::redrawSubGhzMenuItems() {
    if (appState.menuLayout == MENU_LAYOUT_LIST &&
        previousSubGhzMenuScrollOffset != subGhzMenuScrollOffset) {
        tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        for (int i = subGhzMenuScrollOffset; i < min(6, subGhzMenuScrollOffset + 4); ++i)
            drawSubGhzMenuItem(i, i == subGhzMenuSelection);
    } else {
        drawSubGhzMenuItem(previousSubGhzMenuSelection, false);
        drawSubGhzMenuItem(subGhzMenuSelection, true);
    }
}

void DisplayManager::renderSubGhzAnalyzerScreen() {
    if (!subAnalyzerLayoutDrawn) {
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM ANALYZER" : "FREQUENCY ANALYZER", SPECTRUM_ACCENT);
        tft.fillRoundRect(4, 18, 152, 72, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(4, 18, 152, 72, 4, SPECTRUM_BORDER);
        tft.drawFastHLine(7, 74, 146, SPECTRUM_GRID);
        tft.setCursor(7, 94); tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK); tft.print("PEAK");
        drawModernFooter("", "A LOCK", "B BACK");
        subAnalyzerLayoutDrawn = true;
    }
    for (uint8_t i = 0; i < subGhzRawService.analyzerCount(); ++i) {
        const int16_t rssi = subGhzRawService.analyzerRssi(i);
        if (previousSubAnalyzerLevels[i] == rssi) continue;
        const int x = 8 + i * 7;
        const int height = constrain(map(rssi, -110, -30, 0, 52), 0, 52);
        tft.fillRect(x, 21, 5, 53, SPECTRUM_CARD_BG);
        if (height) tft.fillRect(x, 74 - height, 5, height,
            getSignalColor(constrain(map(rssi, -110, -30, 0, 100), 0, 100)));
        previousSubAnalyzerLevels[i] = rssi;
    }
    tft.fillRect(36, 92, 118, 12, ST77XX_BLACK);
    tft.setCursor(38, 94); tft.setTextColor(SPECTRUM_ACCENT, ST77XX_BLACK);
    tft.printf("%.2fMHz %ddBm", subGhzRawService.analyzerPeakFrequency(),
               subGhzRawService.analyzerPeakRssi());
}

void DisplayManager::renderSubGhzPresetsScreen() {
    drawModernHeader(subGhzRawService.simulationMode() ? "SIM PRESETS" : "SUB-GHz PRESETS", SPECTRUM_HIGH);
    tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
    tft.fillRoundRect(6, 21, 148, 68, 5, SPECTRUM_CARD_BG);
    tft.drawRoundRect(6, 21, 148, 68, 5, SPECTRUM_BORDER);
    tft.setCursor(12, 29); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG); tft.print("MODULATION");
    tft.setCursor(12, 43); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
    tft.print(cc1101Manager.presetName());
    tft.setCursor(12, 61); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG); tft.print("TX REGION");
    tft.setCursor(80, 61);
    tft.setTextColor(subGhzRawService.region() == SubGhzRegion::RX_ONLY ?
                     SPECTRUM_CRITICAL : SPECTRUM_LOW, SPECTRUM_CARD_BG);
    tft.print(subGhzRawService.regionName());
    tft.setCursor(12, 76); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.printf("TRG %d  REPLAY %uX", subGhzRawService.triggerThresholdDbm(),
               subGhzRawService.replayRepeatCount());
    drawModernFooter("U/D PRE", "A REGION", "HOLD B REP");
}

void DisplayManager::renderSubGhzPacketScreen() {
    if (!subPacketLayoutDrawn) {
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM PACKETS" : "PACKET ANALYZER", SPECTRUM_ACCENT);
        tft.fillRoundRect(5, 18, 150, 77, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 18, 150, 77, 4, SPECTRUM_BORDER);
        tft.setCursor(10, 24); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG); tft.print("FREQ");
        tft.setCursor(10, 41); tft.print("PACKETS");
        tft.setCursor(10, 58); tft.print("RSSI/LQI");
        tft.setCursor(10, 75); tft.print("DATA");
        drawModernFooter("U/D FREQ", "A CLEAR", "B BACK");
        subPacketLayoutDrawn = true;
    }
    if (needRedraw || previousSubPacketCount != subGhzRawService.packetCount()) {
        tft.fillRect(57, 22, 94, 70, SPECTRUM_CARD_BG);
        tft.setCursor(59, 24); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.printf("%.2f MHz", cc1101Manager.frequencyMHz());
        tft.setCursor(59, 41); tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print(subGhzRawService.packetCount());
        tft.setCursor(59, 58);
        tft.printf("%d/%u %s", subGhzRawService.lastPacketRssi(),
                   subGhzRawService.lastPacketLqi(),
                   subGhzRawService.lastPacketCrcOk() ? "CRC" : "---");
        tft.setCursor(59, 75); tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
        const uint8_t shown = min<uint8_t>(8, subGhzRawService.lastPacketLength());
        for (uint8_t i = 0; i < shown; ++i) tft.printf("%02X", subGhzRawService.lastPacketData()[i]);
        previousSubPacketCount = subGhzRawService.packetCount();
    }
}

void DisplayManager::renderSubGhzRecordScreen() {
    if (!subRecordLayoutDrawn) {
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM RECORD" : "SUB-GHz RECORD", SPECTRUM_CRITICAL);
        tft.fillRoundRect(5, 17, 150, 22, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 22, 4, SPECTRUM_BORDER);
        tft.setCursor(9, 24); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG); tft.print("FREQ");
        tft.fillRoundRect(5, 42, 150, 49, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 42, 150, 49, 4, SPECTRUM_BORDER);
        tft.drawFastHLine(8, 65, 144, SPECTRUM_GRID);
        tft.setCursor(8, 95); tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK); tft.print("P/R/B");
        subRecordLayoutDrawn = true;
    }
    const int frequencyKhz = static_cast<int>(cc1101Manager.frequencyMHz() * 1000.0f);
    if (needRedraw || previousSubFrequencyKhz != frequencyKhz) {
        tft.fillRect(36, 21, 66, 14, SPECTRUM_CARD_BG);
        tft.setCursor(38, 24); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.printf("%.2f", cc1101Manager.frequencyMHz());
        previousSubFrequencyKhz = frequencyKhz;
    }
    const int recordingState = subGhzRawService.isArmed() ? 2 :
                               (subGhzRawService.isRecording() ? 1 : 0);
    if (needRedraw || previousSubRecordState != recordingState) {
        tft.fillRect(101, 21, 50, 14, SPECTRUM_CARD_BG);
        tft.setCursor(105, 24);
        tft.setTextColor(recordingState ? SPECTRUM_CRITICAL : SPECTRUM_LOW, SPECTRUM_CARD_BG);
        tft.print(recordingState == 2 ? "ARM" : (recordingState == 1 ? "REC" : "READY"));
        drawModernFooter("U/D FREQ", subGhzRawService.isRecording() ? "A STOP" : "A START", "B BACK");
        if (recordingState == 1) {
            tft.fillRect(8, 45, 144, 42, SPECTRUM_CARD_BG);
            tft.drawFastHLine(8, 65, 144, SPECTRUM_GRID);
            subGraphProcessedPulses = 0; subGraphX = 8; subGraphInitialized = false;
        }
        previousSubRecordState = recordingState;
    }
    const uint32_t pulses = subGhzRawService.pulseCount();
    if (needRedraw || previousSubPulseCount != pulses || subGhzRawService.isRecording()) {
        tft.fillRect(38, 93, 116, 11, ST77XX_BLACK);
        tft.setCursor(40, 95); tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
        if (!subGhzRawService.isRecording() && subGhzRawService.decodedBitCount())
            tft.printf("%s %ub TE%u", subGhzRawService.detectedProtocol(),
                       subGhzRawService.decodedBitCount(), subGhzRawService.estimatedTeUs());
        else if (subGhzRawService.isRecording() && ((millis() / 1000) & 1U))
            tft.printf("PK%d NF%d %u/s", subGhzRawService.peakRssiDbm(),
                       subGhzRawService.noiseFloorDbm(), subGhzRawService.pulsesPerSecond());
        else
            tft.printf("%lu/%d/%u%%", static_cast<unsigned long>(pulses),
                       subGhzRawService.liveRssiDbm(), subGhzRawService.bufferPercent());
        previousSubPulseCount = pulses;
    }

    // Incremental raw waveform: only newly captured pulse segments are drawn.
    // The graph wraps locally without clearing the header, counters, or footer.
    if (pulses < subGraphProcessedPulses) {
        subGraphProcessedPulses = 0; subGraphX = 8; subGraphInitialized = false;
    }
    if (pulses - subGraphProcessedPulses > 256) {
        subGraphProcessedPulses = pulses - 256;
        subGraphX = 8; subGraphInitialized = false;
        tft.fillRect(8, 45, 144, 42, SPECTRUM_CARD_BG);
        tft.drawFastHLine(8, 65, 144, SPECTRUM_GRID);
    }
    uint32_t pulseDurations[256];
    uint8_t levelAtStart = 0;
    const size_t newCount = subGhzRawService.copyPulses(
        subGraphProcessedPulses, pulseDurations, 256, levelAtStart);
    if (!subGraphInitialized && newCount) {
        subGraphLevel = levelAtStart != 0;
        subGraphInitialized = true;
    }
    for (size_t i = 0; i < newCount; ++i) {
        const int width = constrain(static_cast<int>((pulseDurations[i] + 99) / 100), 1, 24);
        if (subGraphX + width > 152) {
            tft.fillRect(8, 45, 144, 42, SPECTRUM_CARD_BG);
            tft.drawFastHLine(8, 65, 144, SPECTRUM_GRID);
            subGraphX = 8;
        }
        const int y0 = subGraphLevel ? 49 : 83;
        const int y1 = subGraphLevel ? 83 : 49;
        tft.drawFastHLine(subGraphX, y0, width, SPECTRUM_ACCENT);
        tft.drawFastVLine(subGraphX + width - 1, min(y0, y1), abs(y1 - y0) + 1,
                          SPECTRUM_ACCENT);
        subGraphX += width;
        subGraphLevel = !subGraphLevel;
    }
    subGraphProcessedPulses += newCount;
}

void DisplayManager::renderSubGhzEmulateScreen() {
    drawModernHeader(subGhzDeleteArmed ? "DELETE? A YES / B NO" :
                     (subGhzRawService.simulationMode() ? "SIM LIBRARY" : "SUB-GHz LIBRARY"), SPECTRUM_HIGH);
    tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
    if (!subGhzFileCount) {
        tft.setCursor(31, 48); tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
        tft.print("NO RAW RECORDINGS");
    } else {
        for (size_t i = subGhzFileScrollOffset;
             i < min(subGhzFileCount, subGhzFileScrollOffset + 4); ++i)
            drawSubGhzFileItem(i, i == subGhzFileSelection);
    }
    drawModernFooter("U/D FILE", subGhzDeleteArmed ? "A DELETE" :
#if RF_LAB_TX_ENABLED
                     "A REPLAY",
#else
                     "A LOCK",
#endif
                     "B BACK");
}

void DisplayManager::drawSubGhzFileItem(size_t index, bool selected) {
    if (index < subGhzFileScrollOffset || index >= subGhzFileScrollOffset + 4) return;
    const int y = 16 + static_cast<int>(index - subGhzFileScrollOffset) * 22;
    const uint16_t bg = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
    tft.fillRect(3, y, 154, 20, ST77XX_BLACK);
    tft.fillRoundRect(3, y, 154, 20, 4, bg);
    tft.drawRoundRect(3, y, 154, 20, 4,
                      selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER);
    if (selected) tft.fillRoundRect(5, y + 3, 3, 14, 1, SPECTRUM_ACCENT);
    const bool favorite = subGhzRawService.isFavorite(subGhzFiles[index]);
    tft.setCursor(13, y + 7);
    tft.setTextColor(favorite ? SPECTRUM_HIGH : ST77XX_GRAY, bg);
    tft.print(favorite ? "*" : " ");
    String shown = subGhzFiles[index]; if (shown.length() > 19) shown.remove(19);
    tft.setCursor(21, y + 7);
    tft.setTextColor(selected ? ST77XX_WHITE : ST77XX_GRAY, bg);
    tft.print(shown);
}

void DisplayManager::redrawSubGhzFileItems() {
    if (previousSubGhzFileScrollOffset != subGhzFileScrollOffset) {
        tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        for (size_t i = subGhzFileScrollOffset;
             i < min(subGhzFileCount, subGhzFileScrollOffset + 4); ++i)
            drawSubGhzFileItem(i, i == subGhzFileSelection);
    } else {
        drawSubGhzFileItem(previousSubGhzFileSelection, false);
        drawSubGhzFileItem(subGhzFileSelection, true);
    }
}

void DisplayManager::renderSubGhzRfTestScreen() {
    if (!subRfTestLayoutDrawn) {
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM: RF TEST OFF" : "CC1101 RF TEST", SPECTRUM_CRITICAL);
        tft.fillRoundRect(8, 24, 144, 65, 5, SPECTRUM_CARD_BG);
        tft.drawRoundRect(8, 24, 144, 65, 5, SPECTRUM_BORDER);
#if !RF_LAB_TX_ENABLED
        tft.setCursor(20, 43); tft.setTextColor(SPECTRUM_CRITICAL, SPECTRUM_CARD_BG);
        tft.print("RF LAB BUILD REQUIRED");
        tft.setCursor(25, 62); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("Transmission locked");
#else
        tft.setCursor(22, 73); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("REGION "); tft.print(subGhzRawService.regionName());
#endif
        subRfTestLayoutDrawn = true;
    }
#if RF_LAB_TX_ENABLED
    const int testing = subGhzRawService.isRfTesting() ? 1 : 0;
    if (needRedraw || previousSubTestState != testing) {
        tft.fillRect(18, 32, 126, 13, SPECTRUM_CARD_BG);
        tft.setCursor(21, 34);
        tft.setTextColor(testing ? SPECTRUM_CRITICAL : SPECTRUM_LOW, SPECTRUM_CARD_BG);
        if (testing) tft.print("SWEEP TRANSMITTING");
        else if (!strcmp(subGhzRawService.lastError(), "OK") ||
                 !strcmp(subGhzRawService.lastError(), "RF TEST STOPPED"))
            tft.print("READY / STANDBY");
        else {
            String status = subGhzRawService.lastError(); if (status.length() > 20) status.remove(20);
            tft.print(status);
        }
        drawModernFooter("", testing ? "A STOP" : "A START", "B BACK");
        previousSubTestState = testing;
    }
    const int frequencyKhz = static_cast<int>(subGhzRawService.testFrequencyMHz() * 1000.0f);
    if (needRedraw || previousSubFrequencyKhz != frequencyKhz) {
        tft.fillRect(32, 54, 96, 13, SPECTRUM_CARD_BG);
        tft.setCursor(36, 56); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.printf("%.2f MHz", subGhzRawService.testFrequencyMHz());
        previousSubFrequencyKhz = frequencyKhz;
    }
#else
    if (needRedraw) drawModernFooter("", "A LOCK", "B BACK");
#endif
}

void DisplayManager::drawMenuIcon(int index, int centerX, int centerY,
                                  uint16_t color, uint16_t background) {
    switch (index) {
        case 0: // Spectrum bars
            tft.drawFastVLine(centerX - 7, centerY + 1, 5, color);
            tft.drawFastVLine(centerX - 3, centerY - 3, 9, color);
            tft.drawFastVLine(centerX + 1, centerY - 6, 12, color);
            tft.drawFastVLine(centerX + 5, centerY - 1, 7, color);
            tft.drawFastHLine(centerX - 9, centerY + 6, 18, color);
            break;
        case 1: // Waterfall/history
            for (int row = 0; row < 4; row++) {
                tft.drawFastHLine(centerX - 8 + row, centerY - 6 + row * 4,
                                  16 - row * 2, color);
            }
            break;
        case 2: // Magnifier
            tft.drawCircle(centerX - 2, centerY - 2, 6, color);
            tft.drawLine(centerX + 3, centerY + 3, centerX + 8, centerY + 8, color);
            tft.fillCircle(centerX - 2, centerY - 2, 1, color);
            break;
        case 3: // Survey chart
            tft.drawFastHLine(centerX - 9, centerY + 6, 18, color);
            tft.fillRect(centerX - 7, centerY, 3, 6, color);
            tft.fillRect(centerX - 2, centerY - 4, 3, 10, color);
            tft.fillRect(centerX + 3, centerY - 1, 3, 7, color);
            break;
        case 4: // Event marker
            tft.drawCircle(centerX, centerY, 7, color);
            tft.drawLine(centerX, centerY - 5, centerX - 2, centerY + 1, color);
            tft.drawLine(centerX - 2, centerY + 1, centerX + 3, centerY + 1, color);
            tft.drawFastVLine(centerX + 3, centerY + 1, 4, color);
            break;
        case 5: // Recording/logging
            tft.drawRoundRect(centerX - 9, centerY - 7, 18, 14, 3, color);
            tft.fillCircle(centerX, centerY, 4, color);
            break;
        case 6: // RF antenna
            tft.drawFastVLine(centerX, centerY - 4, 10, color);
            tft.fillCircle(centerX, centerY - 5, 2, color);
            tft.drawLine(centerX - 3, centerY + 5, centerX + 3, centerY + 5, color);
            tft.drawLine(centerX - 5, centerY - 3, centerX - 8, centerY, color);
            tft.drawLine(centerX + 5, centerY - 3, centerX + 8, centerY, color);
            break;
        case 7: // Dual-radio diagnostics
            tft.drawRoundRect(centerX - 9, centerY - 6, 7, 12, 2, color);
            tft.drawRoundRect(centerX + 2, centerY - 6, 7, 12, 2, color);
            tft.fillCircle(centerX - 6, centerY + 3, 1, color);
            tft.fillCircle(centerX + 5, centerY + 3, 1, color);
            break;
        case 8: // Profiles
            tft.drawRoundRect(centerX - 9, centerY - 7, 18, 5, 2, color);
            tft.drawRoundRect(centerX - 7, centerY, 14, 5, 2, color);
            tft.drawRoundRect(centerX - 5, centerY + 7, 10, 3, 1, color);
            break;
        case 9: // Settings sliders
            tft.drawFastHLine(centerX - 9, centerY - 5, 18, color);
            tft.drawFastHLine(centerX - 9, centerY, 18, color);
            tft.drawFastHLine(centerX - 9, centerY + 5, 18, color);
            tft.fillCircle(centerX - 3, centerY - 5, 2, background);
            tft.drawCircle(centerX - 3, centerY - 5, 2, color);
            tft.fillCircle(centerX + 4, centerY, 2, background);
            tft.drawCircle(centerX + 4, centerY, 2, color);
            tft.fillCircle(centerX, centerY + 5, 2, background);
            tft.drawCircle(centerX, centerY + 5, 2, color);
            break;
        case 10: // Device status
            tft.drawRoundRect(centerX - 8, centerY - 7, 16, 14, 3, color);
            tft.fillCircle(centerX, centerY - 3, 1, color);
            tft.drawFastVLine(centerX, centerY, 4, color);
            break;
        case 11: // Power/reboot
            tft.drawCircle(centerX, centerY, 7, color);
            tft.fillRect(centerX - 2, centerY - 8, 5, 7, background);
            tft.drawFastVLine(centerX, centerY - 8, 9, color);
            break;
        case 12: // Folder / SD file explorer
            tft.drawRoundRect(centerX - 9, centerY - 5, 18, 12, 2, color);
            tft.fillRect(centerX - 7, centerY - 8, 8, 4, color);
            tft.drawFastHLine(centerX - 6, centerY, 12, color);
            break;
    }
}

void DisplayManager::drawMenuItem(int index, bool selected) {
    const int featureIndex = MenuCatalog::featureIndex(menuPage, index);
    const MenuFeature& feature = MenuCatalog::featureAt(menuPage, index);
    const bool list = appState.menuLayout == MENU_LAYOUT_LIST;
    const int visibleRow = index - menuScrollOffset;
    if (list && (visibleRow < 0 || visibleRow >= 4)) return;
    const int cardWidth = list ? (selected ? 150 : 138) : 74;
    const int cardHeight = list ? 20 : 27;
    const int x = list ? (selected ? 5 : 17) : 4 + (index % 2) * 78;
    const int y = list ? 16 + visibleRow * 22 : 16 + (index / 2) * 29;
    const uint16_t background = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
    const uint16_t border = selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER;
    const uint16_t iconColor = selected ? SPECTRUM_ACCENT : ST77XX_GRAY;

    // Clear only this card's dirty rectangle before rebuilding it.
    if (list) tft.fillRect(3, y, 154, cardHeight, ST77XX_BLACK);
    else tft.fillRect(x, y, cardWidth, cardHeight, ST77XX_BLACK);
    tft.fillRoundRect(x, y, cardWidth, cardHeight, 4, background);
    tft.drawRoundRect(x, y, cardWidth, cardHeight, 4, border);
    if (selected) tft.fillRoundRect(x + 2, y + (list ? 3 : 5), 3,
                                    list ? 14 : 17, 1, SPECTRUM_ACCENT);

    drawMenuIcon(feature.iconId, list ? x + 20 : x + cardWidth / 2,
                 list ? y + 10 : y + 8,
                 iconColor, background);

    const int labelX = list ? x + 39 : x +
        (cardWidth - static_cast<int>(strlen(feature.label)) * 6) / 2;
    tft.setCursor(labelX, list ? y + 7 : y + 17);
    tft.setTextColor(selected ? ST77XX_WHITE : ST77XX_GRAY, background);
    tft.print(feature.label);

    if (featureIndex == 4 && appState.eventCount > 0) {
        tft.fillCircle(x + cardWidth - 8, y + 7, 5, SPECTRUM_HIGH);
        tft.setCursor(x + cardWidth - 11, y + 4);
        tft.setTextColor(ST77XX_BLACK, SPECTRUM_HIGH);
        tft.print(appState.eventCount);
    } else if (featureIndex == 5 && appState.loggingEnabled) {
        tft.fillCircle(x + cardWidth - 8, y + 7, 4, SPECTRUM_CRITICAL);
    }
}

// =============================================================================
// PARTIAL MENU REDRAW (only the two affected items)
// =============================================================================
void DisplayManager::redrawMenuItems(int oldSel, int newSel) {
    if (appState.menuLayout == MENU_LAYOUT_LIST &&
        prevMenuScrollOffset != menuScrollOffset) {
        tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
        const int count = MenuCatalog::pageItemCount(menuPage);
        for (int i = menuScrollOffset; i < min(count, menuScrollOffset + 4); ++i)
            drawMenuItem(i, i == menuSelection);
    } else if (oldSel != newSel) {
        drawMenuItem(oldSel, false);
        drawMenuItem(newSel, true);
    }
}

// =============================================================================
// RENDER MAIN MENU (COMPACT & FIT)
// =============================================================================
void DisplayManager::renderMainMenu() {
    drawModernHeader(MenuCatalog::pageTitle(menuPage), SPECTRUM_ACCENT);
    tft.fillRoundRect(137, 2, 20, 10, 3, SPECTRUM_BORDER);
    tft.setCursor(139, 3);
    tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_BORDER);
    tft.print(menuPage + 1);
    tft.print("/");
    tft.print(MenuCatalog::PAGE_COUNT);

    // Both layouts share the same catalog and selection state. Clear only the
    // viewport on page/layout changes; navigation remains partial.
    // Page changes keep APP_MODE_MENU, so clear the list viewport here to
    // remove rows left by a previous page with more items.
    tft.fillRect(0, 15, 160, 89, ST77XX_BLACK);
    const int count = MenuCatalog::pageItemCount(menuPage);
    const int first = appState.menuLayout == MENU_LAYOUT_LIST ? menuScrollOffset : 0;
    const int last = appState.menuLayout == MENU_LAYOUT_LIST ? min(count, first + 4) : count;
    for (int i = first; i < last; i++) {
        drawMenuItem(i, i == menuSelection);
    }

    drawModernFooter("U/D MOVE", "B PAGE", "A OPEN");
}

// =============================================================================
// RENDER JAMMER SCREEN (COMPACT & FIT)
// =============================================================================
void DisplayManager::renderJammerScreen() {
    if (!radioManager.transmitFeaturesEnabled()) {
        if (!jammerLayoutDrawn) {
            drawModernHeader("RF TEST", SPECTRUM_LOW);
            tft.fillRoundRect(9, 21, 142, 76, 6, SPECTRUM_CARD_BG);
            tft.drawRoundRect(9, 21, 142, 76, 6, SPECTRUM_BORDER);
            tft.fillRoundRect(37, 28, 86, 15, 4, SPECTRUM_HEADER_BG);
            tft.setCursor(44, 32);
            tft.setTextColor(SPECTRUM_LOW, SPECTRUM_CARD_BG);
            tft.setTextColor(SPECTRUM_LOW, SPECTRUM_HEADER_BG);
            tft.print("RX ONLY BUILD");
            tft.setCursor(27, 51);
            tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
            tft.print("Active RF output");
            tft.setCursor(34, 64);
            tft.print("not compiled");
            tft.setCursor(22, 82);
            tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
            tft.print("Analyzer remains RX");
            drawModernFooter("", "", "B BACK");
            jammerLayoutDrawn = true;
        }
        return;
    }
    if (!jammerLayoutDrawn) {
        drawModernHeader("AUTHORIZED RF TEST", SPECTRUM_HIGH);
        tft.fillRoundRect(5, 17, 150, 34, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 34, 4, SPECTRUM_BORDER);
        tft.fillRoundRect(5, 54, 150, 34, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 54, 150, 34, 4, SPECTRUM_BORDER);
        tft.fillRoundRect(5, 92, 150, 11, 3, SPECTRUM_CARD_BG);
        drawModernFooter("U/D TGT", "A START", "B STOP");
        jammerLayoutDrawn = true;
    }

    if (previousJammerTarget != static_cast<int>(appState.jammerTarget)) {
        tft.fillRect(8, 20, 144, 27, SPECTRUM_CARD_BG);
        tft.setCursor(10, 20);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("LAB TARGET  ");
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.println(appState.getJammerTargetName());
        tft.setCursor(10, 34);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.println(appState.getJammerFreqRangeStr());
        previousJammerTarget = static_cast<int>(appState.jammerTarget);
    }

    // Status Box (Active / Standby, Height 34px)
    int statusY = 54;
    const int radio1Channel = appState.currentJamChannel;
    const int radio2Channel = appState.currentJamChannel2;
    if (!jammingStatusValid || previousJamming != appState.jamming) {
        if (appState.jamming) {
            const uint16_t activeBg = DISPLAY_ACTIVE_BG;
            tft.fillRoundRect(5, statusY, 150, 34, 4, activeBg);
            tft.drawRoundRect(5, statusY, 150, 34, 4, SPECTRUM_CRITICAL);
            tft.fillCircle(13, statusY + 9, 3, SPECTRUM_CRITICAL);
            tft.setCursor(20, statusY + 6);
            tft.setTextColor(ST77XX_WHITE, activeBg);
            tft.print("TRANSMITTING - LAB");
        } else {
            tft.fillRoundRect(5, statusY, 150, 34, 4, SPECTRUM_CARD_BG);
            tft.drawRoundRect(5, statusY, 150, 34, 4, SPECTRUM_BORDER);
            tft.fillCircle(13, statusY + 17, 3, SPECTRUM_LOW);
            tft.setCursor(22, statusY + 14);
            tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
            tft.print("READY / STANDBY");
        }
        drawFooterChip(56, 49, appState.jamming ? "A STOP" : "A START");
        previousJamming = appState.jamming;
        previousJamChannel = -1;
        previousJamChannel2 = -1;
        jammingStatusValid = true;
    }

    // Only update the changing value row. Each frequency is derived from the
    // same channel snapshot so a Core 0 hop cannot produce a mismatched pair.
    if (appState.jamming &&
        (previousJamChannel != radio1Channel ||
         previousJamChannel2 != radio2Channel)) {
        const uint16_t activeBg = DISPLAY_ACTIVE_BG;
        tft.fillRect(9, statusY + 18, 142, 12, activeBg);
        tft.drawFastVLine(79, statusY + 19, 10, SPECTRUM_BORDER);
        tft.setCursor(11, statusY + 20);
        tft.setTextColor(ST77XX_WHITE, activeBg);
        tft.printf("R1 %3d %4d", radio1Channel, 2400 + radio1Channel);
        tft.setCursor(83, statusY + 20);
        tft.printf("R2 %3d", radio2Channel);
        previousJamChannel = radio1Channel;
        previousJamChannel2 = radio2Channel;
    }

    // Power & Dwell Info
    if (previousPowerLevel != static_cast<int>(appState.powerLevel) ||
        previousDwellTimeUs != appState.dwellTimeUs) {
        tft.fillRoundRect(5, 92, 150, 11, 3, SPECTRUM_CARD_BG);
        tft.setCursor(9, 94);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("PA ");
        tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
        tft.print(appState.getPowerLevelName());
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("  DWELL ");
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.print(appState.dwellTimeUs);
        tft.print("us");
        previousPowerLevel = static_cast<int>(appState.powerLevel);
        previousDwellTimeUs = appState.dwellTimeUs;
    }
}

// =============================================================================
// RENDER RADIO SPECTRUM ANALYZER (LIVE RF GRAPH - COMPACT)
// =============================================================================
