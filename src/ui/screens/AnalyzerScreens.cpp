#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "drivers/RadioManager.h"
#include "services/SessionRecorder.h"
#include "core/RfEnvironmentMath.h"

using namespace DisplayUi;

static void populateSpectrumTelemetry(SpectrumScanTelemetry& tel) {
    tel.levels = appState.spectrumLevels;
    tel.peakLevels = appState.peakLevels;
    appState.getAnalyzerChannelRange(tel.minChannel, tel.maxChannel);
    tel.cursorChannel = appState.cursorChannel;
    tel.peakChannel = appState.peakChannel;
    tel.peakLevel = appState.peakLevel;
    tel.zoom = appState.analyzerZoom;
    tel.traceModeName = appState.getAnalyzerTraceModeName();
    tel.confidence = appState.analyzerConfidence;
    tel.isFrozen = appState.analyzerFrozen;
    tel.autoScale = appState.analyzerAutoScale;
    tel.baselineValid = appState.baselineValid;
    tel.baselineLevels = appState.baselineLevels;
    tel.eventThreshold = appState.eventThreshold;
}

void DisplayManager::drawSpectrumGrid() {
    drawModernHeader(appState.simulationMode ? "SIM SPECTRUM" : "SPECTRUM", TacticalColor::Cyan);
    SpectrumScanTelemetry tel;
    populateSpectrumTelemetry(tel);
    spectrumScannerView.renderStaticReticle(tft, tel);

    if (appState.analyzerFrozen) {
        drawModernFooter("U/D CUR", "SEL LIVE", "BACK EXIT");
    } else {
        drawModernFooter("U/D BND", "SEL HOLD", "BACK EXIT");
    }
}

void DisplayManager::drawSpectrumBars() {
    SpectrumScanTelemetry tel;
    populateSpectrumTelemetry(tel);
    spectrumScannerView.renderLiveTrace(tft, tel);
}

