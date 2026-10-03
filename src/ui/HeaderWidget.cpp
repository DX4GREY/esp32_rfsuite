#include "ui/HeaderWidget.h"
#include "ui/TacticalWidgets.h"
#include "core/Crc32.h"

HeaderWidget::HeaderWidget()
    : _canvas(HUD_WIDTH, HUD_HEIGHT) {
    _canvas.fillScreen(TacticalColor::PureBlack);
}

void HeaderWidget::invalidate() {
    _lastHash = 0xFFFFFFFF;
}

uint32_t HeaderWidget::computeHash(const HeaderTelemetry& tel, bool blink) const {
    uint32_t h = 0x811C9DC5;
    auto mix = [&h](uint32_t v) {
        h = (h ^ v) * 0x01000193;
    };

    mix(tel.isTx ? 1 : 0);
    mix(tel.isSim ? 2 : 0);
    mix(tel.radio1Connected ? 4 : 0);
    mix(tel.radio2Connected ? 8 : 0);
    mix(tel.radio1Active ? 16 : 0);
    mix(tel.radio2Active ? 32 : 0);
    mix(tel.isRecording ? 64 : 0);
    mix(tel.usingSd ? 128 : 0);
    mix(tel.batteryCharging ? 256 : 0);
    mix(blink ? 512 : 0);
    mix(static_cast<uint32_t>(tel.activeChannel));
    mix(static_cast<uint32_t>(tel.peakLevel));
    mix(static_cast<uint32_t>(tel.batteryPercent));
    mix(static_cast<uint32_t>(tel.page));
    mix(static_cast<uint32_t>(tel.totalPages));

    if (tel.title) {
        for (const char* p = tel.title; *p; ++p) mix(static_cast<uint8_t>(*p));
    }
    if (tel.rfMode) {
        for (const char* p = tel.rfMode; *p; ++p) mix(static_cast<uint8_t>(*p));
    }
    return h;
}

void HeaderWidget::render(Adafruit_ST7735& tft, const HeaderTelemetry& tel, bool forceRedraw) {
    unsigned long now = millis();
    if (now - _lastActivityBlinkMs >= 350) {
        _lastActivityBlinkMs = now;
        _blinkState = !_blinkState;
    }

    uint32_t currentHash = computeHash(tel, _blinkState);
    if (!forceRedraw && currentHash == _lastHash) {
        return; // Zero SPI bandwidth wasted if nothing changed
    }
    _lastHash = currentHash;

    drawToCanvas(tel);

    // Blit entire sprite buffer to TFT in a single contiguous transfer
    tft.drawRGBBitmap(0, 0, _canvas.getBuffer(), HUD_WIDTH, HUD_HEIGHT);
}

void HeaderWidget::drawToCanvas(const HeaderTelemetry& tel) {
    _canvas.fillScreen(TacticalColor::PureBlack);

    // 1. Battery Gauge (Left, x=2..13)
    TacticalWidgets::drawBatteryHUD(_canvas, 2, 3, tel.batteryPercent, tel.batteryCharging);

    // 2. Title or RF Mode / Sub-band Pill (x=16..65)
    _canvas.setTextSize(1);
    _canvas.setTextWrap(false);

    if (tel.title && tel.title[0]) {
        _canvas.setCursor(16, 3);
        _canvas.setTextColor(TacticalColor::HighWhite, TacticalColor::PureBlack);
        char buf[12];
        strncpy(buf, tel.title, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        _canvas.print(buf);
    } else {
        // Mode pill
        const char* modeStr = tel.rfMode ? tel.rfMode : "IDLE";
        uint16_t modeColor = TacticalColor::Cyan;
        if (tel.isTx) modeColor = TacticalColor::ThreatRed;
        else if (tel.isSim) modeColor = TacticalColor::AmberOrange;

        _canvas.drawRect(16, 2, 28, 9, TacticalColor::Wireframe);
        _canvas.setCursor(18, 3);
        _canvas.setTextColor(modeColor, TacticalColor::PureBlack);
        _canvas.print(modeStr);
    }

    // 3. Dual nRF24 Activity Status (x=68..102)
    _canvas.setCursor(66, 3);
    _canvas.setTextColor(TacticalColor::MutedGrey, TacticalColor::PureBlack);
    _canvas.print("1:");
    TacticalWidgets::drawActivityDot(_canvas, 80, 7, tel.radio1Connected,
                                     tel.radio1Active && _blinkState,
                                     tel.isTx ? TacticalColor::ThreatRed : TacticalColor::Cyan);

    _canvas.setCursor(87, 3);
    _canvas.setTextColor(TacticalColor::MutedGrey, TacticalColor::PureBlack);
    _canvas.print("2:");
    TacticalWidgets::drawActivityDot(_canvas, 101, 7, tel.radio2Connected,
                                     tel.radio2Active && _blinkState,
                                     tel.isTx ? TacticalColor::ThreatRed : TacticalColor::Cyan);

    // 4. Channel Indicator or Page Number (x=108..138)
    if (tel.totalPages > 0) {
        _canvas.drawRect(108, 2, 26, 9, TacticalColor::Wireframe);
        _canvas.setCursor(111, 3);
        _canvas.setTextColor(TacticalColor::Cyan, TacticalColor::PureBlack);
        _canvas.print(tel.page);
        _canvas.print("/");
        _canvas.print(tel.totalPages);
    } else if (tel.activeChannel >= 0) {
        TacticalWidgets::drawChannelBadge(_canvas, 107, 2, tel.activeChannel,
                                          tel.isTx ? TacticalColor::ThreatRed : TacticalColor::Cyan);
    }

    // 5. Recording indicator & Storage backend (x=142..158)
    if (tel.isRecording && _blinkState) {
        _canvas.fillCircle(141, 6, 2, TacticalColor::ThreatRed);
    }

    _canvas.setCursor(145, 3);
    _canvas.setTextColor(tel.usingSd ? TacticalColor::Cyan : TacticalColor::AmberOrange,
                         TacticalColor::PureBlack);
    _canvas.print(tel.usingSd ? "SD" : "LF");

    // 6. Tactical bottom wireframe divider line with corner ticks
    _canvas.drawFastHLine(0, 13, HUD_WIDTH, TacticalColor::Wireframe);
    _canvas.drawPixel(0, 12, TacticalColor::Cyan);
    _canvas.drawPixel(HUD_WIDTH - 1, 12, TacticalColor::Cyan);
}
