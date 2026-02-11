#include "wifi_manager.h"
#include "touch.h"
#include "logger.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>

// --- NVS keys ---
static const char* WIFI_NVS_NAMESPACE = "wifi";
static const char* KEY_SSID           = "ssid";
static const char* KEY_PASSWORD       = "password";

// --- Module state ---
static bool        apMode         = false;
static String      apSSID;
static String      deviceId;
static unsigned long lastMonitorCheck   = 0;
static unsigned long lastReconnectTry   = 0;
static int         quickReconnectCount  = 0;
static DNSServer   dnsServer;
static bool        dnsRunning     = false;

// --- Cached scan results ---
static const int   MAX_SCAN_RESULTS = 20;
static WifiNetwork scanResults[MAX_SCAN_RESULTS];
static int         scanCount = 0;
static bool        _scanRequested = false;
static bool        _scanInProgress = false;

// --- Forward declarations ---
static bool     tryConnect(const String& ssid, const String& password);
static void     saveCreds(const String& ssid, const String& password);
static bool     loadCreds(String& ssid, String& password);
static void     buildDeviceId();
static void     scanAndCache();
static void     startAPMode();
static void     stopAP();

// ============================================================
// Internal helpers
// ============================================================

static void buildDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[5];
    snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
    deviceId = String(buf);
}

static bool loadCreds(String& ssid, String& password) {
    Preferences p;
    p.begin(WIFI_NVS_NAMESPACE, true);  // read-only
    ssid     = p.getString(KEY_SSID, "");
    password = p.getString(KEY_PASSWORD, "");
    p.end();
    return ssid.length() > 0;
}

static void saveCreds(const String& ssid, const String& password) {
    Preferences p;
    p.begin(WIFI_NVS_NAMESPACE, false);
    p.putString(KEY_SSID, ssid);
    p.putString(KEY_PASSWORD, password);
    p.end();
    logPrintf("WiFi credentials saved for '%s'", ssid.c_str());
}

static bool tryConnect(const String& ssid, const String& password) {
    logPrintf("WiFi: connecting to '%s'", ssid.c_str());

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    unsigned long backoff = WIFI_RETRY_DELAY_MS;

    for (int attempt = 1; attempt <= WIFI_RETRY_ATTEMPTS; attempt++) {
        logPrintf("WiFi: attempt %d/%d", attempt, WIFI_RETRY_ATTEMPTS);

        WiFi.begin(ssid.c_str(), password.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED &&
               (millis() - start) < (unsigned long)WIFI_CONNECT_TIMEOUT_MS) {
            delay(250);
        }

        if (WiFi.status() == WL_CONNECTED) {
            logPrintf("WiFi: connected to '%s' - IP: %s",
                      ssid.c_str(), WiFi.localIP().toString().c_str());
            return true;
        }

        logPrintf("WiFi: attempt %d failed (status=%d)", attempt, WiFi.status());
        WiFi.disconnect(true);

        if (attempt < WIFI_RETRY_ATTEMPTS) {
            logPrintf("WiFi: backoff %lums before next attempt", backoff);
            delay(backoff);
            backoff *= 2;  // Exponential backoff: 2s, 4s, 8s
        }
    }

    logPrintf("WiFi: all %d attempts failed for '%s'", WIFI_RETRY_ATTEMPTS, ssid.c_str());
    return false;
}

static void scanAndCache() {
    logPrintf("WiFi: scanning networks...");

    // Pause touch during WiFi scan - they share the ADC hardware
    touchPauseForWiFi();

    // Ensure we're in STA mode for scanning (or STA+AP)
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
    }

    int found = WiFi.scanNetworks(false, false);  // Synchronous, no hidden
    scanCount = 0;

    if (found <= 0) {
        logPrintf("WiFi: scan found no networks (result=%d)", found);
        return;
    }

    scanCount = min(found, MAX_SCAN_RESULTS);
    logPrintf("WiFi: scan found %d networks (caching %d)", found, scanCount);

    for (int i = 0; i < scanCount; i++) {
        scanResults[i].ssid      = WiFi.SSID(i);
        scanResults[i].rssi      = WiFi.RSSI(i);
        scanResults[i].encrypted = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    WiFi.scanDelete();  // Free scan memory

    // Resume touch now that WiFi scan is done
    touchResumeAfterWiFi();
}

static void startAPMode() {
    apSSID = String(WIFI_AP_SSID_PREFIX) + deviceId;
    apMode = true;

    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str());

    // Small delay for AP to stabilize
    delay(100);

    // Start DNS server for captive portal (redirect all domains to us)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    dnsRunning = true;

    logPrintf("WiFi: AP mode started - SSID: %s, IP: %s",
              apSSID.c_str(), WiFi.softAPIP().toString().c_str());

    lastReconnectTry = millis();
}

static void stopAP() {
    if (dnsRunning) {
        dnsServer.stop();
        dnsRunning = false;
    }
    WiFi.softAPdisconnect(true);
    apMode = false;
    logPrintf("WiFi: AP mode stopped");
}

