#pragma once
#include <Arduino.h>
#include "config/Config.h"

class Watchdog {
public:
    void init(unsigned long timeoutUs = WATCHDOG_TIMEOUT_US);
    void feed();
    void stop();
    bool isTriggered() const;

private:
    unsigned long timeoutUs = WATCHDOG_TIMEOUT_US;
    unsigned long lastFeedUs = 0;
    bool running = false;
};

extern Watchdog watchdog;
