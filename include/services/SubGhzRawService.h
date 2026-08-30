#pragma once

#include <Arduino.h>

enum class SubGhzRegion : uint8_t { RX_ONLY = 0, ETSI, FCC, COUNT };

class SubGhzRawService {
public:
    void setSimulationMode(bool enabled) { simulation = enabled; }
    bool simulationMode() const { return simulation; }
    bool startRecording(float frequencyMHz);
    bool stopRecording();
    void service();
    void prepareForShutdown();
    bool isRecording() const { return recording; }
    uint32_t pulseCount() const { return capturedCount; }
    uint32_t elapsedMs() const;
    size_t copyPulses(uint32_t fromIndex, uint32_t* output, size_t capacity,
                      uint8_t& levelAtStart) const;
    const String& lastFile() const { return activeFile; }
    const char* lastError() const { return error; }
    bool isArmed() const { return armed; }
    void setAutoTrigger(bool enabled) { autoTrigger = enabled; }
    bool autoTriggerEnabled() const { return autoTrigger; }
    bool autoCompleted() const { return captureAutoCompleted; }
    int16_t liveRssiDbm() const { return liveRssi; }
    int16_t peakRssiDbm() const { return peakRssi; }
    int16_t noiseFloorDbm() const { return noiseFloor; }
    uint16_t pulsesPerSecond() const { return pulseRate; }
    uint8_t bufferPercent() const { return min<uint32_t>(100, capturedCount * 100UL / MAX_PULSES); }
    int16_t triggerThresholdDbm() const { return triggerThreshold; }
    void cycleTriggerThreshold();
    void setTriggerThreshold(int16_t dbm) { triggerThreshold = constrain(dbm, -90, -70); }
    uint8_t replayRepeatCount() const { return replayRepeats; }
    void cycleReplayRepeatCount();
    void setReplayRepeatCount(uint8_t count) { replayRepeats = count >= 5 ? 5 : (count >= 3 ? 3 : 1); }

    size_t listFiles(String* names, size_t capacity);
    bool replay(const String& name, void (*yieldCb)() = nullptr);
    void stopReplay() { abortReplay = true; }
    uint32_t replayPulseIndex() const { return replayProgressPulse; }
    uint32_t replayPulseTotal() const { return replayProgressTotal; }
    uint8_t replayPass() const { return replayProgressPass; }
    uint8_t replayPassTotal() const { return replayProgressPasses; }
    float replayFrequencyMHz() const { return replayProgressFrequency; }
    uint8_t replayPercent() const {
        if (!replayProgressTotal || !replayProgressPasses) return 0;
        const uint64_t done = static_cast<uint64_t>(replayProgressPass) * replayProgressTotal +
                              min(replayProgressPulse, replayProgressTotal);
        const uint64_t total = static_cast<uint64_t>(replayProgressPasses) * replayProgressTotal;
        return min<uint64_t>(100, done * 100 / total);
    }

    bool startRfTest();
    void stopRfTest();
    bool isRfTesting() const { return rfTesting; }
    float testFrequencyMHz() const { return testFrequency; }
    bool cleanFile(const String& name);
    bool exportSubFile(const String& name);
    bool deleteFile(const String& name);
    bool toggleFavorite(const String& name);
    bool isFavorite(const String& name) const;
    bool renameFile(const String& name, String& renamed);
    const char* detectedProtocol() const { return protocolName; }
    uint16_t estimatedTeUs() const { return estimatedTe; }
    uint8_t decodedBitCount() const { return decodedBits; }
    uint64_t decodedKeyValue() const { return decodedKey; }
    uint16_t detectedFramePulses() const { return framePulses; }
    uint8_t detectedRepeatCount() const { return repeatCount; }

    void startAnalyzer();
    void stopAnalyzer();
    bool analyzerRunning() const { return analyzing; }
    uint8_t analyzerCount() const { return ANALYZER_POINTS; }
    float analyzerFrequency(uint8_t index) const;
    int16_t analyzerRssi(uint8_t index) const;
    float analyzerPeakFrequency() const { return analyzerPeakFreq; }
    int16_t analyzerPeakRssi() const { return analyzerPeak; }
    void lockAnalyzerPeak();

    bool startPacketAnalyzer();
    void stopPacketAnalyzer();
    bool packetAnalyzerRunning() const { return packetAnalyzing; }
    uint32_t packetCount() const { return packetsSeen; }
    uint8_t lastPacketLength() const { return packetLength; }
    const uint8_t* lastPacketData() const { return packetData; }
    int16_t lastPacketRssi() const { return packetRssi; }
    uint8_t lastPacketLqi() const { return packetLqi; }
    bool lastPacketCrcOk() const { return packetCrcOk; }

    void cycleRegion();
    void setRegion(SubGhzRegion value) { activeRegion = value; }
    SubGhzRegion region() const { return activeRegion; }
    const char* regionName() const;
    bool txAllowed(float mhz) const;

private:
    static constexpr uint32_t MAX_PULSES = 8192;
    static volatile uint32_t durations[MAX_PULSES];
    static volatile uint32_t capturedCount;
    static volatile uint32_t lastEdgeUs;
    static volatile uint8_t firstLevel;
    static void IRAM_ATTR edgeIsr();
    static constexpr uint8_t ANALYZER_POINTS = 20;

    bool recording = false;
    bool simulation = false;
    bool armed = false;
    bool autoTrigger = true;
    bool captureAutoCompleted = false;
    bool abortReplay = false;
    uint32_t replayProgressPulse = 0;
    uint32_t replayProgressTotal = 0;
    uint8_t replayProgressPass = 0;
    uint8_t replayProgressPasses = 1;
    float replayProgressFrequency = 0.0f;
    bool rfTesting = false;
    uint32_t recordingStartedMs = 0;
    uint32_t lastMetricsMs = 0;
    uint32_t lastPulseRateMs = 0;
    uint32_t lastPulseRateCount = 0;
    int16_t liveRssi = -127;
    int16_t peakRssi = -127;
    int16_t noiseFloor = -127;
    uint16_t pulseRate = 0;
    int16_t triggerThreshold = -80;
    uint8_t replayRepeats = 1;
    uint32_t lastTestHopUs = 0;
    uint32_t rfTestStartedMs = 0;
    uint8_t testPreset = 0;
    float recordingFrequency = 433.92f;
    float testFrequency = 315.0f;
    bool analyzing = false;
    uint8_t analyzerIndex = 0;
    uint32_t lastAnalyzerMs = 0;
    int16_t analyzerLevels[ANALYZER_POINTS] = {};
    float analyzerPeakFreq = 0;
    int16_t analyzerPeak = -127;
    bool packetAnalyzing = false;
    uint32_t packetsSeen = 0;
    uint8_t packetData[61] = {};
    uint8_t packetLength = 0;
    int16_t packetRssi = -127;
    uint8_t packetLqi = 0;
    bool packetCrcOk = false;
    SubGhzRegion activeRegion = SubGhzRegion::RX_ONLY;
    uint32_t lastTransmitEndedMs = 0;
    const char* protocolName = "RAW";
    uint16_t estimatedTe = 0;
    uint8_t decodedBits = 0;
    uint64_t decodedKey = 0;
    uint16_t framePulses = 0;
    uint8_t repeatCount = 0;
    String activeFile;
    const char* error = "OK";
};

extern SubGhzRawService subGhzRawService;
