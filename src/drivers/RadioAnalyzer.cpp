#include "drivers/RadioManager.h"

bool RadioManager::sampleCarrier(uint8_t channel, uint16_t requested, uint16_t& hits, uint16_t& samples) {
    hits = samples = 0; if (!hasAnyRadio() || requested == 0) return false;
    if (!rxModeActive) enterRxMode();
    if (!lockBus(pdMS_TO_TICKS(20))) return false;
    if (radio1Available) radio.setChannel(channel);
    if (radio2Available) radio2.setChannel(channel);
    delayMicroseconds(35);
    for (uint16_t i=0;i<requested;i++) { const bool a=radio1Available&&(radio.testRPD()||radio.testCarrier()); const bool b=radio2Available&&(radio2.testRPD()||radio2.testCarrier()); if(a||b)hits++;samples++;delayMicroseconds(8); }
    unlockBus(); return true;
}

bool RadioManager::sampleCarrierOnRadio(uint8_t radioIndex, uint8_t channel,
                                        uint16_t requested, uint16_t& hits,
                                        uint16_t& samples) {
    hits = samples = 0;
    if (radioIndex < 1 || radioIndex > 2 || channel > MAX_CHANNEL || !requested) return false;
    if ((radioIndex == 1 && !radio1Available) || (radioIndex == 2 && !radio2Available)) return false;
    // Never steal a radio from a long-running acquisition. The caller can retry.
    if (scanActive || (packetSniffer.isRunning() &&
        ((radioIndex == 2) == snifferUsesRadio2))) return false;
    if (!rxModeActive) enterRxMode();
    if (!lockBus(pdMS_TO_TICKS(20))) return false;
    if (scanActive || (packetSniffer.isRunning() &&
        ((radioIndex == 2) == snifferUsesRadio2))) { unlockBus(); return false; }
    RF24& target = radioIndex == 1 ? radio : radio2;
    const uint8_t previousChannel = target.getChannel();
    target.setChannel(channel); delayMicroseconds(35);
    for (uint16_t i = 0; i < requested; ++i) {
        if (target.testRPD() || target.testCarrier()) hits++;
        samples++; delayMicroseconds(8);
    }
    target.setChannel(previousChannel);
    unlockBus();
    return true;
}

