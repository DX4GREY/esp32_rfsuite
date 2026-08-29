#include "drivers/ButtonManager.h"
#include "core/AppState.h"

ButtonManager buttonManager;

namespace {
int physicalPinFor(int logicalPin) {
    static const int clockwisePins[4] = {BTN_UP, BTN_A, BTN_DOWN, BTN_B};
    int logicalIndex = -1;
    for (int i = 0; i < 4; ++i) {
        if (clockwisePins[i] == logicalPin) {
            logicalIndex = i;
            break;
        }
    }
    if (logicalIndex < 0) return logicalPin;
    // Rotation 3 is the board default. Each clockwise display rotation shifts
    // the logical controls by one physical position around the enclosure.
    const int rotationSteps = (appState.displayRotation + 1) % 4;
    return clockwisePins[(logicalIndex + rotationSteps) % 4];
}
}

void ButtonManager::init() {
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
}

int ButtonManager::getPinIndex(int pin) {
    pin = physicalPinFor(pin);
    if (pin == BTN_UP) return 0;
    if (pin == BTN_A) return 1;
    if (pin == BTN_DOWN) return 2;
    if (pin == BTN_B) return 3;
    return -1;
}

int ButtonManager::readButton(int pin) {
    // Preserve each control's screen-relative meaning at every rotation.
    int idx = getPinIndex(pin);
    if (idx < 0) return HIGH;

    int raw = digitalRead(physicalPinFor(pin));
    unsigned long now = millis();

    if (raw != stableState[idx]) {
        // Change detected — start or continue the debounce timer
        if (lastChangeTime[idx] == 0) {
            lastChangeTime[idx] = now;  // Record the transition start
        }
        if (now - lastChangeTime[idx] >= BUTTON_DEBOUNCE_MS) {
            stableState[idx] = raw;     // Accept the new state after it is stable
            lastChangeTime[idx] = 0;    // Reset for the next transition
        }
    } else {
        lastChangeTime[idx] = 0;        // Back to a stable state — reset the timer
    }
    return stableState[idx];
}

bool ButtonManager::inputSuppressed(int idx, int state) {
    if (!suppressedUntilRelease[idx]) return false;
    if (state == HIGH) {
        // Release only rearms the button; it is deliberately not exposed as a
        // short-release event to the newly opened screen.
        suppressedUntilRelease[idx] = false;
        prevState[idx] = HIGH;
        holdStartTime[idx] = 0;
        holdReported[idx] = false;
        shortStartTime[idx] = 0;
    }
    return true;
}

void ButtonManager::suppressHeldButtons() {
    const int pins[4] = {BTN_UP, BTN_A, BTN_DOWN, BTN_B};
    for (int logicalIdx = 0; logicalIdx < 4; ++logicalIdx) {
        const int pin = pins[logicalIdx];
        const int physicalIdx = getPinIndex(pin);
        const int state = readButton(pin);
        suppressedUntilRelease[physicalIdx] = state == LOW;
        prevState[physicalIdx] = state;
        holdStartTime[physicalIdx] = 0;
        holdReported[physicalIdx] = false;
        shortStartTime[physicalIdx] = 0;
    }
}

bool ButtonManager::isPressed(int pin) {
    int idx = getPinIndex(pin);
    if (idx < 0) return false;

    int state = readButton(pin);
    if (inputSuppressed(idx, state)) return false;
    bool pressed = (prevState[idx] == HIGH && state == LOW);
    prevState[idx] = state;
    return pressed;
}

bool ButtonManager::isLongPressed(int pin, unsigned long holdMs) {
    int idx = getPinIndex(pin);
    if (idx < 0) return false;

    const int state = readButton(pin);
    if (inputSuppressed(idx, state)) return false;
    if (state == LOW) {
        if (holdStartTime[idx] == 0) holdStartTime[idx] = millis();
        if (!holdReported[idx] && millis() - holdStartTime[idx] >= holdMs) {
            holdReported[idx] = true;
            return true;
        }
    } else {
        holdStartTime[idx] = 0;
        holdReported[idx] = false;
    }
    return false;
}

bool ButtonManager::isShortReleased(int pin, unsigned long longPressMs) {
    const int idx = getPinIndex(pin);
    if (idx < 0) return false;
    const int state = readButton(pin);
    if (inputSuppressed(idx, state)) return false;
    if (state == LOW) {
        if (shortStartTime[idx] == 0) shortStartTime[idx] = millis();
        return false;
    }
    if (shortStartTime[idx] == 0) return false;
    const unsigned long duration = millis() - shortStartTime[idx];
    shortStartTime[idx] = 0;
    return duration < longPressMs;
}
