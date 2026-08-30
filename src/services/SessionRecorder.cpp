#include "services/SessionRecorder.h"
#include "core/AppState.h"
#include "core/RfEnvironmentState.h"
#include "drivers/RadioManager.h"
#include "services/StorageManager.h"
#include "services/EventLog.h"
#include "core/SessionFormat.h"

SessionRecorder sessionRecorder;

namespace {
constexpr size_t MAX_SESSION_BYTES = 256U * 1024U;
constexpr size_t FLUSH_THRESHOLD = 3072;
constexpr unsigned long FLUSH_INTERVAL_MS = 2000;
}

bool SessionRecorder::begin() {
    ready = storageManager.begin();
    errorMessage = ready ? "none" : "SD and LittleFS mount failed";
    return ready;
}

bool SessionRecorder::start() {
    if (!ready) {
        errorMessage = "storage unavailable";
        return false;
    }
    fs::FS& fs = storageManager.filesystem();
    const char* current = storageManager.sessionPath();
    const char* previous = previousPath();
    const String backup = String(previous) + ".bak";
    fs.remove(backup);
    const bool hadCurrent = fs.exists(current);
    const bool hadPrevious = fs.exists(previous);
    if (hadCurrent && hadPrevious && !fs.rename(previous, backup)) {
        errorMessage = "cannot stage previous session";
        eventLog.error("session", errorMessage);
        return false;
    }
    const bool archived = hadCurrent && fs.rename(current, previous);
    if (hadCurrent && !archived) {
        if (hadPrevious) fs.rename(backup, previous);
        errorMessage = "cannot archive previous session";
        eventLog.error("session", errorMessage);
        return false;
    }
    File file = fs.open(storageManager.sessionPath(), FILE_WRITE);
    if (!file) {
        if (archived) fs.rename(previous, current);
        if (hadPrevious) fs.rename(backup, previous);
        errorMessage = "cannot create session";
        eventLog.error("session", errorMessage);
        return false;
    }
    fs.remove(backup);
    file.println("# RF24 analyzer session v1");
    file.println("# ACTIVITY values are carrier-hit percentages, not dBm");
    file.printf("# firmware_build=%s compiled=%s %s\n", RF_LAB_TX_ENABLED ? "authorized_rf_lab" : "analyzer", __DATE__, __TIME__);
    file.println("# E: type,ms,test,start_ms,duration_ms,radios,min_ch,max_ch,window_s,avg,peak_ch,peak_pct,score,bursts,top5");
    file.println("# P: type,ms,channel,pa,data_rate,payload_size,packets,interval_ms,duration_ms");
    file.println("type,ms,sweep,peak_ch,peak_pct,confidence,band,mode,trace,ch0..ch125");
    file.close();
    pendingLength = 0;
    pending[0] = '\0';
    sweepCount = 0;
    lastFlushMs = millis();
    recording = true;
    errorMessage = "none";
    eventLog.info("session", archived ? "recording started; previous archived" : "recording started");
    return true;
}

void SessionRecorder::stop() {
    flushPending();
    if (recording) eventLog.info("session", "recording stopped");
    recording = false;
}

bool SessionRecorder::flushPending() {
    if (!ready || pendingLength == 0) return ready;
    File file = storageManager.filesystem().open(storageManager.sessionPath(), FILE_APPEND);
    if (!file) {
        errorMessage = "session append failed";
        recording = false;
        eventLog.error("session", errorMessage);
        return false;
    }
    const size_t written = file.write(reinterpret_cast<const uint8_t*>(pending), pendingLength);
    file.close();
    if (written != pendingLength) {
        errorMessage = "short filesystem write";
        recording = false;
        eventLog.error("session", errorMessage);
        return false;
    }
    pendingLength = 0;
    pending[0] = '\0';
    lastFlushMs = millis();
    return true;
}

void SessionRecorder::service() {
    if (recording && pendingLength > 0 &&
        millis() - lastFlushMs >= FLUSH_INTERVAL_MS) flushPending();
}

void SessionRecorder::recordSweep(const AppState& state) {
    if (!recording) return;
    if (fileSize() + pendingLength + 640 >= MAX_SESSION_BYTES) {
        errorMessage = "session size limit reached";
        eventLog.warn("session", errorMessage);
        stop();
        return;
    }
    char line[640];
    int used = snprintf(line, sizeof(line), "S,%lu,%lu,%u,%u,%u,%u,%u,%u",
                        static_cast<unsigned long>(millis()),
                        static_cast<unsigned long>(state.surveySweeps), state.peakChannel,
                        state.peakLevel, state.analyzerConfidence, state.analyzerBand,
                        state.analyzerRadioMode, state.analyzerTraceMode);
    if (used < 0 || static_cast<size_t>(used) >= sizeof(line)) return;
    for (int ch = 0; ch < TOTAL_CHANNELS; ch++) {
        const int added = snprintf(line + used, sizeof(line) - used, ",%u", state.spectrumLevels[ch]);
        if (added < 0 || static_cast<size_t>(added) >= sizeof(line) - used) {
            errorMessage = "sweep formatting overflow"; recording = false; return;
        }
        used += added;
    }
    line[used++] = '\n';
    if (!appendPending(line, used)) return;
    sweepCount++;
    if (pendingLength >= FLUSH_THRESHOLD) flushPending();
}

