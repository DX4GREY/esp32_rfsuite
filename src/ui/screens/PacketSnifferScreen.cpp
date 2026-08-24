#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "services/PacketSniffer.h"

using namespace DisplayUi;

void DisplayManager::renderPacketSnifferScreen() {
    if (appState.simulationMode) {
        if (!snifferLayoutDrawn) {
            drawModernHeader("SIM PACKET SNIFFER", SPECTRUM_ACCENT);
            tft.fillRoundRect(5, 17, 150, 86, 4, SPECTRUM_CARD_BG);
            tft.drawRoundRect(5, 17, 150, 86, 4, SPECTRUM_BORDER);
            tft.setCursor(10, 22); tft.setTextColor(SPECTRUM_LOW, SPECTRUM_CARD_BG);
            tft.print("DEMO DATA - NOT RF");
            tft.setCursor(10, 38); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
            tft.print("PACKETS");
            tft.setCursor(10, 55); tft.print("SAMPLE HEX");
            drawModernFooter("", "SIMULATION", "B BACK");
            snifferLayoutDrawn = true;
        }
        const uint32_t demoPackets = millis() / 700;
        if (previousSnifferPackets != demoPackets) {
            tft.fillRect(70, 35, 78, 12, SPECTRUM_CARD_BG);
            tft.setCursor(72, 38); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
            tft.print(demoPackets);
            tft.fillRect(8, 66, 144, 13, SPECTRUM_CARD_BG);
            tft.setCursor(10, 68); tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
            tft.printf("A5 %02lX 10 2C 7E 01", static_cast<unsigned long>(demoPackets & 0xFF));
            previousSnifferPackets = demoPackets;
        }
        return;
    }
    if (!snifferLayoutDrawn) {
        drawModernHeader("NRF24 PACKET SNIFFER", SPECTRUM_ACCENT);
        tft.fillRoundRect(5, 17, 150, 86, 4, SPECTRUM_CARD_BG);
        tft.drawRoundRect(5, 17, 150, 86, 4, SPECTRUM_BORDER);
        tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.setCursor(10, 22); tft.print("STATE");
        tft.setCursor(10, 34); tft.print("CH / RATE");
        tft.setCursor(10, 46); tft.print("PACKETS");
        tft.drawFastHLine(10, 58, 140, SPECTRUM_GRID);
        drawModernFooter("U/D CH", "A 1M/2M", "B STOP");
        snifferLayoutDrawn = true;
    }

    const bool running = packetSniffer.isRunning();
    const int rate = static_cast<int>(packetSniffer.dataRate());
    if (!snifferRunningValid || previousSnifferRunning != running) {
        tft.fillRect(61, 20, 89, 10, SPECTRUM_CARD_BG);
        tft.setCursor(63, 22);
        tft.setTextColor(running ? SPECTRUM_LOW : SPECTRUM_CRITICAL, SPECTRUM_CARD_BG);
        tft.print(running ? "CAPTURING" : "ERROR");
    }
    if (previousSnifferChannel != packetSniffer.channel() || previousSnifferRate != rate) {
        tft.fillRect(74, 32, 76, 10, SPECTRUM_CARD_BG);
        tft.setCursor(76, 34); tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
        tft.print(packetSniffer.channel()); tft.print(" / ");
        tft.print(packetSniffer.dataRate() == SnifferDataRate::RATE_1_MBPS ? "1M" : "2M");
    }
    if (previousSnifferPackets != packetSniffer.packetCount()) {
        tft.fillRect(74, 44, 76, 10, SPECTRUM_CARD_BG);
        tft.setCursor(76, 46); tft.setTextColor(SPECTRUM_ACCENT, SPECTRUM_CARD_BG);
        tft.print(packetSniffer.packetCount());
    }

    String text = packetSniffer.isRunning() ? packetSniffer.lastHex()
                                             : packetSniffer.lastError();
    if (!text.length()) text = packetSniffer.isRunning() ? "WAITING FOR PACKET..." : "NO RADIO DATA";
    if (!snifferRunningValid || previousSnifferRunning != running || previousSnifferText != text) {
        tft.fillRect(8, 61, 144, 39, SPECTRUM_CARD_BG);
        tft.setCursor(10, 63); tft.setTextColor(ST77XX_GRAY, SPECTRUM_CARD_BG);
        tft.print(running ? "LATEST RAW HEX" : "LAST ERROR");
        for (uint8_t line = 0; line < 3; ++line) {
            const int offset = line * 23;
            if (offset >= static_cast<int>(text.length())) break;
            tft.setCursor(10, 75 + line * 9); tft.setTextColor(ST77XX_WHITE, SPECTRUM_CARD_BG);
            tft.print(text.substring(offset, min<int>(offset + 23, text.length())));
        }
    }
    previousSnifferRunning = running; snifferRunningValid = true;
    previousSnifferChannel = packetSniffer.channel(); previousSnifferRate = rate;
    previousSnifferPackets = packetSniffer.packetCount(); previousSnifferText = text;
}
