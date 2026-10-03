#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "ui/TacticalTheme.h"

/**
 * @file TacticalWidgets.h
 * @brief High-density micro-widgets and HUD indicators for ESP32-DIV UI.
 */

namespace TacticalWidgets {

    /**
     * @brief Render a 4-5 segment tactical signal RSSI bar gauge.
     * @param gfx Target Adafruit_GFX context (Display or GFXcanvas16)
     * @param x Top-left X coordinate
     * @param y Top-left Y coordinate
     * @param levelPercent Signal intensity (0..100)
     * @param numSegments Number of vertical segments (default: 5)
     * @param segWidth Width of each bar segment (default: 2px)
     * @param segGap Gap between segments (default: 1px)
     * @param maxHeight Maximum height of the tallest bar (default: 9px)
     */
    void drawRssiBar(Adafruit_GFX& gfx, int16_t x, int16_t y, uint8_t levelPercent,
                     uint8_t numSegments = 5, uint8_t segWidth = 2,
                     uint8_t segGap = 1, uint8_t maxHeight = 9);

    /**
     * @brief Render a live sparkline / mini-histogram.
     * @param gfx Target graphics context
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Width of sparkline viewport
     * @param h Height of sparkline viewport
     * @param history Ring buffer or array of values (0..100)
     * @param count Number of elements in history
     * @param lineColor Line/Bar foreground color
     * @param frameColor Wireframe bounding border (0 = none)
     * @param filled Whether to fill area beneath trace
     */
    void drawSparkline(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                       const uint8_t* history, size_t count,
                       uint16_t lineColor = TacticalColor::Cyan,
                       uint16_t frameColor = TacticalColor::Wireframe,
                       bool filled = false);

    /**
     * @brief Render a compact tactical channel badge.
     * @param gfx Target graphics context
     * @param x Top-left X
     * @param y Top-left Y
     * @param channel RF Channel number (0..125)
     * @param accent Highlight color
     */
    void drawChannelBadge(Adafruit_GFX& gfx, int16_t x, int16_t y, int channel,
                          uint16_t accent = TacticalColor::Cyan);

    /**
     * @brief Render a compact HUD battery indicator.
     * @param gfx Target graphics context
     * @param x Top-left X
     * @param y Top-left Y
     * @param percent Battery charge level (0..100)
     * @param charging Battery charge state
     */
    void drawBatteryHUD(Adafruit_GFX& gfx, int16_t x, int16_t y, uint8_t percent,
                        bool charging = false);

    /**
     * @brief Render a dual-radio activity status dot.
     * @param gfx Target graphics context
     * @param cx Center X
     * @param cy Center Y
     * @param connected Radio hardware initialized
     * @param active Radio currently executing RX or TX sweep
     * @param activeColor Color when radio is active (default: Cyan)
     */
    void drawActivityDot(Adafruit_GFX& gfx, int16_t cx, int16_t cy,
                         bool connected, bool active,
                         uint16_t activeColor = TacticalColor::Cyan);

    /**
     * @brief Render a tactical wireframe card container with corner reticles.
     * @param gfx Target graphics context
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Card width
     * @param h Card height
     * @param selected Whether card is currently selected/active
     * @param borderColor Inactive 1px border color
     * @param fillColor Inner surface color
     * @param selectAccent Selection border/bracket color
     */
    void drawTacticalCard(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                          bool selected = false,
                          uint16_t borderColor = TacticalColor::Wireframe,
                          uint16_t fillColor = TacticalColor::DarkCharcoal,
                          uint16_t selectAccent = TacticalColor::Cyan);

    /**
     * @brief Render a compact tactical button chip / keybinding legend.
     * @param gfx Target graphics context
     * @param x Top-left X
     * @param y Top-left Y
     * @param w Chip width
     * @param h Chip height
     * @param key Key text (e.g., "SEL", "BACK", "U/D")
     * @param action Action text (e.g., "RUN", "EXIT", "NAV")
     * @param active Highlighted state
     */
    void drawKeyLegendChip(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                           const char* key, const char* action, bool active = false);

} // namespace TacticalWidgets