bool SessionRecorder::appendPending(const char* data, size_t length) {
    if (length > PENDING_CAPACITY) { errorMessage = "record too large"; recording = false; return false; }
    if (pendingLength + length > PENDING_CAPACITY && !flushPending()) return false;
    memcpy(pending + pendingLength, data, length);
    pendingLength += length;
    return true;
}

void SessionRecorder::recordEnvironmentSummary(const RfEnvironmentState& state, const char* testType) {
    if (!recording) return;
    uint8_t top[5]; state.topChannels(top, 5);
    uint32_t bursts=0; for(int ch=0;ch<TOTAL_CHANNELS;ch++) bursts+=state.channels[ch].burstCount;
    String line; line.reserve(240); line="E,"+String(millis())+","+testType+","+
        String(state.startedMs)+","+String(millis()-state.startedMs)+","+
        String(radioManager.availableRadioCount())+","+String(state.config.minChannel)+","+
        String(state.config.maxChannel)+","+String(state.config.sampleWindowSeconds)+","+
        String(state.averageOccupancy())+","+String(top[0])+","+
        String(state.channels[top[0]].peak)+","+String(state.overallScore())+","+String(bursts);
    for(int i=0;i<5;i++){line+=',';line+=String(top[i]);line+=':';line+=String(state.channels[top[i]].movingAverage);} line+='\n';
    appendPending(line.c_str(), line.length()); if(pendingLength>=FLUSH_THRESHOLD) flushPending();
}
void SessionRecorder::recordProbeSummary(uint8_t channel,uint8_t pa,uint8_t rate,uint8_t size,uint16_t packets,uint16_t intervalMs,uint32_t durationMs){
    if(!recording)return; char line[128]; const int length=snprintf(line,sizeof(line),"P,%lu,%u,%u,%u,%u,%u,%u,%lu\n",static_cast<unsigned long>(millis()),channel,pa,rate,size,packets,intervalMs,static_cast<unsigned long>(durationMs)); if(length>0&&static_cast<size_t>(length)<sizeof(line))appendPending(line,length);
}

bool SessionRecorder::exportCsv(Stream& output) {
    flushPending();
    fs::FS& fs = storageManager.filesystem();
    if (!ready || !fs.exists(storageManager.sessionPath())) return false;
    File file = fs.open(storageManager.sessionPath(), FILE_READ);
    if (!file) return false;
    while (file.available()) output.write(file.read());
    file.close();
    return true;
}

bool SessionRecorder::replayLatest(AppState& state) {
    flushPending();
    fs::FS& fs = storageManager.filesystem();
    if (!ready || !fs.exists(storageManager.sessionPath())) return false;
    File file = fs.open(storageManager.sessionPath(), FILE_READ);
    if (!file) return false;
    String lastData;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.startsWith("S,")) lastData = line;
    }
    file.close();
    if (lastData.length() == 0) return false;

    SessionFormat::Sweep parsed;
    if (!SessionFormat::parseSweepLine(lastData.c_str(), parsed)) {
        errorMessage = "invalid session row"; return false;
    }
    state.peakChannel = parsed.peakChannel;
    state.peakLevel = parsed.peakLevel;
    state.analyzerConfidence = parsed.confidence;
    memcpy(state.spectrumLevels, parsed.levels, TOTAL_CHANNELS);
    state.analyzerFrozen = true;
    state.cursorChannel = state.peakChannel;
    return true;
}

const char* SessionRecorder::previousPath() const {
    return storageManager.usingSd() ? "/RFSuite/log/rf_session_previous.csv" : "/rf_session_previous.csv";
}

bool SessionRecorder::hasCurrentSession() const {
    fs::FS& fs = storageManager.filesystem();
    return ready && fs.exists(storageManager.sessionPath());
}

bool SessionRecorder::hasPreviousSession() const {
    fs::FS& fs = storageManager.filesystem();
    return ready && fs.exists(previousPath());
}

bool SessionRecorder::deleteCurrentSession() {
    if (recording) stop();
    fs::FS& fs = storageManager.filesystem();
    if (!ready || !fs.exists(storageManager.sessionPath())) return false;
    const bool deleted = fs.remove(storageManager.sessionPath());
    sweepCount = 0;
    pendingLength = 0;
    if (deleted) eventLog.info("session", "deleted current session");
    return deleted;
}

