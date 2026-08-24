#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "drivers/RadioManager.h"
#include "services/SessionRecorder.h"
#include "core/RfEnvironmentMath.h"

using namespace DisplayUi;

void DisplayManager::drawSpectrumGrid() {
    // Compact status header with a small signal glyph and band badge.
    tft.fillRect(0, 0, 160, 14, SPECTRUM_HEADER_BG);
    tft.drawFastHLine(0, 13, 160, SPECTRUM_ACCENT);
    tft.drawFastVLine(4, 8, 3, SPECTRUM_ACCENT);
    tft.drawFastVLine(7, 6, 5, SPECTRUM_ACCENT);
    tft.drawFastVLine(10, 3, 8, SPECTRUM_ACCENT);
    tft.setCursor(15, 3);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_HEADER_BG);
    tft.print(appState.simulationMode ? "SIM SPECTRUM" : "SPECTRUM");

    tft.fillRoundRect(66, 2, 31, 10, 3, SPECTRUM_BORDER);
    const char* band = compactBandName(appState.analyzerBand);
    int bandX = 66 + (31 - static_cast<int>(strlen(band)) * 6) / 2;
    tft.setCursor(bandX, 3);
    tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_BORDER);
    tft.print(band);

    // Chart card and dotted horizontal guides.
    tft.fillRect(16, 15, 130, 67, SPECTRUM_CARD_BG);
    tft.drawRect(16, 15, 130, 67, SPECTRUM_BORDER);
    for (int x = GRAPH_X_START; x < GRAPH_X_START + GRAPH_WIDTH; x += 4) {
        tft.drawPixel(x, GRAPH_Y_TOP, SPECTRUM_GRID);
        tft.drawPixel(x, GRAPH_Y_TOP + (GRAPH_HEIGHT / 2), SPECTRUM_GRID);
    }
    tft.drawFastHLine(GRAPH_X_START, GRAPH_Y_BASELINE, GRAPH_WIDTH, SPECTRUM_BORDER);

    // Minimal Y-axis labels leave more room for the actual signal plot.
    tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
    tft.setCursor(0, GRAPH_Y_TOP - 2);
    tft.print("100");
    tft.setCursor(3, GRAPH_Y_TOP + (GRAPH_HEIGHT / 2) - 3);
    tft.print("50");
    tft.setCursor(9, GRAPH_Y_BASELINE - 5);
    tft.print("0");

    int bandMin, bandMax;
    appState.getAnalyzerChannelRange(bandMin, bandMax);
    const int visibleCount = max(1, (bandMax - bandMin + 1) / appState.analyzerZoom);
    const int center = constrain(appState.cursorChannel, bandMin, bandMax);
    int visibleMin = constrain(center - visibleCount / 2, bandMin,
                               max(bandMin, bandMax - visibleCount + 1));
    const int visibleMax = min(bandMax, visibleMin + visibleCount - 1);
    tft.fillRect(16, 82, 130, 22, ST77XX_BLACK);
    tft.setCursor(17, 84);
    tft.setTextColor(SPECTRUM_ACCENT, ST77XX_BLACK);
    tft.print(2400 + visibleMin);
    tft.setCursor(111, 84);
    tft.print(2400 + visibleMax);
    tft.setCursor(58, 84);
    tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
    tft.print(appState.getAnalyzerTraceModeName());
    tft.print(" x");
    tft.print(appState.analyzerZoom);

    tft.fillRect(0, 105, 160, 23, ST77XX_BLACK);
    drawFooterChip(2, 37, appState.analyzerFrozen ? "U CH+" : "U BAND");
    String modeChip = "D ";
    modeChip += appState.analyzerFrozen ? "CH-" : appState.getAnalyzerRadioModeName();
    drawFooterChip(41, 37, modeChip.c_str());
    drawFooterChip(80, 37, appState.analyzerFrozen ? "A LIVE" : "A HOLD");
    drawFooterChip(119, 39, "B BACK");
}

