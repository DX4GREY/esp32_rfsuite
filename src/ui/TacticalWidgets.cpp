#include "ui/TacticalWidgets.h"

namespace TacticalWidgets {

void drawRssiBar(Adafruit_GFX& gfx, int16_t x, int16_t y, uint8_t levelPercent,
                 uint8_t numSegments, uint8_t segWidth,
                 uint8_t segGap, uint8_t maxHeight) {
    if (numSegments == 0) return;
    if (numSegments > 8) numSegments = 8;

    for (uint8_t i = 0; i < numSegments; ++i) {
        // Height escalates smoothly up to maxHeight
        int16_t h = 2 + ((maxHeight - 2) * (i + 1)) / numSegments;
        if (h > maxHeight) h = maxHeight;
        int16_t segX = x + i * (segWidth + segGap);
        int16_t segY = y + maxHeight - h;

        // Threshold calculation
        uint8_t thresh = ((i + 1) * 100) / numSegments;
        bool active = (levelPercent >= (thresh > 12 ? thresh - 10 : 2));

        if (active) {
            uint16_t col;
            if (i < 2) col = TacticalColor::SuccessGreen;
            else if (i == 2) col = TacticalColor::Cyan;
            else if (i == 3) col = TacticalColor::AmberOrange;
            else col = TacticalColor::ThreatRed;

            gfx.fillRect(segX, segY, segWidth, h, col);
        } else {
            // Inactive subtle track wireframe
            gfx.fillRect(segX, segY, segWidth, h, TacticalColor::BarTrack);
        }
    }
}

void drawSparkline(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                   const uint8_t* history, size_t count,
                   uint16_t lineColor, uint16_t frameColor,
                   bool filled) {
    if (w <= 2 || h <= 2 || count < 2 || !history) return;

    if (frameColor != 0) {
        gfx.drawRect(x, y, w, h, frameColor);
    }

    const int16_t innerX = x + 1;
    const int16_t innerY = y + 1;
    const int16_t innerW = w - 2;
    const int16_t innerH = h - 2;

    int16_t prevPx = innerX;
    int16_t prevPy = innerY + innerH - 1 - (history[0] * (innerH - 1)) / 100;

    for (size_t i = 1; i < count; ++i) {
        int16_t curPx = innerX + (i * (innerW - 1)) / (count - 1);
        int16_t curPy = innerY + innerH - 1 - (history[i] * (innerH - 1)) / 100;

        gfx.drawLine(prevPx, prevPy, curPx, curPy, lineColor);
        if (filled && curPx > prevPx) {
            gfx.drawLine(curPx, curPy + 1, curPx, innerY + innerH - 1, TacticalColor::DarkCharcoal);
        }

        prevPx = curPx;
        prevPy = curPy;
    }
}

void drawChannelBadge(Adafruit_GFX& gfx, int16_t x, int16_t y, int channel,
                      uint16_t accent) {
    gfx.fillRect(x, y, 32, 9, TacticalColor::DarkCharcoal);
    gfx.drawRect(x, y, 32, 9, TacticalColor::Wireframe);

    gfx.setTextSize(1);
    gfx.setTextWrap(false);
    gfx.setCursor(x + 2, y + 1);
    gfx.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
    gfx.print("C");

    gfx.setCursor(x + 9, y + 1);
    gfx.setTextColor(accent, TacticalColor::DarkCharcoal);
    if (channel >= 0 && channel <= 125) {
        if (channel < 10) gfx.print("0");
        gfx.print(channel);
    } else {
        gfx.print("--");
    }
}

void drawBatteryHUD(Adafruit_GFX& gfx, int16_t x, int16_t y, uint8_t percent,
                    bool charging) {
    // 11x7 wireframe shell
    gfx.drawRect(x, y, 10, 7, TacticalColor::Wireframe);
    gfx.drawFastVLine(x + 10, y + 2, 3, TacticalColor::Wireframe);

    // Inner clear
    gfx.fillRect(x + 1, y + 1, 8, 5, TacticalColor::PureBlack);

    if (charging) {
        // Charging bolt
        gfx.drawPixel(x + 4, y + 2, TacticalColor::AmberOrange);
        gfx.drawPixel(x + 5, y + 3, TacticalColor::AmberOrange);
        gfx.drawPixel(x + 4, y + 4, TacticalColor::AmberOrange);
        return;
    }

    uint16_t col = TacticalColor::SuccessGreen;
    if (percent <= 20) col = TacticalColor::ThreatRed;
    else if (percent <= 45) col = TacticalColor::AmberOrange;

    // 3 discrete micro-bars
    if (percent > 15) gfx.fillRect(x + 2, y + 2, 2, 3, col);
    if (percent > 45) gfx.fillRect(x + 5, y + 2, 2, 3, col);
    if (percent > 80) gfx.fillRect(x + 7, y + 2, 1, 3, col);
}

void drawActivityDot(Adafruit_GFX& gfx, int16_t cx, int16_t cy,
                     bool connected, bool active,
                     uint16_t activeColor) {
    if (!connected) {
        gfx.drawRect(cx - 2, cy - 2, 5, 5, TacticalColor::DarkCharcoal);
        gfx.drawPixel(cx, cy, TacticalColor::MutedGrey);
        return;
    }

    gfx.drawRect(cx - 2, cy - 2, 5, 5, TacticalColor::Wireframe);
    if (active) {
        gfx.fillRect(cx - 1, cy - 1, 3, 3, activeColor);
    } else {
        gfx.fillRect(cx - 1, cy - 1, 3, 3, TacticalColor::SuccessGreen);
    }
}

void drawTacticalCard(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                      bool selected, uint16_t borderColor, uint16_t fillColor,
                      uint16_t selectAccent) {
    gfx.fillRect(x, y, w, h, fillColor);
    gfx.drawRect(x, y, w, h, selected ? selectAccent : borderColor);

    if (selected) {
        // High-contrast corner reticles (ESP32-DIV signature)
        gfx.drawFastHLine(x, y, 3, TacticalColor::HighWhite);
        gfx.drawFastVLine(x, y, 3, TacticalColor::HighWhite);

        gfx.drawFastHLine(x + w - 3, y, 3, TacticalColor::HighWhite);
        gfx.drawFastVLine(x + w - 1, y, 3, TacticalColor::HighWhite);

        gfx.drawFastHLine(x, y + h - 1, 3, TacticalColor::HighWhite);
        gfx.drawFastVLine(x, y + h - 3, 3, TacticalColor::HighWhite);

        gfx.drawFastHLine(x + w - 3, y + h - 1, 3, TacticalColor::HighWhite);
        gfx.drawFastVLine(x + w - 1, y + h - 3, 3, TacticalColor::HighWhite);
    }
}

void drawKeyLegendChip(Adafruit_GFX& gfx, int16_t x, int16_t y, int16_t w, int16_t h,
                       const char* key, const char* action, bool active) {
    gfx.fillRect(x, y, w, h, TacticalColor::DarkCharcoal);
    gfx.drawRect(x, y, w, h, active ? TacticalColor::Cyan : TacticalColor::Wireframe);

    gfx.setTextSize(1);
    gfx.setTextWrap(false);

    int16_t curX = x + 3;
    if (key && key[0]) {
        gfx.setCursor(curX, y + 2);
        gfx.setTextColor(TacticalColor::Cyan, TacticalColor::DarkCharcoal);
        gfx.print("[");
        gfx.print(key);
        gfx.print("]");
        curX += (strlen(key) + 2) * 6 + 2;
    }

    if (action && action[0]) {
        gfx.setCursor(curX, y + 2);
        gfx.setTextColor(active ? TacticalColor::HighWhite : TacticalColor::MutedGrey,
                         TacticalColor::DarkCharcoal);
        gfx.print(action);
    }
}

} // namespace TacticalWidgets
