#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_system.h>
#include <time.h>

#include "config.h"
#include "logger.h"
#include "settings.h"
#include "display.h"
#include "touch.h"
#include "wifi_manager.h"
#include "weather.h"
#include "ota.h"
#include "web_server.h"

// ============================================================
// Display Pages
// ============================================================

enum DisplayPage {
    PAGE_CLOCK_WEATHER = 0,
    PAGE_SYSTEM_INFO,
    PAGE_COUNT
};

static DisplayPage currentPage = PAGE_CLOCK_WEATHER;

// ============================================================
// Time Formatting
// ============================================================

static bool getFormattedTime(char* timeBuf, size_t timeBufLen,
                             char* dateBuf, size_t dateBufLen) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        return false;
    }

    // HH:MM (24-hour)
    strftime(timeBuf, timeBufLen, "%H:%M", &timeinfo);

    // "Mon Feb 10"
    strftime(dateBuf, dateBufLen, "%a %b %d", &timeinfo);

    return true;
}

// ============================================================
// System Info Page Rendering
// ============================================================

static void renderSystemInfo() {
    LGFX* lcd = displayGetLCD();
    if (!lcd) return;

    lcd->fillScreen(TFT_BLACK);
    lcd->setTextColor(TFT_WHITE, TFT_BLACK);
    lcd->setTextSize(1);

    int y = 10;
    int lineHeight = 18;

    lcd->setCursor(10, y);
    lcd->printf("FW: %s", FW_VERSION);
    y += lineHeight;

    lcd->setCursor(10, y);
    if (wifiIsConnected()) {
        lcd->printf("WiFi: %s", wifiGetSSID().c_str());
    } else if (wifiIsAPMode()) {
        lcd->printf("WiFi: AP Mode");
    } else {
        lcd->printf("WiFi: Disconnected");
    }
    y += lineHeight;

    lcd->setCursor(10, y);
    lcd->printf("IP: %s", wifiGetIP().c_str());
    y += lineHeight;

    lcd->setCursor(10, y);
    lcd->printf("RSSI: %d dBm", wifiGetRSSI());
    y += lineHeight;

    lcd->setCursor(10, y);
    lcd->printf("Heap: %u KB", ESP.getFreeHeap() / 1024);
    y += lineHeight;

    // Uptime
    unsigned long uptimeSec = millis() / 1000;
    unsigned long hours   = uptimeSec / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    unsigned long secs    = uptimeSec % 60;

    lcd->setCursor(10, y);
    lcd->printf("Up: %luh %lum %lus", hours, minutes, secs);
    y += lineHeight;

    lcd->setCursor(10, y);
    lcd->printf("OTA: %s", otaIsConfirmed() ? "Confirmed" : "Pending");
    y += lineHeight;

    lcd->setCursor(10, y);
    lcd->printf("MAC: %s", wifiGetMAC().c_str());
}

// ============================================================
// Screen Dimming
// ============================================================

static unsigned long lastTouchTime = 0;
static bool          screenDimmed  = false;
static bool          screenOffByUser = false; // Set when long press turns screen off

static void handleScreenDimming() {
    // Don't auto-dim or auto-wake while user explicitly turned screen off via long press.
    // Wake is handled by the tap event handler instead.
    if (screenOffByUser) return;

    // Auto-dim timeout: dim screen after SCREEN_DIM_MS of no touch events
    if (!screenDimmed && (millis() - lastTouchTime) >= SCREEN_DIM_MS) {
        displaySetBrightness(BRIGHTNESS_DIM);
        screenDimmed = true;
    }
}

// ============================================================
// Setup
// ============================================================