void DisplayManager::drawSpectrumBars() {
    const uint8_t peakRadio1 = appState.radio1Levels[appState.peakChannel];
    const uint8_t peakRadio2 = appState.radio2Levels[appState.peakChannel];
    // Header is redrawn only when its displayed value changes.
    if (previousHeaderPeakChannel != appState.peakChannel ||
        previousHeaderPeakLevel != appState.peakLevel) {
        tft.fillRect(99, 1, 61, 11, SPECTRUM_HEADER_BG);
        tft.setCursor(99, 3);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_HEADER_BG);
        tft.print("P ");
        tft.setTextColor(getSignalColor(appState.peakLevel), SPECTRUM_HEADER_BG);
        tft.print(appState.peakChannel);
        tft.print(" ");
        tft.print(appState.peakLevel);
        tft.print("%");
        previousHeaderPeakChannel = appState.peakChannel;
        previousHeaderPeakLevel = appState.peakLevel;

        previousHeaderRadio1Level = peakRadio1;
        previousHeaderRadio2Level = peakRadio2;
    }

    int bandMin, bandMax;
    appState.getAnalyzerChannelRange(bandMin, bandMax);
    const int visibleCount = max(1, (bandMax - bandMin + 1) / appState.analyzerZoom);
    const int center = constrain(appState.cursorChannel, bandMin, bandMax);
    const int visibleMin = constrain(center - visibleCount / 2, bandMin,
                                     max(bandMin, bandMax - visibleCount + 1));
    const int visibleMax = min(bandMax, visibleMin + visibleCount - 1);
    const int visibleSpan = max(1, visibleMax - visibleMin);
    const int cursorX = GRAPH_X_START +
        ((constrain(appState.cursorChannel, visibleMin, visibleMax) - visibleMin) *
         (GRAPH_WIDTH - 1)) / visibleSpan;

    // Force only the old cursor columns dirty. This restores the graph pixels
    // under the previous triangle without repainting the complete chart.
    if (previousSpectrumCursorX >= GRAPH_X_START &&
        previousSpectrumCursorX != cursorX) {
        const int oldPixel = previousSpectrumCursorX - GRAPH_X_START;
        for (int p = max(0, oldPixel - 2); p <= min(GRAPH_WIDTH - 1, oldPixel + 2); ++p) {
            previousSpectrumLevels[p] = 0xFF;
            previousPeakLevels[p] = 0xFF;
        }
    }

    tft.startWrite();
    for (int pixel = 0; pixel < GRAPH_WIDTH; pixel++) {
        const int ch = visibleMin + (pixel * visibleSpan) / (GRAPH_WIDTH - 1);
        uint8_t lvl = appState.getTraceLevel(ch);
        uint8_t peak = appState.peakLevels[ch];
        if (previousSpectrumLevels[pixel] == lvl && previousPeakLevels[pixel] == peak) {
            continue;
        }

        int x = GRAPH_X_START + pixel;

        int barHeight = (lvl * GRAPH_HEIGHT) / 100;
        int peakHeight = (peak * GRAPH_HEIGHT) / 100;

        int peakTop = GRAPH_Y_BASELINE - peakHeight;

        // Clear only this dirty column and restore the card's dotted guides.
        tft.writeFastVLine(x, GRAPH_Y_TOP, GRAPH_HEIGHT + 1, SPECTRUM_CARD_BG);
        if ((pixel % 4) == 0) {
            tft.writePixel(x, GRAPH_Y_TOP, SPECTRUM_GRID);
            tft.writePixel(x, GRAPH_Y_TOP + (GRAPH_HEIGHT / 2), SPECTRUM_GRID);
        }
        tft.writePixel(x, GRAPH_Y_BASELINE, SPECTRUM_BORDER);

        if (barHeight > 0) {
            // A four-zone vertical gradient makes intensity readable without
            // requiring wider bars or a costly full-frame canvas.
            int remaining = barHeight;
            int cursorY = GRAPH_Y_BASELINE;
            int segment = min(remaining, (GRAPH_HEIGHT * 30) / 100);
            cursorY -= segment;
            tft.writeFastVLine(x, cursorY, segment, SPECTRUM_LOW);
            remaining -= segment;

            segment = min(remaining, (GRAPH_HEIGHT * 35) / 100);
            cursorY -= segment;
            if (segment > 0) tft.writeFastVLine(x, cursorY, segment, SPECTRUM_MID);
            remaining -= segment;

            segment = min(remaining, (GRAPH_HEIGHT * 20) / 100);
            cursorY -= segment;
            if (segment > 0) tft.writeFastVLine(x, cursorY, segment, SPECTRUM_HIGH);
            remaining -= segment;

            if (remaining > 0) {
                cursorY -= remaining;
                tft.writeFastVLine(x, cursorY, remaining, SPECTRUM_CRITICAL);
            }
        }

        // Peak marker is drawn last, so it stays visible over the gradient.
        if (peakHeight > 0 && peakTop >= GRAPH_Y_TOP) {
            tft.writePixel(x, peakTop, ST77XX_WHITE);
        }

        if (appState.watchedChannels[ch]) {
            tft.writePixel(x, GRAPH_Y_TOP + 1, SPECTRUM_ACCENT);
        }
        previousSpectrumLevels[pixel] = lvl;
        previousPeakLevels[pixel] = peak;
    }
    tft.endWrite();

    tft.fillTriangle(cursorX - 2, GRAPH_Y_TOP, cursorX + 2, GRAPH_Y_TOP,
                     cursorX, GRAPH_Y_TOP + 3, ST77XX_WHITE);
    previousSpectrumCursorX = cursorX;

    tft.fillRect(17, 95, 128, 8, ST77XX_BLACK);
    tft.setCursor(18, 95);
    tft.setTextColor(appState.analyzerFrozen ? SPECTRUM_HIGH : ST77XX_GRAY,
                     ST77XX_BLACK);
    tft.print(appState.analyzerFrozen ? "H " : "L ");
    tft.print(appState.cursorChannel);
    tft.print("/");
    tft.print(2400 + appState.cursorChannel);
    tft.print(" ");
    tft.print(appState.getTraceLevel(appState.cursorChannel));
    tft.print(appState.watchedChannels[appState.cursorChannel] ? "%*" : "%");
    const uint16_t cursorMHz=RfEnvironmentMath::frequencyMHz(appState.cursorChannel);
    const uint8_t regions=RfEnvironmentMath::protocolRegions(cursorMHz);
    tft.print(" ");
    const int8_t wifi=RfEnvironmentMath::wifiChannelForMHz(cursorMHz);
    if(wifi>0){tft.print("W");tft.print(wifi);}
    if(regions&4)tft.print("B");
    if(regions&8)tft.print("Z");
}

