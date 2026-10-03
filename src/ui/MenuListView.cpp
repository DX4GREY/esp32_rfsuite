#include "ui/MenuListView.h"
#include "ui/TacticalWidgets.h"

MenuListView::MenuListView()
    : _cardCanvas(CARD_W, CARD_H) {
    _cardCanvas.fillScreen(TacticalColor::DarkCharcoal);
}

void MenuListView::drawIcon(Adafruit_GFX& gfx, uint8_t iconId, int16_t cx, int16_t cy, uint16_t color) {
    switch (iconId) {
        case 0: // Spectrum
            gfx.drawFastVLine(cx - 5, cy + 2, 4, color);
            gfx.drawFastVLine(cx - 2, cy - 3, 9, color);
            gfx.drawFastVLine(cx + 1, cy - 5, 11, color);
            gfx.drawFastVLine(cx + 4, cy, 6, color);
            break;
        case 1: // Waterfall
            for (int r = -4; r <= 4; r += 2) {
                gfx.drawFastHLine(cx - 5, cy + r, 11, color);
            }
            break;
        case 2: // Channel inspector reticle
            gfx.drawCircle(cx, cy, 5, color);
            gfx.drawFastHLine(cx - 7, cy, 15, color);
            gfx.drawFastVLine(cx, cy - 7, 15, color);
            break;
        case 3: // Survey / histogram
            gfx.fillRect(cx - 5, cy + 1, 2, 5, color);
            gfx.fillRect(cx - 2, cy - 3, 2, 9, color);
            gfx.fillRect(cx + 1, cy - 5, 2, 11, color);
            gfx.fillRect(cx + 4, cy - 1, 2, 7, color);
            break;
        case 4: // Event marker
            gfx.drawCircle(cx, cy, 6, color);
            gfx.fillCircle(cx, cy, 2, color);
            break;
        case 5: // Recording / session
            gfx.drawRect(cx - 7, cy - 5, 15, 11, color);
            gfx.fillCircle(cx, cy, 3, color);
            break;
        case 6: // Antenna
            gfx.drawFastVLine(cx, cy - 3, 9, color);
            gfx.fillCircle(cx, cy - 4, 2, color);
            gfx.drawLine(cx - 4, cy + 5, cx + 4, cy + 5, color);
            gfx.drawLine(cx - 5, cy - 2, cx - 2, cy + 1, color);
            gfx.drawLine(cx + 5, cy - 2, cx + 2, cy + 1, color);
            break;
        case 7: // Dual radio
            gfx.drawRect(cx - 7, cy - 5, 6, 11, color);
            gfx.drawRect(cx + 1, cy - 5, 6, 11, color);
            gfx.drawPixel(cx - 4, cy + 2, color);
            gfx.drawPixel(cx + 4, cy + 2, color);
            break;
        case 8: // Profiles
            gfx.drawRect(cx - 6, cy - 5, 13, 4, color);
            gfx.drawRect(cx - 6, cy, 13, 4, color);
            gfx.drawRect(cx - 6, cy + 5, 13, 4, color);
            break;
        case 9: // Settings sliders
            gfx.drawFastHLine(cx - 6, cy - 3, 13, color);
            gfx.drawFastHLine(cx - 6, cy + 3, 13, color);
            gfx.fillRect(cx - 2, cy - 5, 3, 5, color);
            gfx.fillRect(cx + 2, cy + 1, 3, 5, color);
            break;
        case 10: // Device status
            gfx.drawRect(cx - 6, cy - 5, 13, 11, color);
            gfx.drawFastHLine(cx - 4, cy - 2, 9, color);
            gfx.drawFastHLine(cx - 4, cy + 1, 6, color);
            break;
        case 11: // Power
            gfx.drawCircle(cx, cy, 5, color);
            gfx.drawFastVLine(cx, cy - 6, 6, TacticalColor::PureBlack);
            gfx.drawFastVLine(cx, cy - 6, 6, color);
            break;
        case 12: // SD File
        default:
            gfx.drawRect(cx - 6, cy - 4, 13, 10, color);
            gfx.drawFastHLine(cx - 6, cy - 5, 6, color);
            break;
    }
}

