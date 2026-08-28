#include "drivers/RadioManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

RadioManager radioManager;

namespace {
SemaphoreHandle_t radioBusMutex = nullptr;
portMUX_TYPE radioStatsMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t busContentions = 0;
uint32_t busTimeouts = 0;
uint64_t busWaitTotalUs = 0;
uint32_t busLockCount = 0;
uint32_t busMaxWaitUs = 0;

bool lockRadioBus(TickType_t timeout = pdMS_TO_TICKS(100)) {
    if (radioBusMutex == nullptr) return false;
    const uint32_t started = micros();
    bool contended = xSemaphoreTake(radioBusMutex, 0) != pdTRUE;
    bool acquired = !contended;
    if (!acquired) acquired = xSemaphoreTake(radioBusMutex, timeout) == pdTRUE;
    const uint32_t waited = micros() - started;
    portENTER_CRITICAL(&radioStatsMux);
    if (contended) busContentions++;
    if (!acquired) busTimeouts++;
    if (acquired) {
        busLockCount++;
        busWaitTotalUs += waited;
        if (waited > busMaxWaitUs) busMaxWaitUs = waited;
    }
    portEXIT_CRITICAL(&radioStatsMux);
    return acquired;
}

void unlockRadioBus() {
    if (radioBusMutex != nullptr) xSemaphoreGive(radioBusMutex);
}
}

// =============================================================================
// FILE-LOCAL: FAST TX CONFIG for a single nRF24L01+ (packet-storm mode)
// =============================================================================
static void applyFastTxConfig(RF24 &r) {
    r.stopListening();
    r.setAutoAck(false);
    r.setRetries(0, 0);
    r.setPayloadSize(FAST_PAYLOAD_SIZE);
    r.setAddressWidth(FAST_ADDRESS_WIDTH);
    r.setPALevel(appState.powerLevel, true); // true = LNA Gain Enabled!
    r.setDataRate(RF24_2MBPS);
    r.setCRCLength(RF24_CRC_DISABLED);
    r.disableCRC();
    r.disableAckPayload();
    r.disableDynamicPayloads();
}
// =============================================================================
// FILE-LOCAL MAX-AGGRESSION HELPERS (used by the Core 0 jammer task)
//
// Both nRF24L01+ run in REUSE_TX_PL mode: after a single payload is loaded the
// chip re-broadcasts it back-to-back while CE stays HIGH = 100% airtime, and
// hopping only costs a 2-byte SPI write (setChannel) + a cheap reUseTX()
// re-assertion. The SPI bus is barely touched while the radios blast away.
// =============================================================================
namespace {

// Load the payload once and switch the radio into continuous-reuse mode.
void armContinuousJam(RF24 &r) {
    r.setChannel(0);
    r.writeFast(FAST_JAM_PAYLOAD, FAST_PAYLOAD_SIZE); // primer payload on air
    r.reUseTX();                                      // endless re-transmission
}

// One aggressive hop: retune + re-assert reuse + dwell at 100% duty.
inline void hopAndReuse(RF24 &r, uint8_t ch, volatile int &activeChannel) {
    if (lockRadioBus(pdMS_TO_TICKS(20))) {
        r.setChannel(ch);
        activeChannel = ch;
        r.reUseTX();
        unlockRadioBus();
    }
    delayMicroseconds(appState.dwellTimeUs);
}

// Split-sweep: radio A covers even indices, radio B odd -> two frequencies are
// pounded simultaneously.
void hopSplit(RF24 &rA, RF24 &rB, const uint8_t *channels, int count,
              const volatile bool &stop) {
    for (int i = 0; i < count && !stop; i++) {
        hopAndReuse(rA, channels[i], appState.currentJamChannel);
        i++;
        if (i < count && !stop) {
            hopAndReuse(rB, channels[i], appState.currentJamChannel2);
        }
    }
}

void hopSingle(RF24 &r, const uint8_t *channels, int count,
               volatile int &activeChannel, const volatile bool &stop) {
    for (int i = 0; i < count && !stop; i++) {
        hopAndReuse(r, channels[i], activeChannel);
    }
}

// Paired-sweep: two channellists swept in lockstep (e.g. BT even/odd).
void hopPaired(RF24 &rA, const uint8_t *listA, RF24 &rB, const uint8_t *listB,
               int count, const volatile bool &stop) {
    for (int i = 0; i < count && !stop; i++) {
        hopAndReuse(rA, listA[i], appState.currentJamChannel);
        if (i < count && !stop) {
            hopAndReuse(rB, listB[i], appState.currentJamChannel2);
        }
    }
}

// Dual-burst: BOTH radios land on the SAME channel consecutively, doubling the
// dwell / energy on critical channels (BLE advertising 37/38/39).
void hopDuo(RF24 &rA, RF24 &rB, const uint8_t *channels, int count,
            const volatile bool &stop) {
    for (int i = 0; i < count && !stop; i++) {
        hopAndReuse(rA, channels[i], appState.currentJamChannel);
        if (i < count && !stop) {
            hopAndReuse(rB, channels[i], appState.currentJamChannel2);
        }
    }
}

// ZigBee: alternate RF offsets inside every ZigBee band across the two radios.
void hopZigbee(RF24 &rA, RF24 &rB, const volatile bool &stop) {
    for (int channel = 11; channel < 27 && !stop; channel++) {
        int startCh = 4 + 5 * (channel - 11);
        int endCh = (5 + 5 * (channel - 11)) + 2;

        int ch = startCh;
        while (ch <= endCh && !stop) {
            if (ch > MAX_CHANNEL) break;

            hopAndReuse(rA, ch, appState.currentJamChannel);
            ch++;

            if (ch <= endCh && ch <= MAX_CHANNEL && !stop) {
                hopAndReuse(rB, ch, appState.currentJamChannel2);
                ch++;
            }
        }
    }
}

void hopZigbeeSingle(RF24 &r, volatile int &activeChannel,
                     const volatile bool &stop) {
    for (int channel = 11; channel < 27 && !stop; channel++) {
        const int startCh = 4 + 5 * (channel - 11);
        const int endCh = min(MAX_CHANNEL, (5 + 5 * (channel - 11)) + 2);
        for (int ch = startCh; ch <= endCh && !stop; ch++) {
            hopAndReuse(r, ch, activeChannel);
        }
    }
}

} // namespace

