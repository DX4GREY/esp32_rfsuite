#include "drivers/Cc1101Manager.h"

#include <SPI.h>
#include "config/Config.h"
#include "core/AppState.h"

Cc1101Manager cc1101Manager;

namespace {
constexpr uint8_t WRITE_BURST = 0x40;
constexpr uint8_t READ_BURST = 0xC0;
constexpr uint8_t SRES = 0x30;
constexpr uint8_t SRX = 0x34;
constexpr uint8_t STX = 0x35;
constexpr uint8_t SIDLE = 0x36;
constexpr uint8_t SFRX = 0x3A;
constexpr uint8_t PARTNUM = 0x30;
constexpr uint8_t VERSION = 0x31;
constexpr uint8_t RSSI = 0x34;
constexpr uint8_t RXBYTES = 0x3B;
constexpr uint8_t FIFO = 0x3F;
}

void Cc1101Manager::select() {
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    digitalWrite(CC1101_CSN_PIN, LOW);
}

void Cc1101Manager::deselect() {
    digitalWrite(CC1101_CSN_PIN, HIGH);
    SPI.endTransaction();
}

uint8_t Cc1101Manager::transfer(uint8_t value) { return SPI.transfer(value); }

void Cc1101Manager::strobe(uint8_t command) {
    select(); transfer(command); deselect();
}

void Cc1101Manager::writeRegister(uint8_t address, uint8_t value) {
    select(); transfer(address); transfer(value); deselect();
}

uint8_t Cc1101Manager::readStatus(uint8_t address) {
    select(); transfer(address | READ_BURST); const uint8_t value = transfer(0); deselect();
    return value;
}

void Cc1101Manager::readBurst(uint8_t address, uint8_t* data, size_t length) {
    if (!data || !length) return;
    select(); transfer(address | READ_BURST);
    for (size_t i = 0; i < length; ++i) data[i] = transfer(0);
    deselect();
}