void DisplayManager::renderSpectrumAnalyzer() {
    if (needRedraw) {
        drawSpectrumGrid();
        memset(previousSpectrumLevels, 0xFF, sizeof(previousSpectrumLevels));
        memset(previousPeakLevels, 0xFF, sizeof(previousPeakLevels));
        previousHeaderPeakChannel = -1;
        previousHeaderPeakLevel = 0xFF;
        previousHeaderRadio1Level = 0xFF;
        previousHeaderRadio2Level = 0xFF;
        previousSpectrumCursorX = -1;
        needRedraw = false;
    }

    unsigned long now = millis();
    if (lastSpectrumRenderMs != 0 && now - lastSpectrumRenderMs < 50) return;
    lastSpectrumRenderMs = now;
    drawSpectrumBars();
}

void DisplayManager::renderWaterfallScreen() {
    if (needRedraw) {
        drawModernHeader("WATERFALL", SPECTRUM_ACCENT);
        tft.fillRect(16, 17, 130, 74, SPECTRUM_CARD_BG);
        tft.drawRect(16, 17, 130, 74, SPECTRUM_BORDER);
        tft.setCursor(1, 18);
        tft.setTextColor(ST77XX_GRAY, ST77XX_BLACK);
        tft.print("NEW");
        tft.setCursor(1, 82);
        tft.print("OLD");
        drawModernFooter("U BAND", "A CLEAR", "B BACK");
        needRedraw = false;
    }

    tft.fillRect(18, 18, GRAPH_WIDTH, 72, SPECTRUM_CARD_BG);
    const int count = appState.waterfallCount;
    for (int row = 0; row < count; row++) {
        const int source = (appState.waterfallHead - 1 - row + WATERFALL_ROWS) % WATERFALL_ROWS;
        const int y = 18 + row * 3;
        for (int ch = 0; ch < TOTAL_CHANNELS; ch++) {
            const uint8_t level = appState.waterfall[source][ch];
            const uint16_t color = level == 0 ? SPECTRUM_CARD_BG : getSignalColor(level);
            tft.drawFastVLine(18 + ch, y, 3, color);
        }
    }
}

