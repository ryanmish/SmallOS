#include "logger.h"
#include <stdarg.h>

// --- Circular buffer storage (fixed, no heap) ---

static char logBuffer[LOG_BUFFER_SIZE][LOG_LINE_LENGTH];
static int  logHead  = 0;   // Next write position
static int  logCount = 0;   // Entries currently stored (max LOG_BUFFER_SIZE)

// --- Public API ---

void logInit() {
    memset(logBuffer, 0, sizeof(logBuffer));
    logHead  = 0;
    logCount = 0;
    Serial.println(F("[LOG] Logger initialized"));
}

void logPrint(const char* msg) {
    // Build timestamped line: "[  12345] message"
    char line[LOG_LINE_LENGTH];
    snprintf(line, sizeof(line), "[%7lu] %s", millis(), msg);

    // Print to Serial
    Serial.println(line);

    // Store in circular buffer
    strncpy(logBuffer[logHead], line, LOG_LINE_LENGTH - 1);
    logBuffer[logHead][LOG_LINE_LENGTH - 1] = '\0';

    logHead = (logHead + 1) % LOG_BUFFER_SIZE;
    if (logCount < LOG_BUFFER_SIZE) {
        logCount++;
    }
}

void logPrintf(const char* format, ...) {
    char buf[LOG_LINE_LENGTH];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    logPrint(buf);
}

String logGetAll() {
    String out;
    out.reserve(logCount * (LOG_LINE_LENGTH / 2));  // Conservative pre-alloc

    if (logCount == 0) {
        return F("(no log entries)");
    }

    // Start from the oldest entry in the buffer
    int start = (logCount < LOG_BUFFER_SIZE) ? 0 : logHead;

    for (int i = 0; i < logCount; i++) {
        int idx = (start + i) % LOG_BUFFER_SIZE;
        out += logBuffer[idx];
        out += '\n';
    }

    return out;
}