RadioManager::RadioManager() : radio(CE_PIN, CSN_PIN), radio2(CE_PIN_2, CSN_PIN_2) {}

bool RadioManager::lockBus(TickType_t timeout) { return lockRadioBus(timeout); }
void RadioManager::unlockBus() { unlockRadioBus(); }

bool RadioManager::init() {
    if (radioBusMutex == nullptr) radioBusMutex = xSemaphoreCreateMutex();
    if (radioBusMutex == nullptr) {
        Serial.println("FATAL: unable to allocate radio SPI mutex");
        return false;
    }
    // Ensure both radios start de-asserted. If a module was left in TX (e.g.
    // after a brownout reset during MAX-AGGRESSION jamming), CE HIGH holds it
    // transmitting and can stall the next begin(). Pull CE LOW first.
    radio.ce(LOW);
    radio2.ce(LOW);

    // Initialize the custom SPI bus (shared by both radios, 4 MHz)
    SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_PIN);
    SPI.setFrequency(4000000);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);

    // nRF24L01+ modules are famously unresponsive for a few ms right after
    // power-up or after the 3.3V rail sags (brownout reset under heavy RF load).
    // A single begin() attempt followed by a hard hang is fragile -- retry with a
    // settle delay and power-down cleanup between attempts instead.
    const int MAX_ATTEMPTS = 5;
    bool okA = false, okB = false;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; ++attempt) {
        delay(50); // let the 3.3V rail fully rise / module settle

        // Initialize both nRF24L01+ modules (different CSN/CE, same SPI bus)
        if (!okA) okA = radio.begin(&SPI);
        if (!okB) okB = radio2.begin(&SPI);
        if (okA && okB) break;

        Serial.printf("[init] attempt %d/%d: radio1=%s radio2=%s\n",
                      attempt, MAX_ATTEMPTS,
                      okA ? "OK" : "FAIL", okB ? "OK" : "FAIL");

        // Clean up before retrying (begin() may have left the module powered up).
        if (okA) radio.powerDown();
        if (okB) radio2.powerDown();
    }

    if (okA || okB) {
        radio1Available = okA;
        radio2Available = okB;
        if (transmitFeaturesEnabled()) applyTxConfig();
        else enterRxMode();
        Serial.printf("nRF24 ready: R1=%s R2=%s (%u active)\n",
                      okA ? "OK" : "OFFLINE", okB ? "OK" : "OFFLINE",
                      availableRadioCount());
        if (availableRadioCount() == 1) {
            Serial.printf("nRF24 single-radio mode: using R%u for all features\n",
                          radio1Available ? 1U : 2U);
        }
        Serial.println("   Use the display menu or type 'help' in Serial.\n");
        return true;
    }

    radio1Available = false;
    radio2Available = false;
    Serial.println("FATAL: no nRF24L01+ detected after 5 attempts!");
    Serial.println("   Check wiring, 3.3V supply and decoupling caps.\n");
    return false;
}

