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
static const uint16_t COL_BG        = 0x0013;   // ~#1a1a2e dark navy
static const uint16_t COL_WHITE     = 0xFFFF;
static const uint16_t COL_CYAN      = 0x07FF;
static const uint16_t COL_GREEN     = 0x07E0;
static const uint16_t COL_RED       = 0xF800;
static const uint16_t COL_GREY      = 0x7BEF;
static const uint16_t COL_DARK_GREY = 0x3186;

// --- Layout constants ---
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

// --- Differential rendering state ---
struct PreviousDisplayState {
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

static LGFX lcd;
static PreviousDisplayState prev;

// --- Internal helpers ---

static void clearPrevState() {
    memset(&prev, 0, sizeof(prev));
    prev.initialized = false;
    prev.temperature = -999.0f;
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

    clearPrevState();

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

// --- Clock screen (differential) ---

void displayRenderClock(const char* timeStr, const char* dateStr, const WeatherData* weather) {
    lcd.startWrite();

    // Full redraw on first call
    if (!prev.initialized) {
        lcd.fillScreen(COL_BG);
        prev.initialized = true;

        // Draw static divider line
        lcd.drawFastHLine(40, DIVIDER_Y, DISPLAY_WIDTH - 80, COL_DARK_GREY);
    }

    // --- Time (large) ---
    if (strcmp(timeStr, prev.time) != 0) {
        drawCenteredText(CENTER_X, TIME_Y, timeStr,
                         &fonts::Font7, 1.0f, COL_WHITE, COL_BG);
        strncpy(prev.time, timeStr, sizeof(prev.time) - 1);
        prev.time[sizeof(prev.time) - 1] = '\0';
    }

    // --- Date ---
    if (strcmp(dateStr, prev.date) != 0) {
        drawCenteredText(CENTER_X, DATE_Y, dateStr,
                         &fonts::Font2, 1.0f, COL_GREY, COL_BG);
        strncpy(prev.date, dateStr, sizeof(prev.date) - 1);
        prev.date[sizeof(prev.date) - 1] = '\0';
    }

    // --- Weather (bottom half) ---
    if (weather && weather->valid) {
        // Weather description (derived from icon enum)
        const char* desc = weatherIconName(weather->icon);
        if (strcmp(desc, prev.weatherDesc) != 0) {
            drawCenteredText(CENTER_X, WEATHER_Y, desc,
                             &fonts::Font2, 1.0f, COL_WHITE, COL_BG);
            strncpy(prev.weatherDesc, desc, sizeof(prev.weatherDesc) - 1);
            prev.weatherDesc[sizeof(prev.weatherDesc) - 1] = '\0';
        }

        // Temperature (use epsilon to avoid float equality issues)
        if (fabsf(weather->temperature - prev.temperature) > 0.05f) {
            char tempBuf[16];
            snprintf(tempBuf, sizeof(tempBuf), "%.0f%s",
                     weather->temperature,
                     TEMP_UNIT_FAHRENHEIT ? "F" : "C");
            drawCenteredText(CENTER_X, TEMP_Y, tempBuf,
                             &fonts::Font4, 1.0f, COL_CYAN, COL_BG);
            prev.temperature = weather->temperature;
        }

        prev.weatherValid = true;

    } else if (prev.weatherValid || !prev.initialized) {
        // Weather became invalid or first draw with no weather
        drawCenteredText(CENTER_X, WEATHER_Y, "No weather data",
                         &fonts::Font2, 1.0f, COL_DARK_GREY, COL_BG);

        // Clear temperature area (Font4 is ~26px tall, use 30px to be safe)
        lcd.fillRect(0, TEMP_Y - 15, DISPLAY_WIDTH, 30, COL_BG);

        prev.weatherValid = false;
        prev.temperature = -999.0f;
        memset(prev.weatherDesc, 0, sizeof(prev.weatherDesc));
    }

    // --- WiFi status dot (top-right) ---
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    if (wifiUp != prev.wifiConnected) {
        uint16_t dotColor = wifiUp ? COL_GREEN : COL_RED;
        lcd.fillCircle(WIFI_DOT_X, WIFI_DOT_Y, WIFI_DOT_R, dotColor);
        prev.wifiConnected = wifiUp;
    }

    // --- IP address (top, small, only when connected) ---
    if (wifiUp) {
        String ipStr = WiFi.localIP().toString();
        if (strcmp(ipStr.c_str(), prev.ip) != 0) {
            // Clear previous IP area
            lcd.fillRect(0, 0, WIFI_DOT_X - WIFI_DOT_R - 4, 14, COL_BG);
            lcd.setFont(&fonts::Font0);
            lcd.setTextSize(1.0f);
            lcd.setTextColor(COL_DARK_GREY, COL_BG);
            lcd.setTextDatum(lgfx::top_left);
            lcd.drawString(ipStr.c_str(), 4, IP_Y);
            strncpy(prev.ip, ipStr.c_str(), sizeof(prev.ip) - 1);
            prev.ip[sizeof(prev.ip) - 1] = '\0';
        }
    } else if (prev.ip[0] != '\0') {
        // WiFi dropped, clear IP
        lcd.fillRect(0, 0, WIFI_DOT_X - WIFI_DOT_R - 4, 14, COL_BG);
        prev.ip[0] = '\0';
    }

    lcd.endWrite();
}

// --- AP Mode screen ---

void displayRenderAPMode(const char* ssid, const char* password, const char* ip) {
    clearPrevState();

    lcd.startWrite();
    lcd.fillScreen(COL_BG);

    // Title
    drawCenteredText(CENTER_X, 30, "WiFi Setup",
                     &fonts::Font4, 1.0f, COL_CYAN, COL_BG);

    // Divider
    lcd.drawFastHLine(30, 52, DISPLAY_WIDTH - 60, COL_DARK_GREY);

    // Instructions
    drawCenteredText(CENTER_X, 72, "Connect to WiFi:",
                     &fonts::Font2, 1.0f, COL_GREY, COL_BG);

    // SSID
    drawCenteredText(CENTER_X, 100, ssid,
                     &fonts::Font2, 1.0f, COL_WHITE, COL_BG);

    // Password label
    drawCenteredText(CENTER_X, 130, "Password:",
                     &fonts::Font2, 1.0f, COL_GREY, COL_BG);

    // Password value
    drawCenteredText(CENTER_X, 150, password,
                     &fonts::Font2, 1.0f, COL_WHITE, COL_BG);

    // Divider
    lcd.drawFastHLine(30, 175, DISPLAY_WIDTH - 60, COL_DARK_GREY);

    // IP address
    drawCenteredText(CENTER_X, 195, "Then open:",
                     &fonts::Font2, 1.0f, COL_GREY, COL_BG);

    char urlBuf[32];
    snprintf(urlBuf, sizeof(urlBuf), "http://%s", ip);
    drawCenteredText(CENTER_X, 215, urlBuf,
                     &fonts::Font2, 1.0f, COL_CYAN, COL_BG);

    lcd.endWrite();

    logPrintf("Rendered AP mode screen (SSID: %s)", ssid);
}

// --- Full-screen message ---

void displayRenderMessage(const char* msg) {
    clearPrevState();

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
    static bool otaScreenInitialized = false;
    if (!otaScreenInitialized || percent == 0) {
        clearPrevState();
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
