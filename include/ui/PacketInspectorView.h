#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
using String = std::string;
class Adafruit_ST7735;
class Adafruit_GFX;
#endif
#include "ui/TacticalTheme.h"

/**
 * @file PacketInspectorView.h
 * @brief Decoupled Packet Sniffer Stream & Inspector View for ESP32-DIV UI.
 */

struct PacketSnifferTelemetry {
    bool isRunning = false;
    bool isSim = false;
    uint8_t channel = 0;
    int dataRateMbps = 2; // 1 or 2
    uint32_t packetCount = 0;
    const char* rawHex = nullptr;
    const char* errorText = nullptr;
    uint8_t signalStrength = 0; // 0..100
};

class PacketInspectorView {
public:
    PacketInspectorView();
    ~PacketInspectorView() = default;

    /**
     * @brief Render the static tactical frame and HUD cards.
     */
    void renderStatic(Adafruit_ST7735& tft, const PacketSnifferTelemetry& tel);

    /**
     * @brief Render the packet stream and live throughput micro-widgets with zero flicker.
     */
    void renderStream(Adafruit_ST7735& tft, const PacketSnifferTelemetry& tel);

    /**
     * @brief Reset state cache.
     */
    void reset();

private:
    bool _layoutDrawn = false;
    uint32_t _prevPackets = 0xFFFFFFFF;
    uint8_t _prevChannel = 0xFF;
    int _prevRate = -1;
    bool _prevRunning = false;
    String _prevHex;

    // Throughput / packet arrival sparkline
    static constexpr size_t RATE_SAMPLES = 16;
    uint8_t _rateHistory[RATE_SAMPLES] = {};
    size_t _rateCount = 0;
    unsigned long _lastRateSampleMs = 0;
    uint32_t _lastPacketCount = 0;
};