void RadioManager::applyTxConfig() {
    if (!lockRadioBus()) return;
    if (radio1Available) applyFastTxConfig(radio);
    if (radio2Available) applyFastTxConfig(radio2);
    unlockRadioBus();
    rxModeActive = false;
}

// Enter transmission mode: force a clean TX state on both radios, power them
// up and, crucial for a streaming jammer, drive CE HIGH so the TX FIFO keeps
// draining on-air. WITHOUT CE HIGH, writeFast() would just stuff the 3-slot
// FIFO and then block forever waiting for a free slot.
void RadioManager::enterTxMode() {
#if !RF_LAB_TX_ENABLED
    Serial.println("RF Test is disabled in the ANALYZER_ONLY build.");
    return;
#else
    applyTxConfig();      // stopListening + all TX patches on both radios
    if (!lockRadioBus()) return;
    if (radio1Available) { radio.powerUp(); radio.ce(HIGH); }
    if (radio2Available) { radio2.powerUp(); radio2.ce(HIGH); }
    unlockRadioBus();
#endif
}

void RadioManager::enterRxMode() {
    if (!hasAnyRadio() || !lockRadioBus()) return;
    // Fresh RX state on both radios (no unresolved carrier/fifo leftovers)
    if (radio1Available) {
        radio.stopConstCarrier();
        radio.flush_tx();
        radio.flush_rx();
        radio.setAutoAck(false);
        radio.setPALevel(RF24_PA_MAX, true);
        radio.setDataRate(RF24_2MBPS);
        radio.setCRCLength(RF24_CRC_DISABLED);
        radio.startListening();
    }

    if (radio2Available) {
        radio2.stopConstCarrier();
        radio2.flush_tx();
        radio2.flush_rx();
        radio2.setAutoAck(false);
        radio2.setPALevel(RF24_PA_MAX, true);
        radio2.setDataRate(RF24_2MBPS);
        radio2.setCRCLength(RF24_CRC_DISABLED);
        radio2.startListening();
    }
    unlockRadioBus();

    rxModeActive = true;
}

void RadioManager::startJammer(JammerTarget target) {
#if !RF_LAB_TX_ENABLED
    (void)target;
    Serial.println("RF Test unavailable: build AUTHORIZED_RF_LAB to enable it.");
    return;
#else
    if (!hasAnyRadio()) {
        Serial.println("RF Test unavailable: no radio detected.");
        return;
    }
    // If the task is already sweeping, just hot-swap the target
    // (the running loop re-reads appState.jammerTarget each round).
    if (appState.jamming && jammerTaskHandle != NULL) {
        appState.setJammerTarget(target);
        return;
    }

    appState.setJammerTarget(target);
    stopJam = false;

    // Full deterministic TX-handler state, including CE HIGH
    enterTxMode();

    // Max-aggression arming: both radios enter continuous REUSE_TX_PL mode and
    // stream at 100% airtime; the task loop only hops channels afterwards.
    if (!lockRadioBus()) return;
    if (radio1Available) armContinuousJam(radio);
    if (radio2Available) armContinuousJam(radio2);
    unlockRadioBus();

    appState.jamming = true;

    // Launch the ultra-fast aggressive transmission task on Core 0
    xTaskCreatePinnedToCore(
        RadioManager::jammerTaskCode,
        "RFJammer",
        4096,
        this,
        3,
        &jammerTaskHandle,
        0
    );

    Serial.println("JAMMER ACTIVE (Core 0 Background Task): " + String(appState.getJammerTargetName()));
    Serial.println("   Range: " + String(appState.getJammerFreqRangeStr()) + "\n");
#endif
}

