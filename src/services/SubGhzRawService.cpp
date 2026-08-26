#include "services/SubGhzRawService.h"

#include <FS.h>
#include "config/Config.h"
#include "drivers/Cc1101Manager.h"
#include "core/AppState.h"
#include "services/StorageManager.h"

SubGhzRawService subGhzRawService;
volatile uint32_t SubGhzRawService::durations[MAX_PULSES] = {};
volatile uint32_t SubGhzRawService::capturedCount = 0;
volatile uint32_t SubGhzRawService::lastEdgeUs = 0;
volatile uint8_t SubGhzRawService::firstLevel = 0;

namespace {
struct __attribute__((packed)) RawHeader {
    char magic[4];
    uint32_t frequencyHz;
    uint32_t pulseCount;
    uint8_t firstLevel;
    uint8_t reserved[3];
};

constexpr float RF_PRESETS[] = {315.0f, 433.92f, 868.0f, 915.0f};
constexpr float ANALYZER_FREQUENCIES[] = {
    300.0f, 315.0f, 330.0f, 345.0f,
    387.0f, 400.0f, 415.0f, 433.92f, 450.0f, 464.0f,
    779.0f, 800.0f, 820.0f, 840.0f, 868.0f, 880.0f,
    900.0f, 915.0f, 925.0f, 928.0f
};
uint32_t importBuffer[8192];
uint8_t customPresetBuffer[96];

Cc1101Preset presetFromName(const String& value) {
    if (value.indexOf("Ook270") >= 0) return Cc1101Preset::OOK_270;
    if (value.indexOf("2FSKDev238") >= 0) return Cc1101Preset::FSK_2K;
    if (value.indexOf("2FSKDev12K") >= 0) return Cc1101Preset::FSK_12K;
    if (value.indexOf("2FSKDev476") >= 0) return Cc1101Preset::FSK_47K;
    return Cc1101Preset::OOK_650;
}

const char* flipperPreset(Cc1101Preset preset) {
    switch (preset) {
        case Cc1101Preset::OOK_270: return "FuriHalSubGhzPresetOok270Async";
        case Cc1101Preset::FSK_2K: return "FuriHalSubGhzPreset2FSKDev238Async";
        case Cc1101Preset::FSK_12K: return "FuriHalSubGhzPreset2FSKDev12KAsync";
        case Cc1101Preset::FSK_47K: return "FuriHalSubGhzPreset2FSKDev476Async";
        default: return "FuriHalSubGhzPresetOok650Async";
    }
}

int hexNibble(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}
}

void IRAM_ATTR SubGhzRawService::edgeIsr() {
    const uint32_t now = micros();
    const uint32_t index = capturedCount;
    if (index < MAX_PULSES) {
        durations[index] = now - lastEdgeUs;
        capturedCount = index + 1;
    }
    lastEdgeUs = now;
}

bool SubGhzRawService::startRecording(float frequencyMHz) {
    if (recording) return true;
    if (simulation) {
        recordingFrequency = frequencyMHz; capturedCount = 0; firstLevel = 1;
        liveRssi = noiseFloor = -96; peakRssi = -96; pulseRate = 0;
        lastMetricsMs = lastPulseRateMs = recordingStartedMs = millis();
        armed = false; recording = true; activeFile = ""; error = "SIMULATION ONLY";
        return true;
    }
    if (!storageManager.usingSd()) { error = "SD CARD REQUIRED"; return false; }
    if (!cc1101Manager.isConnected()) { error = "CC1101 OFFLINE"; return false; }
    cc1101Manager.setFrequency(frequencyMHz);
    if (!cc1101Manager.enterRawReceive()) { error = "RX START FAILED"; return false; }
    recordingFrequency = frequencyMHz;
    capturedCount = 0;
    liveRssi = peakRssi = noiseFloor = cc1101Manager.readRssiDbm();
    pulseRate = 0; lastPulseRateCount = 0;
    lastMetricsMs = lastPulseRateMs = millis();
    armed = autoTrigger;
    recordingStartedMs = armed ? 0 : millis();
    activeFile = "";
    if (!armed) {
        firstLevel = digitalRead(CC1101_GDO0_PIN);
        lastEdgeUs = micros();
        attachInterrupt(digitalPinToInterrupt(CC1101_GDO0_PIN), edgeIsr, CHANGE);
    }
    recording = true;
    error = armed ? "ARMED" : "RECORDING";
    return true;
}

