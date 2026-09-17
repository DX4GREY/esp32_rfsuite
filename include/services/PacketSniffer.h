#pragma once

#include <Arduino.h>
#include <RF24.h>
#include <SPI.h>

enum class SnifferDataRate : uint8_t {
    RATE_1_MBPS = 0,
    RATE_2_MBPS
};

class PacketSniffer {
public:
    bool start(RF24& target, SPIClass& spi, uint8_t cePin, uint8_t csnPin,
               uint8_t channel, SnifferDataRate rate);
    // Re-apply the promiscuous RX configuration after a transient SPI/radio
    // fault without clearing the current session counters.
    bool recover(RF24& target);
    void stop(RF24& target);
    bool poll(RF24& target);
    bool setChannel(RF24& target, uint8_t channel);
    bool setDataRate(RF24& target, SnifferDataRate rate);

    bool isRunning() const { return running; }
    bool recoveryRequested() const { return recoveryPending; }
    bool hasRadioControl() const { return configured; }
    uint8_t channel() const { return activeChannel; }
    SnifferDataRate dataRate() const { return activeRate; }
    uint32_t packetCount() const { return capturedPackets; }
    uint32_t errorCount() const { return errors; }
    const String& lastHex() const { return lastPacketHex; }
    const String& lastError() const { return errorMessage; }
    const String& storageError() const { return storageErrorMessage; }
    bool isSaving() const { return storageReady; }
    const char* storagePath() const { return "/RFSuite/log/packet_sniffer.csv"; }
    void serviceStorage(bool force = false);
    void closeStorage();
    void clear();

private:
    static constexpr uint8_t RAW_PAYLOAD_SIZE = 32;
    static constexpr uint8_t SETUP_AW_REGISTER = 0x03;
    static constexpr uint8_t WRITE_REGISTER = 0x20;
    static constexpr uint8_t REGISTER_MASK = 0x1F;

    SPIClass* bus = nullptr;
    uint8_t ce = 0;
    uint8_t csn = 0;
    bool running = false;
    bool configured = false;
    bool recoveryPending = false;
    uint8_t activeChannel = 0;
    SnifferDataRate activeRate = SnifferDataRate::RATE_2_MBPS;
    uint32_t capturedPackets = 0;
    uint32_t errors = 0;
    String lastPacketHex;
    String errorMessage;
    String storageErrorMessage;
    String storagePending;
    bool storageReady = false;
    unsigned long lastStorageFlushMs = 0;
    unsigned long lastHealthCheckMs = 0;
    unsigned long lastRecoveryAttemptMs = 0;
    uint8_t connectionFailures = 0;

    static constexpr unsigned long HEALTH_CHECK_INTERVAL_MS = 500;
    static constexpr unsigned long RECOVERY_RETRY_INTERVAL_MS = 250;
    static constexpr uint8_t CONNECTION_FAILURE_LIMIT = 3;

    bool configure(RF24& target);
    bool writeRegisterVerified(uint8_t reg, uint8_t value);
    uint8_t transferRegister(uint8_t command, uint8_t value);
    void setError(const String& message);
    void formatHex(const uint8_t* payload, uint8_t length);
    bool prepareStorage();
    void queueStorageRecord();
};

extern PacketSniffer packetSniffer;
