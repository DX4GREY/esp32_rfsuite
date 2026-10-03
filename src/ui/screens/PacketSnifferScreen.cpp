#include "ui/DisplayManager.h"
#include "ui/DisplaySupport.h"
#include "services/PacketSniffer.h"
#include "ui/PacketInspectorView.h"

using namespace DisplayUi;

void DisplayManager::renderPacketSnifferScreen() {
    if (needRedraw) {
        drawModernHeader(appState.simulationMode ? "SIM SNIFFER" : "PKT SNIFFER", TacticalColor::Cyan);
        packetInspectorView.reset();
        drawModernFooter("U/D CH", "SEL RATE", "BACK EXIT");
        needRedraw = false;
    }

    PacketSnifferTelemetry tel;
    char simHex[64];

    if (appState.simulationMode) {
        tel.isRunning = true;
        tel.isSim = true;
        tel.channel = 37;
        tel.dataRateMbps = 2;
        tel.packetCount = millis() / 500;
        snprintf(simHex, sizeof(simHex), "A5 %02lX 10 2C 7E 01 FF 89 24 55 00",
                 static_cast<unsigned long>(tel.packetCount & 0xFF));
        tel.rawHex = simHex;
        tel.errorText = nullptr;
        tel.signalStrength = 75;
    } else {
        tel.isRunning = packetSniffer.isRunning();
        tel.isSim = false;
        tel.channel = packetSniffer.channel();
        tel.dataRateMbps = (packetSniffer.dataRate() == SnifferDataRate::RATE_1_MBPS) ? 1 : 2;
        tel.packetCount = packetSniffer.packetCount();

        String hexStr = packetSniffer.lastHex();
        String errStr = packetSniffer.lastError();
        tel.rawHex = hexStr.length() ? hexStr.c_str() : nullptr;
        tel.errorText = errStr.length() ? errStr.c_str() : nullptr;
        tel.signalStrength = tel.isRunning ? (tel.packetCount ? 85 : 30) : 0;
    }

    packetInspectorView.renderStream(tft, tel);
}
