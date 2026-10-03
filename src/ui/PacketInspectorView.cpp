#include "ui/PacketInspectorView.h"
#include "ui/TacticalWidgets.h"

PacketInspectorView::PacketInspectorView() {
    reset();
}

void PacketInspectorView::reset() {
    _layoutDrawn = false;
    _prevPackets = 0xFFFFFFFF;
    _prevChannel = 0xFF;
    _prevRate = -1;
    _prevRunning = false;
    _prevHex = "";
    _rateCount = 0;
    _lastRateSampleMs = 0;
    _lastPacketCount = 0;
    memset(_rateHistory, 0, sizeof(_rateHistory));
}

void PacketInspectorView::renderStatic(Adafruit_ST7735& tft, const PacketSnifferTelemetry& tel) {
    // 1. Top HUD Card (y=16..45)
    TacticalWidgets::drawTacticalCard(tft, 4, 16, 152, 30, false,
                                     TacticalColor::Wireframe,
                                     TacticalColor::DarkCharcoal);

    // 2. Stream Data Container Card (y=48..111)
    TacticalWidgets::drawTacticalCard(tft, 4, 48, 152, 63, false,
                                     TacticalColor::Wireframe,
                                     TacticalColor::DarkCharcoal);

    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.setCursor(8, 51);
    tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
    tft.print("FRAME BUFFER STREAM");

    tft.drawFastHLine(8, 60, 144, TacticalColor::Wireframe);

    _layoutDrawn = true;
    _prevPackets = 0xFFFFFFFF;
    _prevChannel = 0xFF;
    _prevRate = -1;
    _prevRunning = false;
    _prevHex = "";
}

void PacketInspectorView::renderStream(Adafruit_ST7735& tft, const PacketSnifferTelemetry& tel) {
    if (!_layoutDrawn) {
        renderStatic(tft, tel);
    }

    // Update throughput sparkline
    unsigned long now = millis();
    if (now - _lastRateSampleMs >= 250) {
        _lastRateSampleMs = now;
        uint32_t delta = (tel.packetCount >= _lastPacketCount) ?
                         (tel.packetCount - _lastPacketCount) : 0;
        _lastPacketCount = tel.packetCount;

        uint8_t scaledRate = constrain(delta * 12, 0, 100);
        if (_rateCount < RATE_SAMPLES) {
            _rateHistory[_rateCount++] = scaledRate;
        } else {
            memmove(&_rateHistory[0], &_rateHistory[1], RATE_SAMPLES - 1);
            _rateHistory[RATE_SAMPLES - 1] = scaledRate;
        }

        // Draw live throughput sparkline in top-right HUD card
        TacticalWidgets::drawSparkline(tft, 104, 20, 48, 10, _rateHistory,
                                       _rateCount,
                                       TacticalColor::Cyan,
                                       TacticalColor::Wireframe);
    }

    // Top Card Elements:
    // 1. Channel Badge
    if (_prevChannel != tel.channel) {
        TacticalWidgets::drawChannelBadge(tft, 8, 20, tel.channel, TacticalColor::Cyan);
        _prevChannel = tel.channel;
    }

    // 2. RSSI Micro-widget & Rate
    if (_prevRate != tel.dataRateMbps) {
        tft.fillRect(44, 20, 56, 10, TacticalColor::DarkCharcoal);
        TacticalWidgets::drawRssiBar(tft, 44, 20, tel.signalStrength, 4, 2, 1, 8);

        tft.setCursor(58, 21);
        tft.setTextColor(TacticalColor::HighWhite, TacticalColor::DarkCharcoal);
        tft.print(tel.dataRateMbps == 1 ? "1M" : "2M");
        _prevRate = tel.dataRateMbps;
    }

    // 3. Status Tag & Packet Count (y=32..42)
    if (_prevRunning != tel.isRunning || _prevPackets != tel.packetCount) {
        tft.fillRect(8, 33, 144, 11, TacticalColor::DarkCharcoal);

        // Status tag
        tft.setCursor(8, 34);
        if (tel.isRunning) {
            tft.setTextColor(TacticalColor::SuccessGreen, TacticalColor::DarkCharcoal);
            tft.print("[RUN]");
        } else {
            tft.setTextColor(TacticalColor::ThreatRed, TacticalColor::DarkCharcoal);
            tft.print("[STP]");
        }

        // Packet count callout
        tft.setCursor(44, 34);
        tft.setTextColor(TacticalColor::MutedGrey, TacticalColor::DarkCharcoal);
        tft.print("PKTS:");
        tft.setTextColor(TacticalColor::Cyan, TacticalColor::DarkCharcoal);
        tft.print(tel.packetCount);

        _prevRunning = tel.isRunning;
        _prevPackets = tel.packetCount;
    }

    // 4. Hex Stream Buffer Area (y=63..108)
    String text = tel.isRunning ? (tel.rawHex ? String(tel.rawHex) : "")
                                : (tel.errorText ? String(tel.errorText) : "");
    if (!text.length()) text = tel.isRunning ? "WAITING FOR PACKET..." : "NO RADIO SIGNAL";

    if (_prevHex != text) {
        tft.fillRect(8, 63, 144, 45, TacticalColor::DarkCharcoal);

        // Render formatted hex rows with byte contrast
        for (uint8_t line = 0; line < 4; ++line) {
            int offset = line * 20;
            if (offset >= static_cast<int>(text.length())) break;

            int rowY = 64 + line * 10;
            tft.setCursor(8, rowY);
            tft.setTextColor(TacticalColor::Cyan, TacticalColor::DarkCharcoal);
            tft.printf("%02X:", line * 8);

            tft.setCursor(28, rowY);
            tft.setTextColor(line == 0 ? TacticalColor::HighWhite : TacticalColor::MutedGrey,
                             TacticalColor::DarkCharcoal);
            tft.print(text.substring(offset, min<int>(offset + 20, text.length())));
        }
        _prevHex = text;
    }
}
