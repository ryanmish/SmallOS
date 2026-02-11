#pragma once

#include <Arduino.h>
#include <WebServer.h>

// ============================================================
// OTA Update Manager
// ArduinoOTA (network) + web upload + rollback safety
// ============================================================
//
// Rollback safety: After an OTA flash, the new firmware must call
// otaConfirmGood() (via the /confirm-good HTTP endpoint) within
// OTA_CONFIRM_TIMEOUT_MS. If it doesn't, the device reboots and
// the bootloader rolls back to the previous partition.

void otaInit();                             // Set up ArduinoOTA + rollback watchdog
void otaUpdate();                           // Call in main loop
void otaConfirmGood();                      // Cancel rollback timer, mark firmware valid
bool otaIsConfirmed();                      // Has firmware been confirmed good?
void otaHandleUpload(WebServer& server);    // HTTP upload handler for /ota endpoint
