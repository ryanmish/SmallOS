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
static void handleReset();
static void handleLog();
static void handleCaptiveRedirect();
static void handleNotFound();
static void addCorsHeaders();

// ============================================================
// Embedded Web UI
// ============================================================

static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmallTV</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#0f0f17;color:#d0d0e0;padding:16px;max-width:480px;margin:0 auto;font-size:15px;line-height:1.5;opacity:0;animation:fadeIn .3s ease forwards}
@keyframes fadeIn{to{opacity:1}}
@keyframes slideDown{from{transform:translateY(-100%);opacity:0}to{transform:translateY(0);opacity:1}}
@keyframes slideUp{to{transform:translateY(-100%);opacity:0}}
h1{font-size:1.3em;margin-bottom:14px;color:#fff;letter-spacing:.5px}
h2{font-size:.75em;margin:0 0 12px;color:#666;text-transform:uppercase;letter-spacing:2px;font-weight:600;padding-bottom:8px;border-bottom:1px solid #2a2a3d}
.card{background:#181825;border:1px solid rgba(255,255,255,.06);border-radius:10px;padding:18px;margin-bottom:16px;box-shadow:0 2px 8px rgba(0,0,0,.3)}
label{display:block;font-size:.85em;color:#888;margin-bottom:4px}
input[type=text],input[type=password],input[type=number]{width:100%;padding:12px;background:#1e1e2e;border:1px solid #333;border-radius:10px;color:#e0e0e0;font-size:.9em;margin-bottom:8px;transition:border-color .2s,box-shadow .2s;outline:none}
input[type=text]:focus,input[type=password]:focus,input[type=number]:focus{border-color:#0cd4c4;box-shadow:0 0 0 2px rgba(12,212,196,.2)}
input[type=range]{-webkit-appearance:none;width:100%;margin:8px 0 12px;background:transparent}
input[type=range]::-webkit-slider-runnable-track{height:4px;background:#2a2a3d;border-radius:2px}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;border-radius:50%;background:#0cd4c4;margin-top:-9px;cursor:pointer;box-shadow:0 1px 4px rgba(0,0,0,.4)}
input[type=range]::-moz-range-track{height:4px;background:#2a2a3d;border-radius:2px;border:none}
input[type=range]::-moz-range-thumb{width:22px;height:22px;border-radius:50%;background:#0cd4c4;border:none;cursor:pointer;box-shadow:0 1px 4px rgba(0,0,0,.4)}
button{padding:10px 18px;border:none;border-radius:6px;cursor:pointer;font-size:.9em;margin:4px 4px 4px 0;min-height:44px;transition:all .15s ease;font-weight:500}
button:hover{transform:scale(1.01)}
button:active{transform:scale(.98)}
.btn{background:#0cd4c4;color:#0f0f17}
.btn:hover{background:#1ae6d6}
.btn-warn{background:#e05555;color:#fff}
.btn-warn:hover{background:#e86b6b}
.btn-sec{background:#2a2a3d;color:#ccc;border:1px solid #3a3a50}
.btn-sec:hover{background:#333350}
.net{padding:10px 12px;margin:6px 0;border-radius:8px;background:#1e1e2e;cursor:pointer;display:flex;justify-content:space-between;align-items:center;transition:background .15s ease}
.net:hover{background:#252540}
.rssi{color:#666;font-size:.85em;font-family:monospace;white-space:nowrap}
.bars{letter-spacing:1px;margin-right:4px}
.status-row{display:flex;justify-content:space-between;padding:5px 0;font-size:.9em;border-bottom:1px dotted #1e1e2e}
.status-row:last-child{border-bottom:none}
.status-row span:first-child{color:#666}
.badge{display:inline-block;background:rgba(12,212,196,.15);color:#0cd4c4;padding:2px 8px;border-radius:10px;font-size:.8em;font-weight:600}
.toggle{display:flex;gap:0}
.toggle button{flex:1;padding:8px;border:1px solid #2a2a3d;background:transparent;color:#666;min-height:40px;transition:all .2s ease}
.toggle button:first-child{border-radius:20px 0 0 20px}
.toggle button:last-child{border-radius:0 20px 20px 0}
.toggle button:hover{transform:none}
.toggle button:active{transform:none}
.toggle button.active{background:#0cd4c4;color:#0f0f17;border-color:#0cd4c4;font-weight:600}
#msg{position:fixed;top:16px;left:50%;transform:translateX(-50%);width:calc(100% - 32px);max-width:448px;padding:12px 16px;border-radius:8px;display:none;font-size:.9em;z-index:100;animation:slideDown .25s ease;box-shadow:0 4px 16px rgba(0,0,0,.4)}
.ok{background:#122a18;color:#4ade80;border-left:3px solid #4ade80}
.err{background:#2a1218;color:#f87171;border-left:3px solid #f87171}
</style>
</head><body>
<h1>SmallTV</h1>
<div id="msg"></div>

<div class="card" id="status-card">
<h2>Status</h2>
<div class="status-row"><span>Firmware</span><span id="s-ver">--</span></div>
<div class="status-row"><span>WiFi</span><span id="s-wifi">--</span></div>
<div class="status-row"><span>IP</span><span id="s-ip">--</span></div>
<div class="status-row"><span>RSSI</span><span id="s-rssi">--</span></div>
<div class="status-row"><span>Uptime</span><span id="s-up">--</span></div>
<div class="status-row"><span>Heap</span><span id="s-heap">--</span></div>
</div>

<div class="card">
<h2>WiFi</h2>
<button class="btn-sec" onclick="doScan()">Scan Networks</button>
<div id="nets"></div>
<label>SSID</label>
<input type="text" id="w-ssid">
<label>Password</label>
<input type="password" id="w-pass">
<button class="btn" onclick="doConnect()">Connect</button>
</div>

<div class="card">
<h2>Settings</h2>
<label>Brightness: <span id="brt-val">--</span>%</label>
<input type="range" id="brt" min="0" max="100" oninput="document.getElementById('brt-val').textContent=this.value" onchange="setParam('brt',this.value)">
<label>Temperature Unit</label>
<div class="toggle">
<button id="btn-f" onclick="setUnit(true)">&#176;F</button>
<button id="btn-c" onclick="setUnit(false)">&#176;C</button>
</div>
<label>GMT Offset (seconds)</label>
<input type="number" id="gmt" onchange="setParam('gmt',this.value)">
<label>Latitude</label>
<input type="number" id="lat" step="0.0001">
<label>Longitude</label>
<input type="number" id="lon" step="0.0001">
<button class="btn" onclick="setLoc()">Save Location</button>
</div>

<div class="card">
<h2>Actions</h2>
<button class="btn-sec" onclick="location.href='/update'">Upload Firmware</button>
<button class="btn-warn" id="rst-btn" onclick="confirmReset()">Factory Reset</button>
</div>

<script>
function msg(t,ok){var m=document.getElementById('msg');m.textContent=t;m.className=ok?'ok':'err';m.style.display='block';m.style.animation='none';m.offsetHeight;m.style.animation='slideDown .25s ease';setTimeout(function(){m.style.animation='slideUp .25s ease forwards';setTimeout(function(){m.style.display='none'},250)},3500)}
function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML}
function api(u,o){return fetch(u,o).then(function(r){return r.json()}).catch(function(e){msg('Request failed','');})}
function rssiToBars(r){if(r>=-50)return'\u2582\u2584\u2586\u2588';if(r>=-65)return'\u2582\u2584\u2586';if(r>=-80)return'\u2582\u2584';return'\u2582'}

function load(){
api('/api/status').then(function(d){
if(!d)return;
var ve=document.getElementById('s-ver');ve.innerHTML='<span class="badge">'+(d.version||'--')+'</span>';
document.getElementById('s-wifi').textContent=d.ssid||'--';
document.getElementById('s-ip').textContent=d.ip||'--';
document.getElementById('s-rssi').textContent=d.rssi!=null?d.rssi+'dBm':'--';
var u=d.uptime||0;var h=Math.floor(u/3600);var m=Math.floor((u%3600)/60);
document.getElementById('s-up').textContent=h+'h '+m+'m';
document.getElementById('s-heap').textContent=d.heap?Math.round(d.heap/1024)+'KB':'--';
if(d.brightness!=null){document.getElementById('brt').value=d.brightness;document.getElementById('brt-val').textContent=d.brightness}
if(d.gmt_offset!=null)document.getElementById('gmt').value=d.gmt_offset;
if(d.temp_f!=null){document.getElementById('btn-f').className=d.temp_f?'active':'';document.getElementById('btn-c').className=d.temp_f?'':'active'}
if(d.lat!=null)document.getElementById('lat').value=d.lat;
if(d.lon!=null)document.getElementById('lon').value=d.lon;
})}

function showNets(d){
if(!d||!d.networks||!d.networks.length){document.getElementById('nets').innerHTML='<div style="padding:10px;color:#666">No networks found</div>';return}
var h='';d.networks.forEach(function(n){var s=esc(n.ssid);var bars=rssiToBars(n.rssi);h+='<div class="net" onclick="document.getElementById(\'w-ssid\').value=\''+s.replace(/'/g,'\\&#39;')+'\'"><span>'+s+(n.enc?' &#128274;':'')+'</span><span class="rssi"><span class="bars">'+bars+'</span> '+n.rssi+'dBm</span></div>'});
document.getElementById('nets').innerHTML=h}
function doScan(){
document.getElementById('nets').innerHTML='<div style="padding:10px;color:#666">Scanning...</div>';
api('/api/scan?start=1').then(function(){
var tries=0;var poll=setInterval(function(){api('/api/scan').then(function(d){
if(d&&!d.scanning){clearInterval(poll);showNets(d)}
else if(++tries>20){clearInterval(poll);showNets(d)}
})},500)})}

function doConnect(){
var s=document.getElementById('w-ssid').value;var p=document.getElementById('w-pass').value;
if(!s){msg('Enter SSID','');return}
msg('Connecting...', true);
fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,password:p})}).then(function(r){return r.json()}).then(function(d){
msg(d.message||'Sent',d.success);if(d.success)setTimeout(function(){location.reload()},5000)}).catch(function(){msg('Failed','');})}

function setParam(k,v){api('/api/set?'+k+'='+v).then(function(d){if(d&&d.success)msg('Saved',true);else msg('Failed','')})}
function setUnit(f){fetch('/api/set?tempF='+(f?'1':'0')).then(function(){load()}).catch(function(){msg('Failed','')})}

function setLoc(){
var la=document.getElementById('lat').value;var lo=document.getElementById('lon').value;
fetch('/api/location',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lat:parseFloat(la),lon:parseFloat(lo)})}).then(function(r){return r.json()}).then(function(d){msg(d.message||'Saved',d.success)}).catch(function(){msg('Failed','')})}

function confirmReset(){if(confirm('Factory reset? All settings and WiFi credentials will be erased.')){fetch('/reset',{method:'POST'}).then(function(){msg('Resetting...',true)}).catch(function(){msg('Failed','')})}}

load();
</script>
</body></html>)rawliteral";

// ============================================================
// OTA upload page
// ============================================================

static const char OTA_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmallTV OTA</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#0f0f17;color:#d0d0e0;padding:16px;max-width:480px;margin:0 auto;font-size:15px;line-height:1.5;opacity:0;animation:fadeIn .3s ease forwards}
@keyframes fadeIn{to{opacity:1}}
h2{font-size:.75em;margin:0 0 12px;color:#666;text-transform:uppercase;letter-spacing:2px;font-weight:600;padding-bottom:8px;border-bottom:1px solid #2a2a3d}
.card{background:#181825;border:1px solid rgba(255,255,255,.06);border-radius:10px;padding:18px;margin-bottom:16px;box-shadow:0 2px 8px rgba(0,0,0,.3)}
a.back{display:inline-block;color:#0cd4c4;text-decoration:none;font-size:.9em;margin-bottom:14px;transition:opacity .15s}
a.back:hover{opacity:.7}
.file-label{display:flex;align-items:center;justify-content:center;padding:14px;background:#1e1e2e;border:2px dashed #2a2a3d;border-radius:10px;cursor:pointer;color:#888;font-size:.9em;transition:border-color .2s,color .2s;margin-bottom:12px;min-height:44px}
.file-label:hover{border-color:#0cd4c4;color:#0cd4c4}
.file-label.has-file{border-color:#0cd4c4;color:#d0d0e0;border-style:solid}
input[type=file]{display:none}
button{padding:12px 24px;background:#0cd4c4;color:#0f0f17;border:none;border-radius:6px;cursor:pointer;font-size:.9em;font-weight:600;min-height:44px;transition:all .15s ease;width:100%}
button:hover{background:#1ae6d6;transform:scale(1.01)}
button:active{transform:scale(.98)}
button:disabled{background:#2a2a3d;color:#555;cursor:default;transform:none}
.warn-box{background:rgba(234,179,8,.08);border:1px solid rgba(234,179,8,.3);border-radius:8px;padding:12px;margin-bottom:14px;color:#eab308;font-size:.85em;text-align:center}
.progress-wrap{margin-top:14px;display:none}
.progress-bar{height:4px;background:#2a2a3d;border-radius:2px;overflow:hidden}
.progress-fill{height:100%;width:0;background:#0cd4c4;border-radius:2px;transition:width .2s ease}
#prog{margin-top:8px;font-size:.85em;color:#666;text-align:center}
.done{color:#4ade80!important}
.fail{color:#f87171!important}
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
xhr.onload=function(){if(xhr.status==200){pf.style.width='100%';prog.textContent='Done! Rebooting...';prog.className='done';ubtn.textContent='Complete'}else{prog.textContent='Upload failed: '+xhr.responseText;prog.className='fail';ubtn.disabled=false;ubtn.textContent='Retry'}};
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
    server.send_P(200, "text/html", INDEX_HTML);
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
