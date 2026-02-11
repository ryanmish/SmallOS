#include "display.h"
#include "weather.h"
#include "config.h"
#include "logger.h"

#include <WiFi.h>
#include <math.h>

// ============================================================
// Display Implementation
// ============================================================

// --- Color palette (RGB565) ---
static const uint16_t COL_BG        = 0x0000;   // Pure black
static const uint16_t COL_WHITE     = 0xFFFF;
static const uint16_t COL_CYAN      = 0x07FF;
static const uint16_t COL_GREEN     = 0x07E0;
static const uint16_t COL_RED       = 0xF800;
static const uint16_t COL_GREY      = 0x7BEF;
static const uint16_t COL_DARK_GREY = 0x3186;

// --- Layout constants (clock page) ---
static const int TIME_Y       = 55;     // Large clock vertical position
static const int DATE_Y       = 110;    // Date line below clock
static const int DIVIDER_Y    = 140;    // Horizontal divider
static const int WEATHER_Y    = 160;    // Weather description
static const int TEMP_Y       = 195;    // Temperature value
static const int WIFI_DOT_X   = 228;    // WiFi indicator position
static const int WIFI_DOT_Y   = 8;
static const int WIFI_DOT_R   = 5;
static const int IP_Y         = 4;      // IP address line
static const int CENTER_X     = DISPLAY_WIDTH / 2;

// --- Differential rendering state (clock page) ---
struct PreviousClockState {
    char time[6];           // "HH:MM\0"
    char date[32];
    char weatherDesc[24];
    float temperature;
    bool weatherValid;
    bool wifiConnected;
    bool showIP;
    char ip[16];
    bool initialized;       // False until first full draw
};

// --- Differential rendering state (system info page) ---
struct PreviousSysInfoState {
    char fwVersion[16];
    char wifiStatus[40];
    char ip[20];
    char rssi[16];
    char heap[16];
    char uptime[24];
    char ota[16];
    char mac[20];
    bool initialized;
};

// --- Page tracking ---
static DisplayPage currentPage = PAGE_CLOCK_WEATHER;
static DisplayPage lastRenderedPage = (DisplayPage)-1;  // Force initial clear

static LGFX lcd;
static PreviousClockState prevClock;
static PreviousSysInfoState prevSysInfo;

// --- Internal helpers ---

static bool apRendered = false;
static bool otaScreenInitialized = false;

static void clearAllPrevState() {
    memset(&prevClock, 0, sizeof(prevClock));
    prevClock.initialized = false;
    prevClock.temperature = -999.0f;

    memset(&prevSysInfo, 0, sizeof(prevSysInfo));
    prevSysInfo.initialized = false;

    apRendered = false;
    otaScreenInitialized = false;
}

// Draw centered text with background fill to avoid flicker.
// The datum is set to middle_center so x,y is the center point.
static void drawCenteredText(int x, int y, const char* text,
                             const lgfx::IFont* font, float scale,
                             uint16_t fg, uint16_t bg) {
    lcd.setFont(font);
    lcd.setTextSize(scale);
    lcd.setTextColor(fg, bg);
    lcd.setTextDatum(lgfx::middle_center);
    lcd.drawString(text, x, y);
}

// --- Boot color test ---

static void bootColorTest() {
    const uint16_t colors[] = { TFT_RED, TFT_GREEN, TFT_BLUE, TFT_WHITE, TFT_BLACK };
    const char* names[] = { "RED", "GREEN", "BLUE", "WHITE", "BLACK" };

    for (int i = 0; i < 5; i++) {
        lcd.fillScreen(colors[i]);
        logPrintf("Display test: %s", names[i]);
        delay(200);
    }

    lcd.fillScreen(COL_BG);
}

// --- Public API ---

