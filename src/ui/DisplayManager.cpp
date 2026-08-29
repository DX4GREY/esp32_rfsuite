#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "drivers/DisplayStorageBus.h"
#include "services/SubGhzRawService.h"

using namespace DisplayUi;

DisplayManager displayManager;

DisplayManager::DisplayManager()
    : tft(&displayStorageSpi(), TFT_CS, TFT_AO, TFT_RST) {
    resetDynamicCaches();
}

void DisplayManager::resetDynamicCaches() {
    memset(previousSpectrumLevels, 0xFF, sizeof(previousSpectrumLevels));
    memset(previousPeakLevels, 0xFF, sizeof(previousPeakLevels));
    previousHeaderPeakLevel = 0xFF;
    previousHeaderPeakChannel = -1;
    previousHeaderRadio1Level = 0xFF;
    previousHeaderRadio2Level = 0xFF;
    previousSpectrumCursorX = -1;
    previousInspectedLevel = 0xFF;
    previousInspectedPeak = 0xFF;
    carrierStatusValid = false;
    lastSpectrumRenderMs = 0;
    lastInspectorRenderMs = 0;
    lastJammerRenderMs = 0;
    lastStatusRenderMs = 0;
    lastEnvRenderMs = 0;
    lastThemeAnimationMs = 0;
    themeAnimationFrame = 0;
    previousEnvHistoryHead = 0xFF;
    previousEnvEventHead = 0xFF;
    previousEnvEventScroll = 0xFF;
    memset(previousEnvTopChannels,0xFF,sizeof(previousEnvTopChannels));
    memset(previousEnvTopLevels,0xFF,sizeof(previousEnvTopLevels));
    previousEnvAverage=previousEnvScore=0xFF;
    memset(previousCompareChannels,0xFF,sizeof(previousCompareChannels));
    memset(previousCompareLevels,0xFF,sizeof(previousCompareLevels));
    memset(previousCompareScores,0xFF,sizeof(previousCompareScores));
    previousEnvBursts=0xFFFF;previousEnvPeakChannel=0xFF;
    previousEnvBandChannel=0xFF;
    previousSnapshotChannel=0xFF;previousBeforeCapturedMs=previousAfterCapturedMs=0xFFFFFFFF;
    envRunningStatusValid = false;
    envLayoutDrawn = false;
    subRecordLayoutDrawn = false;
    subRfTestLayoutDrawn = false;
    subAnalyzerLayoutDrawn = false;
    subPacketLayoutDrawn = false;
    for (auto& level : previousSubAnalyzerLevels) level = -32768;
    previousSubAnalyzerPeakIndex = -1;
    previousSubPacketCount = 0xFFFFFFFF;
    previousSubPulseCount = 0xFFFFFFFF;
    subGraphProcessedPulses = 0;
    subGraphX = 8;
    subGraphLevel = false;
    subGraphInitialized = false;
    previousSubFrequencyKhz = -1;
    previousSubRecordState = -1;
    previousSubTestState = -1;
    subGhzReplayFrame = 0;
    lastSubGhzReplayFrameMs = 0;
    jammerLayoutDrawn = false;
    settingsLayoutDrawn = false;
    powerLayoutDrawn = false;
    snifferLayoutDrawn = false;
    subPresetLayoutDrawn = false;
    previousJammerTarget = -1;
    previousJamChannel = -1;
    previousJamChannel2 = -1;
    previousPowerLevel = -1;
    previousDwellTimeUs = -1;
    previousSettingsSelection = -1;
    previousSettingsTheme = -1;
    previousSettingsSniffSave = -1;
    previousSettingsOrientation = -1;
    previousPowerSelection = -1;
    previousSubPreset = previousSubRegion = previousSubRepeats = -1;
    previousSubTrigger = 999;
    snifferRunningValid = false;
    previousSnifferChannel = 0xFF;
    previousSnifferRate = -1;
    previousSnifferPackets = 0xFFFFFFFF;
    previousSnifferText = "";
    jammingStatusValid = false;
    renderedStatusPage = -1;
    for (int i = 0; i < 6; i++) {
        previousStatusValues[i] = "";
        previousStatusColors[i] = 0;
    }
}

