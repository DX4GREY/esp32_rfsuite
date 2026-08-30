#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "drivers/DisplayStorageBus.h"
#include "drivers/RadioManager.h"
#include "services/SubGhzRawService.h"
#include "services/StorageManager.h"
#include "services/SessionRecorder.h"

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
    previousSettingsAnimation = -1;
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
    drawModernFooter("", "A RERUN", "B LIST");
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
    const bool animateSplash = appState.animationsEnabled && appState.bootAnimationEnabled;
    const uint8_t firstFrame = animateSplash ? 0 : 27;
    for (uint8_t frame = firstFrame; frame < 28; ++frame) {
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
        if (animateSplash)
            delay(appState.scaledAnimationDelay(frame > 20 ? 42 : 28));
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
    const uint8_t flashFrames = animateSplash ? 3 : 1;
    for (uint8_t flash = 0; flash < flashFrames; ++flash) {
        tft.fillRect(21, 99, 118, 10, ST77XX_BLACK);
        if (!(flash & 1)) {
            tft.setCursor(centeredTextX("SIGNAL ACQUIRED"), 101);
            tft.setTextColor(SPECTRUM_CRITICAL, ST77XX_BLACK);
            tft.print("SIGNAL ACQUIRED");
        }
        if (animateSplash) delay(appState.scaledAnimationDelay(95));
    }
    tft.setCursor(centeredTextX("ENTER THE NOISE"), 101);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print("ENTER THE NOISE");
    tft.setCursor(centeredTextX("RFSUITE // Dx4Grey"), 114);
    tft.setTextColor(SPECTRUM_CRITICAL, ST77XX_BLACK);
    tft.print("RFSUITE // Dx4Grey");
    delay(animateSplash ? appState.scaledAnimationDelay(520) : 120);
}

uint16_t DisplayManager::getSignalColor(uint8_t level) {
    if (level < 30) return SPECTRUM_LOW;
    if (level < 65) return SPECTRUM_MID;
    if (level < 85) return SPECTRUM_HIGH;
    return SPECTRUM_CRITICAL;
}

void DisplayManager::drawStatusBar() {
    // 1. Radio 1 & 2 indicators (x=102..115)
    const bool r1 = radioManager.isRadio1Connected() || appState.simulationMode;
    const bool r2 = radioManager.isRadio2Connected() || appState.simulationMode;
    tft.fillRoundRect(102, 2, 7, 10, 1, r1 ? SPECTRUM_LOW : SPECTRUM_BORDER);
    tft.setCursor(103, 3);
    tft.setTextColor(r1 ? ST77XX_BLACK : ST77XX_GRAY, r1 ? SPECTRUM_LOW : SPECTRUM_BORDER);
    tft.print("1");

    tft.fillRoundRect(110, 2, 7, 10, 1, r2 ? SPECTRUM_LOW : SPECTRUM_BORDER);
    tft.setCursor(111, 3);
    tft.setTextColor(r2 ? ST77XX_BLACK : ST77XX_GRAY, r2 ? SPECTRUM_LOW : SPECTRUM_BORDER);
    tft.print("2");

    // 2. Storage backend (SD or LF) at x=119
    const bool sd = storageManager.usingSd();
    tft.fillRoundRect(119, 2, 14, 10, 2, sd ? SPECTRUM_HEADER_BG : SPECTRUM_BORDER);
    tft.setCursor(120, 3);
    tft.setTextColor(sd ? SPECTRUM_ACCENT : SPECTRUM_HIGH, sd ? SPECTRUM_HEADER_BG : SPECTRUM_BORDER);
    tft.print(sd ? "SD" : "LF");

    // 3. REC indicator at x=135
    if (sessionRecorder.isRecording()) {
        tft.fillCircle(137, 7, 3, SPECTRUM_CRITICAL);
    }

    // 4. SIM or RX/TX at x=143
    tft.setCursor(143, 3);
    if (appState.simulationMode) {
        tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_HEADER_BG);
        tft.print("SIM");
    } else if (appState.jamming || subGhzRawService.isRfTesting()) {
        tft.setTextColor(SPECTRUM_CRITICAL, SPECTRUM_HEADER_BG);
        tft.print("TX");
    } else {
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_HEADER_BG);
        tft.print("RX");
    }
}

