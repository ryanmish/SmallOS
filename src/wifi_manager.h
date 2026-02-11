#pragma once

#include <Arduino.h>
#include "config.h"

// ============================================================
// WiFi Manager - Three-tier connection management
// ============================================================
//
// Tier 1: Try saved credentials from NVS
// Tier 2: Fall back to AP mode with captive portal
// Runtime: Monitor connection health, auto-reconnect
//
// Uses scan-then-serve pattern: WiFi networks are scanned
// BEFORE starting AP mode to avoid the crash bug where
// WiFi.scanNetworks() conflicts with active web server handlers.

void    wifiInit();             // Start WiFi (try saved creds, then AP)
void    wifiUpdate();           // Call in main loop (monitors connection, retries)
bool    wifiIsConnected();      // STA connected?
bool    wifiIsAPMode();         // Running as AP?
String  wifiGetIP();            // Current IP (STA or AP)
String  wifiGetSSID();          // Connected SSID or AP SSID
String  wifiGetMAC();
String  wifiGetDeviceId();      // Last 4 hex of MAC
int     wifiGetRSSI();
void    wifiStartAP();          // Force AP mode
void    wifiSaveCredentials(const String& ssid, const String& password);
void    wifiFactoryReset();     // Clear WiFi creds + reboot

// --- Scan results (used by web server) ---

struct WifiNetwork {
    String ssid;
    int    rssi;
    bool   encrypted;
};

int              wifiGetScanCount();
WifiNetwork      wifiGetScanResult(int index);
void             wifiScanNetworks();    // Request a deferred scan (safe to call from HTTP handler)
bool             wifiIsScanInProgress();
