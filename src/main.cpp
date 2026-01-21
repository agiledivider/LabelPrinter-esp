#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConfigManager.h"
#include "ConfigPortal.h"
#include "LabelImage.h"
#include "QRCodeRenderer.h"
#include "Printer.h"
#include "NelkoP21Printer.h"
#include "WiFiManager.h"
#include "MqttManager.h"
#include "PrintError.h"
#include "Log.h"

// ============================================================
// Global Objects
// ============================================================

ConfigManager configManager;
ConfigPortal* configPortal = nullptr;

Printer* printer = nullptr;
WiFiManager wifiManager;
MqttManager mqttManager;

bool portalActive = false;

// ============================================================
// Command Handler Types
// ============================================================

using CommandHandler = void (*)();

struct Command {
    const char* name;
    CommandHandler handler;
};

// ============================================================
// Print Functions
// ============================================================

PrintError printLabel(const char* link, const char* name, const char* id, const char* qrSize = nullptr) {
    if (!printer || !printer->isConnected()) {
        LOG_ERROR("Printer not connected!");
        return PrintError::PrinterNotConnected;
    }

    PrintError statusError = printer->checkReady();
    if (statusError != PrintError::None) {
        LOG_ERRORF("Printer error: %s", printErrorToString(statusError));
        return statusError;
    }

    QRSize size = QRCodeRenderer::sizeFromString(qrSize);
    const char* sizeStr = (size == QRSize::Small) ? "S" : (size == QRSize::Medium) ? "M" : "L";
    LOG_INFOF("Printing label: %s / %s (QR: %s)", name, id, sizeStr);

    LabelImage label(printer->getLabelWidth(), printer->getLabelHeight());
    if (!label.generate(link, name, id, size)) {
        LOG_ERRORF("Label error: %s", printErrorToString(label.getError()));
        return label.getError();
    }

    char* dataUrl = label.toDataURL();
    if (dataUrl) {
        LOG_DEBUG("Label preview:");
        LOG_DEBUG(dataUrl);
        free(dataUrl);
    }

    printer->sendBitmap(label.getData());
    LOG_INFO("Label sent!");
    return PrintError::None;
}

void printFrame() {
    if (!printer || !printer->isConnected()) {
        LOG_ERROR("Printer not connected!");
        return;
    }

    int bitmapSize = printer->getBitmapSize();
    int bytesPerRow = printer->getBytesPerRow();
    int labelHeight = printer->getLabelHeight();

    uint8_t* bitmap = (uint8_t*)malloc(bitmapSize);
    if (!bitmap) return;

    memset(bitmap, 0xFF, bitmapSize);
    memset(bitmap, 0x00, bytesPerRow);
    memset(bitmap + (labelHeight - 1) * bytesPerRow, 0x00, bytesPerRow);

    for (int y = 0; y < labelHeight; y++) {
        bitmap[y * bytesPerRow] &= 0x7F;
        bitmap[y * bytesPerRow + bytesPerRow - 1] &= 0xFE;
    }

    printer->sendBitmap(bitmap);
    free(bitmap);
    LOG_INFO("Frame sent!");
}

// ============================================================
// MQTT Functions
// ============================================================

void sendResult(const char* printId, PrintError error) {
    if (!mqttManager.isConnected()) return;

    JsonDocument doc;
    if (printId && strlen(printId) > 0) {
        doc["printId"] = printId;
    }
    doc["success"] = (error == PrintError::None);
    if (error != PrintError::None) {
        doc["error"] = printErrorToString(error);
    }

    char buffer[256];
    serializeJson(doc, buffer);
    mqttManager.publishResult(buffer);
    LOG_INFOF("Result sent: %s", buffer);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    LOG_INFOF("MQTT message on %s", topic);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);

    if (err) {
        LOG_ERRORF("JSON error: %s", err.c_str());
        // For JSON errors, we still need to send a result - use a generic error
        JsonDocument result;
        result["success"] = false;
        result["error"] = "invalid JSON";
        char buffer[256];
        serializeJson(result, buffer);
        mqttManager.publishResult(buffer);
        return;
    }

    const char* printId = doc["printId"] | "";
    const char* link = doc["link"] | "";
    const char* name = doc["name"] | "";
    const char* id = doc["id"] | "";
    const char* size = doc["size"] | "L";

    PrintError error = printLabel(link, name, id, size);
    sendResult(printId, error);
}