void MenuListView::renderItem(Adafruit_ST7735& tft, size_t index, bool isSelected, int16_t row) {
    if (!_dataSource || index >= _dataSource->getItemCount()) return;

    MenuItemContract item = _dataSource->getItem(index);
    int16_t cardY = VIEWPORT_Y + row * CARD_SPACING;

    // Draw offscreen into _cardCanvas for zero flicker
    _cardCanvas.fillScreen(isSelected ? TacticalColor::CardSurface : TacticalColor::DarkCharcoal);

    // 1. Wireframe border with reticle corners if selected
    TacticalWidgets::drawTacticalCard(_cardCanvas, 0, 0, CARD_W, CARD_H,
                                     isSelected,
                                     TacticalColor::Wireframe,
                                     isSelected ? TacticalColor::CardSurface : TacticalColor::DarkCharcoal,
                                     TacticalColor::Cyan);

    // 2. Left selection notch
    if (isSelected) {
        _cardCanvas.fillRect(0, 0, 3, CARD_H, TacticalColor::Cyan);
    }

    // 3. Index tag: [01]
    _cardCanvas.setTextSize(1);
    _cardCanvas.setTextWrap(false);
    _cardCanvas.setCursor(6, 7);
    _cardCanvas.setTextColor(isSelected ? TacticalColor::Cyan : TacticalColor::MutedGrey,
                             isSelected ? TacticalColor::CardSurface : TacticalColor::DarkCharcoal);
    _cardCanvas.print("[");
    if (index + 1 < 10) _cardCanvas.print("0");
    _cardCanvas.print(index + 1);
    _cardCanvas.print("]");

    // 4. Tactical icon
    drawIcon(_cardCanvas, item.iconId, 36, 10, isSelected ? TacticalColor::Cyan : TacticalColor::MutedGrey);

    // 5. High-contrast label
    _cardCanvas.setCursor(48, 7);
    _cardCanvas.setTextColor(isSelected ? TacticalColor::HighWhite : TacticalColor::MutedGrey,
                             isSelected ? TacticalColor::CardSurface : TacticalColor::DarkCharcoal);
    if (item.label) {
        _cardCanvas.print(item.label);
    }

    // 6. Right micro-badge (e.g. event count or REC indicator)
    if (item.badgeText && item.badgeText[0]) {
        int16_t badgeW = strlen(item.badgeText) * 6 + 6;
        int16_t badgeX = CARD_W - badgeW - 6;
        _cardCanvas.fillRect(badgeX, 4, badgeW, 13, TacticalColor::DarkCharcoal);
        _cardCanvas.drawRect(badgeX, 4, badgeW, 13, item.badgeColor ? item.badgeColor : TacticalColor::Wireframe);
        _cardCanvas.setCursor(badgeX + 3, 7);
        _cardCanvas.setTextColor(item.badgeColor ? item.badgeColor : TacticalColor::HighWhite,
                                 TacticalColor::DarkCharcoal);
        _cardCanvas.print(item.badgeText);
    }

    // Blit completed card to TFT
    tft.drawRGBBitmap(CARD_X, cardY, _cardCanvas.getBuffer(), CARD_W, CARD_H);
}

void MenuListView::render(Adafruit_ST7735& tft, size_t selectedIndex, size_t scrollOffset) {
    if (!_dataSource) return;
    size_t total = _dataSource->getItemCount();

    // Clear viewport background (pure black)
    tft.fillRect(0, VIEWPORT_Y - 1, 160, 4 * CARD_SPACING + 2, TacticalColor::PureBlack);

    for (size_t row = 0; row < VISIBLE_ROWS; ++row) {
        size_t idx = scrollOffset + row;
        if (idx < total) {
            renderItem(tft, idx, idx == selectedIndex, row);
        }
    }

    // Subtle scroll indicators if more items exist
    if (scrollOffset > 0) {
        tft.drawPixel(158, VIEWPORT_Y + 1, TacticalColor::Cyan);
    }
    if (scrollOffset + VISIBLE_ROWS < total) {
        tft.drawPixel(158, VIEWPORT_Y + 4 * CARD_SPACING - 3, TacticalColor::Cyan);
    }
}

void MenuListView::renderPartialSelection(Adafruit_ST7735& tft, size_t oldIndex, size_t newIndex, size_t scrollOffset) {
    if (!_dataSource) return;

    // Check if both old and new are visible in the current scroll window
    int oldRow = static_cast<int>(oldIndex) - static_cast<int>(scrollOffset);
    int newRow = static_cast<int>(newIndex) - static_cast<int>(scrollOffset);

    if (oldRow >= 0 && oldRow < static_cast<int>(VISIBLE_ROWS)) {
        renderItem(tft, oldIndex, false, oldRow);
    }
    if (newRow >= 0 && newRow < static_cast<int>(VISIBLE_ROWS)) {
        renderItem(tft, newIndex, true, newRow);
    }
}
