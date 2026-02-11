#pragma once

#include <Arduino.h>
#include "config.h"

// Simple circular log buffer with Serial output.
// Stores the last LOG_BUFFER_SIZE entries, each up to LOG_LINE_LENGTH chars.
// All entries are timestamped with millis().

void logInit();
void logPrint(const char* msg);
void logPrintf(const char* format, ...);
String logGetAll();