void DisplayManager::renderSurveyScreen() {
    if (needRedraw) {
        drawModernHeader("CHANNEL SURVEY", SPECTRUM_LOW);
        tft.fillRoundRect(5, 17, 150, 86, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 86, 4, SPECTRUM_BORDER);
        drawModernFooter("", "A RESET", "B BACK");
        needRedraw = false;
    }

    tft.fillRect(9, 20, 142, 79, SPECTRUM_CARD_BG);
    tft.setCursor(10, 21);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.print("SWEEPS ");
    tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
    tft.print(appState.surveySweeps);

    int ranked[5] = {0, 0, 0, 0, 0};
    uint8_t rankedLevel[5] = {0, 0, 0, 0, 0};
    if (appState.surveySweeps > 0) {
        int minCh, maxCh;
        appState.getAnalyzerChannelRange(minCh, maxCh);
        for (int ch = minCh; ch <= maxCh; ch++) {
            const uint32_t rawAverage = appState.occupancyTotal[ch] / appState.surveySweeps;
            const uint8_t average = rawAverage > 100 ? 100 : static_cast<uint8_t>(rawAverage);
            for (int pos = 0; pos < 5; pos++) {
                if (average > rankedLevel[pos]) {
                    for (int move = 4; move > pos; move--) {
                        rankedLevel[move] = rankedLevel[move - 1];
                        ranked[move] = ranked[move - 1];
                    }
                    rankedLevel[pos] = average;
                    ranked[pos] = ch;
                    break;
                }
            }
        }
    }

    for (int row = 0; row < 5; row++) {
        const int y = 36 + row * 12;
        tft.setCursor(10, y);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print(row + 1);
        tft.print("  CH");
        tft.print(ranked[row]);
        tft.fillRect(57, y, 72, 7, DISPLAY_BAR_TRACK);
        const int width = map(rankedLevel[row], 0, 100, 0, 72);
        if (width > 0) tft.fillRect(57, y, width, 7, getSignalColor(rankedLevel[row]));
        tft.setCursor(132, y);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print(rankedLevel[row]);
        tft.print("%");
    }
}

void DisplayManager::renderEventsScreen() {
    if (needRedraw) {
        drawModernHeader("RF EVENTS", SPECTRUM_HIGH);
        tft.fillRoundRect(5, 17, 150, 86, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 86, 4, SPECTRUM_BORDER);
        drawModernFooter("U T/D H", "A CLEAR", "B BACK");
        needRedraw = false;
    }

    tft.fillRect(9, 20, 142, 79, SPECTRUM_CARD_BG);
    tft.setCursor(10, 21);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.print("T");
    tft.print(appState.eventThreshold);
    tft.print(" H");
    tft.print(appState.eventHysteresis);
    tft.print(" D");
    tft.print(appState.eventMinSweeps);
    tft.print(" M");
    tft.print(appState.eventMinChannels);
    tft.print(" #");
    tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
    tft.print(appState.eventCount);

    const int rows = appState.eventCount < 6 ? appState.eventCount : 6;
    for (int row = 0; row < rows; row++) {
        const int index = (appState.eventHead - 1 - row + RF_EVENT_COUNT) % RF_EVENT_COUNT;
        const RfEvent &event = appState.rfEvents[index];
        const unsigned long ageSec = (millis() - event.timestampMs) / 1000UL;
        const int y = 36 + row * 10;
        tft.setCursor(10, y);
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.print("CH");
        tft.print(event.channel);
        tft.setCursor(61, y);
        tft.setTextColor(getSignalColor(event.level), SPECTRUM_CARD_BG);
        tft.print(event.level);
        tft.print("%");
        tft.setCursor(108, y);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print(ageSec);
        tft.print("s");
        if (event.channelCount > 1) {
            tft.setCursor(83, y);
            tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
            tft.print("x");
            tft.print(event.channelCount);
        }
    }
}

