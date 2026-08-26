#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "drivers/DisplayStorageBus.h"

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
    previousSettingsMenuLayout = -1;
    previousSettingsSniffSave = -1;
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
    tft.setRotation(3);          // Landscape 160 x 128
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.setTextSize(1);
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

    tft.fillRoundRect(45, 28, 70, 70, 6, SPECTRUM_CARD_BG);
    tft.drawRoundRect(45, 28, 70, 70, 6, SPECTRUM_BORDER);

    String title = "RF24 SUITE";
    int16_t titleX = centeredTextX(title, 1);
    tft.setCursor(titleX, 10);
    tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
    tft.print(title);
    tft.drawFastHLine(54, 21, 52, SPECTRUM_ACCENT);

    // Draw logo from byte array (centered)
    int logoWidth = 64;
    int logoHeight = 64;
    int logoX = (160 - logoWidth) / 2;
    int logoY = 31;
    // splashLogo is an RGB888 bitmap stored as 32-bit unsigned long in PROGMEM
    // (64x64 pixels, one pixel per unsigned long with format 0x00RRGGBB).
    // Adafruit_GFX::drawBitmap() expects 1-bit uint8_t data, and drawRGBBitmap()
    // expects 16-bit RGB565 — neither matches the actual data type. We draw
    // pixel-by-pixel, reading from PROGMEM and converting RGB888 -> RGB565.
    tft.startWrite();
    for (int16_t j = 0; j < logoHeight; j++) {
        for (int16_t i = 0; i < logoWidth; i++) {
            uint32_t pixel = pgm_read_dword(&splashLogo[j * logoWidth + i]);
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t b = pixel & 0xFF;
            tft.writePixel(logoX + i, logoY + j, tft.color565(r, g, b));
        }
    }
    tft.endWrite();

    // Subtitle "by Dx4Grey" (textSize 1, centered below the splashLogo)
    String subtitle = "by Dx4Grey";
    int16_t subtitleX = centeredTextX(subtitle, 1);
    tft.setTextColor(SPECTRUM_ACCENT, ST77XX_BLACK);
    tft.setCursor(subtitleX, 106);
    tft.print(subtitle);

    // Keep the splash visible for a moment before the menu appears
    delay(2500);
}

uint16_t DisplayManager::getSignalColor(uint8_t level) {
    if (level < 30) return SPECTRUM_LOW;
    if (level < 65) return SPECTRUM_MID;
    if (level < 85) return SPECTRUM_HIGH;
    return SPECTRUM_CRITICAL;
}

void DisplayManager::drawModernHeader(const char* title, uint16_t accent) {
    tft.fillRect(0, 0, 160, 14, SPECTRUM_HEADER_BG);
    tft.drawFastHLine(0, 13, 160, accent);
    tft.fillCircle(7, 7, 3, accent);
    tft.drawCircle(7, 7, 5, SPECTRUM_BORDER);
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
    tft.setCursor(centeredTextX(String(title)), 3);
    tft.print(title);
}

void DisplayManager::drawFooterChip(int x, int width, const char* label) {
    tft.fillRoundRect(x, 107, width, 16, 3, DISPLAY_FOOTER_BG);
    int textX = x + (width - static_cast<int>(strlen(label)) * 6) / 2;
    tft.setCursor(textX, 111);
    tft.setTextColor(SPECTRUM_ACCENT, DISPLAY_FOOTER_BG);
    tft.print(label);
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
