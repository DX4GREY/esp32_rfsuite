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
#ifndef GFXCANVAS16_MOCK_DEFINED
#define GFXCANVAS16_MOCK_DEFINED
class GFXcanvas16 {};
#endif
#endif
#include "ui/TacticalTheme.h"

/**
 * @file HeaderWidget.h
 * @brief Decoupled HUD Header Bar for ESP32-DIV UI.
 *
 * Implements double-buffered sprite rendering (160x14) to completely
 * eliminate display tearing and flicker.
 */

struct HeaderTelemetry {
    const char* title = nullptr;
    const char* rfMode = "RX";       // "RX", "TX", "JAM", "SCAN", "SIM", "SUBG"
    bool isTx = false;
    bool isSim = false;
    bool radio1Connected = false;
    bool radio2Connected = false;
    bool radio1Active = false;
    bool radio2Active = false;
    int activeChannel = -1;          // -1 if none
    uint8_t peakLevel = 0;           // 0..100%
    bool isRecording = false;
    bool usingSd = false;
    uint8_t batteryPercent = 100;    // 0..100
    bool batteryCharging = false;
    int page = 0;
    int totalPages = 0;
};

class HeaderWidget {
public:
    static constexpr int16_t HUD_WIDTH = 160;
    static constexpr int16_t HUD_HEIGHT = 14;

    HeaderWidget();
    ~HeaderWidget() = default;

    /**
     * @brief Render the top HUD using offscreen sprite double-buffering.
     * Only transfers pixels to TFT if telemetry state changed or forceRedraw is true.
     */
    void render(Adafruit_ST7735& tft, const HeaderTelemetry& tel, bool forceRedraw = false);

    /**
     * @brief Mark HUD dirty to force next render to push to TFT.
     */
    void invalidate();

private:
    GFXcanvas16 _canvas; // 160x14 sprite buffer (4,480 bytes)
    uint32_t _lastHash = 0xFFFFFFFF;
    unsigned long _lastActivityBlinkMs = 0;
    bool _blinkState = false;

    uint32_t computeHash(const HeaderTelemetry& tel, bool blink) const;
    void drawToCanvas(const HeaderTelemetry& tel);
};