void DisplayManager::drawModernHeader(const char* title, uint16_t accent, int page, int totalPages) {
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
    tft.setCursor(16, 3);
    const int maxTitleLen = (totalPages > 0) ? 19 : 14;
    if (title) {
        if (static_cast<int>(strlen(title)) <= maxTitleLen) {
            tft.print(title);
        } else {
            char buf[24];
            strncpy(buf, title, maxTitleLen - 1);
            buf[maxTitleLen - 1] = '~';
            buf[maxTitleLen] = '\0';
            tft.print(buf);
        }
    }

    if (totalPages > 0) {
        tft.fillRoundRect(137, 2, 21, 10, 3, SPECTRUM_BORDER);
        tft.setCursor(139, 3);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_BORDER);
        tft.printf("%d/%d", page, totalPages);
    } else {
        drawStatusBar();
    }
}

void DisplayManager::showToast(const char* message, ToastType type, uint16_t durationMs) {
    if (!message) return;
    snprintf(toastMessage, sizeof(toastMessage), "%s", message);
    toastType = type;
    toastStartMs = millis();
    toastDurationMs = durationMs;
    toastActive = true;
    needRedraw = true;
}

void DisplayManager::drawToastOverlay() {
    if (!toastActive) return;
    const unsigned long now = millis();
    if (now - toastStartMs >= toastDurationMs) {
        toastActive = false;
        needRedraw = true;
        return;
    }

    uint16_t borderColor = SPECTRUM_ACCENT;
    uint16_t iconColor = SPECTRUM_ACCENT;
    const char* icon = "[i]";
    if (toastType == TOAST_SUCCESS) {
        borderColor = SPECTRUM_LOW;
        iconColor = SPECTRUM_LOW;
        icon = "[OK]";
    } else if (toastType == TOAST_WARN) {
        borderColor = SPECTRUM_HIGH;
        iconColor = SPECTRUM_HIGH;
        icon = "[!]";
    } else if (toastType == TOAST_ERROR) {
        borderColor = SPECTRUM_CRITICAL;
        iconColor = SPECTRUM_CRITICAL;
        icon = "[X]";
    }

    const int w = min(152, (static_cast<int>(strlen(toastMessage)) + 5) * 6 + 10);
    const int x = (160 - w) / 2;
    const int y = 88;

    tft.fillRoundRect(x, y, w, 15, 3, SPECTRUM_CARD_BG);
    tft.drawRoundRect(x, y, w, 15, 3, borderColor);
    tft.setCursor(x + 4, y + 4);
    tft.setTextColor(iconColor, SPECTRUM_CARD_BG);
    tft.print(icon);
    tft.print(" ");
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.print(toastMessage);
}

void DisplayManager::showActionableError(const char* title, const char* subtitle, const char* details) {
    errorModalTitle = title ? title : "ERROR";
    errorModalSubtitle = subtitle ? subtitle : "";
    errorModalDetails = details ? details : "";
    errorModalShowingDetails = false;
    errorModalActive = true;
    needRedraw = true;
}

void DisplayManager::closeActionableError() {
    errorModalActive = false;
    errorModalShowingDetails = false;
    needRedraw = true;
}

