#pragma once

#include <Arduino.h>
#include "config.h"

// NVS-backed persistent settings with boot safety counters.
//
// The settings struct is versioned. When SETTINGS_VERSION changes
// (because the struct layout changed between firmware versions),
// stored settings are discarded and defaults are applied.

struct Settings {
    uint8_t version;
    uint8_t brightness;       // 0-100
    bool    tempFahrenheit;   // true = Fahrenheit, false = Celsius
    float   latitude;
    float   longitude;
    char    hostname[32];     // mDNS hostname
    long    gmtOffsetSec;     // Timezone offset in seconds
};

// --- Settings lifecycle ---
void     settingsInit();                // Load from NVS or apply defaults
void     settingsSave();                // Persist current settings to NVS
Settings& settingsGet();                // Get mutable reference to live settings
void     settingsClear();               // Wipe all NVS (no reboot)
void     settingsReset();               // Wipe all NVS and reboot (factory reset)

// --- Boot failure counter ---
// Tracks consecutive boots that didn't reach the "success" checkpoint.
// If the device keeps crashing during init, this triggers an emergency reset.
void bootCounterIncrement();            // Call at very start of setup()
void bootCounterReset();                // Call once boot is confirmed good
bool bootCounterCheck();                // True if threshold exceeded

// --- Power cycle counter ---
// Tracks rapid power cycles (user yanking power repeatedly).
// If the device stays up past POWER_CYCLE_WINDOW_MS, the counter resets.
// Exceeding the threshold triggers a factory reset.
void powerCycleIncrement();             // Call at boot
void powerCycleReset();                 // Call after POWER_CYCLE_WINDOW_MS of uptime
bool powerCycleCheck();                 // True if threshold exceeded
