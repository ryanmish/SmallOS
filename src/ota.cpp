#include "ota.h"
#include "config.h"
#include "logger.h"
#include "display.h"

#include <ArduinoOTA.h>
#include <Update.h>
#include <esp_ota_ops.h>

// --- Module state ---

static bool          firmwareConfirmed = false;
static unsigned long bootTimeMs        = 0;

// --- Rollback watchdog ---

static void checkRollbackTimeout() {
    if (firmwareConfirmed) {
        return;
    }

    unsigned long elapsed = millis() - bootTimeMs;

    if (elapsed >= OTA_CONFIRM_TIMEOUT_MS) {
        logPrintf("[OTA] Rollback timeout expired (%lu ms without /confirm-good)",
                  OTA_CONFIRM_TIMEOUT_MS);
        logPrintf("[OTA] Rebooting to trigger bootloader rollback...");
        delay(500);
        ESP.restart();
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

    // Check if the running firmware is pending verification (i.e., was just OTA-flashed).
    // If the partition is already marked valid, skip the rollback watchdog entirely.
    // This prevents unnecessary 10-minute reboot timers on normal (non-OTA) boots.
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK &&
        state == ESP_OTA_IMG_PENDING_VERIFY) {
        firmwareConfirmed = false;
        logPrintf("[OTA] Firmware pending verification - rollback watchdog active (%lu ms)",
                  OTA_CONFIRM_TIMEOUT_MS);
    } else {
        // Normal boot or already verified - no rollback needed
        firmwareConfirmed = true;
        logPrintf("[OTA] Firmware already verified, rollback watchdog not needed");
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

    esp_ota_mark_app_valid_cancel_rollback();
    firmwareConfirmed = true;

    unsigned long elapsed = millis() - bootTimeMs;
    logPrintf("[OTA] Firmware confirmed good after %lu ms", elapsed);
    logPrintf("[OTA] Rollback watchdog cancelled");
}

bool otaIsConfirmed() {
    return firmwareConfirmed;
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
