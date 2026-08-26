#pragma once
#include <Arduino.h>
class RfAuthorizedProbe {
public:
    bool start(); void stop(); bool stopAndWait(uint32_t timeoutMs = 1000);
    bool isRunning() const { return running; }
    uint16_t packetsSent() const { return sent; }
private:
    TaskHandle_t task=nullptr; volatile bool stopRequested=false, running=false;
    volatile uint16_t sent=0; uint32_t startedMs=0,endedMs=0;
    static void taskEntry(void*); void run();
};
extern RfAuthorizedProbe rfAuthorizedProbe;