bool SubGhzRawService::stopRecording() {
    if (!recording) return false;
    if (simulation) {
        recording = false; armed = false; error = "SIMULATION - NOT SAVED";
        return true;
    }
    if (!armed) detachInterrupt(digitalPinToInterrupt(CC1101_GDO0_PIN));
    recording = false;
    armed = false;
    cc1101Manager.idle();
    uint32_t count = capturedCount;
    if (count < 2) { error = "NO SIGNAL CAPTURED"; return false; }

    // Remove start silence and paired sub-80us glitches without changing the
    // HIGH/LOW phase of subsequent samples.
    uint32_t read = durations[0] > 20000 ? 1 : 0;
    if (read) firstLevel ^= 1U;
    uint32_t write = 0;
    while (read < count) {
        if (durations[read] < 80 && read + 1 < count && write > 0) {
            durations[write - 1] += durations[read] + durations[read + 1];
            read += 2;
        } else {
            durations[write++] = durations[read++];
        }
    }
    count = write;
    capturedCount = count;

    estimatedTe = 0; decodedBits = 0; decodedKey = 0; protocolName = "RAW";
    for (uint32_t i = 0; i < count; ++i) {
        if (durations[i] >= 100 && durations[i] <= 2000 &&
            (!estimatedTe || durations[i] < estimatedTe)) estimatedTe = durations[i];
    }
    if (estimatedTe) {
        for (uint32_t i = 0; i + 1 < count && decodedBits < 64; i += 2) {
            const uint32_t a = durations[i], b = durations[i + 1];
            const bool shortA = a < estimatedTe * 2U, shortB = b < estimatedTe * 2U;
            if (shortA == shortB) { decodedBits = 0; decodedKey = 0; break; }
            decodedKey = (decodedKey << 1) | (!shortA && shortB ? 1ULL : 0ULL);
            decodedBits++;
        }
        if (decodedBits >= 8) protocolName = "PWM/OOK";
    }
    framePulses = 0; repeatCount = 0;
    for (uint16_t frame = 8; frame * 2 <= count && frame <= 512; ++frame) {
        bool matches = true;
        for (uint16_t i = 0; i < frame; ++i) {
            const uint32_t a = durations[i], b = durations[i + frame];
            const uint32_t tolerance = max<uint32_t>(80, max(a, b) / 5);
            if (a > b + tolerance || b > a + tolerance) { matches = false; break; }
        }
        if (matches) {
            framePulses = frame;
            repeatCount = min<uint32_t>(255, count / frame);
            break;
        }
    }

    activeFile = String(storageManager.subGhzPath()) + "/RAW_" +
                 String(static_cast<uint32_t>(recordingFrequency * 100.0f)) + "_" +
                 String(millis()) + ".rfr";
    File file = storageManager.filesystem().open(activeFile, FILE_WRITE);
    if (!file) { error = "FILE OPEN FAILED"; activeFile = ""; return false; }
    RawHeader header{{'R','F','S','1'},
                     static_cast<uint32_t>(recordingFrequency * 1000000.0f),
                     count, firstLevel,
                     {static_cast<uint8_t>(cc1101Manager.preset()),
                      static_cast<uint8_t>(activeRegion), 0}};
    bool ok = file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) == sizeof(header);
    for (uint32_t offset = 0; ok && offset < count; offset += 256) {
        const uint32_t chunk = min<uint32_t>(256, count - offset);
        uint32_t copy[256];
        noInterrupts();
        for (uint32_t i = 0; i < chunk; ++i) copy[i] = durations[offset + i];
        interrupts();
        ok = file.write(reinterpret_cast<const uint8_t*>(copy), chunk * sizeof(uint32_t)) ==
             chunk * sizeof(uint32_t);
    }
    file.close();
    error = ok ? "SAVED" : "WRITE FAILED";
    return ok;
}

