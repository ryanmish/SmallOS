#include "settings.h"
#include "logger.h"
#include <Preferences.h>

// --- NVS keys ---
static const char* KEY_VERSION       = "version";
static const char* KEY_BRIGHTNESS    = "bright";
static const char* KEY_TEMP_UNIT     = "tempF";
static const char* KEY_LATITUDE      = "lat";
static const char* KEY_LONGITUDE     = "lon";
static const char* KEY_HOSTNAME      = "hostname";
static const char* KEY_GMT_OFFSET    = "gmtOff";
static const char* KEY_BOOT_FAILS    = "bootFails";
static const char* KEY_POWER_CYCLES  = "pwrCycles";

// --- Module state ---
static Settings currentSettings;
static Preferences prefs;

// --- Internal helpers ---

static void applyDefaults() {
    currentSettings.version        = SETTINGS_VERSION;
    currentSettings.brightness     = BRIGHTNESS_DEFAULT;
    currentSettings.tempFahrenheit = TEMP_UNIT_FAHRENHEIT;
    currentSettings.latitude       = WEATHER_DEFAULT_LAT;
    currentSettings.longitude      = WEATHER_DEFAULT_LON;
    currentSettings.gmtOffsetSec   = 0;
    strncpy(currentSettings.hostname, "smalltv", sizeof(currentSettings.hostname) - 1);
    currentSettings.hostname[sizeof(currentSettings.hostname) - 1] = '\0';
}

static void loadFromNVS() {
    currentSettings.version        = prefs.getUChar(KEY_VERSION, SETTINGS_VERSION);
    currentSettings.brightness     = prefs.getUChar(KEY_BRIGHTNESS, BRIGHTNESS_DEFAULT);
    currentSettings.tempFahrenheit = prefs.getBool(KEY_TEMP_UNIT, TEMP_UNIT_FAHRENHEIT);
    currentSettings.latitude       = prefs.getFloat(KEY_LATITUDE, WEATHER_DEFAULT_LAT);
    currentSettings.longitude      = prefs.getFloat(KEY_LONGITUDE, WEATHER_DEFAULT_LON);
    currentSettings.gmtOffsetSec   = prefs.getLong(KEY_GMT_OFFSET, 0);

    String storedHostname = prefs.getString(KEY_HOSTNAME, "smalltv");
    strncpy(currentSettings.hostname, storedHostname.c_str(), sizeof(currentSettings.hostname) - 1);
    currentSettings.hostname[sizeof(currentSettings.hostname) - 1] = '\0';
}

static void writeToNVS() {
    prefs.putUChar(KEY_VERSION, currentSettings.version);
    prefs.putUChar(KEY_BRIGHTNESS, currentSettings.brightness);
    prefs.putBool(KEY_TEMP_UNIT, currentSettings.tempFahrenheit);
    prefs.putFloat(KEY_LATITUDE, currentSettings.latitude);
    prefs.putFloat(KEY_LONGITUDE, currentSettings.longitude);
    prefs.putLong(KEY_GMT_OFFSET, currentSettings.gmtOffsetSec);
    prefs.putString(KEY_HOSTNAME, currentSettings.hostname);
}

// --- Public API: Settings ---

void settingsInit() {
    prefs.begin(NVS_NAMESPACE, false);

    uint8_t storedVersion = prefs.getUChar(KEY_VERSION, 0);

    if (storedVersion != SETTINGS_VERSION) {
        logPrintf("Settings version mismatch (stored=%u, current=%u) - applying defaults",
                  storedVersion, SETTINGS_VERSION);
        applyDefaults();
        writeToNVS();
    } else {
        loadFromNVS();
        logPrintf("Settings loaded from NVS (v%u)", currentSettings.version);
    }

    logPrintf("  brightness=%u, tempF=%s, lat=%.4f, lon=%.4f",
              currentSettings.brightness,
              currentSettings.tempFahrenheit ? "true" : "false",
              currentSettings.latitude,
              currentSettings.longitude);
    logPrintf("  hostname=%s, gmtOffset=%ld",
              currentSettings.hostname,
              currentSettings.gmtOffsetSec);
}

void settingsSave() {
    writeToNVS();
    logPrintf("Settings saved to NVS");
}

Settings& settingsGet() {
    return currentSettings;
}

void settingsClear() {
    logPrintf("Settings: clearing all NVS data");
    prefs.clear();
}

void settingsReset() {
    logPrintf("Factory reset: clearing all NVS and rebooting");
    settingsClear();
    prefs.end();
    delay(500);
    ESP.restart();
}

// --- Public API: Boot failure counter ---

void bootCounterIncrement() {
    int count = prefs.getInt(KEY_BOOT_FAILS, 0) + 1;
    prefs.putInt(KEY_BOOT_FAILS, count);
    logPrintf("Boot failure counter: %d/%d", count, BOOT_FAIL_THRESHOLD);
}

void bootCounterReset() {
    prefs.putInt(KEY_BOOT_FAILS, 0);
    logPrintf("Boot failure counter reset (boot successful)");
}

bool bootCounterCheck() {
    int count = prefs.getInt(KEY_BOOT_FAILS, 0);
    if (count >= BOOT_FAIL_THRESHOLD) {
        logPrintf("Boot failure threshold exceeded (%d >= %d)", count, BOOT_FAIL_THRESHOLD);
        return true;
    }
    return false;
}

// --- Public API: Power cycle counter ---

void powerCycleIncrement() {
    int count = prefs.getInt(KEY_POWER_CYCLES, 0) + 1;
    prefs.putInt(KEY_POWER_CYCLES, count);
    logPrintf("Power cycle counter: %d/%d", count, POWER_CYCLE_THRESHOLD);
}

void powerCycleReset() {
    prefs.putInt(KEY_POWER_CYCLES, 0);
    logPrintf("Power cycle counter reset (uptime exceeded %dms window)", POWER_CYCLE_WINDOW_MS);
}

bool powerCycleCheck() {
    int count = prefs.getInt(KEY_POWER_CYCLES, 0);
    if (count >= POWER_CYCLE_THRESHOLD) {
        logPrintf("Power cycle threshold exceeded (%d >= %d)", count, POWER_CYCLE_THRESHOLD);
        return true;
    }
    return false;
}
