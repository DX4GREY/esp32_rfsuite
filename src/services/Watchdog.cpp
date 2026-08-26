#include "services/Watchdog.h"

Watchdog watchdog;

void Watchdog::init(unsigned long requestedTimeoutUs) {
    timeoutUs = requestedTimeoutUs;
    lastFeedUs = micros();
    running = true;
}

void Watchdog::feed() {
    if (running) lastFeedUs = micros();
}

void Watchdog::stop() {
    running = false;
}

bool Watchdog::isTriggered() const {
    return running && static_cast<unsigned long>(micros() - lastFeedUs) >= timeoutUs;
}
