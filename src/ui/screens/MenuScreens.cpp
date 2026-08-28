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

void DisplayManager::drawThemedMenuCard(int x, int y, int width, int height,
                                        bool selected, bool list,
                                        uint16_t background, uint16_t border) {
    int radius = 4;
    switch (appState.displayTheme) {
        case DISPLAY_THEME_OCEAN: radius = 8; break;
        case DISPLAY_THEME_VIOLET: radius = 6; break;
        case DISPLAY_THEME_ICE: radius = 5; break;
        case DISPLAY_THEME_FLIPPER: radius = 2; break;
        case DISPLAY_THEME_NEON: radius = 6; break;
        case DISPLAY_THEME_AMBER: radius = 1; break;
        case DISPLAY_THEME_MATRIX:
        case DISPLAY_THEME_RETRO:
        case DISPLAY_THEME_TERMINAL: radius = 0; break;
        default: break;
    }

    const uint8_t motion = selected ? min<uint8_t>(3, menuTransitionPhase) : 3;
    const uint16_t animatedBorder = selected && motion < 2 ? SPECTRUM_GRID : border;
    tft.fillRoundRect(x, y, width, height, radius, background);
    tft.drawRoundRect(x, y, width, height, radius, animatedBorder);

    switch (appState.displayTheme) {
        case DISPLAY_THEME_FLIPPER:
            if (selected) {
                const int marker = list ? max(1, (5 * (motion + 1)) / 4) :
                                          max(4, ((width - 6) * (motion + 1)) / 4);
                tft.fillRect(x + 3, y + 3, marker, list ? height - 6 : 3,
                             SPECTRUM_ACCENT);
                if (!list && motion >= 2) tft.drawFastHLine(x + 7, y + height - 3,
                                             ((width - 14) * motion) / 3,
                                             SPECTRUM_ACCENT);
            }
            break;
        case DISPLAY_THEME_NEON:
            if (selected && motion >= 2 && width > 8 && height > 8)
                tft.drawRoundRect(x + 2, y + 2, width - 4, height - 4,
                                  max(1, radius - 2), SPECTRUM_GRID);
            break;
        case DISPLAY_THEME_RETRO:
            tft.drawFastHLine(x + 2, y + 2, width - 4,
                              selected ? SPECTRUM_ACCENT : SPECTRUM_GRID);
            tft.drawPixel(x + width - 2, y + height - 2, SPECTRUM_HIGH);
            break;
        case DISPLAY_THEME_TERMINAL:
            if (selected) {
                const int markerHeight = max(2, ((height - 4) * (motion + 1)) / 4);
                tft.drawFastVLine(x + 2, y + 2, markerHeight, SPECTRUM_ACCENT);
                if (motion >= 2) tft.drawPixel(x + 4, y + height / 2, SPECTRUM_ACCENT);
            }
            break;
        case DISPLAY_THEME_MATRIX:
            if (selected) {
                const int bracket = 2 + motion * 2;
                tft.drawFastHLine(x, y, bracket, SPECTRUM_ACCENT);
                tft.drawFastHLine(x + width - bracket, y + height - 1, bracket,
                                  SPECTRUM_ACCENT);
            }
            break;
        case DISPLAY_THEME_AMBER:
            tft.drawPixel(x + 1, y + 1, SPECTRUM_HIGH);
            tft.drawPixel(x + width - 2, y + height - 2, SPECTRUM_HIGH);
            break;
        case DISPLAY_THEME_VIOLET:
            if (selected) tft.drawFastHLine(x + 7, y + height - 2,
                                            max(2, ((width - 14) * (motion + 1)) / 4),
                                            SPECTRUM_ACCENT);
            break;
        case DISPLAY_THEME_OCEAN:
            if (selected) tft.fillCircle(x + width - 9, y + height / 2,
                                         motion >= 2 ? 2 : 1,
                                         SPECTRUM_ACCENT);
            break;
        case DISPLAY_THEME_ICE:
            if (selected && motion >= 1) {
                tft.drawLine(x + width - 10, y + 2, x + width - 3, y + height / 2,
                             SPECTRUM_ACCENT);
                tft.drawLine(x + width - 3, y + height / 2, x + width - 10,
                             y + height - 3, SPECTRUM_ACCENT);
            }
            break;
        default:
            if (selected) tft.fillRoundRect(x + 2, y + (list ? 3 : 5),
                                            max(1, static_cast<int>(motion)),
                                            max(2, ((list ? height - 6 : height - 10) *
                                                    (motion + 1)) / 4), 1,
                                            SPECTRUM_ACCENT);
            break;
    }
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
    drawThemedMenuCard(x, y, width, height, selected, list, bg, edge);
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
            drawMainMenuItem(page * 6 + slot, slot, false);
        for (menuTransitionPhase = 0; menuTransitionPhase < 4; ++menuTransitionPhase) {
            drawMainMenuItem(bandSelection, bandSelection % 6, true);
            delay(12); yield();
        }
        menuTransitionPhase = 3;
    } else {
        drawMainMenuItem(previousBandSelection, previousBandSelection % 6, false);
        for (menuTransitionPhase = 0; menuTransitionPhase < 4; ++menuTransitionPhase) {
            drawMainMenuItem(bandSelection, bandSelection % 6, true);
            delay(12); yield();
        }
        menuTransitionPhase = 3;
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
    drawThemedMenuCard(x, y, width, height, selected, list, bg, edge);
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
    const bool subGhz = appState.radioBand == RADIO_BAND_SUB_GHZ;
    drawModernHeader(subGhz ? "CC1101 OFFLINE" : "NRF24 OFFLINE", SPECTRUM_CRITICAL);
    tft.fillRoundRect(6, 20, 148, 70, 5, SPECTRUM_CARD_BG);
    tft.drawRoundRect(6, 20, 148, 70, 5, SPECTRUM_CRITICAL);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.setCursor(14, 29); tft.print(subGhz ? "CC1101 NOT" : "NRF24 NOT");
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
            drawSubGhzMenuItem(i, false);
        for (menuTransitionPhase = 0; menuTransitionPhase < 4; ++menuTransitionPhase) {
            drawSubGhzMenuItem(subGhzMenuSelection, true);
            delay(12); yield();
        }
        menuTransitionPhase = 3;
    } else {
        drawSubGhzMenuItem(previousSubGhzMenuSelection, false);
        for (menuTransitionPhase = 0; menuTransitionPhase < 4; ++menuTransitionPhase) {
            drawSubGhzMenuItem(subGhzMenuSelection, true);
            delay(12); yield();
        }
        menuTransitionPhase = 3;
    }
}

