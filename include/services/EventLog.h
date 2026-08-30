#pragma once

#include <Arduino.h>

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

private:
    bool ready = false;
    void rotateIfNeeded();
};

extern EventLog eventLog;