void DisplayManager::renderSpectrumAnalyzer() {
    if (needRedraw) {
        drawSpectrumGrid();
        spectrumScannerView.resetCache();
        needRedraw = false;
    }

    unsigned long now = millis();
    if (lastSpectrumRenderMs != 0 && now - lastSpectrumRenderMs < 40) return;
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
        drawModernFooter("U/D BND", "A CLEAR", "B BACK");
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
        drawModernHeader("CH SURVEY", SPECTRUM_LOW);
        tft.fillRoundRect(5, 17, 150, 86, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 86, 4, SPECTRUM_BORDER);
        drawModernFooter("U/D BND", "A RESET", "B BACK");
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
        drawModernFooter("U/D THR", "A CLEAR", "B BACK");
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
    drawModernHeader("LOGGING", appState.loggingEnabled ?
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
    tft.print("USB + ");
    tft.print(sessionRecorder.storageName());
    tft.print(" CSV");
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
    drawModernHeader("RADIO DIAG", SPECTRUM_ACCENT);
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
    drawModernFooter("", "A SCAN", "B BACK");
    needRedraw = false;
}

void DisplayManager::renderProfilesScreen() {
    drawModernHeader("PROFILES", SPECTRUM_ACCENT);
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
    drawModernFooter("U/D SEL", "A SAMPLE", "B BACK");
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
        drawModernHeader("CH INSPECT", TacticalColor::Cyan);

        // Top info card
        TacticalWidgets::drawTacticalCard(tft, 4, 16, 152, 28, false,
                                         TacticalColor::Wireframe,
                                         TacticalColor::DarkCharcoal);

        // Bottom signal card
        TacticalWidgets::drawTacticalCard(tft, 4, 46, 152, 55, false,
                                         TacticalColor::Wireframe,
                                         TacticalColor::DarkCharcoal);

        int ch = appState.inspectedChannel;
        int freq = 2400 + ch;

        // Channel Badge & Frequency
        TacticalWidgets::drawChannelBadge(tft, 8, 19, ch, TacticalColor::Cyan);

        tft.setTextSize(1);
        tft.setTextWrap(false);
        tft.setCursor(44, 20);
        tft.setTextColor(TacticalColor::HighWhite, TacticalColor::DarkCharcoal);
        tft.print(freq);
        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
        tft.print(" MHz");

        // Protocol Information Mapping
        tft.setCursor(44, 31);
        tft.setTextColor(TacticalColor::Cyan, TacticalColor::DarkCharcoal);
        if (ch >= 1 && ch <= 73) {
            tft.print("Wi-Fi Spectrum");
        } else if (ch >= 74 && ch <= 80) {
            tft.print("Bluetooth Band");
        } else {
            tft.print("RF Extended");
        }

        tft.setCursor(42, 51);
        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
        tft.print("SIGNAL RSSI");

        tft.drawRect(8, 62, 144, 12, TacticalColor::Wireframe);
        drawModernFooter("U/D +/-", "SEL +10", "BACK EXIT");

        needRedraw = false;
        previousInspectedLevel = 0xFF;
        previousInspectedPeak = 0xFF;
        carrierStatusValid = false;
    }

    unsigned long now = millis();
    if (lastInspectorRenderMs != 0 && now - lastInspectorRenderMs < 40) return;
    lastInspectorRenderMs = now;

    const bool valuesChanged = previousInspectedLevel != appState.inspectedLevel ||
                               previousInspectedPeak != appState.inspectedPeak;
    const bool carrierDetected = appState.inspectedLevel > 20;

    // ---------------------------------------------------------------
    // DYNAMIC part: per-frame update without a full-screen clear (anti-flicker)
    // ---------------------------------------------------------------
    if (valuesChanged) {
        // Draw micro-widget RSSI bar in top-left of signal card
        TacticalWidgets::drawRssiBar(tft, 8, 50, appState.inspectedLevel, 5, 4, 1, 9);

        // Clear signal bar interior
        tft.fillRect(10, 64, 140, 8, TacticalColor::DarkCharcoal);

        int barW = map(appState.inspectedLevel, 0, 100, 0, 140);
        if (barW > 0) {
            uint16_t col = TacticalColor::SuccessGreen;
            if (appState.inspectedLevel > 80) col = TacticalColor::ThreatRed;
            else if (appState.inspectedLevel > 50) col = TacticalColor::AmberOrange;
            else if (appState.inspectedLevel > 25) col = TacticalColor::Cyan;
            tft.fillRect(10, 64, barW, 8, col);
        }

        int peakX = 10 + map(appState.inspectedPeak, 0, 100, 0, 139);
        tft.drawFastVLine(peakX, 63, 10, TacticalColor::HighWhite);

        tft.fillRect(8, 77, 144, 10, TacticalColor::DarkCharcoal);
        tft.setCursor(10, 78);
        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
        tft.print("LVL: ");
        tft.setTextColor(TacticalColor::HighWhite, TacticalColor::DarkCharcoal);
        tft.print(appState.inspectedLevel);
        tft.print("%  ");

        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
        tft.print("PEAK: ");
        tft.setTextColor(TacticalColor::Cyan, TacticalColor::DarkCharcoal);
        tft.print(appState.inspectedPeak);
        tft.print("%");

        previousInspectedLevel = appState.inspectedLevel;
        previousInspectedPeak = appState.inspectedPeak;
    }

    if (!carrierStatusValid || previousCarrierDetected != carrierDetected) {
        tft.fillRect(8, 88, 144, 10, TacticalColor::DarkCharcoal);
        tft.setCursor(10, 89);
        tft.setTextColor(carrierDetected ? TacticalColor::ThreatRed : TacticalColor::SuccessGreen,
                         TacticalColor::DarkCharcoal);
        tft.print(carrierDetected ? "> CARRIER DETECTED" : "> NO RF ACTIVITY");
        previousCarrierDetected = carrierDetected;
        carrierStatusValid = true;
    }
}