void DisplayManager::renderSubGhzAnalyzerScreen() {
    if (!subAnalyzerLayoutDrawn) {
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM RF ANALYZER" : "SUB-GHz ANALYZER", SPECTRUM_ACCENT);
        tft.fillRoundRect(4, 17, 152, 68, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(4, 17, 152, 68, 4, SPECTRUM_BORDER);
        // Two subtle reference lines make relative RSSI changes easier to read.
        for (int x = 8; x <= 151; x += 4) {
            tft.drawPixel(x, 38, SPECTRUM_GRID);
            tft.drawPixel(x, 54, SPECTRUM_GRID);
        }
        tft.drawFastHLine(8, 70, 144, SPECTRUM_BORDER);
        tft.setCursor(8, 75); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.printf("%.0f", subGhzRawService.analyzerFrequency(0));
        tft.setCursor(127, 75);
        tft.printf("%.0fM", subGhzRawService.analyzerFrequency(subGhzRawService.analyzerCount() - 1));

        tft.fillRoundRect(4, 88, 152, 15, 4, SPECTRUM_HEADER_BG);
        tft.setCursor(9, 92); tft.setTextColor(ST77XX_GRAY, SPECTRUM_HEADER_BG);
        tft.print("PEAK");
        drawModernFooter("", "A TUNE", "B BACK");
        subAnalyzerLayoutDrawn = true;
    }

    int8_t peakIndex = 0;
    float peakDistance = 10000.0f;
    for (uint8_t i = 0; i < subGhzRawService.analyzerCount(); ++i) {
        const float distance = fabsf(subGhzRawService.analyzerFrequency(i) -
                                     subGhzRawService.analyzerPeakFrequency());
        if (distance < peakDistance) { peakDistance = distance; peakIndex = i; }
    }
    for (uint8_t i = 0; i < subGhzRawService.analyzerCount(); ++i) {
        const int16_t rssi = subGhzRawService.analyzerRssi(i);
        if (previousSubAnalyzerLevels[i] == rssi && i != peakIndex &&
            i != previousSubAnalyzerPeakIndex) continue;
        const int x = 10 + i * 7;
        const int height = constrain(map(rssi, -110, -30, 0, 48), 0, 48);
        tft.fillRect(x, 21, 5, 49, SPECTRUM_CARD_BG);
        // Restore the dotted guides behind a bar that becomes shorter.
        if ((x - 8) % 4 == 0) {
            tft.drawPixel(x, 38, SPECTRUM_GRID);
            tft.drawPixel(x, 54, SPECTRUM_GRID);
        }
        if (height) tft.fillRoundRect(x, 70 - height, 5, height, 1,
            getSignalColor(constrain(map(rssi, -110, -30, 0, 100), 0, 100)));
        if (i == peakIndex && height > 0)
            tft.drawFastHLine(x, max(21, 69 - height), 5, ST77XX_WHITE);
        previousSubAnalyzerLevels[i] = rssi;
    }
    previousSubAnalyzerPeakIndex = peakIndex;
    tft.fillRect(38, 90, 114, 11, SPECTRUM_HEADER_BG);
    tft.setCursor(42, 92); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_HEADER_BG);
    tft.printf("%.2f MHz  %d dBm", subGhzRawService.analyzerPeakFrequency(),
               subGhzRawService.analyzerPeakRssi());
}

