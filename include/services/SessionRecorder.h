#pragma once

#include <Arduino.h>

struct AppState;
class RfEnvironmentState;

struct SessionComparison {
    uint32_t previousSweeps = 0;
    uint32_t currentSweeps = 0;
    uint8_t previousPeakChannel = 0;
    uint8_t currentPeakChannel = 0;
    uint8_t previousAverage = 0;
    uint8_t currentAverage = 0;
    int16_t averageDelta = 0;
};

class SessionRecorder {
public:
    bool begin();
    bool start();
    void stop();
    void service();
    void recordSweep(const AppState& state);
    void recordEnvironmentSummary(const RfEnvironmentState& state, const char* testType);
    void recordProbeSummary(uint8_t channel, uint8_t pa, uint8_t rate,
                            uint8_t size, uint16_t packets, uint16_t intervalMs,
                            uint32_t durationMs);
    bool exportCsv(Stream& output);
    bool replayLatest(AppState& state);
    bool compareWithPrevious(SessionComparison& result);
    bool isReady() const { return ready; }
    bool isRecording() const { return recording; }
    size_t fileSize() const;
    uint32_t recordedSweeps() const { return sweepCount; }
    const char* lastError() const { return errorMessage; }
    const char* storageName() const;
    const char* path() const;

private:
    bool flushPending();
    bool appendPending(const char* data, size_t length);
    const char* previousPath() const;
    bool summarize(const char* filePath, uint32_t& sweeps, uint8_t& peakChannel,
                   uint8_t& average);
    bool ready = false;
    bool recording = false;
    static constexpr size_t PENDING_CAPACITY = 4096;
    char pending[PENDING_CAPACITY] = {};
    size_t pendingLength = 0;
    uint32_t sweepCount = 0;
    unsigned long lastFlushMs = 0;
    const char* errorMessage = "not initialized";
};

extern SessionRecorder sessionRecorder;