void RadioManager::stopJammer() {
    if (!appState.jamming && jammerTaskHandle == NULL) return;

    stopJam = true;

    // Keep CE high while the task unwinds so the TX FIFO keeps draining and
    // writeFast() can never block forever. The loop checks stopJam between
    // every payload, so the task exits in microseconds.
    if (jammerTaskHandle != NULL) {
        unsigned long startWait = millis();
        while (jammerTaskHandle != NULL && (millis() - startWait < 500)) {
            delay(2);
        }
    }

    // Now stop both radios cleanly and drop any residual FIFO payloads
    if (lockRadioBus()) {
        if (radio1Available) { radio.ce(LOW); radio.flush_tx(); radio.powerDown(); }
        if (radio2Available) { radio2.ce(LOW); radio2.flush_tx(); radio2.powerDown(); }
        unlockRadioBus();
    }
    appState.jamming = false;
    Serial.println("Jammer Stopped.");
}

bool RadioManager::stopAllAndWait(uint32_t timeoutMs) {
    stopAll();
    const uint32_t started = millis();
    while ((jammerTaskHandle != nullptr || scanActive) && millis() - started < timeoutMs) {
        delay(2);
    }
    return jammerTaskHandle == nullptr && !scanActive;
}

void RadioManager::stopAll() {
    stopPacketSniffer();
    stopJammer();
    scanAbortRequested = true;
    // scanSpectrum owns this same non-recursive mutex. When stopAll() is
    // invoked from its UI yield callback, waiting here would self-block until
    // timeout. The scan loop observes the flag within a few samples and does
    // the RX shutdown before releasing the bus.
    if (scanActive) return;
    if (lockRadioBus()) {
        if (radio1Available) radio.stopListening();
        if (radio2Available) radio2.stopListening();
        unlockRadioBus();
    }
    rxModeActive = false;
}

void RadioManager::requestScanAbort() { scanAbortRequested = true; }

bool RadioManager::startPacketSniffer(uint8_t channel, SnifferDataRate rate) {
    stopJammer();
    scanAbortRequested = true;
    if (!hasAnyRadio()) {
        Serial.println("[sniffer] no radio is available");
        return false;
    }
    if (!lockBus()) {
        Serial.println("[sniffer] SPI bus timeout during start");
        return false;
    }
    snifferUsesRadio2 = !radio1Available && radio2Available;
    RF24& target = snifferUsesRadio2 ? radio2 : radio;
    const uint8_t cePin = snifferUsesRadio2 ? CE_PIN_2 : CE_PIN;
    const uint8_t csnPin = snifferUsesRadio2 ? CSN_PIN_2 : CSN_PIN;
    const bool ok = packetSniffer.start(target, SPI, cePin, csnPin, channel, rate);
    if (!ok) {
        packetSniffer.stop(target);
        target.flush_rx();
        target.setAddressWidth(5);
        target.setAutoAck(false);
        target.setDataRate(RF24_2MBPS);
        target.setCRCLength(RF24_CRC_DISABLED);
        target.startListening();
    }
    unlockBus();
    if (!ok) packetSniffer.closeStorage();
    return ok;
}

void RadioManager::stopPacketSniffer() {
    if (!packetSniffer.hasRadioControl()) return;
    if (!lockBus()) {
        Serial.println("[sniffer] SPI bus timeout during stop");
        return;
    }
    RF24& target = snifferUsesRadio2 ? radio2 : radio;
    packetSniffer.stop(target);
    target.flush_rx();
    target.setAddressWidth(5);
    target.setAutoAck(false);
    target.setDataRate(RF24_2MBPS);
    target.setCRCLength(RF24_CRC_DISABLED);
    target.startListening();
    unlockBus();
    packetSniffer.closeStorage();
}

bool RadioManager::servicePacketSniffer() {
    if (!packetSniffer.isRunning()) return false;
    if (!lockBus(pdMS_TO_TICKS(20))) return false;
    RF24& target = snifferUsesRadio2 ? radio2 : radio;
    const bool captured = packetSniffer.poll(target);
    unlockBus();
    packetSniffer.serviceStorage();
    return captured;
}

bool RadioManager::setPacketSnifferChannel(uint8_t channel) {
    if (!packetSniffer.isRunning() || !lockBus()) return false;
    RF24& target = snifferUsesRadio2 ? radio2 : radio;
    const bool ok = packetSniffer.setChannel(target, channel);
    unlockBus();
    return ok;
}

