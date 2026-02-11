#include "web_server.h"
#include "wifi_manager.h"
#include "settings.h"
#include "logger.h"
#include "display.h"
#include "weather.h"
#include "ota.h"

#include <WebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "index_html_gz.h"

// --- Module state ---
static WebServer server(WEB_SERVER_PORT);

// --- Forward declarations ---
static void handleRoot();
static void handleStatus();
static void handleSet();
static void handleWeather();
static void handleScan();
static void handleConnect();
static void handleGetLocation();
static void handleSetLocation();
static void handleOTAPage();
static void handleConfirmGood();
static void handleRollback();
static void handleReset();
static void handleLog();
static void handleCaptiveRedirect();
static void handleNotFound();
static void addCorsHeaders();

// ============================================================
// Embedded Web UI (gzip-compressed, included from index_html_gz.h)
// Source: web-ui/index.html
// ============================================================


// ============================================================
// OTA upload page
// ============================================================

static const char OTA_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmallTV OTA</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',system-ui,sans-serif;background:#f2f1ed;color:#3a3428;padding:20px;max-width:460px;margin:0 auto;font-size:16px;line-height:1.5;opacity:0;animation:fadeIn .3s ease forwards;-webkit-font-smoothing:antialiased}
@keyframes fadeIn{to{opacity:1}}
h2{font-size:.75em;margin:0 0 14px;color:#a59d8e;text-transform:uppercase;letter-spacing:1.6px;font-weight:700;padding-bottom:10px;border-bottom:1px solid rgba(90,80,65,.1)}
.card{background:#faf9f7;border:1px solid rgba(90,80,65,.1);border-radius:16px;padding:20px;margin-bottom:16px;box-shadow:0 1px 3px rgba(100,90,70,.06),0 4px 12px rgba(100,90,70,.04)}
a.back{display:inline-block;color:#7e9462;text-decoration:none;font-size:.95em;font-weight:500;margin-bottom:14px;transition:opacity .15s}
a.back:hover{opacity:.7}
.file-label{display:flex;align-items:center;justify-content:center;padding:16px;background:#f5f4f1;border:2px dashed rgba(90,80,65,.15);border-radius:11px;cursor:pointer;color:#a59d8e;font-size:.95em;transition:border-color .2s,color .2s;margin-bottom:14px;min-height:44px}
.file-label:hover{border-color:#7e9462;color:#7e9462}
.file-label.has-file{border-color:#7e9462;color:#3a3428;border-style:solid}
input[type=file]{display:none}
button{padding:12px 24px;background:#7e9462;color:#fff;border:none;border-radius:11px;cursor:pointer;font-size:.95em;font-weight:600;min-height:44px;transition:all .15s ease;width:100%;box-shadow:0 2px 6px rgba(126,148,98,.2)}
button:hover{background:#6e8354;box-shadow:0 3px 10px rgba(126,148,98,.28)}
button:active{transform:scale(.97)}
button:disabled{background:#efede8;color:#a59d8e;cursor:default;transform:none;box-shadow:none}
.warn-box{background:rgba(180,140,40,.06);border:1px solid rgba(180,140,40,.2);border-radius:11px;padding:12px;margin-bottom:14px;color:#9a7a20;font-size:.88em;text-align:center}
.progress-wrap{margin-top:14px;display:none}
.progress-bar{height:6px;background:#efede8;border-radius:3px;overflow:hidden;border:1px solid rgba(90,80,65,.1)}
.progress-fill{height:100%;width:0;background:#7e9462;border-radius:3px;transition:width .2s ease}
#prog{margin-top:8px;font-size:.88em;color:#78705f;text-align:center}
.done{color:#4a7a3a!important}
.fail{color:#bf5f55!important}
.spinner{display:inline-block;width:14px;height:14px;border:2px solid #efede8;border-top-color:#4a7a3a;border-radius:50%;animation:spin 1.5s linear infinite;vertical-align:-2px;margin-left:6px}
@keyframes spin{to{transform:rotate(360deg)}}
</style>
</head><body>
<a class="back" href="/">&larr; Back</a>
<div class="card">
<h2>Firmware Update</h2>
<form method="POST" action="/ota" enctype="multipart/form-data" id="uf">
<label class="file-label" id="fl" onclick="document.getElementById('fi').click()">Choose .bin file</label>
<input type="file" name="update" id="fi" accept=".bin" required>
<div class="warn-box">Do not power off the device during upload.</div>
<button type="submit" id="ubtn">Upload Firmware</button>
</form>
<div class="progress-wrap" id="pw">
<div class="progress-bar"><div class="progress-fill" id="pf"></div></div>
<div id="prog"></div>
</div>
</div>
<script>
document.getElementById('fi').addEventListener('change',function(){var fl=document.getElementById('fl');if(this.files.length){fl.textContent=this.files[0].name;fl.classList.add('has-file')}else{fl.textContent='Choose .bin file';fl.classList.remove('has-file')}});
document.getElementById('uf').addEventListener('submit',function(e){
e.preventDefault();
var fd=new FormData(this);
var xhr=new XMLHttpRequest();
var pw=document.getElementById('pw');
var pf=document.getElementById('pf');
var prog=document.getElementById('prog');
var ubtn=document.getElementById('ubtn');
pw.style.display='block';ubtn.disabled=true;ubtn.textContent='Uploading...';
xhr.open('POST','/ota');
xhr.upload.onprogress=function(e){if(e.lengthComputable){var pct=Math.round(e.loaded/e.total*100);pf.style.width=pct+'%';prog.textContent=pct+'%'}};
xhr.onload=function(){if(xhr.status==200){pf.style.width='100%';prog.className='done';ubtn.textContent='Complete';var sec=10;prog.innerHTML='Rebooting\u2026 '+sec+'s <span class="spinner"></span>';var ci=setInterval(function(){sec--;if(sec<=0){clearInterval(ci);prog.innerHTML='Redirecting\u2026';window.location.href='/'}else{prog.innerHTML='Rebooting\u2026 '+sec+'s <span class="spinner"></span>'}},1000)}else{prog.textContent='Upload failed: '+xhr.responseText;prog.className='fail';ubtn.disabled=false;ubtn.textContent='Retry'}};
xhr.onerror=function(){prog.textContent='Upload failed';prog.className='fail';ubtn.disabled=false;ubtn.textContent='Retry'};
xhr.send(fd)})
</script>
</body></html>)rawliteral";

// ============================================================
// CORS helper
// ============================================================

static void addCorsHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// ============================================================
// Route handlers
// ============================================================

static void handleRoot() {
    addCorsHeaders();
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "text/html", (const char*)INDEX_HTML_GZ, INDEX_HTML_GZ_LEN);
}

static void handleStatus() {
    addCorsHeaders();

    Settings& s = settingsGet();
    JsonDocument doc;

    doc["version"]    = FW_VERSION;
    doc["ssid"]       = wifiGetSSID();
    doc["ip"]         = wifiGetIP();
    doc["mac"]        = wifiGetMAC();
    doc["rssi"]       = wifiGetRSSI();
    doc["ap_mode"]    = wifiIsAPMode();
    doc["connected"]  = wifiIsConnected();
    doc["heap"]       = ESP.getFreeHeap();
    doc["uptime"]     = millis() / 1000;
    doc["brightness"] = s.brightness;
    doc["temp_f"]     = s.tempFahrenheit;
    doc["gmt_offset"] = s.gmtOffsetSec;
    doc["lat"]        = s.latitude;
    doc["lon"]        = s.longitude;
    doc["ota_confirmed"] = otaIsConfirmed();

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

static void handleSet() {
    addCorsHeaders();

    Settings& s = settingsGet();
    bool changed = false;

    if (server.hasArg("brt")) {
        int brt = server.arg("brt").toInt();
        brt = constrain(brt, 0, 100);
        s.brightness = (uint8_t)brt;
        displaySetBrightness(s.brightness);
        logPrintf("Web: brightness set to %d", s.brightness);
        changed = true;
    }

    if (server.hasArg("gmt")) {
        s.gmtOffsetSec = server.arg("gmt").toInt();
        logPrintf("Web: GMT offset set to %ld", s.gmtOffsetSec);
        changed = true;
    }

    if (server.hasArg("tempF")) {
        s.tempFahrenheit = (server.arg("tempF") == "1");
        logPrintf("Web: temp unit set to %s", s.tempFahrenheit ? "F" : "C");
        changed = true;
    }

    if (changed) {
        settingsSave();
    }

    JsonDocument doc;
    doc["success"] = true;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

static void handleWeather() {
    addCorsHeaders();

    const WeatherData& w = weatherGet();
    JsonDocument doc;

    doc["valid"]       = w.valid;
    doc["temperature"] = w.temperature;
    doc["code"]        = w.weatherCode;
    doc["icon"]        = weatherIconName(w.icon);
    doc["is_day"]      = w.isDay;
    doc["last_fetch"]  = w.lastFetchMs / 1000;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

static void handleScan() {
    addCorsHeaders();

    // If scan requested via ?start=1, trigger a deferred scan (runs in main loop)
    if (server.hasArg("start")) {
        wifiScanNetworks();
        server.send(200, "application/json", "{\"scanning\":true}");
        return;
    }

    // Return cached scan results and scanning status
    JsonDocument doc;
    doc["scanning"] = wifiIsScanInProgress();
    JsonArray networks = doc["networks"].to<JsonArray>();

    for (int i = 0; i < wifiGetScanCount(); i++) {
        WifiNetwork net = wifiGetScanResult(i);
        JsonObject obj = networks.add<JsonObject>();
        obj["ssid"] = net.ssid;
        obj["rssi"] = net.rssi;
        obj["enc"]  = net.encrypted;
    }

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

static void handleConnect() {
    addCorsHeaders();

    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"No body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));

    if (err) {
        logPrintf("Web: JSON parse error: %s", err.c_str());
        server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }

    String ssid     = doc["ssid"] | "";
    String password = doc["password"] | "";

    if (ssid.length() == 0) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"SSID required\"}");
        return;
    }

    logPrintf("Web: connect request for '%s'", ssid.c_str());

    // Send response before attempting connection (connection will disrupt AP)
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Connecting... device will reboot if successful.\"}");

    // Give the response time to send
    delay(500);

    // Save credentials through wifi_manager API so they persist across reboot
    wifiSaveCredentials(ssid, password);

    logPrintf("Web: credentials saved, rebooting to connect");
    delay(200);
    ESP.restart();
}

static void handleGetLocation() {
    addCorsHeaders();

    Settings& s = settingsGet();
    JsonDocument doc;
    doc["lat"] = s.latitude;
    doc["lon"] = s.longitude;

    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json);
}

static void handleSetLocation() {
    addCorsHeaders();

    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"No body\"}");
        return;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));

    if (err) {
        server.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }

    Settings& s = settingsGet();
    s.latitude  = doc["lat"] | s.latitude;
    s.longitude = doc["lon"] | s.longitude;
    settingsSave();

    logPrintf("Web: location set to lat=%.4f, lon=%.4f", s.latitude, s.longitude);

    server.send(200, "application/json", "{\"success\":true,\"message\":\"Location saved\"}");
}

static void handleOTAPage() {
    addCorsHeaders();
    server.send_P(200, "text/html", OTA_HTML);
}

static void handleConfirmGood() {
    addCorsHeaders();
    otaConfirmGood();
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Firmware confirmed\"}");
}

static void handleRollback() {
    addCorsHeaders();
    logPrintf("Web: rollback requested");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Rolling back...\"}");
    delay(500);
    otaRollback();  // Marks firmware invalid and reboots
}

static void handleReset() {
    addCorsHeaders();
    logPrintf("Web: factory reset requested");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Resetting...\"}");
    delay(500);

    // Clear user settings first (without rebooting), then clear WiFi creds + reboot
    settingsClear();
    wifiFactoryReset();  // Clears WiFi creds and calls ESP.restart()
}

static void handleLog() {
    addCorsHeaders();
    String logs = logGetAll();
    server.send(200, "text/plain", logs);
}

static void handleCaptiveRedirect() {
    server.sendHeader("Location", "http://" + wifiGetIP());
    server.send(302, "text/plain", "");
    logPrintf("Web: captive portal redirect");
}

static void handleNotFound() {
    // In AP mode, redirect unknown paths to portal (captive portal behavior)
    if (wifiIsAPMode()) {
        server.sendHeader("Location", "http://" + wifiGetIP());
        server.send(302, "text/plain", "");
        return;
    }
    server.send(404, "text/plain", "Not found");
}

// ============================================================
// Public API
// ============================================================

void webServerInit() {
    logPrintf("Web: initializing server on port %d", WEB_SERVER_PORT);

    // Main pages
    server.on("/", HTTP_GET, handleRoot);
    server.on("/update", HTTP_GET, handleOTAPage);

    // API endpoints
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/set", HTTP_GET, handleSet);
    server.on("/api/weather", HTTP_GET, handleWeather);
    server.on("/api/scan", HTTP_GET, handleScan);
    server.on("/api/connect", HTTP_POST, handleConnect);
    server.on("/api/location", HTTP_GET, handleGetLocation);
    server.on("/api/location", HTTP_POST, handleSetLocation);

    // OTA - delegate to ota module's upload handler
    server.on("/ota", HTTP_POST, []() {
        // Completion callback: send response and reboot if update succeeded
        if (Update.hasError()) {
            server.send(500, "text/plain", "Upload failed");
        } else {
            server.send(200, "text/plain", "OK - rebooting");
            delay(500);
            ESP.restart();
        }
    }, []() {
        otaHandleUpload(server);
    });

    // Utility
    server.on("/confirm-good", HTTP_GET, handleConfirmGood);
    server.on("/rollback", HTTP_POST, handleRollback);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/log", HTTP_GET, handleLog);

    // Captive portal detection endpoints
    server.on("/generate_204", HTTP_GET, handleCaptiveRedirect);        // Android
    server.on("/hotspot-detect.html", HTTP_GET, handleCaptiveRedirect); // iOS
    server.on("/connecttest.txt", HTTP_GET, handleCaptiveRedirect);     // Windows
    server.on("/redirect", HTTP_GET, handleCaptiveRedirect);            // Generic

    // CORS preflight
    server.on("/api/connect", HTTP_OPTIONS, []() {
        addCorsHeaders();
        server.send(204);
    });
    server.on("/api/location", HTTP_OPTIONS, []() {
        addCorsHeaders();
        server.send(204);
    });

    // Catch-all
    server.onNotFound(handleNotFound);

    server.begin();
    logPrintf("Web: server started");
}

void webServerUpdate() {
    server.handleClient();
}
