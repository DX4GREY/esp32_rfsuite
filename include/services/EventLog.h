#pragma once

#include <Arduino.h>

struct LogEntry {
    unsigned long timestampMs = 0;
    char level[8] = {};
    char source[16] = {};
    char message[48] = {};
};

class EventLog {
public:
    bool begin();
    void write(const char* level, const char* source, const char* message);
    void info(const char* source, const char* message) { write("INFO", source, message); }
    void warn(const char* source, const char* message) { write("WARN", source, message); }
    void error(const char* source, const char* message) { write("ERROR", source, message); }
    bool exportTo(Stream& output);
    bool clear();
    const char* path() const;
    size_t countEntries();
    size_t getEntries(LogEntry* entries, size_t maxEntries, size_t offset = 0);

private:
    bool ready = false;
    void rotateIfNeeded();
};

extern EventLog eventLog;
