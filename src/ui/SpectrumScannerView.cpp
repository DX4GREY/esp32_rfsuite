#include "ui/SpectrumScannerView.h"
#include "ui/TacticalWidgets.h"

SpectrumScannerView::SpectrumScannerView() {
    resetCache();
}

void SpectrumScannerView::resetCache() {
    memset(_prevLevels, 0xFF, sizeof(_prevLevels));
    memset(_prevPeaks, 0xFF, sizeof(_prevPeaks));
    _prevCursorX = -1;
    _prevPeakChannel = -1;
    _prevPeakLevel = 0xFF;
    _prevCursorChannel = -1;
    _prevCursorLevel = 0xFF;
}

void SpectrumScannerView::renderStaticReticle(Adafruit_ST7735& tft, const SpectrumScanTelemetry& tel) {
    // 1. Chart Frame: 1px wireframe border, dark charcoal surface
    tft.fillRect(16, 15, 130, 67, TacticalColor::DarkCharcoal);
    tft.drawRect(16, 15, 130, 67, TacticalColor::Wireframe);

    // 2. Corner tactical reticle brackets
    tft.drawPixel(16, 15, TacticalColor::Cyan);
    tft.drawPixel(145, 15, TacticalColor::Cyan);
    tft.drawPixel(16, 81, TacticalColor::Cyan);
    tft.drawPixel(145, 81, TacticalColor::Cyan);

    // 3. Grid dotted lines
    for (int x = GRAPH_X; x < GRAPH_X + GRAPH_W; x += 4) {
        tft.drawPixel(x, GRAPH_Y, TacticalColor::Wireframe);
        tft.drawPixel(x, GRAPH_Y + (GRAPH_H / 2), TacticalColor::Wireframe);
    }
    tft.drawFastHLine(GRAPH_X, GRAPH_BASELINE, GRAPH_W, TacticalColor::WireframeLight);

    // 4. Minimal Y-axis labels
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::PureBlack);
    tft.setCursor(0, GRAPH_Y - 2);
    tft.print(tel.autoScale ? "AUT" : "100");
    tft.setCursor(3, GRAPH_Y + (GRAPH_H / 2) - 3);
    tft.print("50");
    tft.setCursor(9, GRAPH_BASELINE - 5);
    tft.print("0");

    // 5. Lower metrics area
    tft.fillRect(0, 83, 160, 27, TacticalColor::PureBlack);

    resetCache();
}

