#include "services/PacketSniffer.h"
#include "core/AppState.h"
#include "services/StorageManager.h"

PacketSniffer packetSniffer;

uint8_t PacketSniffer::transferRegister(uint8_t command, uint8_t value) {
    bus->beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
    digitalWrite(csn, LOW);
    bus->transfer(command);
    const uint8_t result = bus->transfer(value);
    digitalWrite(csn, HIGH);
    bus->endTransaction();
    return result;
}

bool PacketSniffer::writeRegisterVerified(uint8_t reg, uint8_t value) {
    transferRegister(WRITE_REGISTER | (REGISTER_MASK & reg), value);
    const uint8_t readBack = transferRegister(REGISTER_MASK & reg, 0xFF);
    if (readBack != value) {
        setError("SPI verify failed for SETUP_AW");
        return false;
    }
    return true;
}

void PacketSniffer::setError(const String& message) {
    errorMessage = message;
    ++errors;
    Serial.println("[sniffer] " + message);
}

void PacketSniffer::formatHex(const uint8_t* payload, uint8_t length) {
    static const char HEX_DIGITS[] = "0123456789ABCDEF";
    lastPacketHex = "";
    lastPacketHex.reserve(length * 3);
    for (uint8_t i = 0; i < length; ++i) {
        if (i) lastPacketHex += ' ';
        lastPacketHex += HEX_DIGITS[payload[i] >> 4];
        lastPacketHex += HEX_DIGITS[payload[i] & 0x0F];
    }
}

bool PacketSniffer::start(RF24& target, SPIClass& spi, uint8_t cePin,
                          uint8_t csnPin, uint8_t channel,
                          SnifferDataRate rate) {
    bus = &spi;
    ce = cePin;
    csn = csnPin;
    clear();

    if (appState.saveSniffPacketsToSd) prepareStorage();

    if (!target.isChipConnected()) {
        setError("selected nRF24 is not responding");
        return false;
    }

    configured = true;
    target.stopListening();
    target.ce(LOW);
    target.flush_rx();
    target.setPayloadSize(RAW_PAYLOAD_SIZE);
    target.disableDynamicPayloads();
    const uint64_t catchAddress = 0x00AAULL;
    target.openReadingPipe(1, catchAddress);

    // Experimental promiscuous sequence. SETUP_AW=0 requests the undocumented
    // 2-byte address mode used by common nRF24 sniffing techniques.
    target.setAutoAck(false);                         // EN_AA = 0
    target.disableCRC();                              // EN_CRC = 0
    if (!writeRegisterVerified(SETUP_AW_REGISTER, 0)) return false;
    if (!setDataRate(target, rate)) return false;
    if (!setChannel(target, channel)) return false;
    target.startListening();                          // PRIM_RX = 1, CE = HIGH

    running = true;
    Serial.printf("[sniffer] started ch=%u rate=%s address-width=2(experimental)\n",
                  activeChannel,
                  activeRate == SnifferDataRate::RATE_1_MBPS ? "1Mbps" : "2Mbps");
    return true;
}

void PacketSniffer::stop(RF24& target) {
    if (configured) target.stopListening();
    running = false;
    configured = false;
}

bool PacketSniffer::poll(RF24& target) {
    if (!running) return false;
    // isChipConnected() rejects SETUP_AW=0 by design. Reading CONFIG directly
    // still detects the all-ones response produced by a disconnected SPI bus.
    if (transferRegister(0x00, 0xFF) == 0xFF) {
        running = false;
        setError("SPI connection lost while capturing");
        return false;
    }

    bool captured = false;
    uint8_t payload[RAW_PAYLOAD_SIZE];
    uint8_t drained = 0;
    while (target.available() && drained++ < 3) {
        target.read(payload, sizeof(payload));
        formatHex(payload, sizeof(payload));
        ++capturedPackets;
        captured = true;
        Serial.printf("[sniffer ch=%u #%lu] %s\n", activeChannel,
                      static_cast<unsigned long>(capturedPackets),
                      lastPacketHex.c_str());
        queueStorageRecord();
    }
    return captured;
}

bool PacketSniffer::setChannel(RF24& target, uint8_t channel) {
    if (channel > 125) {
        setError("channel must be between 0 and 125");
        return false;
    }
    target.setChannel(channel);
    if (target.getChannel() != channel) {
        setError("SPI verify failed while setting channel");
        return false;
    }
    activeChannel = channel;
    return true;
}

bool PacketSniffer::setDataRate(RF24& target, SnifferDataRate rate) {
    const rf24_datarate_e selected = rate == SnifferDataRate::RATE_1_MBPS
        ? RF24_1MBPS : RF24_2MBPS;
    if (!target.setDataRate(selected)) {
        setError("nRF24 rejected requested data rate");
        return false;
    }
    activeRate = rate;
    return true;
}

void PacketSniffer::clear() {
    capturedPackets = 0;
    errors = 0;
    lastPacketHex = "";
    errorMessage = "";
}

bool PacketSniffer::prepareStorage() {
    storageReady = false;
    storageErrorMessage = "";
    if (!storageManager.usingSd()) {
        storageErrorMessage = "SD card not detected";
        Serial.println("[sniffer] " + storageErrorMessage);
        return false;
    }
    fs::FS& fs = storageManager.filesystem();
    const bool needsHeader = !fs.exists(storagePath());
    File file = fs.open(storagePath(), FILE_APPEND);
    if (!file) {
        storageErrorMessage = "cannot open packet log";
        Serial.println("[sniffer] " + storageErrorMessage);
        return false;
    }
    if (needsHeader) file.println("ms,channel,data_rate_mbps,payload_hex");
    file.close();
    storagePending.reserve(3072);
    storagePending = "";
    lastStorageFlushMs = millis();
    storageReady = true;
    return true;
}

void PacketSniffer::queueStorageRecord() {
    if (!appState.saveSniffPacketsToSd) return;
    if (!storageReady && !prepareStorage()) return;
    storagePending += String(millis()) + ',' + String(activeChannel) + ',' +
        (activeRate == SnifferDataRate::RATE_1_MBPS ? "1" : "2") + ",\"" +
        lastPacketHex + "\"\n";
}

void PacketSniffer::serviceStorage(bool force) {
    if (!storageReady || storagePending.length() == 0) return;
    if (!force && storagePending.length() < 2048 &&
        millis() - lastStorageFlushMs < 2000) return;
    File file = storageManager.filesystem().open(storagePath(), FILE_APPEND);
    if (!file) {
        storageErrorMessage = "packet log append failed";
        Serial.println("[sniffer] " + storageErrorMessage);
        storageReady = false;
        storagePending = "";
        return;
    }
    const size_t expected = storagePending.length();
    const size_t written = file.print(storagePending);
    file.close();
    if (written != expected) {
        storageErrorMessage = "short packet log write";
        Serial.println("[sniffer] " + storageErrorMessage);
        storageReady = false;
    }
    storagePending = "";
    lastStorageFlushMs = millis();
}

void PacketSniffer::closeStorage() {
    serviceStorage(true);
    storageReady = false;
}
