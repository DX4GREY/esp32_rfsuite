#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
class Adafruit_ST7735;
class Adafruit_GFX;
#endif
#include "ui/TacticalTheme.h"

/**
 * @file SpectrumScannerView.h
 * @brief Decoupled Live Spectrum Scanner view with micro-widgets for ESP32-DIV.
 */

struct SpectrumScanTelemetry {
    const uint8_t* levels = nullptr;        // Array of 126 channels (0..100)
    const uint8_t* peakLevels = nullptr;    // Peak hold (0..100)
    int minChannel = 0;
    int maxChannel = 125;
    int cursorChannel = 36;
    int peakChannel = 0;
    uint8_t peakLevel = 0;
    uint8_t zoom = 1;
    const char* traceModeName = "LIVE";
    uint8_t confidence = 0;
    bool isFrozen = false;
    bool autoScale = false;
    bool baselineValid = false;
    const uint8_t* baselineLevels = nullptr;
    uint8_t eventThreshold = 0;
};

class SpectrumScannerView {
public:
    static constexpr int GRAPH_X = 18;
    static constexpr int GRAPH_W = 126;
    static constexpr int GRAPH_Y = 16;
    static constexpr int GRAPH_H = 64;
    static constexpr int GRAPH_BASELINE = 80;

    SpectrumScannerView();
    ~SpectrumScannerView() = default;

    /**
     * @brief Render the static reticle and wireframe bounding frame.
     */
    void renderStaticReticle(Adafruit_ST7735& tft, const SpectrumScanTelemetry& tel);

    /**
     * @brief Render live spectrum bars with zero-flicker column differential updates.
     */
    void renderLiveTrace(Adafruit_ST7735& tft, const SpectrumScanTelemetry& tel);

    /**
     * @brief Reset cached states for full screen redraw.
     */
    void resetCache();

private:
    uint8_t _prevLevels[GRAPH_W];
    uint8_t _prevPeaks[GRAPH_W];
    int _prevCursorX = -1;
    int _prevPeakChannel = -1;
    uint8_t _prevPeakLevel = 0xFF;
    int _prevCursorChannel = -1;
    uint8_t _prevCursorLevel = 0xFF;

    // Live peak sparkline history
    static constexpr size_t SPARKLINE_SAMPLES = 16;
    uint8_t _peakHistory[SPARKLINE_SAMPLES] = {};
    size_t _sparklineCount = 0;
    unsigned long _lastSparklineUpdateMs = 0;
};
