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