void DisplayManager::drawActionableErrorModal() {
    if (!errorModalActive) return;

    tft.fillRoundRect(8, 16, 144, 88, 5, SPECTRUM_CARD_BG);
    tft.drawRoundRect(8, 16, 144, 88, 5, SPECTRUM_CRITICAL);

    if (!errorModalShowingDetails) {
        tft.fillRoundRect(10, 18, 140, 14, 3, SPECTRUM_HEADER_BG);
        tft.setCursor(14, 21);
        tft.setTextColor(SPECTRUM_CRITICAL, SPECTRUM_HEADER_BG);
        tft.print("[!] ");
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
        tft.print(errorModalTitle);

        tft.setCursor(14, 38);
        tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
        tft.print(errorModalSubtitle);

        tft.setCursor(14, 52);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("Backend: ");
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.print(storageManager.backendName());

        tft.setCursor(14, 64);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("Status : ");
        tft.setTextColor(storageManager.usingSd() ? SPECTRUM_LOW : SPECTRUM_CRITICAL, SPECTRUM_CARD_BG);
        tft.print(storageManager.sdStatus());

        tft.fillRoundRect(12, 84, 66, 16, 3, SPECTRUM_HEADER_BG);
        tft.drawRoundRect(12, 84, 66, 16, 3, SPECTRUM_BORDER);
        tft.setCursor(16, 88);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_HEADER_BG);
        tft.print("A: DETAILS");

        tft.fillRoundRect(82, 84, 66, 16, 3, SPECTRUM_HEADER_BG);
        tft.drawRoundRect(82, 84, 66, 16, 3, SPECTRUM_BORDER);
        tft.setCursor(90, 88);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
        tft.print("B: CLOSE");
    } else {
        tft.fillRoundRect(10, 18, 140, 14, 3, SPECTRUM_HEADER_BG);
        tft.setCursor(14, 21);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_HEADER_BG);
        tft.print("ERROR DETAILS");

        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        int lineY = 36;
        int start = 0;
        for (int i = 0; i <= errorModalDetails.length(); ++i) {
            if (i == errorModalDetails.length() || errorModalDetails[i] == '\n') {
                String sub = errorModalDetails.substring(start, i);
                tft.setCursor(12, lineY);
                tft.print(sub);
                lineY += 10;
                start = i + 1;
                if (lineY > 74) break;
            }
        }

        tft.fillRoundRect(12, 84, 66, 16, 3, SPECTRUM_HEADER_BG);
        tft.drawRoundRect(12, 84, 66, 16, 3, SPECTRUM_BORDER);
        tft.setCursor(16, 88);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_HEADER_BG);
        tft.print("A: SUMMARY");

        tft.fillRoundRect(82, 84, 66, 16, 3, SPECTRUM_HEADER_BG);
        tft.drawRoundRect(82, 84, 66, 16, 3, SPECTRUM_BORDER);
        tft.setCursor(90, 88);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
        tft.print("B: CLOSE");
    }
}

void DisplayManager::drawEmptyState(const char* title, const char* message, const char* actionA, const char* actionB) {
    tft.fillRoundRect(8, 20, 144, 82, 5, SPECTRUM_CARD_BG);
    tft.drawRoundRect(8, 20, 144, 82, 5, SPECTRUM_BORDER);

    tft.setCursor(max(0, static_cast<int>(centeredTextX(title ? title : "", 1))), 26);
    tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
    tft.print(title ? title : "");

    tft.drawFastHLine(14, 37, 132, SPECTRUM_GRID);

    int y = 43;
    if (message && message[0]) {
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        int start = 0;
        int len = strlen(message);
        for (int i = 0; i <= len; ++i) {
            if (i == len || message[i] == '\n') {
                String sub = String(message).substring(start, i);
                tft.setCursor(max(0, static_cast<int>(centeredTextX(sub, 1))), y);
                tft.print(sub);
                y += 11;
                start = i + 1;
                if (y > 75) break;
            }
        }
    }

    drawModernFooter(actionA && actionA[0] ? actionA : "", "", actionB && actionB[0] ? actionB : "B BACK");
}

void DisplayManager::drawFooterChip(int x, int width, const char* label) {
    if (!label || !label[0]) return;
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
    
    char buf[12];
    int maxChars = (width - 4) / 6;
    if (maxChars < 1) maxChars = 1;
    strncpy(buf, label, maxChars);
    buf[maxChars] = '\0';
    int textLen = strlen(buf);
    int textX = x + (width - textLen * 6) / 2;
    if (textX < x + 1) textX = x + 1;
    tft.setCursor(textX, 111);
    tft.setTextColor(SPECTRUM_ACCENT, DISPLAY_FOOTER_BG);
    tft.print(buf);
}

void DisplayManager::drawThemeAnimation() {
    if (!appState.animationsEnabled || !appState.themeAnimationEnabled) return;
    const unsigned long now = millis();
    // Keep decoration off timing-sensitive RF and full-surface media screens.
    if (subGhzReplayActive || subGhzRawService.isRecording() ||
        subGhzRawService.isRfTesting() || appState.appMode == APP_MODE_VIDEO_PLAYER ||
        appState.appMode == APP_MODE_PHOTO_VIEWER || appState.appMode == APP_MODE_REBOOT ||
        appState.appMode == APP_MODE_SHUTDOWN) return;
    if (now - lastThemeAnimationMs < appState.scaledAnimationDelay(180)) return;
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
