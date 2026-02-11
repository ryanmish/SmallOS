#pragma once

#include <Arduino.h>

// ============================================================
// Capacitive Touch Driver
// Self-calibrating with tap, long-press, and double-tap detection
// ============================================================

void     touchInit();
void     touchUpdate();             // Call every loop iteration

// Gesture events (consuming - flag clears on read)
bool     touchWasTapped();
bool     touchWasLongPressed();
bool     touchWasDoubleTapped();

// Non-consuming state
bool     touchIsTouched();
uint16_t touchGetRaw();             // Raw ADC reading for diagnostics
uint16_t touchGetBaseline();        // Current adaptive baseline

// WiFi coordination (touch pin shares ADC with WiFi radio)
void     touchPauseForWiFi();
void     touchResumeAfterWiFi();