void SubGhzRawService::service() {
    if (simulation && recording && millis() - lastMetricsMs >= 50) {
        const uint32_t phase = millis() / 50;
        liveRssi = -92 + static_cast<int16_t>((phase * 7) % 36);
        peakRssi = max(peakRssi, liveRssi); noiseFloor = -96; pulseRate = 20;
        for (uint8_t i = 0; i < 4 && capturedCount < MAX_PULSES; ++i)
            durations[capturedCount++] = ((phase + i) & 1U) ? 350 : 1050;
        lastMetricsMs = millis();
    }
    if (!simulation && recording && millis() - lastMetricsMs >= 50) {
        liveRssi = cc1101Manager.readRssiDbm();
        peakRssi = max(peakRssi, liveRssi);
        noiseFloor = noiseFloor == -127 ? liveRssi : (noiseFloor * 7 + liveRssi) / 8;
        lastMetricsMs = millis();
        if (armed && liveRssi >= triggerThreshold) {
            armed = false;
            capturedCount = 0;
            firstLevel = digitalRead(CC1101_GDO0_PIN);
            lastEdgeUs = micros();
            recordingStartedMs = millis();
            lastPulseRateMs = millis();
            attachInterrupt(digitalPinToInterrupt(CC1101_GDO0_PIN), edgeIsr, CHANGE);
            error = "TRIGGERED";
        }
    }
    if (recording && !armed && millis() - lastPulseRateMs >= 1000) {
        const uint32_t current = capturedCount;
        pulseRate = min<uint32_t>(65535, current - lastPulseRateCount);
        lastPulseRateCount = current; lastPulseRateMs = millis();
    }
    if (recording && !armed && (capturedCount >= MAX_PULSES || elapsedMs() >= 30000)) stopRecording();

    if (analyzing && millis() - lastAnalyzerMs >= 12) {
        if (analyzerIndex == 0) { analyzerPeak = -127; analyzerPeakFreq = 0; }
        const float frequency = ANALYZER_FREQUENCIES[analyzerIndex];
        int16_t rssi;
        if (simulation) {
            const int distance = abs(static_cast<int>(analyzerIndex) - 7);
            rssi = -48 - distance * 6 - static_cast<int16_t>((millis() / 80 + analyzerIndex) % 5);
        } else {
            cc1101Manager.setFrequency(frequency);
            rssi = cc1101Manager.readRssiDbm();
        }
        analyzerLevels[analyzerIndex] = rssi;
        if (rssi > analyzerPeak) { analyzerPeak = rssi; analyzerPeakFreq = frequency; }
        analyzerIndex = (analyzerIndex + 1) % ANALYZER_POINTS;
        lastAnalyzerMs = millis();
    }
    if (packetAnalyzing) {
        if (simulation && millis() - lastMetricsMs >= 700) {
            packetLength = 6; packetRssi = -62; packetLqi = 96; packetCrcOk = true;
            for (uint8_t i = 0; i < packetLength; ++i) packetData[i] = 0xA0 + ((packetsSeen + i) & 0x0F);
            packetsSeen++; lastMetricsMs = millis();
        }
        uint8_t length = 0; int16_t rssi = -127; uint8_t lqi = 0; bool crc = false;
        if (!simulation && cc1101Manager.readPacket(packetData, length, rssi, lqi, crc)) {
            packetLength = length; packetRssi = rssi; packetLqi = lqi;
            packetCrcOk = crc; packetsSeen++;
            if (storageManager.usingSd()) {
                const String packetPath = String(storageManager.subGhzPath()) + "/packets.csv";
                const bool needsHeader = !storageManager.filesystem().exists(packetPath);
                File log = storageManager.filesystem().open(packetPath, FILE_APPEND);
                if (log) {
                    if (needsHeader) log.println("ms,frequency_mhz,rssi_dbm,lqi,crc,length,hex");
                    log.printf("%lu,%.3f,%d,%u,%u,%u,", millis(), cc1101Manager.frequencyMHz(),
                               rssi, lqi, crc ? 1 : 0, length);
                    for (uint8_t i = 0; i < length; ++i) log.printf("%02X", packetData[i]);
                    log.println(); log.close();
                }
            }
        }
    }
    if (!rfTesting) return;
#if RF_LAB_TX_ENABLED
    if (millis() - rfTestStartedMs >= 10000) { stopRfTest(); error = "TX LIMIT REACHED"; return; }
    cc1101Manager.setRawData((millis() / 2) & 1);
    if (micros() - lastTestHopUs >= static_cast<uint32_t>(appState.dwellTimeUs)) {
        do {
            testPreset = (testPreset + 1) % 4;
            testFrequency = RF_PRESETS[testPreset];
        } while (!txAllowed(testFrequency) && testPreset != 0);
        cc1101Manager.idle();
        cc1101Manager.setFrequency(testFrequency);
        cc1101Manager.enterRawTransmit();
        lastTestHopUs = micros();
    }
#endif
}

