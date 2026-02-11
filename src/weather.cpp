#include "weather.h"
#include "settings.h"
#include "logger.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- Module state ---

static WeatherData currentWeather;
static unsigned long lastFetchAttempt = 0;

// --- WMO weather code to icon mapping ---
// Reference: https://open-meteo.com/en/docs (WMO Weather interpretation codes)
//
//  0        = Clear sky
//  1, 2, 3  = Mainly clear, Partly cloudy, Overcast
//  45, 48   = Fog, Depositing rime fog
//  51, 53, 55 = Drizzle (light, moderate, dense)
//  56, 57   = Freezing drizzle (light, dense)
//  61, 63, 65 = Rain (slight, moderate, heavy)
//  66, 67   = Freezing rain (light, heavy)
//  71, 73, 75 = Snow fall (slight, moderate, heavy)
//  77       = Snow grains
//  80, 81, 82 = Rain showers (slight, moderate, violent)
//  85, 86   = Snow showers (slight, heavy)
//  95       = Thunderstorm (slight or moderate)
//  96, 99   = Thunderstorm with hail (slight, heavy)

WeatherIcon weatherCodeToIcon(int code, bool isDay) {
    switch (code) {
        case 0:
            return isDay ? ICON_CLEAR_DAY : ICON_CLEAR_NIGHT;

        case 1:
        case 2:
            return ICON_PARTLY_CLOUDY;

        case 3:
            return ICON_CLOUDY;

        case 45:
        case 48:
            return ICON_FOG;

        case 51:
        case 53:
        case 55:
        case 56:
        case 57:
            return ICON_DRIZZLE;

        case 61:
        case 63:
        case 65:
        case 66:
        case 67:
        case 80:
        case 81:
        case 82:
            return ICON_RAIN;

        case 71:
        case 73:
        case 75:
        case 77:
        case 85:
        case 86:
            return ICON_SNOW;

        case 95:
        case 96:
        case 99:
            return ICON_THUNDERSTORM;

        default:
            return ICON_UNKNOWN;
    }
}

const char* weatherIconName(WeatherIcon icon) {
    switch (icon) {
        case ICON_CLEAR_DAY:     return "Clear";
        case ICON_CLEAR_NIGHT:   return "Clear Night";
        case ICON_PARTLY_CLOUDY: return "Partly Cloudy";
        case ICON_CLOUDY:        return "Cloudy";
        case ICON_FOG:           return "Fog";
        case ICON_DRIZZLE:       return "Drizzle";
        case ICON_RAIN:          return "Rain";
        case ICON_SNOW:          return "Snow";
        case ICON_THUNDERSTORM:  return "Thunderstorm";
        case ICON_UNKNOWN:       return "Unknown";
        default:                 return "Unknown";
    }
}

// --- HTTP fetch ---

static bool fetchWeather() {
    Settings& settings = settingsGet();

    // Skip if location is not configured
    if (settings.latitude == 0.0f && settings.longitude == 0.0f) {
        logPrintf("[WEATHER] Skipping fetch: location not configured (lat/lon both 0)");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        logPrintf("[WEATHER] Skipping fetch: WiFi not connected");
        return false;
    }

    // Build URL with temperature unit from settings
    const char* tempUnit = settings.tempFahrenheit ? "fahrenheit" : "celsius";
    char url[256];
    snprintf(url, sizeof(url),
             "%s?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code,is_day"
             "&temperature_unit=%s&timezone=auto",
             WEATHER_API_BASE,
             settings.latitude,
             settings.longitude,
             tempUnit);

    logPrintf("[WEATHER] Fetching: lat=%.4f, lon=%.4f, unit=%s",
              settings.latitude, settings.longitude, tempUnit);

    WiFiClientSecure client;
    client.setInsecure();  // Skip cert verification (still TLS-encrypted)

    HTTPClient http;
    http.setTimeout(WEATHER_TIMEOUT_MS);

    if (!http.begin(client, url)) {
        logPrintf("[WEATHER] HTTPClient.begin() failed");
        return false;
    }

    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        logPrintf("[WEATHER] HTTP error: %d", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    // Parse JSON response
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        logPrintf("[WEATHER] JSON parse error: %s", err.c_str());
        return false;
    }

    // Extract current weather values
    JsonObject current = doc["current"];
    if (current.isNull()) {
        logPrintf("[WEATHER] No 'current' object in response");
        return false;
    }

    float temp    = current["temperature_2m"] | 0.0f;
    int   wmoCode = current["weather_code"]   | -1;
    int   isDayInt = current["is_day"]        | 1;
    bool  isDay   = (isDayInt != 0);

    if (wmoCode < 0) {
        logPrintf("[WEATHER] Invalid weather_code in response");
        return false;
    }

    // Update state
    currentWeather.temperature  = temp;
    currentWeather.weatherCode  = wmoCode;
    currentWeather.isDay        = isDay;
    currentWeather.icon         = weatherCodeToIcon(wmoCode, isDay);
    currentWeather.valid        = true;
    currentWeather.lastFetchMs  = millis();

    logPrintf("[WEATHER] Updated: %.1f%s, code=%d (%s), %s",
              temp,
              settings.tempFahrenheit ? "F" : "C",
              wmoCode,
              weatherIconName(currentWeather.icon),
              isDay ? "day" : "night");

    return true;
}

// --- Public API ---

void weatherInit() {
    memset(&currentWeather, 0, sizeof(currentWeather));
    currentWeather.valid       = false;
    currentWeather.icon        = ICON_UNKNOWN;
    currentWeather.lastFetchMs = 0;

    // Set initial fetch time so the first fetch happens 10 seconds after boot
    // instead of immediately (which would block the main loop for up to 10s)
    lastFetchAttempt = millis() - WEATHER_FETCH_INTERVAL + 10000;

    logPrintf("[WEATHER] Weather client initialized (first fetch in ~10s)");
}

void weatherUpdate() {
    unsigned long now = millis();

    // First fetch: try immediately once WiFi is available
    // Subsequent fetches: respect the interval
    if (lastFetchAttempt == 0 ||
        (now - lastFetchAttempt) >= WEATHER_FETCH_INTERVAL) {
        lastFetchAttempt = now;
        fetchWeather();
    }
}

void weatherFetchNow() {
    logPrintf("[WEATHER] Forced fetch requested");
    lastFetchAttempt = millis();
    fetchWeather();
}

const WeatherData& weatherGet() {
    return currentWeather;
}
