#pragma once

#include <Arduino.h>

enum class Cc1101Preset : uint8_t {
    OOK_270 = 0, OOK_650, FSK_2K, FSK_12K, FSK_47K, COUNT
};

class Cc1101Manager {
public:
    bool init();
    bool isConnected() const { return connected; }
    uint8_t partNumber() const { return part; }
    uint8_t version() const { return chipVersion; }
    float frequencyMHz() const { return frequency; }
    bool setFrequency(float mhz);
    int16_t readRssiDbm();
    bool enterRawReceive();
    bool enterRawTransmit();
    void idle();
    void setRawData(bool high);
    void updatePowerLevel();
    void setPreset(Cc1101Preset value);
    Cc1101Preset preset() const { return activePreset; }
    const char* presetName() const;
    bool carrierDetected(int16_t thresholdDbm = -85);
    bool enterPacketReceive();
    bool readPacket(uint8_t* payload, uint8_t& length, int16_t& rssiDbm,
                    uint8_t& lqi, bool& crcOk);
    bool applyCustomPreset(const uint8_t* registerPairs, size_t length);
    const char* lastError() const { return error; }

private:
    bool connected = false;
    uint8_t part = 0xFF;
    uint8_t chipVersion = 0xFF;
    float frequency = 433.92f;
    Cc1101Preset activePreset = Cc1101Preset::OOK_650;
    const char* error = "NOT INITIALIZED";

    void select();
    void deselect();
    uint8_t transfer(uint8_t value);
    void strobe(uint8_t command);
    void writeRegister(uint8_t address, uint8_t value);
    uint8_t readStatus(uint8_t address);
    void readBurst(uint8_t address, uint8_t* data, size_t length);
    bool reset();
    void configureReceiver();
};

extern Cc1101Manager cc1101Manager;