void SubGhzRawService::prepareForShutdown() {
    // Do not analyze or write a capture while the sleep code is stopping the
    // other CPU and disabling peripherals.
    abortReplay = true;
    if (recording && !simulation && !armed) {
        detachInterrupt(digitalPinToInterrupt(CC1101_GDO0_PIN));
    }
    recording = false;
    armed = false;
    analyzing = false;
    packetAnalyzing = false;
    rfTesting = false;
    if (!simulation && cc1101Manager.isConnected()) {
        cc1101Manager.setRawData(false);
        cc1101Manager.idle();
    }
}

uint32_t SubGhzRawService::elapsedMs() const {
    return recording && !armed ? millis() - recordingStartedMs : 0;
}

size_t SubGhzRawService::copyPulses(uint32_t fromIndex, uint32_t* output,
                                    size_t capacity, uint8_t& levelAtStart) const {
    if (!output || !capacity) return 0;
    noInterrupts();
    const uint32_t available = capturedCount;
    if (fromIndex >= available) {
        levelAtStart = firstLevel ^ (fromIndex & 1U);
        interrupts();
        return 0;
    }
    const size_t count = min<size_t>(capacity, available - fromIndex);
    levelAtStart = firstLevel ^ (fromIndex & 1U);
    for (size_t i = 0; i < count; ++i) output[i] = durations[fromIndex + i];
    interrupts();
    return count;
}

float SubGhzRawService::analyzerFrequency(uint8_t index) const {
    return index < ANALYZER_POINTS ? ANALYZER_FREQUENCIES[index] : 0;
}

int16_t SubGhzRawService::analyzerRssi(uint8_t index) const {
    return index < ANALYZER_POINTS ? analyzerLevels[index] : -127;
}

void SubGhzRawService::startAnalyzer() {
    stopPacketAnalyzer();
    analyzing = simulation || cc1101Manager.isConnected();
    analyzerIndex = 0; analyzerPeak = -127; analyzerPeakFreq = 0;
    for (auto& level : analyzerLevels) level = -127;
    lastAnalyzerMs = 0;
}

void SubGhzRawService::stopAnalyzer() {
    analyzing = false;
    if (!simulation) cc1101Manager.idle();
}

void SubGhzRawService::lockAnalyzerPeak() {
    if (simulation) { recordingFrequency = analyzerPeakFreq; return; }
    if (analyzerPeakFreq > 0) {
        float bestFrequency = analyzerPeakFreq;
        int16_t bestRssi = analyzerPeak;
        for (int offset = -10; offset <= 10; ++offset) {
            const float candidate = analyzerPeakFreq + offset * 0.1f;
            cc1101Manager.setFrequency(candidate);
            const int16_t rssi = cc1101Manager.readRssiDbm();
            if (rssi > bestRssi) { bestRssi = rssi; bestFrequency = candidate; }
        }
        analyzerPeakFreq = bestFrequency; analyzerPeak = bestRssi;
        recordingFrequency = bestFrequency;
        cc1101Manager.setFrequency(bestFrequency);
    }
}

bool SubGhzRawService::startPacketAnalyzer() {
    stopAnalyzer();
    packetsSeen = 0; packetLength = 0; packetRssi = -127; packetLqi = 0;
    packetCrcOk = false;
    packetAnalyzing = simulation ? true : cc1101Manager.enterPacketReceive();
    return packetAnalyzing;
}