void displayInit() {
    lcd.init();
    lcd.setRotation(0);
    lcd.setBrightness(0);  // Start dark, brightness set after boot
    lcd.fillScreen(COL_BG);

    clearAllPrevState();

    logPrintf("Display initialized (%dx%d ST7789V)", DISPLAY_WIDTH, DISPLAY_HEIGHT);

    bootColorTest();

    // Set default brightness
    lcd.setBrightness((BRIGHTNESS_DEFAULT * 255) / 100);
    logPrintf("Brightness set to %d%%", BRIGHTNESS_DEFAULT);
}

void displaySetBrightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    uint8_t hw = (brightness * 255) / 100;
    lcd.setBrightness(hw);
}

LGFX* displayGetLCD() {
    return &lcd;
}

void displaySetPage(DisplayPage page) {
    currentPage = page;
}

DisplayPage displayGetPage() {
    return currentPage;
}

// --- Clock screen (differential) ---

static void renderClock(const char* timeStr, const char* dateStr, const WeatherData* weather) {
    // Full redraw on first call after page entry
    if (!prevClock.initialized) {
        prevClock.initialized = true;

        // Draw static divider line
        lcd.drawFastHLine(40, DIVIDER_Y, DISPLAY_WIDTH - 80, COL_DARK_GREY);
    }

    // --- Time (large) ---
    if (strcmp(timeStr, prevClock.time) != 0) {
        drawCenteredText(CENTER_X, TIME_Y, timeStr,
                         &fonts::Font7, 1.0f, COL_WHITE, COL_BG);
        strncpy(prevClock.time, timeStr, sizeof(prevClock.time) - 1);
        prevClock.time[sizeof(prevClock.time) - 1] = '\0';
    }

    // --- Date ---
    if (strcmp(dateStr, prevClock.date) != 0) {
        drawCenteredText(CENTER_X, DATE_Y, dateStr,
                         &fonts::Font2, 1.0f, COL_GREY, COL_BG);
        strncpy(prevClock.date, dateStr, sizeof(prevClock.date) - 1);
        prevClock.date[sizeof(prevClock.date) - 1] = '\0';
    }

    // --- Weather (bottom half) ---
    if (weather && weather->valid) {
        // Weather description (derived from icon enum)
        const char* desc = weatherIconName(weather->icon);
        if (strcmp(desc, prevClock.weatherDesc) != 0) {
            drawCenteredText(CENTER_X, WEATHER_Y, desc,
                             &fonts::Font2, 1.0f, COL_WHITE, COL_BG);
            strncpy(prevClock.weatherDesc, desc, sizeof(prevClock.weatherDesc) - 1);
            prevClock.weatherDesc[sizeof(prevClock.weatherDesc) - 1] = '\0';
        }

        // Temperature (use epsilon to avoid float equality issues)
        if (fabsf(weather->temperature - prevClock.temperature) > 0.05f) {
            char tempBuf[16];
            snprintf(tempBuf, sizeof(tempBuf), "%.0f%s",
                     weather->temperature,
                     TEMP_UNIT_FAHRENHEIT ? "F" : "C");
            drawCenteredText(CENTER_X, TEMP_Y, tempBuf,
                             &fonts::Font4, 1.0f, COL_CYAN, COL_BG);
            prevClock.temperature = weather->temperature;
        }

        prevClock.weatherValid = true;

    } else if (prevClock.weatherValid) {
        // Weather became invalid or first draw with no weather
        drawCenteredText(CENTER_X, WEATHER_Y, "No weather data",
                         &fonts::Font2, 1.0f, COL_DARK_GREY, COL_BG);

        // Clear temperature area (Font4 is ~26px tall, use 30px to be safe)
        lcd.fillRect(0, TEMP_Y - 15, DISPLAY_WIDTH, 30, COL_BG);

        prevClock.weatherValid = false;
        prevClock.temperature = -999.0f;
        memset(prevClock.weatherDesc, 0, sizeof(prevClock.weatherDesc));
    }

    // --- WiFi status dot (top-right) ---
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    if (wifiUp != prevClock.wifiConnected) {
        uint16_t dotColor = wifiUp ? COL_GREEN : COL_RED;
        lcd.fillCircle(WIFI_DOT_X, WIFI_DOT_Y, WIFI_DOT_R, dotColor);
        prevClock.wifiConnected = wifiUp;
    }

    // --- IP address (top, small, only when connected) ---
    if (wifiUp) {
        String ipStr = WiFi.localIP().toString();
        if (strcmp(ipStr.c_str(), prevClock.ip) != 0) {
            // Clear previous IP area
            lcd.fillRect(0, 0, WIFI_DOT_X - WIFI_DOT_R - 4, 14, COL_BG);
            lcd.setFont(&fonts::Font0);
            lcd.setTextSize(1.0f);
            lcd.setTextColor(COL_DARK_GREY, COL_BG);
            lcd.setTextDatum(lgfx::top_left);
            lcd.drawString(ipStr.c_str(), 4, IP_Y);
            strncpy(prevClock.ip, ipStr.c_str(), sizeof(prevClock.ip) - 1);
            prevClock.ip[sizeof(prevClock.ip) - 1] = '\0';
        }
    } else if (prevClock.ip[0] != '\0') {
        // WiFi dropped, clear IP
        lcd.fillRect(0, 0, WIFI_DOT_X - WIFI_DOT_R - 4, 14, COL_BG);
        prevClock.ip[0] = '\0';
    }
}

