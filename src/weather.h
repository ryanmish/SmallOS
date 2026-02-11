#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// Weather Client - Open-Meteo API
// Fetches current conditions every WEATHER_FETCH_INTERVAL ms.
// Uses WMO weather interpretation codes (0-99).
// ============================================================

enum WeatherIcon {
    ICON_CLEAR_DAY,
    ICON_CLEAR_NIGHT,
    ICON_PARTLY_CLOUDY,
    ICON_CLOUDY,
    ICON_FOG,
    ICON_DRIZZLE,
    ICON_RAIN,
    ICON_SNOW,
    ICON_THUNDERSTORM,
    ICON_UNKNOWN
};

struct WeatherData {
    float         temperature;
    int           weatherCode;      // WMO interpretation code
    WeatherIcon   icon;
    bool          isDay;
    bool          valid;            // false if fetch failed or never fetched
    unsigned long lastFetchMs;
};

void               weatherInit();
void               weatherUpdate();                         // Call in main loop (fetches every 15 min)
void               weatherFetchNow();                       // Force immediate fetch
const WeatherData& weatherGet();
const char*        weatherIconName(WeatherIcon icon);       // "Clear", "Cloudy", etc.
WeatherIcon        weatherCodeToIcon(int code, bool isDay);