void DisplayManager::init() {
    // Keep the SD card deselected while the shared hardware SPI bus and TFT
    // are initialized. StorageManager later mounts SD on this same bus.
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    displayStorageSpi().begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    tft.initR(INITR_BLACKTAB);   // ST7735 128x160
    tft.setRotation(appState.displayRotation);
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setTextSize(1);
    needRedraw = true;
}

void DisplayManager::applyOrientation() {
    tft.setRotation(appState.displayRotation);
    tft.fillScreen(ST77XX_BLACK);
    renderedMode = -1;
    resetDynamicCaches();
    needRedraw = true;
}

void DisplayManager::requestRedraw() {
    needRedraw = true;
}

void DisplayManager::prepareForShutdown() {
    tft.enableDisplay(false);
    delay(20);
    tft.enableSleep(true);
}

uint16_t DisplayManager::luaGuiColor(const char* name) const {
    String value = name ? String(name) : String("white");
    value.toLowerCase();
    if (value == "accent" || value == "cyan") return SPECTRUM_ACCENT;
    if (value == "green" || value == "low") return SPECTRUM_LOW;
    if (value == "yellow" || value == "mid") return SPECTRUM_MID;
    if (value == "orange" || value == "high") return SPECTRUM_HIGH;
    if (value == "red" || value == "critical") return SPECTRUM_CRITICAL;
    if (value == "gray" || value == "grey") return ST77XX_GRAY;
    if (value == "black") return ST77XX_BLACK;
    return ST77XX_WHITE;
}

void DisplayManager::luaGuiBegin(const char* title) {
    luaShowingGui = true;
    drawModernHeader(title && title[0] ? title : "LUA GUI", SPECTRUM_ACCENT);
    tft.fillRect(3, 16, 154, 88, ST77XX_BLACK);
    tft.drawRect(2, 15, 156, 90, SPECTRUM_BORDER);
    drawModernFooter("B LIST", "", "A RERUN");
}

void DisplayManager::luaGuiClose() {
    luaShowingGui = false;
    luaShowingOutput = false;
    luaReturnToListRequested = true;
    needRedraw = true;
}

void DisplayManager::luaGuiFooter(const char* left, const char* middle, const char* right) {
    String l = left ? String(left) : String();
    String m = middle ? String(middle) : String();
    String r = right ? String(right) : String();
    if (l.length() > 7) l.remove(7);
    if (m.length() > 7) m.remove(7);
    if (r.length() > 7) r.remove(7);
    drawModernFooter(l.c_str(), m.c_str(), r.c_str());
}

void DisplayManager::luaGuiClear() {
    tft.fillRect(4, 17, 152, 86, ST77XX_BLACK);
}

void DisplayManager::luaGuiText(int x, int y, const char* text, const char* color) {
    x = constrain(x, 0, 151); y = constrain(y, 0, 85);
    const int maxChars = max(0, (152 - x) / 6);
    String shown = text ? String(text) : String();
    shown.replace("\n", " "); shown.replace("\r", " ");
    if (shown.length() > static_cast<size_t>(maxChars)) shown.remove(maxChars);
    tft.setTextSize(1); tft.setTextWrap(false);
    tft.setTextColor(luaGuiColor(color), ST77XX_BLACK);
    tft.setCursor(4 + x, 17 + y); tft.print(shown);
}

void DisplayManager::luaGuiPixel(int x, int y, const char* color) {
    if (x < 0 || x >= 152 || y < 0 || y >= 86) return;
    tft.drawPixel(4 + x, 17 + y, luaGuiColor(color));
}

void DisplayManager::luaGuiLine(int x0, int y0, int x1, int y1, const char* color) {
    x0 = constrain(x0, 0, 151); x1 = constrain(x1, 0, 151);
    y0 = constrain(y0, 0, 85); y1 = constrain(y1, 0, 85);
    tft.drawLine(4 + x0, 17 + y0, 4 + x1, 17 + y1, luaGuiColor(color));
}

void DisplayManager::luaGuiRect(int x, int y, int width, int height,
                                const char* color, bool filled) {
    x = constrain(x, 0, 151); y = constrain(y, 0, 85);
    width = constrain(width, 1, 152 - x); height = constrain(height, 1, 86 - y);
    if (filled) tft.fillRect(4 + x, 17 + y, width, height, luaGuiColor(color));
    else tft.drawRect(4 + x, 17 + y, width, height, luaGuiColor(color));
}