// --- System info screen (differential) ---

static void renderSystemInfo(const char* fwVersion, bool wifiConnected, bool wifiAP,
                             const char* ssid, const char* ip, int rssi,
                             const char* mac, uint32_t freeHeapKB,
                             unsigned long uptimeSec, bool otaConfirmed) {
    const int lineHeight = 18;
    const int startX = 10;
    int y = 10;

    // Helper: draw a line only if its text changed. Uses two-arg setTextColor
    // to overwrite old text with background color (no fillScreen needed).
    auto drawInfoLine = [&](int lineY, const char* newText, char* prevBuf, size_t prevBufSize) {
        if (strcmp(newText, prevBuf) != 0) {
            // Set font state for this draw
            lcd.setFont(&fonts::Font2);
            lcd.setTextSize(1);
            lcd.setTextDatum(lgfx::top_left);

            // Erase old text by drawing it in background color
            if (prevBuf[0] != '\0') {
                lcd.setTextColor(COL_BG, COL_BG);
                lcd.drawString(prevBuf, startX, lineY);
            }

            // Draw new text
            lcd.setTextColor(COL_WHITE, COL_BG);
            lcd.drawString(newText, startX, lineY);

            strncpy(prevBuf, newText, prevBufSize - 1);
            prevBuf[prevBufSize - 1] = '\0';
        }
    };

    // Build current strings
    char fwBuf[sizeof(prevSysInfo.fwVersion)];
    snprintf(fwBuf, sizeof(fwBuf), "FW: %s", fwVersion);
    drawInfoLine(y, fwBuf, prevSysInfo.fwVersion, sizeof(prevSysInfo.fwVersion));
    y += lineHeight;

    char wifiBuf[sizeof(prevSysInfo.wifiStatus)];
    if (wifiConnected) {
        snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: %s", ssid);
    } else if (wifiAP) {
        snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: AP Mode");
    } else {
        snprintf(wifiBuf, sizeof(wifiBuf), "WiFi: Disconnected");
    }
    drawInfoLine(y, wifiBuf, prevSysInfo.wifiStatus, sizeof(prevSysInfo.wifiStatus));
    y += lineHeight;

    char ipBuf[sizeof(prevSysInfo.ip)];
    snprintf(ipBuf, sizeof(ipBuf), "IP: %s", ip);
    drawInfoLine(y, ipBuf, prevSysInfo.ip, sizeof(prevSysInfo.ip));
    y += lineHeight;

    char rssiBuf[sizeof(prevSysInfo.rssi)];
    snprintf(rssiBuf, sizeof(rssiBuf), "RSSI: %d dBm", rssi);
    drawInfoLine(y, rssiBuf, prevSysInfo.rssi, sizeof(prevSysInfo.rssi));
    y += lineHeight;

    char heapBuf[sizeof(prevSysInfo.heap)];
    snprintf(heapBuf, sizeof(heapBuf), "Heap: %u KB", freeHeapKB);
    drawInfoLine(y, heapBuf, prevSysInfo.heap, sizeof(prevSysInfo.heap));
    y += lineHeight;

    unsigned long hours   = uptimeSec / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    unsigned long secs    = uptimeSec % 60;
    char uptimeBuf[sizeof(prevSysInfo.uptime)];
    snprintf(uptimeBuf, sizeof(uptimeBuf), "Up: %luh %lum %lus", hours, minutes, secs);
    drawInfoLine(y, uptimeBuf, prevSysInfo.uptime, sizeof(prevSysInfo.uptime));
    y += lineHeight;

    char otaBuf[sizeof(prevSysInfo.ota)];
    snprintf(otaBuf, sizeof(otaBuf), "OTA: %s", otaConfirmed ? "Confirmed" : "Pending");
    drawInfoLine(y, otaBuf, prevSysInfo.ota, sizeof(prevSysInfo.ota));
    y += lineHeight;

    char macBuf[sizeof(prevSysInfo.mac)];
    snprintf(macBuf, sizeof(macBuf), "MAC: %s", mac);
    drawInfoLine(y, macBuf, prevSysInfo.mac, sizeof(prevSysInfo.mac));

    prevSysInfo.initialized = true;
}

