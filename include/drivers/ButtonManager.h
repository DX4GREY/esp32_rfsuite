#pragma once
#include <Arduino.h>
#include "config/Config.h"

class ButtonManager {
public:
    void init();
    int readButton(int pin);
    bool isPressed(int pin);
    bool isLongPressed(int pin, unsigned long holdMs = 700);
    bool isShortReleased(int pin, unsigned long longPressMs = 700);
    // Prevent a button that opened a new screen from also activating a control
    // on that screen when the same physical press is released.
    void suppressHeldButtons();

private:
    unsigned long lastChangeTime[4] = {0, 0, 0, 0};
    int stableState[4] = {HIGH, HIGH, HIGH, HIGH};
    int prevState[4] = {HIGH, HIGH, HIGH, HIGH};
    unsigned long holdStartTime[4] = {0, 0, 0, 0};
    bool holdReported[4] = {false, false, false, false};
    unsigned long shortStartTime[4] = {0, 0, 0, 0};
    bool suppressedUntilRelease[4] = {false, false, false, false};

    int getPinIndex(int pin);
    bool inputSuppressed(int idx, int state);
};

extern ButtonManager buttonManager;
