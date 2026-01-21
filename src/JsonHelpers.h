#ifndef JSON_HELPERS_H
#define JSON_HELPERS_H

#include <ArduinoJson.h>
#include "PrintError.h"

/**
 * Helper functions for building MQTT JSON messages.
 * Centralizes JSON structure definitions for consistency.
 */
namespace JsonHelpers {

/**
 * Build a print result JSON message.
 * @param buffer Output buffer (must be at least 256 bytes)
 * @param bufferSize Size of output buffer
 * @param printId Optional print job ID
 * @param error PrintError::None for success, error code otherwise
 * @return Number of bytes written
 */
inline size_t buildResult(char* buffer, size_t bufferSize,
                          const char* printId, PrintError error) {
    JsonDocument doc;

    if (printId && strlen(printId) > 0) {
        doc["printId"] = printId;
    }
    doc["success"] = (error == PrintError::None);
    if (error != PrintError::None) {
        doc["error"] = printErrorToString(error);
    }

    return serializeJson(doc, buffer, bufferSize);
}

/**
 * Build an error result JSON message (for non-PrintError errors).
 * @param buffer Output buffer (must be at least 256 bytes)
 * @param bufferSize Size of output buffer
 * @param errorMsg Error message string
 * @return Number of bytes written
 */
inline size_t buildErrorResult(char* buffer, size_t bufferSize,
                               const char* errorMsg) {
    JsonDocument doc;
    doc["success"] = false;
    doc["error"] = errorMsg;
    return serializeJson(doc, buffer, bufferSize);
}

/**
 * Build a status JSON message.
 * @param buffer Output buffer (must be at least 256 bytes)
 * @param bufferSize Size of output buffer
 * @param printerConnected Whether printer is connected
 * @param battery Battery level (0-100, or -1 if unknown)
 * @param lastSeenSec Seconds since last printer activity (0 to omit)
 * @param wifiRssi WiFi signal strength in dBm
 * @param freeHeap Free heap memory in bytes
 * @param uptimeSec System uptime in seconds
 * @return Number of bytes written
 */
inline size_t buildStatus(char* buffer, size_t bufferSize,
                          bool printerConnected, int battery,
                          unsigned long lastSeenSec, int wifiRssi,
                          uint32_t freeHeap, unsigned long uptimeSec) {
    JsonDocument doc;

    doc["printer"] = printerConnected ? "connected" : "disconnected";
    if (battery >= 0) {
        doc["battery"] = battery;
    }
    if (lastSeenSec > 0) {
        doc["lastSeen"] = lastSeenSec;
    }
    doc["wifi"] = wifiRssi;
    doc["heap"] = freeHeap;
    doc["uptime"] = uptimeSec;

    return serializeJson(doc, buffer, bufferSize);
}

}  // namespace JsonHelpers

#endif