// =============================================================================
// SESSION MANAGER SCREEN (Requirement 12)
// =============================================================================
void DisplayManager::renderDataMenuScreen() {
    drawModernHeader("DATA", SPECTRUM_ACCENT);
    const char* labels[3] = {"SESSIONS", "STORAGE HEALTH", "EVENT LOG"};
    const char* details[3] = {"record / compare / export", "SD + LittleFS status", "persistent diagnostics"};
    for (uint8_t item = 0; item < 3; ++item) {
        const int y = 19 + item * 27;
        const bool selected = item == dataMenuSelection;
        const uint16_t background = selected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;
        tft.fillRoundRect(7, y, 146, 22, 4, background);
        tft.drawRoundRect(7, y, 146, 22, 4, selected ? SPECTRUM_ACCENT : SPECTRUM_BORDER);
        tft.setCursor(13, y + 4); tft.setTextColor(selected ? SPECTRUM_ACCENT : ST77XX_WHITE, background); tft.print(labels[item]);
        tft.setCursor(13, y + 13); tft.setTextColor(ST77XX_GRAY, background); tft.print(details[item]);
    }
    drawModernFooter("U/D SEL", "A OPEN", "B BACK");
}

void DisplayManager::renderSessionManagerScreen() {
    drawModernHeader("SESSIONS", SPECTRUM_ACCENT);

    tft.fillRoundRect(4, 16, 152, 88, 4, SPECTRUM_CARD_BG);
    tft.drawRoundRect(4, 16, 152, 88, 4, SPECTRUM_BORDER);

    const bool isRec = sessionRecorder.isRecording();
    const bool hasCur = sessionRecorder.hasCurrentSession();
    const bool hasPrev = sessionRecorder.hasPreviousSession();

    const char* options[6] = {
        isRec ? "RECORD: [ACTIVE]" : "RECORD: [STOPPED]",
        "CURRENT SESSION",
        "PREVIOUS SESSION",
        "COMPARE SESSIONS",
        "EXPORT CSV",
        "DELETE CURRENT"
    };

    uint32_t sweeps = 0;
    uint8_t peakCh = 0, peakLvl = 0, avg = 0;
    if (hasCur) {
        sessionRecorder.summarizeCurrent(sweeps, peakCh, peakLvl, avg);
    }

    for (int i = 0; i < 6; ++i) {
        const int y = 18 + i * 14;
        const bool isSelected = (i == sessionManagerSelection);
        const uint16_t rowBg = isSelected ? SPECTRUM_HEADER_BG : SPECTRUM_CARD_BG;

        tft.fillRect(6, y, 148, 13, rowBg);
        if (isSelected) {
            tft.drawRoundRect(6, y, 148, 13, 2, SPECTRUM_ACCENT);
        }

        tft.setCursor(10, y + 3);
        uint16_t textColor = isSelected ? ST77XX_WHITE : ST77XX_GRAY;
        if (i == 0) textColor = isRec ? SPECTRUM_CRITICAL : SPECTRUM_LOW;
        else if (i == 5) textColor = SPECTRUM_CRITICAL;
        else if (i == 3) textColor = SPECTRUM_ACCENT;

        tft.setTextColor(textColor, rowBg);
        tft.print(options[i]);

        // Right side info
        tft.setCursor(108, y + 3);
        if (i == 0 && isRec) {
            tft.setTextColor(SPECTRUM_CRITICAL, rowBg);
            tft.print("REC");
        } else if (i == 1 && hasCur) {
            tft.setTextColor(SPECTRUM_ACCENT, rowBg);
            tft.printf("%lu swp", sweeps);
        } else if (i == 2 && hasPrev) {
            tft.setTextColor(SPECTRUM_LOW, rowBg);
            tft.print("SAVED");
        }
    }

    drawModernFooter("U/D SEL", "A SELECT", "B BACK");
    needRedraw = false;
}