void SubGhzRawService::stopPacketAnalyzer() {
    if (packetAnalyzing && !simulation) cc1101Manager.idle();
    packetAnalyzing = false;
}

void SubGhzRawService::cycleRegion() {
    activeRegion = static_cast<SubGhzRegion>(
        (static_cast<uint8_t>(activeRegion) + 1) % static_cast<uint8_t>(SubGhzRegion::COUNT));
}

void SubGhzRawService::cycleTriggerThreshold() {
    triggerThreshold += 5;
    if (triggerThreshold > -70) triggerThreshold = -90;
}

const char* SubGhzRawService::regionName() const {
    switch (activeRegion) {
        case SubGhzRegion::ETSI: return "ETSI";
        case SubGhzRegion::FCC: return "FCC";
        default: return "RX ONLY";
    }
}

bool SubGhzRawService::txAllowed(float mhz) const {
    if (activeRegion == SubGhzRegion::ETSI)
        return (mhz >= 433.05f && mhz <= 434.79f) || (mhz >= 863.0f && mhz <= 870.0f);
    if (activeRegion == SubGhzRegion::FCC)
        return (mhz >= 300.0f && mhz <= 348.0f) || (mhz >= 902.0f && mhz <= 928.0f);
    return false;
}

size_t SubGhzRawService::listFiles(String* names, size_t capacity) {
    if (!storageManager.usingSd() || !names || !capacity) return 0;
    File dir = storageManager.filesystem().open(storageManager.subGhzPath());
    if (!dir || !dir.isDirectory()) return 0;
    size_t count = 0;
    File entry;
    while (count < capacity && (entry = dir.openNextFile())) {
        const String name = entry.name();
        if (!entry.isDirectory() && (name.endsWith(".rfr") || name.endsWith(".sub"))) {
            const int slash = name.lastIndexOf('/');
            names[count++] = slash >= 0 ? name.substring(slash + 1) : name;
        }
        entry.close();
    }
    dir.close();
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            const bool favoriteI = isFavorite(names[i]);
            const bool favoriteJ = isFavorite(names[j]);
            String a = names[i]; a.toLowerCase();
            String b = names[j]; b.toLowerCase();
            if ((!favoriteI && favoriteJ) || (favoriteI == favoriteJ && b.compareTo(a) < 0)) {
                const String swap = names[i]; names[i] = names[j]; names[j] = swap;
            }
        }
    }
    return count;
}

bool SubGhzRawService::cleanFile(const String& name) {
    if (!name.endsWith(".rfr")) { error = "CLEAN SUPPORTS RFR"; return false; }
    const String path = String(storageManager.subGhzPath()) + "/" + name;
    File file = storageManager.filesystem().open(path, FILE_READ);
    RawHeader header{};
    if (!file || file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
        memcmp(header.magic, "RFS1", 4) || header.pulseCount > MAX_PULSES) {
        if (file) file.close(); error = "INVALID RAW FILE"; return false;
    }
    const size_t bytes = header.pulseCount * sizeof(uint32_t);
    if (file.read(reinterpret_cast<uint8_t*>(importBuffer), bytes) != bytes) {
        file.close(); error = "READ FAILED"; return false;
    }
    file.close();
    uint32_t read = importBuffer[0] > 20000 ? 1 : 0, write = 0;
    if (read) header.firstLevel ^= 1U;
    while (read < header.pulseCount) {
        if (importBuffer[read] < 80 && read + 1 < header.pulseCount && write > 0) {
            importBuffer[write - 1] += importBuffer[read] + importBuffer[read + 1]; read += 2;
        } else importBuffer[write++] = importBuffer[read++];
    }
    header.pulseCount = write;
    const String tempPath = path + ".tmp";
    storageManager.filesystem().remove(tempPath);
    file = storageManager.filesystem().open(tempPath, FILE_WRITE);
    const bool ok = file &&
        file.write(reinterpret_cast<uint8_t*>(&header), sizeof(header)) == sizeof(header) &&
        file.write(reinterpret_cast<uint8_t*>(importBuffer), write * sizeof(uint32_t)) ==
            write * sizeof(uint32_t);
    if (file) file.close();
    if (!ok) { error = "WRITE FAILED"; return false; }
    if (!storageManager.filesystem().remove(path) ||
        !storageManager.filesystem().rename(tempPath, path)) {
        error = "REPLACE FAILED (.tmp kept)"; return false;
    }
    error = "SIGNAL CLEANED"; return true;
}

