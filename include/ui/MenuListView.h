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
#ifndef GFXCANVAS16_MOCK_DEFINED
#define GFXCANVAS16_MOCK_DEFINED
class GFXcanvas16 {};
#endif
#endif
#include "ui/TacticalTheme.h"

/**
 * @file MenuListView.h
 * @brief Tactical Card-based Menu List View for ESP32-DIV aesthetic.
 *
 * Provides decoupled menu contracts and sprite-buffered partial redraw
 * for zero-flicker rapid scrolling.
 */

struct MenuItemContract {
    const char* label = nullptr;
    uint8_t iconId = 0;
    const char* badgeText = nullptr;
    uint16_t badgeColor = 0;
};

class IMenuDataSource {
public:
    virtual ~IMenuDataSource() = default;
    virtual size_t getItemCount() const = 0;
    virtual MenuItemContract getItem(size_t index) const = 0;
};

class MenuListView {
public:
    static constexpr int16_t CARD_X = 4;
    static constexpr int16_t CARD_W = 152;
    static constexpr int16_t CARD_H = 21;
    static constexpr int16_t CARD_SPACING = 23;
    static constexpr int16_t VIEWPORT_Y = 16;
    static constexpr size_t VISIBLE_ROWS = 4;

    MenuListView();
    ~MenuListView() = default;

    void setDataSource(IMenuDataSource* source) { _dataSource = source; }

    /**
     * @brief Full render of visible menu items in the viewport.
     */
    void render(Adafruit_ST7735& tft, size_t selectedIndex, size_t scrollOffset);

    /**
     * @brief Zero-flicker partial redraw of just the two affected rows.
     */
    void renderPartialSelection(Adafruit_ST7735& tft, size_t oldIndex, size_t newIndex, size_t scrollOffset);

    /**
     * @brief Render a single menu item card directly or via sprite.
     */
    void renderItem(Adafruit_ST7735& tft, size_t index, bool isSelected, int16_t row);

private:
    IMenuDataSource* _dataSource = nullptr;
    GFXcanvas16 _cardCanvas; // 152x21 offscreen sprite (6,384 bytes)

    void drawIcon(Adafruit_GFX& gfx, uint8_t iconId, int16_t cx, int16_t cy, uint16_t color);
};