bool RadioManager::setPacketSnifferDataRate(SnifferDataRate rate) {
    if (!packetSniffer.isRunning() || !lockBus()) return false;
    RF24& target = snifferUsesRadio2 ? radio2 : radio;
    const bool ok = packetSniffer.setDataRate(target, rate);
    unlockBus();
    return ok;
}

void RadioManager::updatePALevel(rf24_pa_dbm_e pwr) {
    appState.powerLevel = pwr;
    if (!lockRadioBus()) return;
    if (radio1Available) radio.setPALevel(pwr, true);
    if (radio2Available) radio2.setPALevel(pwr, true);
    unlockRadioBus();
}

bool RadioManager::transmitProbePacket(uint8_t channel, uint8_t pa, uint8_t rate, uint8_t size, const uint8_t* payload) {
#if !RF_LAB_TX_ENABLED
    (void)channel;(void)pa;(void)rate;(void)size;(void)payload;return false;
#else
    if(!hasAnyRadio()||channel>125||pa>RF24_PA_MAX||(rate!=RF24_250KBPS&&rate!=RF24_1MBPS&&rate!=RF24_2MBPS)||size<1||size>32||payload==nullptr)return false;
    if(!lockBus(pdMS_TO_TICKS(50)))return false;
    RF24& target=radio1Available?radio:radio2;
    target.ce(LOW);target.stopListening();target.setChannel(channel);target.setPALevel(static_cast<rf24_pa_dbm_e>(pa),true);
    target.setDataRate(static_cast<rf24_datarate_e>(rate));target.setAutoAck(false);target.setRetries(0,0);target.setPayloadSize(size);target.setCRCLength(RF24_CRC_16);
    const bool ok=target.write(payload,size,false);target.ce(LOW);target.flush_tx();target.startListening();unlockBus();rxModeActive=true;return ok;
#endif
}

bool RadioManager::isConnected() {
    return hasAnyRadio();
}

bool RadioManager::isRadio1Connected() {
    if (!radio1Available) return false;
    if (!lockRadioBus(pdMS_TO_TICKS(20))) return radio1Available;
    const bool connected = radio.isChipConnected();
    unlockRadioBus();
    return connected;
}

bool RadioManager::isRadio2Connected() {
    if (!radio2Available) return false;
    if (!lockRadioBus(pdMS_TO_TICKS(20))) return radio2Available;
    const bool connected = radio2.isChipConnected();
    unlockRadioBus();
    return connected;
}

bool RadioManager::hasAnyRadio() const { return radio1Available || radio2Available; }
uint8_t RadioManager::availableRadioCount() const {
    return static_cast<uint8_t>(radio1Available) + static_cast<uint8_t>(radio2Available);
}
uint32_t RadioManager::getBusContentions() const {
    portENTER_CRITICAL(&radioStatsMux);
    const uint32_t value = busContentions;
    portEXIT_CRITICAL(&radioStatsMux);
    return value;
}
uint32_t RadioManager::getBusTimeouts() const {
    portENTER_CRITICAL(&radioStatsMux);
    const uint32_t value = busTimeouts;
    portEXIT_CRITICAL(&radioStatsMux);
    return value;
}
uint32_t RadioManager::getMaxBusWaitUs() const {
    portENTER_CRITICAL(&radioStatsMux);
    const uint32_t value = busMaxWaitUs;
    portEXIT_CRITICAL(&radioStatsMux);
    return value;
}
uint32_t RadioManager::getAverageBusWaitUs() const {
    portENTER_CRITICAL(&radioStatsMux);
    const uint32_t value = busLockCount == 0 ? 0 :
        static_cast<uint32_t>(busWaitTotalUs / busLockCount);
    portEXIT_CRITICAL(&radioStatsMux);
    return value;
}