// =============================================================================
// SESSION COMPARE SCREEN (Requirement 6)
// =============================================================================
void DisplayManager::renderSessionCompareScreen() {
    if (!sessionRecorder.hasCurrentSession() || !sessionRecorder.hasPreviousSession()) {
        drawModernHeader("COMPARE", SPECTRUM_ACCENT);
        drawEmptyState("NO PREV SESSION", "Record two sessions\nto compare results.", "A RECORD", "B BACK");
        needRedraw = false;
        return;
    }

    SessionComparison comp;
    if (!sessionRecorder.compareWithPrevious(comp)) {
        drawModernHeader("COMPARE", SPECTRUM_ACCENT);
        drawEmptyState("COMPARE FAILED", "Session files invalid.", "A RETRY", "B BACK");
        needRedraw = false;
        return;
    }

    drawModernHeader("COMPARE", SPECTRUM_ACCENT);

    // Summary Card
    tft.fillRoundRect(4, 16, 152, 42, 4, SPECTRUM_CARD_BG);
    tft.drawRoundRect(4, 16, 152, 42, 4, SPECTRUM_BORDER);

    // Delta line
    tft.setCursor(8, 20);
    tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
    tft.print("ENV CHANGE: ");
    uint16_t deltaColor = comp.averageDelta > 0 ? SPECTRUM_CRITICAL : (comp.averageDelta < 0 ? SPECTRUM_LOW : ST77XX_WHITE);
    tft.setTextColor(deltaColor, SPECTRUM_CARD_BG);
    tft.printf("%+d%%", comp.averageDelta);

    // Mini comparative bars on top right
    tft.fillRoundRect(106, 20, 44, 6, 1, DISPLAY_BAR_TRACK);
    tft.fillRoundRect(106, 20, map(comp.previousAverage, 0, 100, 0, 44), 6, 1, SPECTRUM_ACCENT);
    tft.fillRoundRect(106, 28, 44, 6, 1, DISPLAY_BAR_TRACK);
    tft.fillRoundRect(106, 28, map(comp.currentAverage, 0, 100, 0, 44), 6, 1, SPECTRUM_HIGH);

    // PREV stats
    tft.setCursor(8, 32);
    tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
    tft.printf("PREV %2u%% CH%2u", comp.previousAverage, comp.previousPeakChannel);

    // NOW stats
    tft.setCursor(8, 44);
    tft.setTextColor(SPECTRUM_HIGH, SPECTRUM_CARD_BG);
    tft.printf("NOW  %2u%% CH%2u", comp.currentAverage, comp.currentPeakChannel);

    // Top Deltas Card
    tft.fillRoundRect(4, 60, 152, 44, 4, SPECTRUM_CARD_BG);
    tft.drawRoundRect(4, 60, 152, 44, 4, SPECTRUM_BORDER);

    for (int i = 0; i < 4; ++i) {
        const int y = 63 + i * 10;
        const auto& d = comp.topDeltas[i];
        tft.setCursor(8, y);
        tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.printf("CH%-2u ", d.channel);

        uint16_t dColor = d.delta > 0 ? SPECTRUM_CRITICAL : (d.delta < 0 ? SPECTRUM_LOW : ST77XX_GRAY);
        tft.setTextColor(dColor, SPECTRUM_CARD_BG);
        tft.printf("%+3d%%", d.delta);

        // Delta mini bar
        const int barW = map(abs(d.delta), 0, 100, 0, 50);
        tft.fillRect(66, y + 2, 50, 5, DISPLAY_BAR_TRACK);
        if (barW > 0) {
            tft.fillRect(66, y + 2, barW, 5, dColor);
        }
    }

    drawModernFooter("A RECORD", "", "B BACK");
    needRedraw = false;
}