void DisplayManager::luaGuiCircle(int x, int y, int radius,
                                  const char* color, bool filled) {
    x = constrain(x, 0, 151); y = constrain(y, 0, 85);
    const int maxRadius = min(min(x, 151 - x), min(y, 85 - y));
    if (maxRadius < 1) return;
    radius = constrain(radius, 1, maxRadius);
    if (filled) tft.fillCircle(4 + x, 17 + y, radius, luaGuiColor(color));
    else tft.drawCircle(4 + x, 17 + y, radius, luaGuiColor(color));
}

// =============================================================================
// SPLASH SCREEN (SHOWN BEFORE THE MAIN MENU)
// =============================================================================
void DisplayManager::showSplash() {
    tft.fillScreen(ST77XX_BLACK);

    // RF Suite boot identity: a spectrum/radar sequence inspired by compact
    // ESP32 device boot UIs, but with original RF instrumentation geometry.
    tft.setTextWrap(false);
    tft.setTextSize(1);
    tft.setCursor(centeredTextX("RF//SUITE"), 7);
    tft.setTextColor(SPECTRUM_CRITICAL, ST77XX_BLACK);
    tft.print("RF//SUITE");
    tft.drawFastHLine(25, 18, 110, SPECTRUM_CRITICAL);
    tft.drawPixel(21, 18, SPECTRUM_HIGH);
    tft.drawPixel(139, 18, SPECTRUM_HIGH);

    static const int8_t sweepX[12] = {0, 6, 10, 12, 10, 6, 0, -6, -10, -12, -10, -6};
    static const int8_t sweepY[12] = {-12, -10, -6, 0, 6, 10, 12, 10, 6, 0, -6, -10};
    for (uint8_t frame = 0; frame < 28; ++frame) {
        tft.fillRect(8, 24, 144, 73, ST77XX_BLACK);

        // Deterministic glitch cuts make the boot feel unstable without using
        // random(), heap allocation, or long display transactions.
        if ((frame % 5) == 1) {
            const int glitchY = 27 + ((frame * 11) % 58);
            tft.drawFastHLine(8, glitchY, 30 + ((frame * 7) % 100),
                              SPECTRUM_CRITICAL);
            tft.drawFastHLine(25, glitchY + 2, 95, SPECTRUM_GRID);
        }

        // Left: seven live spectrum bins with a travelling RF peak.
        tft.drawFastHLine(13, 78, 46, SPECTRUM_BORDER);
        for (uint8_t bar = 0; bar < 7; ++bar) {
            const uint8_t phase = (frame + bar * 3) % 12;
            const int height = 5 + ((phase <= 6 ? phase : 12 - phase) * 5);
            const uint16_t color = height > 27 ? SPECTRUM_CRITICAL :
                                   (height > 18 ? SPECTRUM_HIGH : SPECTRUM_ACCENT);
            tft.fillRect(14 + bar * 6, 78 - height, 4, height, color);
        }

        // A sharp heartbeat cuts through the spectrum bank.
        const int pulse = 28 + ((frame * 4) % 28);
        tft.drawLine(13, 83, pulse - 5, 83, SPECTRUM_GRID);
        tft.drawLine(pulse - 5, 83, pulse - 2, 76, SPECTRUM_CRITICAL);
        tft.drawLine(pulse - 2, 76, pulse + 1, 88, SPECTRUM_CRITICAL);
        tft.drawLine(pulse + 1, 88, pulse + 4, 83, SPECTRUM_CRITICAL);

        // Right: RF locator with a rotating vector and pulsing target dot.
        const int cx = 108, cy = 56;
        tft.drawCircle(cx, cy, 20, frame & 1 ? SPECTRUM_GRID : SPECTRUM_CRITICAL);
        tft.drawCircle(cx, cy, 12, SPECTRUM_BORDER);
        tft.drawFastHLine(cx - 22, cy, 45, SPECTRUM_GRID);
        tft.drawFastVLine(cx, cy - 22, 45, SPECTRUM_GRID);
        const uint8_t sweep = frame % 12;
        tft.drawLine(cx, cy, cx + sweepX[sweep] * 5 / 3,
                     cy + sweepY[sweep] * 5 / 3, SPECTRUM_CRITICAL);
        tft.fillCircle(cx + 9, cy - 7, 1 + ((frame / 2) & 1),
                       SPECTRUM_CRITICAL);
        if (frame > 18) {
            tft.drawLine(cx + 5, cy - 11, cx + 13, cy - 3, SPECTRUM_HIGH);
            tft.drawLine(cx + 13, cy - 11, cx + 5, cy - 3, SPECTRUM_HIGH);
        }

        const int progress = (frame + 1) * 132 / 28;
        tft.drawRoundRect(13, 88, 134, 6, 2, SPECTRUM_BORDER);
        tft.fillRoundRect(14, 89, progress, 4, 1,
                          frame > 20 ? SPECTRUM_CRITICAL : SPECTRUM_HIGH);
        delay(frame > 20 ? 42 : 28);
    }

    // Resolve the moving instruments into a compact RF monogram.
    tft.fillRect(8, 24, 144, 73, ST77XX_BLACK);
    tft.drawLine(80, 27, 114, 59, SPECTRUM_CRITICAL);
    tft.drawLine(114, 59, 80, 91, SPECTRUM_CRITICAL);
    tft.drawLine(80, 91, 46, 59, SPECTRUM_CRITICAL);
    tft.drawLine(46, 59, 80, 27, SPECTRUM_CRITICAL);
    tft.drawCircle(80, 59, 20, SPECTRUM_GRID);
    tft.drawCircle(80, 59, 12, SPECTRUM_HIGH);
    tft.fillCircle(80, 59, 4, SPECTRUM_CRITICAL);
    for (uint8_t ray = 0; ray < 12; ray += 3)
        tft.drawLine(80, 59, 80 + sweepX[ray] * 2, 59 + sweepY[ray] * 2,
                     SPECTRUM_CRITICAL);
    for (uint8_t flash = 0; flash < 3; ++flash) {
        tft.fillRect(21, 99, 118, 10, ST77XX_BLACK);
        if (!(flash & 1)) {
            tft.setCursor(centeredTextX("SIGNAL ACQUIRED"), 101);
            tft.setTextColor(SPECTRUM_CRITICAL, ST77XX_BLACK);
            tft.print("SIGNAL ACQUIRED");
        }
        delay(95);
    }
    tft.setCursor(centeredTextX("ENTER THE NOISE"), 101);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print("ENTER THE NOISE");
    tft.setCursor(centeredTextX("RFSUITE // Dx4Grey"), 114);
    tft.setTextColor(SPECTRUM_CRITICAL, ST77XX_BLACK);
    tft.print("RFSUITE // Dx4Grey");
    delay(520);
}