void setup() {
    // 1. Serial
    Serial.begin(SERIAL_BAUD);
    delay(500);

    // 2. Logger
    logInit();

    // 3. Firmware and chip info
    logPrintf("SmallTV Firmware v%s", FW_VERSION);
    logPrintf("Chip: %s, Rev %d, %d cores, %d MHz",
              ESP.getChipModel(),
              ESP.getChipRevision(),
              ESP.getChipCores(),
              ESP.getCpuFreqMHz());
    logPrintf("Flash: %u KB, Heap: %u KB",
              ESP.getFlashChipSize() / 1024,
              ESP.getFreeHeap() / 1024);

    // 4. Settings
    settingsInit();
    Settings& settings = settingsGet();

    // 5. Boot failure counter
    bootCounterIncrement();

    // 6. Emergency: if boot keeps failing and OTA is pending, roll back firmware.
    //    Otherwise fall through to settings reset.
    if (bootCounterCheck()) {
        if (otaIsPending()) {
            logPrintf("Boot crash loop detected with pending OTA - rolling back firmware");
            otaRollback();  // Marks invalid + reboots, does not return
        }
        logPrintf("Emergency reset triggered by boot failure counter");
        settingsReset();  // Wipes NVS and reboots
        // Does not return
    }

    // 7. Power cycle counter
    powerCycleIncrement();

    // 8a. OTA rollback via 3 quick power cycles (only when firmware is pending)
    if (otaIsPending() && powerCycleCount() >= POWER_CYCLE_ROLLBACK) {
        logPrintf("Rapid power cycle rollback (%d cycles with pending OTA)", powerCycleCount());
        powerCycleReset();
        otaRollback();  // Marks invalid + reboots, does not return
    }

    // 8b. Factory reset if rapid power cycling detected (5 cycles)
    if (powerCycleCheck()) {
        logPrintf("Factory reset triggered by rapid power cycling");
        // Clear the power cycle counter FIRST to prevent infinite reboot loop
        powerCycleReset();
        // Clear user settings (this won't reboot because we use settingsClear)
        settingsClear();
        wifiFactoryReset();  // Clears WiFi creds and reboots
        // Does not return
    }

    // 9. Display
    displayInit();

    // 10. Brightness
    displaySetBrightness(settings.brightness);

    // 11. Touch
    touchInit();

    // 12. WiFi
    wifiInit();

    // 13. mDNS
    if (wifiIsConnected()) {
        delay(200);  // Let WiFi stack fully settle before mDNS

        if (MDNS.begin(settings.hostname)) {
            MDNS.addService("http", "tcp", WEB_SERVER_PORT);
            logPrintf("mDNS started: %s.local", settings.hostname);
        } else {
            logPrintf("mDNS failed to start");
        }
    }

    // 14. Web server
    webServerInit();

    // 15. Weather
    weatherInit();

    // 16. OTA
    otaInit();

    // 17. NTP time sync
    configTime(settings.gmtOffsetSec, 0, "pool.ntp.org");
    logPrintf("NTP configured: gmtOffset=%ld", settings.gmtOffsetSec);

    // 18. Mark successful boot
    bootCounterReset();

    // Initialize touch timer for dimming
    lastTouchTime = millis();

    // 19. Done
    logPrintf("Setup complete");
}

// ============================================================
// Main Loop
// ============================================================

static unsigned long lastDisplayUpdate = 0;

void loop() {
    // 1. Input polling
    touchUpdate();

    // 2. Network services
    wifiUpdate();
    webServerUpdate();
    weatherUpdate();
    otaUpdate();

    // 3. Touch events: tap cycles pages, long press toggles backlight
    if (touchWasTapped()) {
        lastTouchTime = millis();

        // If screen was off (by user or auto-dim), wake it on tap instead of cycling pages
        if (screenDimmed || screenOffByUser) {
            displaySetBrightness(settingsGet().brightness);
            screenDimmed = false;
            screenOffByUser = false;
        } else {
            currentPage = (DisplayPage)((currentPage + 1) % PAGE_COUNT);
            logPrintf("Page changed to %d", (int)currentPage);
            lastDisplayUpdate = 0;  // Force immediate redraw
        }
    }

    if (touchWasLongPressed()) {
        lastTouchTime = millis();

        if (screenDimmed || screenOffByUser) {
            // Wake from any dimmed/off state
            displaySetBrightness(settingsGet().brightness);
            screenDimmed = false;
            screenOffByUser = false;
        } else {
            // Turn screen off
            displaySetBrightness(0);
            screenOffByUser = true;
        }
        logPrintf("Backlight toggled (off=%s)", screenOffByUser ? "true" : "false");
    }

    // 4. Display update (every DISPLAY_UPDATE_MS)
    unsigned long now = millis();

    if ((now - lastDisplayUpdate) >= DISPLAY_UPDATE_MS) {
        lastDisplayUpdate = now;

        if (wifiIsAPMode()) {
            // Show AP setup screen with SSID and IP
            displayRenderAPMode(wifiGetSSID().c_str(), wifiGetIP().c_str());
        } else {
            switch (currentPage) {
                case PAGE_CLOCK_WEATHER: {
                    char timeBuf[8];
                    char dateBuf[16];

                    if (getFormattedTime(timeBuf, sizeof(timeBuf),
                                         dateBuf, sizeof(dateBuf))) {
                        const WeatherData& weather = weatherGet();
                        displayRenderClock(timeBuf, dateBuf,
                                           weather.valid ? &weather : nullptr);
                    } else {
                        displayRenderMessage("Waiting for NTP...");
                    }
                    break;
                }

                case PAGE_SYSTEM_INFO: {
                    renderSystemInfo();
                    break;
                }

                default:
                    break;
            }
        }
    }

    // 5. Power cycle counter: reset once uptime exceeds window
    if (now >= POWER_CYCLE_WINDOW_MS) {
        // Only reset once (powerCycleReset logs, so guard with a static flag)
        static bool powerCycleCleared = false;
        if (!powerCycleCleared) {
            powerCycleReset();
            powerCycleCleared = true;
        }
    }

    // 6. Screen dimming
    handleScreenDimming();

    // 7. Yield
    delay(10);
}