void SpectrumScannerView::renderLiveTrace(Adafruit_ST7735& tft, const SpectrumScanTelemetry& tel) {
    if (!tel.levels || !tel.peakLevels) return;

    // Track peak history sparkline
    unsigned long now = millis();
    if (now - _lastSparklineUpdateMs >= 200) {
        _lastSparklineUpdateMs = now;
        if (_sparklineCount < SPARKLINE_SAMPLES) {
            _peakHistory[_sparklineCount++] = tel.peakLevel;
        } else {
            memmove(&_peakHistory[0], &_peakHistory[1], SPARKLINE_SAMPLES - 1);
            _peakHistory[SPARKLINE_SAMPLES - 1] = tel.peakLevel;
        }
    }

    const int visibleCount = max(1, (tel.maxChannel - tel.minChannel + 1) / max<uint8_t>(1, tel.zoom));
    const int center = constrain(tel.cursorChannel, tel.minChannel, tel.maxChannel);
    int visibleMin = constrain(center - visibleCount / 2, tel.minChannel,
                               max(tel.minChannel, tel.maxChannel - visibleCount + 1));
    const int visibleMax = min(tel.maxChannel, visibleMin + visibleCount - 1);
    const int visibleSpan = max(1, visibleMax - visibleMin);

    const int cursorX = GRAPH_X +
        ((constrain(tel.cursorChannel, visibleMin, visibleMax) - visibleMin) *
         (GRAPH_W - 1)) / visibleSpan;

    // Clean old cursor triangle columns if cursor moved
    if (_prevCursorX >= GRAPH_X && _prevCursorX != cursorX) {
        const int oldPixel = _prevCursorX - GRAPH_X;
        for (int p = max(0, oldPixel - 2); p <= min(GRAPH_W - 1, oldPixel + 2); ++p) {
            _prevLevels[p] = 0xFF;
            _prevPeaks[p] = 0xFF;
        }
    }

    const uint8_t maxScale = tel.autoScale ? max<uint8_t>(25, tel.peakLevel) : 100;

    tft.startWrite();
    for (int pixel = 0; pixel < GRAPH_W; pixel++) {
        const int ch = visibleMin + (pixel * visibleSpan) / (GRAPH_W - 1);
        uint8_t lvl = tel.levels[ch];
        uint8_t peak = tel.peakLevels[ch];

        if (_prevLevels[pixel] == lvl && _prevPeaks[pixel] == peak) {
            continue;
        }

        int x = GRAPH_X + pixel;
        int barHeight = (lvl * GRAPH_H) / maxScale;
        int peakHeight = (peak * GRAPH_H) / maxScale;
        int peakTop = GRAPH_BASELINE - peakHeight;

        // Reset column to dark charcoal card background
        tft.writeFastVLine(x, GRAPH_Y, GRAPH_H + 1, TacticalColor::DarkCharcoal);

        // Grid dots
        if ((pixel % 4) == 0) {
            tft.writePixel(x, GRAPH_Y, TacticalColor::Wireframe);
            tft.writePixel(x, GRAPH_Y + (GRAPH_H / 2), TacticalColor::Wireframe);
        }
        tft.writePixel(x, GRAPH_BASELINE, TacticalColor::WireframeLight);

        // Threshold guideline
        if (tel.eventThreshold > 0 && tel.eventThreshold <= 100) {
            const int thrY = GRAPH_BASELINE - (tel.eventThreshold * GRAPH_H) / maxScale;
            if (thrY >= GRAPH_Y && thrY <= GRAPH_BASELINE && (pixel % 6) == 0) {
                tft.writePixel(x, thrY, TacticalColor::AmberOrange);
            }
        }

        // Baseline guideline
        if (tel.baselineValid && tel.baselineLevels && tel.baselineLevels[ch] > 0) {
            const int baseTop = GRAPH_BASELINE - (tel.baselineLevels[ch] * GRAPH_H) / maxScale;
            if (baseTop >= GRAPH_Y && baseTop <= GRAPH_BASELINE) {
                tft.writePixel(x, baseTop, TacticalColor::Cyan);
            }
        }

        // Tactical 4-Zone Intensity Gradient
        if (barHeight > 0) {
            int remaining = barHeight;
            int curY = GRAPH_BASELINE;

            // Zone 1: Green (low 0..30%)
            int seg = min(remaining, (GRAPH_H * 30) / 100);
            curY -= seg;
            tft.writeFastVLine(x, curY, seg, TacticalColor::SuccessGreen);
            remaining -= seg;

            // Zone 2: Cyan (mid 30..65%)
            seg = min(remaining, (GRAPH_H * 35) / 100);
            curY -= seg;
            if (seg > 0) tft.writeFastVLine(x, curY, seg, TacticalColor::Cyan);
            remaining -= seg;

            // Zone 3: Amber (high 65..85%)
            seg = min(remaining, (GRAPH_H * 20) / 100);
            curY -= seg;
            if (seg > 0) tft.writeFastVLine(x, curY, seg, TacticalColor::AmberOrange);
            remaining -= seg;

            // Zone 4: Threat Red (peak 85..100%)
            if (remaining > 0) {
                curY -= remaining;
                tft.writeFastVLine(x, curY, remaining, TacticalColor::ThreatRed);
            }
        }

        // Peak Hold Dot (High White)
        if (peakHeight > 0 && peakTop >= GRAPH_Y) {
            tft.writePixel(x, peakTop, TacticalColor::HighWhite);
        }

        _prevLevels[pixel] = lvl;
        _prevPeaks[pixel] = peak;
    }
    tft.endWrite();

    // Cursor reticle chevron
    tft.fillTriangle(cursorX - 2, GRAPH_Y, cursorX + 2, GRAPH_Y,
                     cursorX, GRAPH_Y + 3, TacticalColor::HighWhite);
    _prevCursorX = cursorX;

    // --- Tactical Micro-Widgets in Bottom HUD (y=84..108) ---
    uint8_t cursorLvl = tel.levels[tel.cursorChannel];

    if (_prevCursorChannel != tel.cursorChannel || _prevCursorLevel != cursorLvl) {
        // Channel & Frequency
        tft.fillRect(2, 84, 52, 9, TacticalColor::PureBlack);
        tft.setTextSize(1);
        tft.setCursor(3, 85);
        tft.setTextColor(tel.isFrozen ? TacticalColor::AmberOrange : TacticalColor::Cyan,
                         TacticalColor::PureBlack);
        tft.print(tel.isFrozen ? "[H]" : "[L]");
        tft.print(" ");
        tft.setTextColor(TacticalColor::HighWhite, TacticalColor::PureBlack);
        tft.print(tel.cursorChannel);

        // Micro-widget: 5-segment RSSI bar
        TacticalWidgets::drawRssiBar(tft, 32, 85, cursorLvl, 5, 2, 1, 7);

        // Cursor percentage
        tft.fillRect(49, 84, 25, 9, TacticalColor::PureBlack);
        tft.setCursor(50, 85);
        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::PureBlack);
        tft.print(cursorLvl);
        tft.print("%");

        _prevCursorChannel = tel.cursorChannel;
        _prevCursorLevel = cursorLvl;
    }

    if (_prevPeakChannel != tel.peakChannel || _prevPeakLevel != tel.peakLevel) {
        // Peak Sparkline Micro-widget (x=78..110)
        TacticalWidgets::drawSparkline(tft, 78, 84, 30, 9, _peakHistory,
                                       _sparklineCount,
                                       TacticalColor::AmberOrange,
                                       TacticalColor::Wireframe);

        // Peak channel callout
        tft.fillRect(112, 84, 46, 9, TacticalColor::PureBlack);
        tft.drawRect(112, 84, 46, 9, TacticalColor::Wireframe);
        tft.setCursor(114, 85);
        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::PureBlack);
        tft.print("P:");
        tft.setTextColor(TacticalColor::AmberOrange, TacticalColor::PureBlack);
        tft.print(tel.peakChannel);
        tft.print(" ");
        tft.print(tel.peakLevel);

        _prevPeakChannel = tel.peakChannel;
        _prevPeakLevel = tel.peakLevel;
    }

    // Secondary sub-line (y=96..105): Trace mode, Zoom, and Frequency callout
    tft.setCursor(3, 96);
    tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::PureBlack);
    tft.print(2400 + tel.cursorChannel);
    tft.print("MHz ");
    tft.setTextColor(TacticalColor::Cyan, TacticalColor::PureBlack);
    tft.print(tel.traceModeName ? tel.traceModeName : "LIVE");
    tft.print(" x");
    tft.print(tel.zoom);

    // Confidence badge
    tft.setCursor(102, 96);
    tft.setTextColor(tel.confidence >= 70 ? TacticalColor::SuccessGreen :
                     (tel.confidence >= 35 ? TacticalColor::Cyan : TacticalColor::AmberOrange),
                     TacticalColor::PureBlack);
    tft.print(tel.confidence >= 70 ? "HIGH CONF" : (tel.confidence >= 35 ? "GOOD" : "LOW DATA"));
}
