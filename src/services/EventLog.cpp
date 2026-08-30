#include "services/EventLog.h"

#include <FS.h>
#include "services/StorageManager.h"

namespace {
constexpr size_t MAX_EVENT_LOG_BYTES = 16U * 1024U;
constexpr const char* SD_EVENT_PATH = "/RFSuite/log/events.csv";
constexpr const char* FLASH_EVENT_PATH = "/events.csv";
}

EventLog eventLog;

const char* EventLog::path() const {
    return storageManager.usingSd() ? SD_EVENT_PATH : FLASH_EVENT_PATH;
}

bool EventLog::begin() {
    if (!storageManager.usingSd() && !storageManager.flashUsable()) return false;
    ready = true;
    fs::FS& fs = storageManager.filesystem();
    if (!fs.exists(path())) {
        File file = fs.open(path(), FILE_WRITE);
        if (!file) { ready = false; return false; }
        file.println("ms,level,source,message");
        file.close();
    }
    info("system", "boot");
    return true;
}

void EventLog::rotateIfNeeded() {
    if (!ready) return;
    fs::FS& fs = storageManager.filesystem();
    File file = fs.open(path(), FILE_READ);
    const size_t size = file ? file.size() : 0;
    if (file) file.close();
    if (size < MAX_EVENT_LOG_BYTES) return;
    const String previous = String(path()) + ".1";
    fs.remove(previous);
    fs.rename(path(), previous);
    file = fs.open(path(), FILE_WRITE);
    if (file) { file.println("ms,level,source,message"); file.close(); }
}

void EventLog::write(const char* level, const char* source, const char* message) {
    if (!ready) return;
    rotateIfNeeded();
    File file = storageManager.filesystem().open(path(), FILE_APPEND);
    if (!file) { ready = false; return; }
    file.printf("%lu,%s,%s,\"", static_cast<unsigned long>(millis()), level, source);
    for (const char* p = message ? message : ""; *p; ++p) {
        if (*p == '"') file.print('"');
        if (*p != '\r' && *p != '\n') file.print(*p);
    }
    file.println('"');
    file.close();
}

bool EventLog::exportTo(Stream& output) {
    if (!ready) return false;
    File file = storageManager.filesystem().open(path(), FILE_READ);
    if (!file) return false;
    uint8_t buffer[128];
    while (file.available()) {
        const size_t count = file.read(buffer, sizeof(buffer));
        if (!count) break;
        output.write(buffer, count);
        yield();
    }
    file.close();
    return true;
}

bool EventLog::clear() {
    if (!ready) return false;
    fs::FS& fs = storageManager.filesystem();
    fs.remove(String(path()) + ".1");
    fs.remove(path());
    File file = fs.open(path(), FILE_WRITE);
    if (!file) return false;
    file.println("ms,level,source,message");
    file.close();
    return true;
}

size_t EventLog::countEntries() {
    if (!ready) return 0;
    File file = storageManager.filesystem().open(path(), FILE_READ);
    if (!file) return 0;
    size_t count = 0;
    bool first = true;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (first) { first = false; continue; }
        if (line.length() > 0) count++;
    }
    file.close();
    return count;
}

size_t EventLog::getEntries(LogEntry* entries, size_t maxEntries, size_t offset) {
    if (!ready || !entries || maxEntries == 0) return 0;
    File file = storageManager.filesystem().open(path(), FILE_READ);
    if (!file) return 0;

    // Buffer lines to return newest first
    // First count total non-header lines
    size_t total = 0;
    bool first = true;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (first) { first = false; continue; }
        if (line.length() > 0) total++;
    }
    file.close();

    if (offset >= total) return 0;
    size_t fetchCount = min(maxEntries, total - offset);

    // Reopen and parse lines
    file = storageManager.filesystem().open(path(), FILE_READ);
    if (!file) return 0;

    first = true;
    size_t currentIdx = 0;
    size_t populated = 0;
    // We want newest first: index (total - 1 - offset) down to (total - offset - fetchCount)
    size_t targetStart = total > offset ? total - 1 - offset : 0;
    size_t targetEnd = total >= (offset + fetchCount) ? total - (offset + fetchCount) : 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (first) { first = false; continue; }
        if (line.length() == 0) continue;

        if (currentIdx <= targetStart && currentIdx >= targetEnd) {
            size_t outSlot = targetStart - currentIdx;
            if (outSlot < fetchCount) {
                // Parse CSV: ms,level,source,"message"
                int c1 = line.indexOf(',');
                int c2 = c1 >= 0 ? line.indexOf(',', c1 + 1) : -1;
                int c3 = c2 >= 0 ? line.indexOf(',', c2 + 1) : -1;

                if (c1 > 0) {
                    entries[outSlot].timestampMs = strtoul(line.substring(0, c1).c_str(), nullptr, 10);
                }
                if (c1 >= 0 && c2 > c1) {
                    String lvl = line.substring(c1 + 1, c2);
                    lvl.trim();
                    snprintf(entries[outSlot].level, sizeof(entries[outSlot].level), "%s", lvl.c_str());
                }
                if (c2 >= 0 && c3 > c2) {
                    String src = line.substring(c2 + 1, c3);
                    src.trim();
                    snprintf(entries[outSlot].source, sizeof(entries[outSlot].source), "%s", src.c_str());
                }
                if (c3 >= 0) {
                    String msg = line.substring(c3 + 1);
                    msg.trim();
                    if (msg.startsWith("\"") && msg.endsWith("\"") && msg.length() >= 2) {
                        msg = msg.substring(1, msg.length() - 1);
                    }
                    snprintf(entries[outSlot].message, sizeof(entries[outSlot].message), "%s", msg.c_str());
                }
                populated++;
            }
        }
        currentIdx++;
    }
    file.close();
    return populated;
}