bool Cc1101Manager::reset() {
    digitalWrite(CC1101_CSN_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(CC1101_CSN_PIN, LOW); delayMicroseconds(10);
    digitalWrite(CC1101_CSN_PIN, HIGH); delayMicroseconds(45);
    select();
    const unsigned long started = micros();
    while (digitalRead(MISO_PIN) == HIGH && micros() - started < 1000) delayMicroseconds(1);
    if (digitalRead(MISO_PIN) == HIGH) { deselect(); return false; }
    transfer(SRES);
    const unsigned long resetStarted = micros();
    while (digitalRead(MISO_PIN) == HIGH && micros() - resetStarted < 1000) delayMicroseconds(1);
    const bool ready = digitalRead(MISO_PIN) == LOW;
    deselect(); delay(1);
    return ready;
}

void Cc1101Manager::configureReceiver() {
    strobe(SIDLE);
    writeRegister(0x00, 0x06); // GDO2 asserts on sync/packet (available for future use)
    writeRegister(0x02, 0x0D); // GDO0 high impedance
    writeRegister(0x08, 0x05); // infinite packet length, CRC enabled
    writeRegister(0x0B, 0x06);
    writeRegister(0x0C, 0x00);
    setPreset(activePreset);
    writeRegister(0x18, 0x18); // remain in RX after a packet
    writeRegister(0x19, 0x16);
    writeRegister(0x1A, 0x6C);
    writeRegister(0x1B, 0x43);
    writeRegister(0x1C, 0x40);
    writeRegister(0x1D, 0x91);
    writeRegister(0x21, 0x56);
    writeRegister(0x22, 0x10);
    writeRegister(0x23, 0xE9);
    writeRegister(0x24, 0x2A);
    writeRegister(0x25, 0x00);
    writeRegister(0x26, 0x1F);
    setFrequency(frequency);
}

void Cc1101Manager::setPreset(Cc1101Preset value) {
    activePreset = value < Cc1101Preset::COUNT ? value : Cc1101Preset::OOK_650;
    if (!connected) return;
    strobe(SIDLE);
    uint8_t mdmcfg4 = 0x67, mdmcfg3 = 0x83, mdmcfg2 = 0x30, deviatn = 0x00;
    switch (activePreset) {
        case Cc1101Preset::OOK_270: break;
        case Cc1101Preset::OOK_650: mdmcfg4 = 0x17; break;
        case Cc1101Preset::FSK_2K:  mdmcfg2 = 0x00; deviatn = 0x04; break;
        case Cc1101Preset::FSK_12K: mdmcfg2 = 0x00; deviatn = 0x34; break;
        case Cc1101Preset::FSK_47K: mdmcfg2 = 0x00; deviatn = 0x47; break;
        default: break;
    }
    writeRegister(0x10, mdmcfg4);
    writeRegister(0x11, mdmcfg3);
    writeRegister(0x12, mdmcfg2);
    writeRegister(0x15, deviatn);
    setFrequency(frequency);
}

const char* Cc1101Manager::presetName() const {
    switch (activePreset) {
        case Cc1101Preset::OOK_270: return "OOK 270K";
        case Cc1101Preset::OOK_650: return "OOK 650K";
        case Cc1101Preset::FSK_2K: return "2FSK 2K";
        case Cc1101Preset::FSK_12K: return "2FSK 12K";
        case Cc1101Preset::FSK_47K: return "2FSK 47K";
        default: return "UNKNOWN";
    }
}

bool Cc1101Manager::applyCustomPreset(const uint8_t* data, size_t length) {
    if (!connected || !data || length < 2) return false;
    strobe(SIDLE);
    for (size_t i = 0; i + 1 < length; i += 2) {
        if (data[i] == 0 && data[i + 1] == 0) break;
        if (data[i] <= 0x2E) writeRegister(data[i], data[i + 1]);
    }
    setFrequency(frequency);
    return true;
}

bool Cc1101Manager::init() {
    pinMode(CC1101_CSN_PIN, OUTPUT);
    digitalWrite(CC1101_CSN_PIN, HIGH);
    if (!reset()) { error = "MISO TIMEOUT"; return false; }
    part = readStatus(PARTNUM);
    chipVersion = readStatus(VERSION);
    connected = part == 0x00 && chipVersion != 0x00 && chipVersion != 0xFF;
    if (!connected) { error = "CHIP NOT FOUND"; return false; }
    configureReceiver();
    error = "OK";
    return true;
}

bool Cc1101Manager::setFrequency(float mhz) {
    if (mhz < 300.0f || mhz > 928.0f) return false;
    frequency = mhz;
    if (!connected) return true;
    const uint32_t word = static_cast<uint32_t>((mhz * 65536.0f) / 26.0f + 0.5f);
    strobe(SIDLE);
    writeRegister(0x0D, (word >> 16) & 0xFF);
    writeRegister(0x0E, (word >> 8) & 0xFF);
    writeRegister(0x0F, word & 0xFF);
    strobe(SRX);
    delay(2);
    return true;
}

int16_t Cc1101Manager::readRssiDbm() {
    if (!connected) return -127;
    const uint8_t raw = readStatus(RSSI);
    const int16_t signedRaw = raw >= 128 ? static_cast<int16_t>(raw) - 256 : raw;
    return signedRaw / 2 - 74;
}

bool Cc1101Manager::carrierDetected(int16_t thresholdDbm) {
    return connected && readRssiDbm() >= thresholdDbm;
}

bool Cc1101Manager::enterPacketReceive() {
    if (!connected) return false;
    strobe(SIDLE); strobe(SFRX);
    writeRegister(0x02, 0x06); // GDO0 asserts on sync and deasserts at packet end
    writeRegister(0x06, 61);   // maximum payload fitting FIFO with status bytes
    writeRegister(0x07, 0x04); // append RSSI/LQI status
    writeRegister(0x08, 0x05); // variable length + CRC
    const bool ook = activePreset == Cc1101Preset::OOK_270 ||
                     activePreset == Cc1101Preset::OOK_650;
    writeRegister(0x12, ook ? 0x32 : 0x02); // modulation + 16/16 sync detection
    pinMode(CC1101_GDO0_PIN, INPUT);
    strobe(SRX);
    return true;
}

bool Cc1101Manager::readPacket(uint8_t* payload, uint8_t& length,
                               int16_t& rssiDbm, uint8_t& lqi, bool& crcOk) {
    if (!connected || !payload) return false;
    const uint8_t rxBytes = readStatus(RXBYTES);
    if (rxBytes & 0x80) { strobe(SIDLE); strobe(SFRX); strobe(SRX); return false; }
    if ((rxBytes & 0x7F) < 3) return false;
    uint8_t packetLength = 0;
    readBurst(FIFO, &packetLength, 1);
    if (!packetLength || packetLength > 61 || (rxBytes & 0x7F) < packetLength + 3) {
        strobe(SIDLE); strobe(SFRX); strobe(SRX); return false;
    }
    uint8_t data[63];
    readBurst(FIFO, data, packetLength + 2);
    memcpy(payload, data, packetLength);
    length = packetLength;
    const int16_t signedRssi = data[packetLength] >= 128 ?
        static_cast<int16_t>(data[packetLength]) - 256 : data[packetLength];
    rssiDbm = signedRssi / 2 - 74;
    lqi = data[packetLength + 1] & 0x7F;
    crcOk = (data[packetLength + 1] & 0x80) != 0;
    return true;
}

bool Cc1101Manager::enterRawReceive() {
    if (!connected) return false;
    strobe(SIDLE);
    writeRegister(0x02, 0x0D); // GDO0: asynchronous serial data output
    writeRegister(0x08, 0x32); // asynchronous serial, infinite packet mode
    pinMode(CC1101_GDO0_PIN, INPUT);
    strobe(SRX);
    return true;
}

bool Cc1101Manager::enterRawTransmit() {
#if !RF_LAB_TX_ENABLED
    return false;
#else
    if (!connected) return false;
    strobe(SIDLE);
    writeRegister(0x02, 0x0D); // GDO0: asynchronous serial data input in TX
    writeRegister(0x08, 0x32);
    updatePowerLevel();
    pinMode(CC1101_GDO0_PIN, OUTPUT);
    digitalWrite(CC1101_GDO0_PIN, LOW);
    strobe(STX);
    delayMicroseconds(200);
    return true;
#endif
}

void Cc1101Manager::updatePowerLevel() {
    if (!connected) return;
    // Common CC1101 PA-table steps, mapped to the four global UI levels.
    // Exact conducted power varies with the fitted module/band matching.
    static constexpr uint8_t paTable[] = {0x12, 0x34, 0x60, 0xC0};
    const int level = constrain(static_cast<int>(appState.powerLevel), 0, 3);
    writeRegister(0x3E, paTable[level]);
}

void Cc1101Manager::idle() {
    if (connected) strobe(SIDLE);
    pinMode(CC1101_GDO0_PIN, INPUT);
}

void Cc1101Manager::setRawData(bool high) {
#if RF_LAB_TX_ENABLED
    digitalWrite(CC1101_GDO0_PIN, high ? HIGH : LOW);
#else
    (void)high;
#endif
}
