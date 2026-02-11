#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// Web Server - HTTP API + Embedded Web UI
// ============================================================
//
// Synchronous WebServer.h (NOT ESPAsyncWebServer) to avoid
// crash bugs with WiFi.scanNetworks() during active requests.
//
// Provides:
// - Status/settings API (JSON via ArduinoJson v7)
// - WiFi configuration portal
// - OTA firmware upload
// - Captive portal detection endpoints
// - Embedded single-page dark-theme web UI

void webServerInit();       // Register all routes, start server
void webServerUpdate();     // Call in main loop (handle clients)