unsigned long lastStatusTime = 0;
const unsigned long STATUS_INTERVAL = 30000;

void publishStatus() {
    if (!mqttManager.isConnected() || !printer) return;

    int battery = printer->getBattery();

    JsonDocument doc;
    doc["printer"] = printer->isConnected() ? "connected" : "disconnected";
    if (battery >= 0) {
        doc["battery"] = battery;
    }
    if (printer->getLastSeenMs() > 0) {
        doc["lastSeen"] = (millis() - printer->getLastSeenMs()) / 1000;
    }
    doc["wifi"] = wifiManager.getRSSI();
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

    char buffer[256];
    serializeJson(doc, buffer);
    mqttManager.publishStatus(buffer);
    LOG_DEBUGF("Status sent: %s", buffer);
}

// ============================================================
// Configuration Portal
// ============================================================

void startConfigPortal() {
    LOG_INFO("\nStarting configuration portal...");

    if (configPortal == nullptr) {
        configPortal = new ConfigPortal(configManager);
    }

    if (configPortal->begin()) {
        portalActive = true;
    } else {
        LOG_ERROR("Failed to start portal!");
        delete configPortal;
        configPortal = nullptr;
    }
}

void stopConfigPortal() {
    if (configPortal != nullptr) {
        configPortal->end();
        delete configPortal;
        configPortal = nullptr;
    }
    portalActive = false;
}

// ============================================================
// User Interface
// ============================================================

void printHelp() {
    LOG_INFO("\n=== Nelko P21 MQTT Printer ===");
    LOG_INFO("Status:");
    LOG_INFOF("  WiFi: %s (%d dBm)", wifiManager.isConnected() ? "Connected" : "Disconnected", wifiManager.getRSSI());
    LOG_INFOF("  MQTT: %s", mqttManager.isConnected() ? "Connected" : "Disconnected");
    LOG_INFOF("  Printer: %s", (printer && printer->isConnected()) ? "Connected" : "Disconnected");
    if (printer) {
        int battery = printer->getBattery();
        if (battery >= 0) {
            LOG_INFOF("  Battery: %d%%", battery);
        }
    }
    LOG_INFO("Commands:");
    LOG_INFO("  scan        - Scan & connect printer");
    LOG_INFO("  disconnect  - Disconnect printer");
    LOG_INFO("  status      - Printer status (paper, etc.)");
    LOG_INFO("  config      - Printer configuration");
    LOG_INFO("  battery     - Query battery level");
    LOG_INFO("  frame       - Test: Print frame");
    LOG_INFO("  qrcode      - Test: Print QR code");
    LOG_INFO("  wifi        - Reconnect WiFi");
    LOG_INFO("  mqtt        - Reconnect MQTT");
    LOG_INFO("  setup       - Start configuration portal");
    LOG_INFO("  clearconfig - Clear config & restart");
    LOG_INFO("  help        - Show this help");
    LOG_INFO("==============================\n");
}

// ============================================================
// Command Handlers
// ============================================================

void cmdScan() { if (printer) printer->connect(); }
void cmdDisconnect() { if (printer) printer->disconnect(); }
void cmdFrame() { printFrame(); }
void cmdQrcode() { printLabel("https://zeug.makerspacebonn.de/i/1259", "Test Item", "1259", "L"); }
void cmdWifi() { wifiManager.connect(); }
void cmdMqtt() { mqttManager.connect(); }
void cmdHelp() { printHelp(); }
void cmdBattery() { if (printer) printer->getBattery(); }
void cmdConfig() { if (printer) printer->queryConfig(); }
void cmdReady() { if (printer) printer->queryStatus(); }
void cmdSetup() { startConfigPortal(); }
void cmdClearConfig() {
    LOG_INFO("Clearing configuration and restarting...");
    configManager.clear();
    delay(1000);
    ESP.restart();
}