// =============================================================================
// FREERTOS TASK: CORE 0 MAX-AGGRESSION DUAL-RADIO TRANSMISSION LOOP
//
// Both radios were armed in REUSE_TX_PL mode (see armContinuousJam) and CE is
// HIGH, so they continuously blast the last payload at 100% airtime. This loop
// only retunes channels - each channel is hammered for JAMMER_DWELL_US before
// hopping, and stopJam is re-checked at every hop boundary.
// =============================================================================
void RadioManager::jammerTaskCode(void *param) {
    RadioManager *self = static_cast<RadioManager*>(param);
    vTaskPrioritySet(NULL, 3);

    const volatile bool &stop = self->stopJam;
    const bool dual = self->radio1Available && self->radio2Available;
    RF24 &singleRadio = self->radio1Available ? self->radio : self->radio2;
    volatile int &singleChannel = self->radio1Available ?
                                  appState.currentJamChannel :
                                  appState.currentJamChannel2;

    while (!stop) {
        switch (appState.jammerTarget) {
            // -----------------------------------------------------------------
            // 1. TARGET WI-FI (50 Programmed Channels via wifi_channels[])
            //    Both radios hammer DIFFERENT halves simultaneously.
            // -----------------------------------------------------------------
            case JAM_TARGET_WIFI:
                if (dual) hopSplit(self->radio, self->radio2, wifi_channels,
                                   WIFI_CHANNELS_COUNT, stop);
                else hopSingle(singleRadio, wifi_channels, WIFI_CHANNELS_COUNT,
                               singleChannel, stop);
                break;

            // -----------------------------------------------------------------
            // 2. TARGET BLUETOOTH (Full 1-80 hop)
            //    Radio1 pounds even channels, radio2 odd channels IN LOCKSTEP,
            //    so both transceivers radiate at the same time.
            // -----------------------------------------------------------------
            case JAM_TARGET_BT:
                if (dual) hopPaired(self->radio, bluetooth_even_channels,
                                    self->radio2, bluetooth_odd_channels,
                                    BLUETOOTH_EVEN_CHANNELS_COUNT, stop);
                else hopSingle(singleRadio, bluetooth_even_channels,
                               BLUETOOTH_EVEN_CHANNELS_COUNT, singleChannel, stop);
                break;

            // -----------------------------------------------------------------
            // 3. TARGET BLE ADVERTISING (Ch 37/38/39 = RF 2/26/80 + hops)
            //    Both radios land on EVERY channel together = 2x dwell/energy
            //    on the critical advertising frequencies.
            // -----------------------------------------------------------------
            case JAM_TARGET_BLE_ADV:
                if (dual) hopDuo(self->radio, self->radio2, ble_channels,
                                 BLE_CHANNELS_COUNT, stop);
                else hopSingle(singleRadio, ble_channels, BLE_CHANNELS_COUNT,
                               singleChannel, stop);
                break;

            // -----------------------------------------------------------------
            // 4. TARGET BLE DATA (12 Channels from 3 Channel Groups)
            //    Split hop across both radios for 2x coverage.
            // -----------------------------------------------------------------
            case JAM_TARGET_BLE_DATA:
                if (dual) hopSplit(self->radio, self->radio2, BLE_DATA_CHANNELS,
                                   BLE_DATA_CHANNELS_COUNT, stop);
                else hopSingle(singleRadio, BLE_DATA_CHANNELS,
                               BLE_DATA_CHANNELS_COUNT, singleChannel, stop);
                break;

            // -----------------------------------------------------------------
            // 5. TARGET ALL BAND / DRONE (Channels 1 - 100 via full_channels[])
            //    Split across both radios: each sweep cycle covers 2x the band.
            // -----------------------------------------------------------------
            case JAM_TARGET_ALL:
                if (dual) hopSplit(self->radio, self->radio2, full_channels,
                                   FULL_CHANNELS_COUNT, stop);
                else hopSingle(singleRadio, full_channels, FULL_CHANNELS_COUNT,
                               singleChannel, stop);
                break;

            // -----------------------------------------------------------------
            // 6. TARGET ZIGBEE (Channels 11 - 26)
            //    RF offsets inside every ZigBee band are alternated between
            //    the two radios for parallel band coverage.
            // -----------------------------------------------------------------
            case JAM_TARGET_ZIGBEE:
                if (dual) hopZigbee(self->radio, self->radio2, stop);
                else hopZigbeeSingle(singleRadio, singleChannel, stop);
                break;
        }
        vTaskDelay(1); // Yield 1 tick so Core 0 IDLE0 can reset the Task Watchdog
    }
    // Publish completion before deleting the task. Waiting code must not call
    // eTaskGetState() on a handle whose task has already been freed.
    self->jammerTaskHandle = nullptr;
    vTaskDelete(NULL);
}

// =============================================================================
// SPECTRUM ANALYZER & CHANNEL INSPECTOR
// =============================================================================