void DisplayManager::renderLoggingScreen() {
    drawModernHeader("SERIAL LOGGING", appState.loggingEnabled ?
                     SPECTRUM_CRITICAL : SPECTRUM_ACCENT);
    tft.fillRoundRect(12, 22, 136, 76, 7, SPECTRUM_CARD_BG);
    tft.drawRoundRect(12, 22, 136, 76, 7, SPECTRUM_BORDER);
    tft.fillCircle(80, 43, 9, appState.loggingEnabled ?
                   SPECTRUM_CRITICAL : ST77XX_GRAY);
    tft.setCursor(appState.loggingEnabled ? 50 : 56, 59);
    tft.setTextColor(appState.loggingEnabled ? SPECTRUM_CRITICAL : ST77XX_GRAY,
                     SPECTRUM_CARD_BG);
    tft.print(appState.loggingEnabled ? "RECORDING" : "STOPPED");
    tft.setCursor(34, 74);
    tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
    tft.print("USB + LITTLEFS CSV");
    tft.setCursor(32, 86);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    if (sessionRecorder.isReady()) {
        tft.print(sessionRecorder.recordedSweeps());
        tft.print(" sweeps / ");
        tft.print(sessionRecorder.fileSize() / 1024);
        tft.print("K");
    } else {
        tft.print("STORAGE UNAVAILABLE");
    }
    drawModernFooter("", "A TOGGLE", "B BACK");
    needRedraw = false;
}

void DisplayManager::renderRadioDiagScreen() {
    drawModernHeader("RADIO DIAGNOSTICS", SPECTRUM_ACCENT);
    const bool ok1 = radioManager.isRadio1Connected();
    const bool ok2 = radioManager.isRadio2Connected();
    for (int radioIndex = 0; radioIndex < 2; radioIndex++) {
        const int y = 20 + radioIndex * 40;
        const bool ok = radioIndex == 0 ? ok1 : ok2;
        tft.fillRoundRect(8, y, 144, 34, 5, SPECTRUM_CARD_BG);
        tft.drawRoundRect(8, y, 144, 34, 5, ok ? SPECTRUM_LOW : SPECTRUM_CRITICAL);
        tft.fillCircle(18, y + 10, 4, ok ? SPECTRUM_LOW : SPECTRUM_CRITICAL);
        tft.setCursor(28, y + 6);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print("NRF24 RADIO ");
        tft.print(radioIndex + 1);
        tft.setCursor(28, y + 19);
        tft.setTextColor(ok ? SPECTRUM_LOW : SPECTRUM_CRITICAL, SPECTRUM_CARD_BG);
        tft.print(ok ? "CONNECTED" : "NOT DETECTED");
    }
    drawModernFooter("", "A REFRESH", "B BACK");
    needRedraw = false;
}

void DisplayManager::renderProfilesScreen() {
    drawModernHeader("SCAN PROFILES", SPECTRUM_ACCENT);
    const char* names[4] = {"FAST", "BALANCED", "DEEP", "CUSTOM"};
    const int samples[4] = {
        12,
        SPECTRUM_SAMPLES_PER_CH,
        60,
        appState.customSpectrumSamples
    };
    for (int profile = 0; profile < 4; profile++) {
        const int y = 17 + profile * 21;
        const bool selected = static_cast<int>(appState.scanProfile) == profile;
        const uint16_t background = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        tft.fillRoundRect(8, y, 144, 18, 4, background);
        tft.drawRoundRect(8, y, 144, 18, 4,
                          selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER);
        tft.setCursor(15, y + 6);
        tft.setTextColor(selected ? SPECTRUM_ACCENT : ST77XX_GRAY, background);
        tft.print(names[profile]);
        tft.setCursor(98, y + 6);
        tft.setTextColor(ST77XX_WHITE, background);
        tft.print(samples[profile]);
        tft.print(" smp");
    }
    drawModernFooter("U/D SET", "A VALUE", "B BACK");
    needRedraw = false;
}