// ============================================================
// Centralized display update
// ============================================================
// All normal display rendering passes through here. This function
// detects page/mode changes and calls fillScreen ONLY on transitions,
// then routes to the appropriate page renderer.

void displayUpdate(const char* timeStr, const char* dateStr,
                   const WeatherData* weather, bool apMode,
                   const char* apSSID, const char* apIP,
                   const char* fwVersion, bool wifiConnected,
                   bool wifiAP, const char* ssid, const char* ip,
                   int rssi, const char* mac, uint32_t freeHeapKB,
                   unsigned long uptimeSec, bool otaConfirmed) {

    // AP mode overrides everything. Treat it as a special "page" for
    // transition detection. The existing render-once logic (apRendered)
    // is still respected inside displayRenderAPMode.
    if (apMode) {
        // Detect transition INTO AP mode
        if (lastRenderedPage != (DisplayPage)-2) {
            // -2 is our sentinel for "AP mode active"
            clearAllPrevState();
            // apRendered was cleared by clearAllPrevState, so
            // displayRenderAPMode will do its full draw including fillScreen
            lastRenderedPage = (DisplayPage)-2;
        }
        displayRenderAPMode(apSSID, apIP);
        return;
    }

    lcd.startWrite();

    // Detect page change (including return from AP mode)
    if (currentPage != lastRenderedPage) {
        lcd.fillScreen(COL_BG);
        clearAllPrevState();
        lastRenderedPage = currentPage;
        logPrintf("Display: page transition -> %d (screen cleared)", (int)currentPage);
    }

    switch (currentPage) {
        case PAGE_CLOCK_WEATHER:
            if (timeStr && dateStr) {
                renderClock(timeStr, dateStr, weather);
            }
            break;

        case PAGE_SYSTEM_INFO:
            renderSystemInfo(fwVersion, wifiConnected, wifiAP, ssid, ip,
                             rssi, mac, freeHeapKB, uptimeSec, otaConfirmed);
            break;

        default:
            break;
    }

    lcd.endWrite();
}

// --- AP Mode screen (standalone, render-once) ---

