#include "ota.h"
#include "config.h"
#include "logger.h"
#include "display.h"

#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

// --- NVS keys for rollback ---
static const char* OTA_NVS_NAMESPACE = "ota";
static const char* KEY_PENDING       = "pending";

// --- Module state ---

static bool          firmwareConfirmed = false;
static unsigned long bootTimeMs        = 0;

// --- NVS helpers ---

static void setPendingFlag(bool pending) {
    Preferences p;
    p.begin(OTA_NVS_NAMESPACE, false);
    p.putBool(KEY_PENDING, pending);
    p.end();
}

static bool getPendingFlag() {
    Preferences p;
    p.begin(OTA_NVS_NAMESPACE, true);
    bool val = p.getBool(KEY_PENDING, false);
    p.end();
    return val;
}

// --- Rollback watchdog ---

static void checkRollbackTimeout() {
    if (firmwareConfirmed) {
        return;
    }

    unsigned long elapsed = millis() - bootTimeMs;

    if (elapsed >= OTA_CONFIRM_TIMEOUT_MS) {
        logPrintf("[OTA] Rollback timeout expired (%lu ms without /confirm-good)",
                  OTA_CONFIRM_TIMEOUT_MS);
        logPrintf("[OTA] Rolling back to previous firmware...");
        setPendingFlag(false);
        delay(500);
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

// --- Firmware validation ---

static bool validateFirmwareHeader(uint8_t* data, size_t len) {
    if (len < 4) {
        return false;
    }

    // ESP32 firmware starts with magic byte 0xE9
    if (data[0] != 0xE9) {
        logPrintf("[OTA] Invalid firmware: bad magic byte 0x%02X (expected 0xE9)", data[0]);
        return false;
    }

    return true;
}

// --- ArduinoOTA callbacks ---

static void setupArduinoOTA() {
    ArduinoOTA.setHostname("smalltv");
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        logPrintf("[OTA] ArduinoOTA start: %s", type.c_str());
    });

    ArduinoOTA.onEnd([]() {
        logPrintf("[OTA] ArduinoOTA complete");
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        int pct = (progress * 100) / total;
        // Log at 25% intervals to avoid flooding
        if (pct % 25 == 0) {
            logPrintf("[OTA] ArduinoOTA progress: %u%%", pct);
        }
        displayRenderOTAProgress(pct);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        const char* errStr = "Unknown";
        switch (error) {
            case OTA_AUTH_ERROR:    errStr = "Auth failed";     break;
            case OTA_BEGIN_ERROR:   errStr = "Begin failed";    break;
            case OTA_CONNECT_ERROR: errStr = "Connect failed";  break;
            case OTA_RECEIVE_ERROR: errStr = "Receive failed";  break;
            case OTA_END_ERROR:     errStr = "End failed";      break;
        }
        logPrintf("[OTA] ArduinoOTA error: %s (%u)", errStr, error);
    });

    ArduinoOTA.begin();
    logPrintf("[OTA] ArduinoOTA ready (password protected)");
}

// --- Public API ---

void otaInit() {
    bootTimeMs = millis();

    // Check NVS flag set by the upload handler before rebooting.
    // This works around Update.end(true) auto-validating the partition.
    if (getPendingFlag()) {
        firmwareConfirmed = false;
        logPrintf("[OTA] Firmware pending verification - rollback watchdog active (%lu ms)",
                  OTA_CONFIRM_TIMEOUT_MS);
    } else {
        firmwareConfirmed = true;
        logPrintf("[OTA] Normal boot, rollback watchdog not needed");
    }

    setupArduinoOTA();

    logPrintf("[OTA] OTA manager initialized");
}

void otaUpdate() {
    ArduinoOTA.handle();
    checkRollbackTimeout();
}

void otaConfirmGood() {
    if (firmwareConfirmed) {
        logPrintf("[OTA] Firmware already confirmed");
        return;
    }

    setPendingFlag(false);
    firmwareConfirmed = true;

    unsigned long elapsed = millis() - bootTimeMs;
    logPrintf("[OTA] Firmware confirmed good after %lu ms", elapsed);
    logPrintf("[OTA] Rollback watchdog cancelled");
}

void otaRollback() {
    logPrintf("[OTA] Manual rollback requested");
    setPendingFlag(false);
    delay(200);
    esp_ota_mark_app_invalid_rollback_and_reboot();
}

bool otaIsConfirmed() {
    return firmwareConfirmed;
}

bool otaIsPending() {
    return getPendingFlag();
}

void otaHandleUpload(WebServer& server) {
    HTTPUpload& upload = server.upload();

    switch (upload.status) {
        case UPLOAD_FILE_START: {
            logPrintf("[OTA] Web upload start: %s", upload.filename.c_str());

            // Estimate available space (use half of total flash as safe upper bound)
            size_t maxSize = (ESP.getFreeSketchSpace() > 0)
                             ? ESP.getFreeSketchSpace()
                             : 0x1E0000;  // ~1.9MB fallback

            if (!Update.begin(maxSize, U_FLASH)) {
                logPrintf("[OTA] Update.begin() failed: %s", Update.errorString());
            }
            break;
        }

        case UPLOAD_FILE_WRITE: {
            // Validate firmware header on first chunk
            if (upload.totalSize == 0) {
                if (!validateFirmwareHeader(upload.buf, upload.currentSize)) {
                    logPrintf("[OTA] Firmware validation failed, aborting");
                    Update.abort();
                    return;
                }
            }

            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                logPrintf("[OTA] Update.write() failed: %s", Update.errorString());
            }

            // Progress reporting
            size_t written = upload.totalSize + upload.currentSize;
            int pct = (Update.size() > 0)
                      ? (int)((written * 100) / Update.size())
                      : 0;
            displayRenderOTAProgress(pct);
            break;
        }

        case UPLOAD_FILE_END: {
            if (Update.end(true)) {
                logPrintf("[OTA] Web upload complete: %u bytes", upload.totalSize);
                // Set NVS flag so the new firmware activates rollback watchdog on boot
                setPendingFlag(true);
                logPrintf("[OTA] Pending flag set (new firmware requires /confirm-good)");
                logPrintf("[OTA] Rebooting to apply update...");
            } else {
                logPrintf("[OTA] Update.end() failed: %s", Update.errorString());
            }
            break;
        }

        case UPLOAD_FILE_ABORTED: {
            logPrintf("[OTA] Web upload aborted");
            Update.abort();
            break;
        }
    }
}