void RadioManager::scanSpectrum(void (*yieldCb)()) {
    if (!hasAnyRadio()) return;
    if (!rxModeActive) {
        enterRxMode();
    }
    if (!lockBus(pdMS_TO_TICKS(250))) return;
    scanAbortRequested = false;
    scanActive = true;
    bool completed = true;
    const AnalyzerBand scanBand = appState.analyzerBand;
    auto abortPending = [&]() {
        return scanAbortRequested || appState.analyzerFrozen ||
               appState.analyzerBand != scanBand;
    };

    int minCh, maxCh;
    appState.getAnalyzerChannelRange(minCh, maxCh);

    int highestCh = minCh;
    uint8_t highestLvl = 0;
    const int sampleCount = appState.getSpectrumSampleCount();

    auto storeLevel = [&](int ch, uint8_t combined, uint8_t level1, uint8_t level2) {
        appState.spectrumLevels[ch] = combined;
        appState.radio1Levels[ch] = level1;
        appState.radio2Levels[ch] = level2;
        if (combined > appState.peakLevels[ch]) appState.peakLevels[ch] = combined;
        if (combined > highestLvl) {
            highestLvl = combined;
            highestCh = ch;
        }
    };

    if (appState.analyzerRadioMode == ANALYZER_RADIO_FAST &&
        radio1Available && radio2Available) {
        // Each radio listens on a different channel during the same sample
        // window. Shared SPI setup remains sequential, but RF observation is
        // parallel, reducing a full-band sweep to roughly half as many windows.
        for (int ch = minCh; ch <= maxCh; ch += 2) {
            if (abortPending()) { completed = false; break; }
            const int ch2 = ch + 1;
            const bool hasSecondChannel = ch2 <= maxCh;
            radio.setChannel(ch);
            if (hasSecondChannel) radio2.setChannel(ch2);
            delayMicroseconds(35);

            int hits1 = 0;
            int hits2 = 0;
            for (int s = 0; s < sampleCount; s++) {
                if ((s & 7) == 0 && abortPending()) { completed = false; break; }
                if (radio.testRPD() || radio.testCarrier()) hits1++;
                if (hasSecondChannel && (radio2.testRPD() || radio2.testCarrier())) hits2++;
                delayMicroseconds(8);
            }
            if (!completed) break;

            const uint8_t level1 = (hits1 * 100) / sampleCount;
            storeLevel(ch, level1, level1, 0);
            if (hasSecondChannel) {
                const uint8_t level2 = (hits2 * 100) / sampleCount;
                storeLevel(ch2, level2, 0, level2);
            }

            if ((((ch - minCh) / 2) % 8 == 0) && yieldCb) {
                yieldCb();
                if (abortPending()) { completed = false; break; }
            }
        }
    } else {
        for (int ch = minCh; ch <= maxCh; ch++) {
            if (abortPending()) { completed = false; break; }
            bool useRadio1 = radio1Available &&
                             appState.analyzerRadioMode != ANALYZER_RADIO_2;
            bool useRadio2 = radio2Available &&
                             appState.analyzerRadioMode != ANALYZER_RADIO_1;
            // Requested radio may be offline: transparently use the remaining
            // receiver so the analyzer continues to produce valid data.
            if (!useRadio1 && !useRadio2) {
                useRadio1 = radio1Available;
                useRadio2 = !useRadio1 && radio2Available;
            }
            if (useRadio1) radio.setChannel(ch);
            if (useRadio2) radio2.setChannel(ch);
            delayMicroseconds(35);

            int hits1 = 0;
            int hits2 = 0;
            int combinedHits = 0;
            for (int s = 0; s < sampleCount; s++) {
                if ((s & 7) == 0 && abortPending()) { completed = false; break; }
                const bool hit1 = useRadio1 && (radio.testRPD() || radio.testCarrier());
                const bool hit2 = useRadio2 && (radio2.testRPD() || radio2.testCarrier());
                if (hit1) hits1++;
                if (hit2) hits2++;
                if (hit1 || hit2) combinedHits++;
                delayMicroseconds(8);
            }
            if (!completed) break;

            const uint8_t level1 = (hits1 * 100) / sampleCount;
            const uint8_t level2 = (hits2 * 100) / sampleCount;
            const uint8_t combined = (combinedHits * 100) / sampleCount;
            storeLevel(ch, combined, level1, level2);

            if (((ch - minCh) % 16 == 0 && yieldCb)) {
                yieldCb();
                if (abortPending()) { completed = false; break; }
            }
        }
    }

    if (completed) {
        appState.peakChannel = highestCh;
        appState.peakLevel = highestLvl;
        appState.decayPeaks();
    } else if (scanAbortRequested) {
        if (radio1Available) radio.stopListening();
        if (radio2Available) radio2.stopListening();
        rxModeActive = false;
    }
    unlockBus();
    scanActive = false;
    if (completed) appState.recordCompletedSweep(availableRadioCount());
}

uint8_t RadioManager::inspectChannel(int channel) {
    if (!hasAnyRadio()) return 0;
    if (!rxModeActive) {
        enterRxMode();
    }
    if (!lockBus(pdMS_TO_TICKS(250))) return appState.inspectedLevel;

    channel = constrain(channel, MIN_CHANNEL, MAX_CHANNEL);
    if (radio1Available) radio.setChannel(channel);
    if (radio2Available) radio2.setChannel(channel);
    delayMicroseconds(40);

    const int sampleCount = appState.getInspectSampleCount();
    int hits = 0;
    for (int s = 0; s < sampleCount; s++) {
        const bool hit1 = radio1Available && (radio.testRPD() || radio.testCarrier());
        const bool hit2 = radio2Available && (radio2.testRPD() || radio2.testCarrier());
        if (hit1 || hit2) {
            hits++;
        }
        delayMicroseconds(12);
    }

    uint8_t lvl = (hits * 100) / sampleCount;
    appState.inspectedLevel = lvl;

    if (lvl > appState.inspectedPeak) {
        appState.inspectedPeak = lvl;
    }

    unlockBus();

    return lvl;
}