uint16_t DisplayManager::getSignalColor(uint8_t level) {
    if (level < 30) return SPECTRUM_LOW;
    if (level < 65) return SPECTRUM_MID;
    if (level < 85) return SPECTRUM_HIGH;
    return SPECTRUM_CRITICAL;
}

void DisplayManager::drawModernHeader(const char* title, uint16_t accent) {
    tft.fillRect(0, 0, 160, 14, SPECTRUM_HEADER_BG);
    if (appState.displayTheme == DISPLAY_THEME_TERMINAL) {
        tft.drawFastHLine(0, 12, 160, SPECTRUM_GRID);
        tft.drawFastHLine(0, 13, 160, accent);
        tft.drawRect(2, 3, 10, 8, accent);
    } else if (appState.displayTheme == DISPLAY_THEME_RETRO) {
        tft.drawFastHLine(0, 11, 160, SPECTRUM_GRID);
        tft.drawFastHLine(0, 13, 160, accent);
        tft.fillRect(3, 4, 9, 6, accent);
    } else if (appState.displayTheme == DISPLAY_THEME_FLIPPER) {
        tft.drawFastHLine(0, 13, 160, accent);
        tft.fillRoundRect(2, 3, 11, 8, 2, accent);
        tft.fillRect(5, 5, 5, 4, ST77XX_BLACK);
    } else if (appState.displayTheme == DISPLAY_THEME_NEON) {
        tft.drawFastHLine(0, 13, 80, SPECTRUM_ACCENT);
        tft.drawFastHLine(80, 13, 80, SPECTRUM_BORDER);
        tft.drawCircle(7, 7, 5, SPECTRUM_BORDER);
        tft.fillCircle(7, 7, 2, accent);
    } else if (appState.displayTheme == DISPLAY_THEME_OCEAN) {
        tft.drawFastHLine(0, 13, 160, accent);
        tft.drawCircle(7, 9, 6, SPECTRUM_BORDER);
        tft.drawCircle(7, 9, 3, accent);
    } else if (appState.displayTheme == DISPLAY_THEME_AMBER) {
        tft.drawFastHLine(0, 13, 160, accent);
        tft.drawFastHLine(2, 3, 11, accent);
        tft.drawFastVLine(2, 3, 8, accent);
        tft.drawPixel(12, 10, SPECTRUM_HIGH);
    } else if (appState.displayTheme == DISPLAY_THEME_MATRIX) {
        tft.drawFastHLine(0, 13, 160, SPECTRUM_GRID);
        tft.drawRect(2, 2, 11, 10, SPECTRUM_BORDER);
        tft.drawFastVLine(5, 4, 6, accent);
        tft.drawFastVLine(9, 6, 4, accent);
    } else if (appState.displayTheme == DISPLAY_THEME_VIOLET) {
        tft.drawFastHLine(0, 13, 160, accent);
        tft.drawLine(7, 2, 13, 7, SPECTRUM_BORDER);
        tft.drawLine(13, 7, 7, 12, accent);
        tft.drawLine(7, 12, 1, 7, SPECTRUM_BORDER);
        tft.drawLine(1, 7, 7, 2, accent);
    } else if (appState.displayTheme == DISPLAY_THEME_ICE) {
        tft.drawFastHLine(0, 13, 160, SPECTRUM_BORDER);
        tft.drawFastHLine(2, 7, 11, accent);
        tft.drawFastVLine(7, 2, 11, accent);
        tft.drawLine(3, 3, 11, 11, SPECTRUM_GRID);
        tft.drawLine(11, 3, 3, 11, SPECTRUM_GRID);
    } else {
        tft.drawFastHLine(0, 13, 160, accent);
        tft.fillCircle(7, 7, 3, accent);
        tft.drawCircle(7, 7, 5, SPECTRUM_BORDER);
    }
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
    tft.setCursor(centeredTextX(String(title)), 3);
    tft.print(title);
}

