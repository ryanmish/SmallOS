#pragma once

// ============================================================
// SmallTV Firmware Configuration
// ============================================================

// --- Firmware ---
#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// --- Serial ---
#define SERIAL_BAUD 115200

// --- Display ---
// Pin definitions come from platformio.ini build flags:
// TFT_SCK, TFT_MOSI, TFT_DC, TFT_RST, TFT_BL, DISPLAY_WIDTH, DISPLAY_HEIGHT
#define DISPLAY_UPDATE_MS       1000    // Clock refresh interval
#define BRIGHTNESS_DEFAULT      25      // 0-100, low default (cheap panel blows out at high)
#define BRIGHTNESS_DIM          5       // Dim mode brightness
#define SCREEN_DIM_MS           60000   // Dim after 1 minute of no touch

// --- Touch ---
// TOUCH_PIN comes from platformio.ini (T9 = GPIO32)
#define TOUCH_SAMPLES           8       // Readings averaged per poll
#define TOUCH_BASELINE_SAMPLES  16      // Readings for initial calibration
#define TOUCH_DEBOUNCE_MS       50
#define TOUCH_LONG_PRESS_MS     2000
#define TOUCH_DOUBLE_TAP_MS     300
#define TOUCH_THRESHOLD_PCT     85      // Touch detected when reading drops below N% of baseline

// --- WiFi ---
#define WIFI_AP_SSID_PREFIX     "SmallTV-"
#define WIFI_AP_PASSWORD        "smalltv123"
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RETRY_ATTEMPTS     3
#define WIFI_RETRY_DELAY_MS     2000
#define WIFI_MONITOR_INTERVAL   60000   // Check WiFi health every 60s
#define WIFI_RECONNECT_INTERVAL 300000  // Retry saved creds every 5 min in AP mode
#define WIFI_CAPTIVE_TIMEOUT_S  180     // Captive portal timeout

// --- Web Server ---
#define WEB_SERVER_PORT         80
#define DNS_PORT                53

// --- mDNS ---
#define MDNS_HOSTNAME_PREFIX    "smalltv"   // becomes smalltv-XXXX.local

// --- Weather ---
#define WEATHER_FETCH_INTERVAL  900000  // 15 minutes (matches API update cadence)
#define WEATHER_API_BASE        "https://api.open-meteo.com/v1/forecast"
#define WEATHER_TIMEOUT_MS      10000
#define WEATHER_DEFAULT_LAT     0.0
#define WEATHER_DEFAULT_LON     0.0
#define TEMP_UNIT_FAHRENHEIT    true    // Default to Fahrenheit

// --- OTA ---
#define OTA_CONFIRM_TIMEOUT_MS  300000  // 5 minutes to call /confirm-good
#define OTA_PASSWORD            "smalltv"

// --- Settings (NVS) ---
#define NVS_NAMESPACE           "smalltv"
#define SETTINGS_VERSION        2       // Increment when settings struct changes

// --- Logger ---
#define LOG_BUFFER_SIZE         30      // Number of log lines
#define LOG_LINE_LENGTH         128     // Max chars per line

// --- Boot Safety ---
#define BOOT_FAIL_THRESHOLD     5       // Emergency reset after N consecutive failures
#define POWER_CYCLE_ROLLBACK    3       // OTA rollback after N quick power cycles (when pending)
#define POWER_CYCLE_THRESHOLD   5       // Factory reset after N quick power cycles
#define POWER_CYCLE_WINDOW_MS   10000   // Must stay up this long to reset power cycle counter