bool SubGhzRawService::exportSubFile(const String& name) {
    if (!name.endsWith(".rfr")) { error = "EXPORT SUPPORTS RFR"; return false; }
    const String base = String(storageManager.subGhzPath()) + "/" + name;
    File input = storageManager.filesystem().open(base, FILE_READ);
    RawHeader header{};
    if (!input || input.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
        memcmp(header.magic, "RFS1", 4)) {
        if (input) input.close(); error = "INVALID RAW FILE"; return false;
    }
    String outputPath = base.substring(0, base.length() - 4) + ".sub";
    File output = storageManager.filesystem().open(outputPath, FILE_WRITE);
    if (!output) { input.close(); error = "EXPORT OPEN FAILED"; return false; }
    output.println("Filetype: Flipper SubGhz RAW File");
    output.println("Version: 1");
    output.println("Frequency: " + String(header.frequencyHz));
    const Cc1101Preset preset = header.reserved[0] < static_cast<uint8_t>(Cc1101Preset::COUNT) ?
        static_cast<Cc1101Preset>(header.reserved[0]) : Cc1101Preset::OOK_650;
    output.println("Preset: " + String(flipperPreset(preset)));
    output.println("Protocol: RAW");
    const uint32_t start = header.firstLevel ? 0 : 1;
    for (uint32_t i = 0; i < header.pulseCount; ++i) {
        uint32_t duration = 0;
        if (input.read(reinterpret_cast<uint8_t*>(&duration), 4) != 4) break;
        if (i < start) continue;
        const uint32_t exported = i - start;
        if (exported % 128 == 0) output.print("RAW_Data:");
        output.print((exported & 1U) ? " -" : " ");
        output.print(duration);
        if (exported % 128 == 127 || i + 1 == header.pulseCount) output.println();
    }
    input.close(); output.close(); error = "SUB EXPORTED"; return true;
}

bool SubGhzRawService::deleteFile(const String& name) {
    const String path = String(storageManager.subGhzPath()) + "/" + name;
    storageManager.filesystem().remove(path + ".fav");
    const bool ok = storageManager.filesystem().remove(path);
    error = ok ? "FILE DELETED" : "DELETE FAILED"; return ok;
}

bool SubGhzRawService::toggleFavorite(const String& name) {
    const String marker = String(storageManager.subGhzPath()) + "/" + name + ".fav";
    if (storageManager.filesystem().exists(marker)) {
        const bool ok = storageManager.filesystem().remove(marker);
        error = ok ? "FAVORITE OFF" : "FAVORITE FAILED"; return ok;
    }
    File file = storageManager.filesystem().open(marker, FILE_WRITE);
    const bool ok = static_cast<bool>(file); if (file) file.close();
    error = ok ? "FAVORITE ON" : "FAVORITE FAILED"; return ok;
}

bool SubGhzRawService::isFavorite(const String& name) const {
    return storageManager.filesystem().exists(
        String(storageManager.subGhzPath()) + "/" + name + ".fav");
}

bool SubGhzRawService::renameFile(const String& name, String& renamed) {
    const int dot = name.lastIndexOf('.');
    const String extension = dot >= 0 ? name.substring(dot) : String(".rfr");
    fs::FS& fs = storageManager.filesystem();
    for (uint16_t index = 1; index < 1000; ++index) {
        renamed = "SIGNAL_" + String(index) + extension;
        const String target = String(storageManager.subGhzPath()) + "/" + renamed;
        if (fs.exists(target)) continue;
        const String source = String(storageManager.subGhzPath()) + "/" + name;
        const bool favorite = isFavorite(name);
        if (!fs.rename(source, target)) { error = "RENAME FAILED"; return false; }
        if (favorite) {
            fs.rename(source + ".fav", target + ".fav");
        }
        error = "FILE RENAMED"; return true;
    }
    error = "NO FREE NAME"; return false;
}