void DisplayManager::drawFooterChip(int x, int width, const char* label) {
    int radius = 3;
    if (appState.displayTheme == DISPLAY_THEME_TERMINAL ||
        appState.displayTheme == DISPLAY_THEME_RETRO ||
        appState.displayTheme == DISPLAY_THEME_MATRIX) radius = 0;
    else if (appState.displayTheme == DISPLAY_THEME_FLIPPER ||
             appState.displayTheme == DISPLAY_THEME_AMBER) radius = 2;
    else if (appState.displayTheme == DISPLAY_THEME_OCEAN) radius = 7;
    else if (appState.displayTheme == DISPLAY_THEME_VIOLET ||
             appState.displayTheme == DISPLAY_THEME_NEON) radius = 5;
    tft.fillRoundRect(x, 107, width, 16, radius, DISPLAY_FOOTER_BG);
    if (appState.displayTheme == DISPLAY_THEME_FLIPPER)
        tft.drawRoundRect(x, 107, width, 16, radius, SPECTRUM_ACCENT);
    else if (appState.displayTheme == DISPLAY_THEME_NEON)
        tft.drawRoundRect(x, 107, width, 16, radius, SPECTRUM_BORDER);
    else if (appState.displayTheme == DISPLAY_THEME_RETRO)
        tft.drawFastHLine(x + 2, 109, width - 4, SPECTRUM_HIGH);
    else if (appState.displayTheme == DISPLAY_THEME_TERMINAL)
        tft.drawFastVLine(x + 2, 110, 10, SPECTRUM_ACCENT);
    int textX = x + (width - static_cast<int>(strlen(label)) * 6) / 2;
    tft.setCursor(textX, 111);
    tft.setTextColor(SPECTRUM_ACCENT, DISPLAY_FOOTER_BG);
    tft.print(label);
}

