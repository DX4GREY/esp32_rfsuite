#pragma once
#include <Arduino.h>
#include "core/RfEnvironmentState.h"
class RfEnvironmentAnalyzer {
public:
    bool start(RfEnvMode mode = RF_ENV_OCCUPANCY);
    void stop();
    bool stopAndWait(uint32_t timeoutMs = 1000);
    void service();
    bool setRange(int minCh, int maxCh);
    bool setWindow(int seconds);
private:
    TaskHandle_t task = nullptr;
    volatile bool stopRequested = false;
    static void taskEntry(void* arg);
    void run();
};
extern RfEnvironmentAnalyzer rfEnvironmentAnalyzer;