// ============================================================
// Public API
// ============================================================

void wifiInit() {
    logPrintf("WiFi: initializing");

    buildDeviceId();
    logPrintf("WiFi: device ID = %s, MAC = %s",
              deviceId.c_str(), WiFi.macAddress().c_str());

    // Tier 1: Try saved credentials
    String ssid, password;
    if (loadCreds(ssid, password)) {
        logPrintf("WiFi: found saved credentials for '%s'", ssid.c_str());
        if (tryConnect(ssid, password)) {
            lastMonitorCheck = millis();
            return;
        }
    } else {
        logPrintf("WiFi: no saved credentials found");
    }

    // Tier 2: Scan networks first, then start AP
    scanAndCache();
    startAPMode();
}

void wifiUpdate() {
    // Process DNS requests when in AP mode
    if (dnsRunning) {
        dnsServer.processNextRequest();
    }

    // Handle deferred scan request (triggered by web UI, executed here safely)
    if (_scanRequested && !_scanInProgress) {
        _scanRequested = false;
        _scanInProgress = true;
        scanAndCache();
        _scanInProgress = false;
    }

    unsigned long now = millis();

    if (apMode) {
        // In AP mode: periodically try to reconnect using saved creds
        if ((now - lastReconnectTry) >= WIFI_RECONNECT_INTERVAL) {
            lastReconnectTry = now;

            String ssid, password;
            if (loadCreds(ssid, password)) {
                logPrintf("WiFi: periodic reconnect attempt to '%s'", ssid.c_str());

                // Briefly stop AP, try STA, restart AP if it fails
                stopAP();

                if (tryConnect(ssid, password)) {
                    logPrintf("WiFi: reconnected to saved network");
                    lastMonitorCheck = now;
                    quickReconnectCount = 0;
                    return;
                }

                // Failed; scan again and restart AP
                scanAndCache();
                startAPMode();
            }
        }
        return;
    }

    // In STA mode: monitor connection health
    if ((now - lastMonitorCheck) >= WIFI_MONITOR_INTERVAL) {
        lastMonitorCheck = now;

        if (WiFi.status() != WL_CONNECTED) {
            logPrintf("WiFi: connection lost, attempting quick reconnect");

            quickReconnectCount++;

            if (quickReconnectCount <= 3) {
                // Quick reconnect: just try again with saved creds
                String ssid, password;
                if (loadCreds(ssid, password)) {
                    WiFi.disconnect(true);
                    WiFi.begin(ssid.c_str(), password.c_str());

                    unsigned long start = millis();
                    while (WiFi.status() != WL_CONNECTED &&
                           (millis() - start) < 10000) {
                        delay(250);
                    }

                    if (WiFi.status() == WL_CONNECTED) {
                        logPrintf("WiFi: quick reconnect #%d succeeded", quickReconnectCount);
                        quickReconnectCount = 0;
                        return;
                    }
                }
                logPrintf("WiFi: quick reconnect #%d failed", quickReconnectCount);
            } else {
                // All quick reconnects failed; fall back to AP mode
                logPrintf("WiFi: quick reconnects exhausted, falling back to AP");
                quickReconnectCount = 0;
                scanAndCache();
                startAPMode();
            }
        } else {
            quickReconnectCount = 0;  // Reset counter on healthy check
        }
    }
}

bool wifiIsConnected() {
    return (!apMode && WiFi.status() == WL_CONNECTED);
}

bool wifiIsAPMode() {
    return apMode;
}

String wifiGetIP() {
    if (apMode) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

String wifiGetSSID() {
    if (apMode) {
        return apSSID;
    }
    return WiFi.SSID();
}

String wifiGetMAC() {
    return WiFi.macAddress();
}

String wifiGetDeviceId() {
    return deviceId;
}

int wifiGetRSSI() {
    if (apMode) {
        return 0;
    }
    return WiFi.RSSI();
}

void wifiStartAP() {
    logPrintf("WiFi: forced AP mode requested");
    if (!apMode) {
        WiFi.disconnect(true);
        scanAndCache();
        startAPMode();
    }
}

void wifiSaveCredentials(const String& ssid, const String& password) {
    saveCreds(ssid, password);
}

void wifiFactoryReset() {
    logPrintf("WiFi: factory reset - clearing credentials");
    Preferences p;
    p.begin(WIFI_NVS_NAMESPACE, false);
    p.clear();
    p.end();
    delay(500);
    ESP.restart();
}

// --- Scan results API ---

int wifiGetScanCount() {
    return scanCount;
}

WifiNetwork wifiGetScanResult(int index) {
    if (index >= 0 && index < scanCount) {
        return scanResults[index];
    }
    return {"", 0, false};
}

void wifiScanNetworks() {
    _scanRequested = true;
}

bool wifiIsScanInProgress() {
    return _scanInProgress || _scanRequested;
}