void DisplayManager::renderSubGhzPresetsScreen() {
    if (!subPresetLayoutDrawn) {
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM PRESETS" : "SUB-GHz PRESETS", SPECTRUM_HIGH);
        tft.fillRoundRect(6, 21, 148, 68, 5, SPECTRUM_CARD_BG);
        tft.drawRoundRect(6, 21, 148, 68, 5, SPECTRUM_BORDER);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.setCursor(12, 29); tft.print("MODULATION");
        tft.setCursor(12, 61); tft.print("TX REGION");
        drawModernFooter("U/D PRE", "A REGION", "HOLD B REP");
        subPresetLayoutDrawn = true;
    }
    const int preset = static_cast<int>(cc1101Manager.preset());
    const int region = static_cast<int>(subGhzRawService.region());
    const int trigger = subGhzRawService.triggerThresholdDbm();
    const int repeats = subGhzRawService.replayRepeatCount();
    if (previousSubPreset != preset) {
        tft.fillRect(10, 40, 140, 11, SPECTRUM_CARD_BG);
        tft.setCursor(12, 43); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.print(cc1101Manager.presetName());
    }
    if (previousSubRegion != region) {
        tft.fillRect(78, 58, 72, 11, SPECTRUM_CARD_BG);
        tft.setCursor(80, 61);
        tft.setTextColor(subGhzRawService.region() == SubGhzRegion::RX_ONLY ?
                         SPECTRUM_CRITICAL : SPECTRUM_LOW, SPECTRUM_CARD_BG);
        tft.print(subGhzRawService.regionName());
    }
    if (previousSubTrigger != trigger || previousSubRepeats != repeats) {
        tft.fillRect(10, 73, 140, 11, SPECTRUM_CARD_BG);
        tft.setCursor(12, 76); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.printf("TRG %d  REPLAY %dX", trigger, repeats);
    }
    previousSubPreset = preset; previousSubRegion = region;
    previousSubTrigger = trigger; previousSubRepeats = repeats;
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
        drawModernHeader(subGhzRawService.simulationMode() ? "SIM RAW CAPTURE" : "SUB-GHz CAPTURE", SPECTRUM_CRITICAL);
        tft.fillRoundRect(5, 17, 150, 21, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 21, 4, SPECTRUM_BORDER);
        tft.setCursor(10, 24); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG); tft.print("MHz");
        tft.fillRoundRect(5, 41, 150, 39, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 41, 150, 39, 4, SPECTRUM_BORDER);
        tft.drawFastHLine(8, 60, 144, SPECTRUM_GRID);
        tft.fillRoundRect(5, 83, 150, 20, 4, SPECTRUM_HEADER_BG);
        tft.setCursor(9, 87); tft.setTextColor(ST77XX_GRAY, SPECTRUM_HEADER_BG); tft.print("PULSE");
        subRecordLayoutDrawn = true;
    }
    const int frequencyKhz = static_cast<int>(cc1101Manager.frequencyMHz() * 1000.0f);
    if (needRedraw || previousSubFrequencyKhz != frequencyKhz) {
        tft.fillRect(36, 20, 66, 15, SPECTRUM_CARD_BG);
        tft.setCursor(39, 24); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.printf("%.2f", cc1101Manager.frequencyMHz());
        previousSubFrequencyKhz = frequencyKhz;
    }
    const int recordingState = subGhzRawService.isArmed() ? 2 :
                               (subGhzRawService.isRecording() ? 1 : 0);
    if (needRedraw || previousSubRecordState != recordingState) {
        tft.fillRect(101, 20, 50, 15, SPECTRUM_CARD_BG);
        tft.fillRoundRect(105, 22, 44, 12, 3,
                          recordingState ? DISPLAY_ACTIVE_BG : SPECTRUM_BORDER);
        tft.setCursor(recordingState == 0 ? 112 : 116, 24);
        tft.setTextColor(recordingState ? SPECTRUM_CRITICAL : SPECTRUM_LOW,
                         recordingState ? DISPLAY_ACTIVE_BG : SPECTRUM_BORDER);
        tft.print(recordingState == 2 ? "ARMED" : (recordingState == 1 ? "REC" : "READY"));
        drawModernFooter("U/D FREQ", subGhzRawService.isRecording() ? "A STOP" : "A START", "B BACK");
        if (recordingState == 1) {
            tft.fillRect(8, 44, 144, 33, SPECTRUM_CARD_BG);
            tft.drawFastHLine(8, 60, 144, SPECTRUM_GRID);
            subGraphProcessedPulses = 0; subGraphX = 8; subGraphInitialized = false;
        }
        previousSubRecordState = recordingState;
    }
    const uint32_t pulses = subGhzRawService.pulseCount();
    if (needRedraw || previousSubPulseCount != pulses || subGhzRawService.isRecording()) {
        tft.fillRect(43, 85, 109, 16, SPECTRUM_HEADER_BG);
        tft.setCursor(45, 87); tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
        if (!subGhzRawService.isRecording() && subGhzRawService.decodedBitCount())
            tft.printf("%s %ub", subGhzRawService.detectedProtocol(),
                       subGhzRawService.decodedBitCount());
        else tft.printf("%lu  %d dBm", static_cast<unsigned long>(pulses),
                        subGhzRawService.liveRssiDbm());
        tft.setCursor(45, 95); tft.setTextColor(ST77XX_GRAY, SPECTRUM_HEADER_BG);
        tft.printf("%u/s  BUF %u%%", subGhzRawService.pulsesPerSecond(),
                   subGhzRawService.bufferPercent());
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
        tft.fillRect(8, 44, 144, 33, SPECTRUM_CARD_BG);
        tft.drawFastHLine(8, 60, 144, SPECTRUM_GRID);
    }
    uint8_t levelAtStart = 0;
    const size_t newCount = subGhzRawService.copyPulses(
        subGraphProcessedPulses, subGraphPulseBuffer, 256, levelAtStart);
    if (!subGraphInitialized && newCount) {
        subGraphLevel = levelAtStart != 0;
        subGraphInitialized = true;
    }
    for (size_t i = 0; i < newCount; ++i) {
        const int width = constrain(static_cast<int>((subGraphPulseBuffer[i] + 99) / 100), 1, 24);
        if (subGraphX + width > 152) {
            tft.fillRect(8, 44, 144, 33, SPECTRUM_CARD_BG);
            tft.drawFastHLine(8, 60, 144, SPECTRUM_GRID);
            subGraphX = 8;
        }
        const int y0 = subGraphLevel ? 46 : 75;
        const int y1 = subGraphLevel ? 75 : 46;
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

void DisplayManager::renderSubGhzReplayAnimation() {
    const unsigned long now = millis();
    const bool firstFrame = lastSubGhzReplayFrameMs == 0;
    if (!firstFrame && now - lastSubGhzReplayFrameMs < 120) return;
    lastSubGhzReplayFrameMs = now;

    if (firstFrame) {
        drawModernHeader("SUB-GHz REPLAY", SPECTRUM_CRITICAL);
        tft.fillRoundRect(4, 17, 152, 86, 5, SPECTRUM_CARD_BG);
        tft.drawRoundRect(4, 17, 152, 86, 5, SPECTRUM_BORDER);

        String shown = subGhzReplayFile;
        if (shown.length() > 20) shown = shown.substring(0, 17) + "...";
        tft.setCursor(centeredTextX(shown), 21);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print(shown);
        drawModernFooter("", "SENDING", "B ABORT");
    }

    // Flipper-inspired replay page: compact transmitter, animated waves, and
    // real progress rather than an indeterminate activity screen.
    tft.fillRect(9, 32, 142, 53, SPECTRUM_CARD_BG);
    const int phase = subGhzReplayFrame % 4;
    const uint16_t waveColors[4] = {
        SPECTRUM_CRITICAL, SPECTRUM_HIGH, SPECTRUM_MID, SPECTRUM_BORDER
    };

    // Pocket transmitter body and antenna.
    tft.fillRoundRect(14, 39, 31, 27, 5, SPECTRUM_HEADER_BG);
    tft.drawRoundRect(14, 39, 31, 27, 5, SPECTRUM_CRITICAL);
    tft.drawFastVLine(37, 33, 7, SPECTRUM_CRITICAL);
    tft.drawLine(37, 33, 41, 30, SPECTRUM_CRITICAL);
    tft.fillCircle(23, 48, 3, waveColors[phase]);
    tft.drawRoundRect(20, 56, 18, 5, 2, SPECTRUM_BORDER);
    tft.fillRect(22, 58, 4 + phase * 3, 1, SPECTRUM_LOW);

    // Expanding chevrons read clearly as motion on the tiny 160x128 panel.
    for (int wave = 0; wave < 3; ++wave) {
        const int animated = (wave + phase) % 4;
        const int x = 54 + wave * 14;
        const int spread = 5 + wave * 3;
        const uint16_t color = waveColors[animated];
        tft.drawLine(x, 52 - spread, x + 7, 52, color);
        tft.drawLine(x + 7, 52, x, 52 + spread, color);
        if (animated == 0) {
            tft.drawLine(x + 2, 52 - spread + 2, x + 8, 52, ST77XX_WHITE);
            tft.drawLine(x + 8, 52, x + 2, 52 + spread - 2, ST77XX_WHITE);
        }
    }

    const uint8_t percent = subGhzRawService.replayPercent();
    const uint8_t pass = min<uint8_t>(subGhzRawService.replayPass() + 1,
                                      subGhzRawService.replayPassTotal());
    tft.setCursor(103, 36);
    tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
    if (subGhzRawService.replayFrequencyMHz() > 0.0f)
        tft.printf("%.2f", subGhzRawService.replayFrequencyMHz());
    else tft.print("LOAD...");
    tft.setCursor(103, 47);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.printf("PASS %u/%u", pass, subGhzRawService.replayPassTotal());
    tft.setCursor(103, 58);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.printf("%lu/%lu", static_cast<unsigned long>(subGhzRawService.replayPulseIndex()),
               static_cast<unsigned long>(subGhzRawService.replayPulseTotal()));

    tft.setCursor(10, 72);
    tft.setTextColor(SPECTRUM_CRITICAL, SPECTRUM_CARD_BG);
    tft.printf("TX %3u%%", percent);
    tft.fillRoundRect(56, 73, 88, 7, 3, SPECTRUM_BORDER);
    if (percent) tft.fillRoundRect(57, 74, max(2, percent * 86 / 100), 5, 2,
                                   SPECTRUM_CRITICAL);

    tft.fillRect(9, 87, 142, 12, SPECTRUM_CARD_BG);
    tft.setCursor(28, 89);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.print("Sending raw signal...");

    const char spinner[] = {'|', '/', '-', '\\'};
    tft.fillRect(134, 22, 10, 9, SPECTRUM_CARD_BG);
    tft.setCursor(136, 23);
    tft.setTextColor(SPECTRUM_CRITICAL, SPECTRUM_CARD_BG);
    tft.print(spinner[phase]);
    ++subGhzReplayFrame;
}

void DisplayManager::renderSubGhzReplayResult() {
    const uint16_t accent = subGhzReplaySucceeded ? SPECTRUM_LOW : SPECTRUM_CRITICAL;
    drawModernHeader(subGhzReplaySucceeded ? "REPLAY COMPLETE" : "REPLAY STOPPED", accent);
    tft.fillRoundRect(5, 18, 150, 84, 5, SPECTRUM_CARD_BG);
    tft.drawRoundRect(5, 18, 150, 84, 5, accent);

    // Large success/failure emblem keeps the outcome readable at a glance.
    tft.drawCircle(80, 43, 15, accent);
    tft.drawCircle(80, 43, 16, SPECTRUM_BORDER);
    if (subGhzReplaySucceeded) {
        tft.drawLine(72, 43, 78, 49, accent);
        tft.drawLine(78, 49, 89, 36, accent);
        tft.drawLine(72, 44, 78, 50, ST77XX_WHITE);
    } else {
        tft.drawLine(73, 36, 87, 50, accent);
        tft.drawLine(87, 36, 73, 50, accent);
    }

    String shown = subGhzReplayFile;
    if (shown.length() > 20) shown = shown.substring(0, 17) + "...";
    tft.setCursor(centeredTextX(shown), 64);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.print(shown);

    tft.setCursor(16, 77);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    if (subGhzReplaySucceeded) {
        tft.printf("%.2f MHz  %lu pulses", subGhzRawService.replayFrequencyMHz(),
                   static_cast<unsigned long>(subGhzRawService.replayPulseTotal()));
    } else {
        String status = subGhzRawService.lastError();
        if (status.length() > 21) status.remove(21);
        tft.setCursor(centeredTextX(status), 77);
        tft.print(status);
    }
    tft.setCursor(27, 90);
    tft.setTextColor(accent, SPECTRUM_CARD_BG);
    tft.print(subGhzReplaySucceeded ? "Signal sent successfully" : "Signal was not sent");

    drawModernFooter("", "A REPLAY", "B BACK");
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
    drawThemedMenuCard(x, y, cardWidth, cardHeight, selected, list,
                       background, border);

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
            drawMenuItem(i, false);
        for (menuTransitionPhase = 0; menuTransitionPhase < 4; ++menuTransitionPhase) {
            drawMenuItem(menuSelection, true);
            delay(12); yield();
        }
        menuTransitionPhase = 3;
    } else if (oldSel != newSel) {
        drawMenuItem(oldSel, false);
        for (menuTransitionPhase = 0; menuTransitionPhase < 4; ++menuTransitionPhase) {
            drawMenuItem(newSel, true);
            delay(12); yield();
        }
        menuTransitionPhase = 3;
    }
}

// =============================================================================
// RENDER MAIN MENU (COMPACT & FIT)
// =============================================================================
void DisplayManager::renderMainMenu() {
    drawModernHeader(appState.simulationMode ? "2.4 GHz [SIM]" : MenuCatalog::pageTitle(menuPage), SPECTRUM_ACCENT);
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
    if (appState.simulationMode || !radioManager.transmitFeaturesEnabled()) {
        if (!jammerLayoutDrawn) {
            drawModernHeader("RF TEST", SPECTRUM_LOW);
            tft.fillRoundRect(9, 21, 142, 76, 6, SPECTRUM_CARD_BG);
            tft.drawRoundRect(9, 21, 142, 76, 6, SPECTRUM_BORDER);
            tft.fillRoundRect(37, 28, 86, 15, 4, SPECTRUM_HEADER_BG);
            tft.setCursor(44, 32);
            tft.setTextColor(SPECTRUM_LOW, SPECTRUM_CARD_BG);
            tft.setTextColor(SPECTRUM_LOW, SPECTRUM_HEADER_BG);
            tft.print(appState.simulationMode ? "SIMULATION MODE" : "RX ONLY BUILD");
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