bool SessionRecorder::summarize(const char* filePath, uint32_t& sweeps,
                                uint8_t& peakChannel, uint8_t& peakLevel,
                                uint8_t& average, uint8_t* channelAvgs) {
    File file = storageManager.filesystem().open(filePath, FILE_READ);
    if (!file) return false;
    uint64_t totals[TOTAL_CHANNELS] = {};
    uint8_t maxPerChannel[TOTAL_CHANNELS] = {};
    sweeps = 0;
    String line;
    while (file.available()) {
        line = file.readStringUntil('\n');
        if (!line.startsWith("S,")) continue;
        SessionFormat::Sweep parsed;
        if (!SessionFormat::parseSweepLine(line.c_str(), parsed)) continue;
        for (size_t channel = 0; channel < TOTAL_CHANNELS; ++channel) {
            totals[channel] += parsed.levels[channel];
            if (parsed.levels[channel] > maxPerChannel[channel]) {
                maxPerChannel[channel] = parsed.levels[channel];
            }
        }
        ++sweeps;
        yield();
    }
    file.close();
    if (!sweeps) return false;
    uint64_t grandTotal = 0, bestTotal = 0;
    peakChannel = 0;
    peakLevel = 0;
    for (size_t channel = 0; channel < TOTAL_CHANNELS; ++channel) {
        grandTotal += totals[channel];
        if (totals[channel] > bestTotal) {
            bestTotal = totals[channel];
            peakChannel = static_cast<uint8_t>(channel);
            peakLevel = maxPerChannel[channel];
        }
        if (channelAvgs) {
            channelAvgs[channel] = static_cast<uint8_t>(totals[channel] / sweeps);
        }
    }
    average = static_cast<uint8_t>(grandTotal / (static_cast<uint64_t>(sweeps) * TOTAL_CHANNELS));
    return true;
}

bool SessionRecorder::summarizeCurrent(uint32_t& sweeps, uint8_t& peakChannel, uint8_t& peakLevel, uint8_t& average) {
    flushPending();
    return summarize(storageManager.sessionPath(), sweeps, peakChannel, peakLevel, average, nullptr);
}

bool SessionRecorder::summarizePrevious(uint32_t& sweeps, uint8_t& peakChannel, uint8_t& peakLevel, uint8_t& average) {
    return summarize(previousPath(), sweeps, peakChannel, peakLevel, average, nullptr);
}

bool SessionRecorder::compareWithPrevious(SessionComparison& result) {
    flushPending();
    SessionComparison compared{};
    if (!summarize(previousPath(), compared.previousSweeps, compared.previousPeakChannel,
                   compared.previousPeakLevel, compared.previousAverage, compared.previousChannelAvg) ||
        !summarize(storageManager.sessionPath(), compared.currentSweeps,
                   compared.currentPeakChannel, compared.currentPeakLevel, compared.currentAverage, compared.currentChannelAvg)) {
        errorMessage = "two valid sessions required";
        return false;
    }
    compared.averageDelta = static_cast<int16_t>(compared.currentAverage) - compared.previousAverage;

    // Find top 5 channels with highest absolute delta
    ChannelDelta ranked[TOTAL_CHANNELS];
    for (int ch = 0; ch < TOTAL_CHANNELS; ++ch) {
        ranked[ch].channel = static_cast<uint8_t>(ch);
        ranked[ch].delta = static_cast<int16_t>(compared.currentChannelAvg[ch]) - compared.previousChannelAvg[ch];
    }
    // Simple top 5 selection
    for (int pos = 0; pos < 5; ++pos) {
        int bestIdx = pos;
        int bestAbs = abs(ranked[pos].delta);
        for (int i = pos + 1; i < TOTAL_CHANNELS; ++i) {
            int curAbs = abs(ranked[i].delta);
            if (curAbs > bestAbs) {
                bestAbs = curAbs;
                bestIdx = i;
            }
        }
        ChannelDelta temp = ranked[pos];
        ranked[pos] = ranked[bestIdx];
        ranked[bestIdx] = temp;
        compared.topDeltas[pos] = ranked[pos];
    }

    result = compared;
    return true;
}

size_t SessionRecorder::fileSize() const {
    fs::FS& fs = storageManager.filesystem();
    if (!ready || !fs.exists(storageManager.sessionPath())) return 0;
    File file = fs.open(storageManager.sessionPath(), FILE_READ);
    if (!file) return 0;
    const size_t size = file.size();
    file.close();
    return size;
}

const char* SessionRecorder::storageName() const { return storageManager.backendName(); }
const char* SessionRecorder::path() const { return storageManager.sessionPath(); }