bool SubGhzRawService::replay(const String& name, void (*yieldCb)()) {
#if !RF_LAB_TX_ENABLED
    (void)name; (void)yieldCb; error = "RF LAB BUILD REQUIRED"; return false;
#else
    replayProgressPulse = 0;
    replayProgressTotal = 0;
    replayProgressPass = 0;
    replayProgressPasses = replayRepeats;
    replayProgressFrequency = 0.0f;
    if (!storageManager.usingSd() || !cc1101Manager.isConnected()) {
        error = "RADIO OR SD OFFLINE"; return false;
    }
    if (millis() - lastTransmitEndedMs < 1000) { error = "TX COOLDOWN"; return false; }
    const String path = String(storageManager.subGhzPath()) + "/" + name;
    File file = storageManager.filesystem().open(path, FILE_READ);
    if (!file) { error = "FILE OPEN FAILED"; return false; }
    uint32_t frequencyHz = 0, pulseTotal = 0;
    uint8_t initialLevel = 0;
    size_t customPresetLength = 0;
    Cc1101Preset preset = Cc1101Preset::OOK_650;
    if (name.endsWith(".sub")) {
        while (file.available()) {
            String line = file.readStringUntil('\n'); line.trim();
            if (line.startsWith("Frequency:")) frequencyHz = line.substring(10).toInt();
            else if (line.startsWith("Preset:")) preset = presetFromName(line);
            else if (line.startsWith("Custom_preset_data:")) {
                int pos = line.indexOf(':') + 1;
                while (pos < static_cast<int>(line.length()) && customPresetLength < sizeof(customPresetBuffer)) {
                    while (pos < static_cast<int>(line.length()) && line[pos] == ' ') ++pos;
                    if (pos + 1 >= static_cast<int>(line.length())) break;
                    const int high = hexNibble(line[pos]), low = hexNibble(line[pos + 1]);
                    if (high < 0 || low < 0) { ++pos; continue; }
                    customPresetBuffer[customPresetLength++] = (high << 4) | low; pos += 2;
                }
            }
            else if (line.startsWith("RAW_Data:")) {
                int pos = line.indexOf(':') + 1;
                while (pos > 0 && pos < static_cast<int>(line.length()) && pulseTotal < MAX_PULSES) {
                    while (pos < static_cast<int>(line.length()) && line[pos] == ' ') ++pos;
                    bool positive = true;
                    if (pos < static_cast<int>(line.length()) && line[pos] == '-') { positive = false; ++pos; }
                    else if (pos < static_cast<int>(line.length()) && line[pos] == '+') ++pos;
                    uint32_t value = 0; bool digits = false;
                    while (pos < static_cast<int>(line.length()) && isDigit(line[pos])) {
                        value = value * 10 + (line[pos++] - '0'); digits = true;
                    }
                    if (digits && value) {
                        if (pulseTotal == 0) initialLevel = positive ? 1 : 0;
                        importBuffer[pulseTotal++] = value;
                    } else if (pos < static_cast<int>(line.length())) ++pos;
                }
            }
        }
    } else {
        RawHeader header{};
        if (file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header)) != sizeof(header) ||
            memcmp(header.magic, "RFS1", 4) || header.pulseCount > MAX_PULSES) {
            file.close(); error = "INVALID RAW FILE"; return false;
        }
        frequencyHz = header.frequencyHz; pulseTotal = header.pulseCount;
        initialLevel = header.firstLevel;
        if (header.reserved[0] < static_cast<uint8_t>(Cc1101Preset::COUNT))
            preset = static_cast<Cc1101Preset>(header.reserved[0]);
        const size_t bytes = pulseTotal * sizeof(uint32_t);
        if (file.read(reinterpret_cast<uint8_t*>(importBuffer), bytes) != bytes) {
            file.close(); error = "READ FAILED"; return false;
        }
    }
    file.close();
    const float frequency = frequencyHz / 1000000.0f;
    replayProgressTotal = pulseTotal;
    replayProgressFrequency = frequency;
    if (!pulseTotal || !txAllowed(frequency)) { error = "REGION BLOCKED"; return false; }
    cc1101Manager.setPreset(preset);
    if (customPresetLength) cc1101Manager.applyCustomPreset(customPresetBuffer, customPresetLength);
    cc1101Manager.setFrequency(frequency);
    cc1101Manager.enterRawReceive();
    abortReplay = false;
    const uint32_t ccaStarted = millis();
    while (cc1101Manager.carrierDetected(-75) && millis() - ccaStarted < 500) {
        delay(10); if (yieldCb) yieldCb();
    }
    if (abortReplay) { cc1101Manager.idle(); error = "REPLAY ABORTED"; return false; }
    if (cc1101Manager.carrierDetected(-75)) { cc1101Manager.idle(); error = "CHANNEL BUSY"; return false; }
    if (!cc1101Manager.enterRawTransmit()) { error = "TX START FAILED"; return false; }
    const uint32_t txStarted = millis();
    for (uint8_t pass = 0; pass < replayRepeats && !abortReplay && millis() - txStarted < 10000; ++pass) {
        replayProgressPass = pass;
        replayProgressPulse = 0;
        bool level = initialLevel;
        cc1101Manager.setRawData(level);
        for (uint32_t i = 0; i < pulseTotal && !abortReplay && millis() - txStarted < 10000; ++i) {
            replayProgressPulse = i + 1;
            uint32_t duration = importBuffer[i];
            while (duration > 10000 && !abortReplay) {
                delayMicroseconds(10000); duration -= 10000;
                if (yieldCb) yieldCb();
            }
            if (duration) delayMicroseconds(duration);
            level = !level;
            cc1101Manager.setRawData(level);
            if ((i & 0x7F) == 0 && yieldCb) yieldCb();
        }
        if (pass + 1 < replayRepeats) delay(20);
    }
    if (!abortReplay) {
        replayProgressPass = replayRepeats;
        replayProgressPulse = pulseTotal;
        if (yieldCb) yieldCb();
    }
    cc1101Manager.idle();
    lastTransmitEndedMs = millis();
    error = abortReplay ? "REPLAY ABORTED" : "REPLAY COMPLETE";
    return !abortReplay;