// =============================================================================
// RENDER CHANNEL INSPECTOR (COMPACT & FIT)
// =============================================================================
void DisplayManager::renderChannelInspector() {
    // ---------------------------------------------------------------
    // STATIC part: drawn only once (when entering the mode / changing channel)
    // ---------------------------------------------------------------
    if (needRedraw) {
        drawModernHeader("CHANNEL INSPECT", SPECTRUM_ACCENT);
        tft.fillRoundRect(5, 17, 150, 27, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 27, 4, SPECTRUM_BORDER);
        tft.fillRoundRect(5, 47, 150, 48, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 47, 150, 48, 4, SPECTRUM_BORDER);

        int ch = appState.inspectedChannel;
        int freq = 2400 + ch;

        // Channel & Frequency
        tft.setCursor(10, 20);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("CH ");
        tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
        tft.print(ch);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print("  ");
        tft.print(freq);
        tft.print(" MHz");

        // Protocol Information Mapping
        tft.setCursor(10, 32);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("BAND  ");
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        if (ch >= 1 && ch <= 73) {
            tft.print("Wi-Fi Band");
        } else if (ch >= 74 && ch <= 80) {
            tft.print("BT High Band");
        } else {
            tft.print("RF Extended");
        }

        tft.setCursor(10, 51);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("LIVE SIGNAL");
        tft.drawRoundRect(10, 62, 140, 13, 3, SPECTRUM_BORDER);
        drawModernFooter("U/D +/-", "A +10", "B BACK");

        needRedraw = false;
        previousInspectedLevel = 0xFF;
        previousInspectedPeak = 0xFF;
        carrierStatusValid = false;
    }

    unsigned long now = millis();
    if (lastInspectorRenderMs != 0 && now - lastInspectorRenderMs < 50) return;
    lastInspectorRenderMs = now;

    const bool valuesChanged = previousInspectedLevel != appState.inspectedLevel ||
                               previousInspectedPeak != appState.inspectedPeak;
    const bool carrierDetected = appState.inspectedLevel > 20;

    // ---------------------------------------------------------------
    // DYNAMIC part: per-frame update without a full-screen clear (anti-flicker)
    // ---------------------------------------------------------------
    // Clear the gauge area (incl. border), then redraw border + fill
    if (valuesChanged) {
        // Keep the static border intact; update only its interior.
        tft.fillRect(12, 64, 136, 9, SPECTRUM_CARD_BG);

        int barW = map(appState.inspectedLevel, 0, 100, 0, 136);
        if (barW > 0) {
            tft.fillRect(12, 64, barW, 9, getSignalColor(appState.inspectedLevel));
        }

        int peakX = 12 + map(appState.inspectedPeak, 0, 100, 0, 135);
        tft.drawFastVLine(peakX, 63, 11, ST77XX_WHITE);

        tft.fillRect(10, 79, 140, 9, SPECTRUM_CARD_BG);
        tft.setCursor(10, 80);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("NOW ");
        tft.setTextColor(getSignalColor(appState.inspectedLevel), SPECTRUM_CARD_BG);
        tft.print(appState.inspectedLevel);
        tft.print("%   ");

        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print("PEAK ");
        tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.print(appState.inspectedPeak);
        tft.print("%");

        previousInspectedLevel = appState.inspectedLevel;
        previousInspectedPeak = appState.inspectedPeak;
    }

    if (!carrierStatusValid || previousCarrierDetected != carrierDetected) {
        tft.fillRect(10, 88, 140, 6, SPECTRUM_CARD_BG);
        tft.setCursor(10, 88);
        tft.setTextColor(carrierDetected ? SPECTRUM_CRITICAL : SPECTRUM_LOW,
                         SPECTRUM_CARD_BG);
        tft.print(carrierDetected ? "> RF ACTIVITY" : "> CHANNEL CLEAR");
        previousCarrierDetected = carrierDetected;
        carrierStatusValid = true;
    }
}

// =============================================================================
// RENDER STATUS SCREEN (COMPACT & FIT)
// =============================================================================
// =============================================================================
// Settings rendering lives in SystemScreens.cpp.
// =============================================================================
