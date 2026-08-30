#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>
#include <freertos/FreeRTOS.h>
#include "config/Config.h"
#include "core/AppState.h"
#include "services/PacketSniffer.h"

class RadioManager {
public:
    struct LoopbackResult {
        bool radio1Detected = false, radio2Detected = false;
        bool txTestEnabled = false;
        bool radio1Tx = false, radio1Rx = false;
        bool radio2Tx = false, radio2Rx = false;
        bool radio1To2 = false, radio2To1 = false;
    };
    RadioManager();
    bool init();

    // Jammer Control (Dual-Core FreeRTOS)
    void startJammer(JammerTarget target);
    void stopJammer();

    // Analyzer Control
    void enterRxMode();
    void enterTxMode();
    void scanSpectrum(void (*yieldCb)() = nullptr);
    void requestScanAbort();
    uint8_t inspectChannel(int channel);
    bool sampleCarrier(uint8_t channel, uint16_t requested, uint16_t& hits, uint16_t& samples);
    bool sampleCarrierOnRadio(uint8_t radioIndex, uint8_t channel, uint16_t requested,
                              uint16_t& hits, uint16_t& samples);
    bool transmitProbePacket(uint8_t channel, uint8_t pa, uint8_t rate, uint8_t size, const uint8_t* payload);
    bool transmitProbePacketOnRadio(uint8_t radioIndex, uint8_t channel, uint8_t pa,
                                    uint8_t rate, uint8_t size, const uint8_t* payload);

    // Passive raw-payload capture using whichever receiver is available.
    bool startPacketSniffer(uint8_t channel, SnifferDataRate rate);
    void stopPacketSniffer();
    bool servicePacketSniffer();
    bool setPacketSnifferChannel(uint8_t channel);
    bool setPacketSnifferDataRate(SnifferDataRate rate);

    // Utility
    void stopAll();
    bool stopAllAndWait(uint32_t timeoutMs = 1000);
    void updatePALevel(rf24_pa_dbm_e pwr);
    bool isConnected();
    bool isRadio1Connected();
    bool isRadio2Connected();
    bool hasAnyRadio() const;
    uint8_t availableRadioCount() const;
    uint32_t getBusContentions() const;
    uint32_t getBusTimeouts() const;
    uint32_t getMaxBusWaitUs() const;
    uint32_t getAverageBusWaitUs() const;
    LoopbackResult runLoopbackDiagnostic(uint8_t channel = 76);
    static constexpr bool transmitFeaturesEnabled() {
#if RF_LAB_TX_ENABLED
        return true;
#else
        return false;
#endif
    }
private:
    RF24 radio;
    RF24 radio2;
    volatile bool radio1Available = false;
    volatile bool radio2Available = false;
    bool rxModeActive = false;
    TaskHandle_t jammerTaskHandle = NULL;
    volatile bool stopJam = false;
    volatile bool scanActive = false;
    volatile bool scanAbortRequested = false;
    bool snifferUsesRadio2 = false;

    bool lockBus(TickType_t timeout = pdMS_TO_TICKS(100));
    void unlockBus();
    void applyTxConfig();
    static void jammerTaskCode(void *param);
};

extern RadioManager radioManager;