void DisplayManager::drawThemeAnimation() {
    const unsigned long now = millis();
    // Keep decoration off timing-sensitive RF and full-surface media screens.
    if (subGhzReplayActive || subGhzRawService.isRecording() ||
        subGhzRawService.isRfTesting() || appState.appMode == APP_MODE_VIDEO_PLAYER ||
        appState.appMode == APP_MODE_PHOTO_VIEWER || appState.appMode == APP_MODE_REBOOT ||
        appState.appMode == APP_MODE_SHUTDOWN) return;
    if (now - lastThemeAnimationMs < 180) return;
    lastThemeAnimationMs = now;
    const uint8_t frame = themeAnimationFrame++;
    tft.fillRect(0, 1, 15, 11, SPECTRUM_HEADER_BG);
    switch (appState.displayTheme) {
        case DISPLAY_THEME_FLIPPER: {
            const int active = frame % 3;
            for (int i = 0; i < 3; ++i) {
                const uint16_t color = i == active ? SPECTRUM_ACCENT : SPECTRUM_GRID;
                tft.drawLine(2 + i * 4, 8 - i * 2, 4 + i * 4, 6, color);
                tft.drawLine(4 + i * 4, 6, 2 + i * 4, 4 + i * 2, color);
            }
            break;
        }
        case DISPLAY_THEME_TERMINAL:
        case DISPLAY_THEME_MATRIX:
            for (int x = 2; x < 14; x += 4) {
                const int y = 2 + ((frame + x) % 8);
                tft.drawPixel(x, y, SPECTRUM_ACCENT);
                if (y > 2) tft.drawPixel(x, y - 2, SPECTRUM_GRID);
            }
            break;
        case DISPLAY_THEME_RETRO:
            tft.drawRect(2, 3, 11, 7, SPECTRUM_GRID);
            tft.drawFastHLine(3, 4 + (frame % 5), 9, SPECTRUM_ACCENT);
            break;
        case DISPLAY_THEME_NEON: {
            static const int8_t orbit[4][2] = {{7,2},{12,6},{7,10},{2,6}};
            tft.drawCircle(7, 6, 5, SPECTRUM_GRID);
            tft.fillCircle(orbit[frame % 4][0], orbit[frame % 4][1], 1,
                           SPECTRUM_ACCENT);
            break;
        }
        case DISPLAY_THEME_OCEAN:
            tft.drawCircle(7, 7, 2 + (frame % 4), SPECTRUM_ACCENT);
            tft.drawPixel(2 + (frame % 11), 10, SPECTRUM_LOW);
            break;
        case DISPLAY_THEME_AMBER:
            tft.drawFastHLine(2, 3 + (frame % 7), 11, SPECTRUM_HIGH);
            tft.drawFastVLine(2 + (frame % 11), 2, 9, SPECTRUM_GRID);
            break;
        case DISPLAY_THEME_VIOLET: {
            const int width = 3 + (frame % 9);
            tft.drawFastHLine(7 - width / 2, 4, width, SPECTRUM_ACCENT);
            tft.drawFastHLine(7 - width / 2, 8, width, SPECTRUM_BORDER);
            break;
        }
        case DISPLAY_THEME_ICE:
            tft.drawLine(7, 1, 7, 11, SPECTRUM_ACCENT);
            tft.drawLine(2, 6, 12, 6, SPECTRUM_ACCENT);
            tft.drawLine(3, 2 + (frame % 3), 11, 10 - (frame % 3), SPECTRUM_GRID);
            break;
        case DISPLAY_THEME_CYBER:
        default:
            for (int i = 0; i < 3; ++i)
                tft.drawFastVLine(3 + i * 4, 9 - ((frame + i) % 4),
                                  2 + ((frame + i) % 4), SPECTRUM_ACCENT);
            break;
    }
}

void DisplayManager::drawModernFooter(const char* left, const char* middle, const char* right) {
    tft.fillRect(0, 105, 160, 23, ST77XX_BLACK);
    if (left && left[0]) drawFooterChip(3, 48, left);
    if (middle && middle[0]) drawFooterChip(56, 49, middle);
    if (right && right[0]) drawFooterChip(110, 47, right);
}

// =============================================================================
// MENU ITEM HELPER (single row renderer)
// =============================================================================