void displayRenderAPMode(const char* ssid, const char* ip) {
    if (apRendered) return;
    apRendered = true;

    lcd.startWrite();
    lcd.fillScreen(COL_BG);

    // Title - "SmallTV"
    drawCenteredText(CENTER_X, 35, "SmallTV",
                     &fonts::Font4, 1.0f, COL_WHITE, COL_BG);

    // Firmware version
    char verBuf[24];
    snprintf(verBuf, sizeof(verBuf), "v%s", FW_VERSION);
    drawCenteredText(CENTER_X, 62, verBuf,
                     &fonts::Font2, 1.0f, COL_GREY, COL_BG);

    // Divider
    lcd.drawFastHLine(30, 82, DISPLAY_WIDTH - 60, COL_DARK_GREY);

    // "Connect to WiFi:" label
    drawCenteredText(CENTER_X, 102, "Connect to WiFi:",
                     &fonts::Font2, 1.0f, COL_GREY, COL_BG);

    // AP SSID name
    drawCenteredText(CENTER_X, 130, ssid,
                     &fonts::Font4, 1.0f, COL_CYAN, COL_BG);

    // Divider
    lcd.drawFastHLine(30, 158, DISPLAY_WIDTH - 60, COL_DARK_GREY);

    // "Then open:" label
    drawCenteredText(CENTER_X, 178, "Then open:",
                     &fonts::Font2, 1.0f, COL_GREY, COL_BG);

    // IP address
    char urlBuf[32];
    snprintf(urlBuf, sizeof(urlBuf), "http://%s", ip);
    drawCenteredText(CENTER_X, 206, urlBuf,
                     &fonts::Font2, 1.0f, COL_CYAN, COL_BG);

    lcd.endWrite();

    logPrintf("Rendered AP mode screen (SSID: %s, IP: %s)", ssid, ip);
}

// --- Full-screen message ---

void displayRenderMessage(const char* msg) {
    clearAllPrevState();
    // Force re-clear on next displayUpdate call
    lastRenderedPage = (DisplayPage)-1;

    lcd.startWrite();
    lcd.fillScreen(COL_BG);

    drawCenteredText(CENTER_X, DISPLAY_HEIGHT / 2, msg,
                     &fonts::Font4, 1.0f, COL_WHITE, COL_BG);

    lcd.endWrite();
}

// --- OTA progress screen ---

void displayRenderOTAProgress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    lcd.startWrite();

    // Only redraw full background on first call (percent == 0)
    if (!otaScreenInitialized || percent == 0) {
        clearAllPrevState();
        // Force re-clear on next displayUpdate call
        lastRenderedPage = (DisplayPage)-1;
        lcd.fillScreen(COL_BG);

        drawCenteredText(CENTER_X, 60, "Updating...",
                         &fonts::Font4, 1.0f, COL_CYAN, COL_BG);

        drawCenteredText(CENTER_X, 90, "Do not power off",
                         &fonts::Font2, 1.0f, COL_GREY, COL_BG);

        otaScreenInitialized = true;
    }

    // Progress bar dimensions
    const int barX = 30;
    const int barY = 130;
    const int barW = DISPLAY_WIDTH - 60;
    const int barH = 20;

    // Bar outline
    lcd.drawRect(barX, barY, barW, barH, COL_GREY);

    // Filled portion
    int fillW = ((barW - 2) * percent) / 100;
    if (fillW > 0) {
        lcd.fillRect(barX + 1, barY + 1, fillW, barH - 2, COL_CYAN);
    }

    // Clear remaining bar interior (in case percent decreased, which shouldn't
    // happen but keeps things robust)
    int remainW = (barW - 2) - fillW;
    if (remainW > 0) {
        lcd.fillRect(barX + 1 + fillW, barY + 1, remainW, barH - 2, COL_BG);
    }

    // Percentage text
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", percent);
    drawCenteredText(CENTER_X, barY + barH + 25, pctBuf,
                     &fonts::Font4, 1.0f, COL_WHITE, COL_BG);

    lcd.endWrite();

    // Reset flag when complete so next OTA starts fresh
    if (percent >= 100) {
        otaScreenInitialized = false;
    }
}