// Command lookup table
const Command commands[] = {
    {"scan", cmdScan},
    {"disconnect", cmdDisconnect},
    {"frame", cmdFrame},
    {"qrcode", cmdQrcode},
    {"wifi", cmdWifi},
    {"mqtt", cmdMqtt},
    {"help", cmdHelp},
    {"?", cmdHelp},
    {"status", cmdHelp},
    {"battery", cmdBattery},
    {"config", cmdConfig},
    {"ready", cmdReady},
    {"setup", cmdSetup},
    {"clearconfig", cmdClearConfig},
};
const int commandCount = sizeof(commands) / sizeof(commands[0]);

bool executeCommand(const String& cmd) {
    for (int i = 0; i < commandCount; i++) {
        if (cmd == commands[i].name) {
            commands[i].handler();
            return true;
        }
    }
    return false;
}

// ============================================================
// Setup & Loop
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    LOG_INFO("\n========================================");
    LOG_INFO("  Nelko P21 MQTT Label Printer");
    LOG_INFO("========================================");
    LOG_INFOF("Free heap: %d bytes", ESP.getFreeHeap());
    LOG_INFOF("PSRAM: %d bytes", ESP.getPsramSize());

    // Load configuration from NVS
    configManager.load();

    // Check if configured
    if (!configManager.isConfigured()) {
        LOG_INFO("\nDevice not configured!");
        startConfigPortal();
        return;  // Portal will run in loop()
    }

    // Configuration exists - proceed with normal startup
    const ConfigManager::Config& config = configManager.getConfig();

    LOG_INFO("\nConfiguration loaded:");
    LOG_INFOF("  Device: %s", config.deviceName);
    LOG_INFOF("  WiFi: %s", config.wifiSsid);
    LOG_INFOF("  MQTT: %s:%d", config.mqttServer, config.mqttPort);

    // Connect WiFi using stored credentials
    wifiManager.setCredentials(config.wifiSsid, config.wifiPassword);
    wifiManager.connect();

    // Initialize and connect MQTT (before Bluetooth - SSL needs RAM during handshake)
    mqttManager.begin(wifiManager);
    mqttManager.setConfig(config.mqttServer, config.mqttPort, config.mqttUseSsl,
                          config.mqttUser, config.mqttPassword, config.deviceName);
    mqttManager.setTopics(config.mqttTopicPrint, config.mqttTopicStatus, config.mqttTopicResult);
    mqttManager.setCallback(mqttCallback);
    mqttManager.connect();

    // Initialize printer
    printer = new NelkoP21Printer();
    if (printer->connect()) {
        LOG_INFO("Printer ready.");
    }
}

void loop() {
    // Handle configuration portal if active
    if (portalActive && configPortal != nullptr) {
        if (!configPortal->loop()) {
            // Timeout - restart device
            LOG_INFO("Portal timeout, restarting...");
            stopConfigPortal();
            delay(1000);
            ESP.restart();
        }

        if (configPortal->wasConfigSaved()) {
            // Config saved - restart to apply
            LOG_INFO("Configuration saved, restarting...");
            stopConfigPortal();
            delay(2000);
            ESP.restart();
        }

        return;  // Don't run normal loop while portal is active
    }

    // Normal operation
    wifiManager.loop();
    mqttManager.loop();

    // Periodic status
    if (millis() - lastStatusTime >= STATUS_INTERVAL) {
        lastStatusTime = millis();
        publishStatus();
    }

    // Serial commands
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        String cmdLower = cmd;
        cmdLower.toLowerCase();

        // Try command table lookup first
        if (!executeCommand(cmdLower)) {
            // Handle commands with arguments
            if (cmdLower.startsWith("send ")) {
                if (printer) printer->sendCommand(cmd.substring(5).c_str());
            }
        }
    }

    // Process printer messages
    if (printer) {
        printer->processIncoming();
    }

    delay(10);
}