#endif
}

void SubGhzRawService::cycleReplayRepeatCount() {
    replayRepeats = replayRepeats == 1 ? 3 : (replayRepeats == 3 ? 5 : 1);
}

bool SubGhzRawService::startRfTest() {
#if !RF_LAB_TX_ENABLED
    error = "RF LAB BUILD REQUIRED"; return false;
#else
    if (!cc1101Manager.isConnected()) { error = "CC1101 OFFLINE"; return false; }
    if (activeRegion == SubGhzRegion::RX_ONLY) { error = "SELECT TX REGION"; return false; }
    if (millis() - lastTransmitEndedMs < 1000) { error = "TX COOLDOWN"; return false; }
    testPreset = 0;
    while (testPreset < 4 && !txAllowed(RF_PRESETS[testPreset])) ++testPreset;
    if (testPreset >= 4) { error = "NO ALLOWED PRESET"; return false; }
    testFrequency = RF_PRESETS[testPreset];
    cc1101Manager.enterRawReceive();
    delay(5);
    if (cc1101Manager.carrierDetected(-75)) { cc1101Manager.idle(); error = "CHANNEL BUSY"; return false; }
    cc1101Manager.setFrequency(testFrequency);
    if (!cc1101Manager.enterRawTransmit()) { error = "TX START FAILED"; return false; }
    lastTestHopUs = micros(); rfTestStartedMs = millis(); rfTesting = true;
    error = "RF TEST ACTIVE"; return true;
#endif
}

void SubGhzRawService::stopRfTest() {
    const bool wasActive = rfTesting;
    rfTesting = false;
    cc1101Manager.setRawData(false);
    cc1101Manager.idle();
    if (wasActive) lastTransmitEndedMs = millis();
    error = "RF TEST STOPPED";
}
