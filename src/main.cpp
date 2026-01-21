#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "ConfigManager.h"
#include "ConfigPortal.h"
#include "LabelImage.h"
#include "QRCodeRenderer.h"
#include "Printer.h"
#include "NelkoP21Printer.h"
#include "WiFiManager.h"

// ============================================================
// Global Objects
// ============================================================

ConfigManager configManager;
ConfigPortal* configPortal = nullptr;

Printer* printer = nullptr;
WiFiManager wifiManager;
WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient;

bool mqttConnected = false;
bool portalActive = false;

// ============================================================
// Print Functions
// ============================================================

const char* printLabel(const char* link, const char* name, const char* id, const char* qrSize = nullptr) {
    if (!printer || !printer->isConnected()) {
        Serial.println("Drucker nicht verbunden!");
        return "printer not connected";
    }

    const char* statusError = printer->checkReady();
    if (statusError) {
        Serial.printf("Drucker-Fehler: %s\n", statusError);
        return statusError;
    }

    QRSize size = QRCodeRenderer::sizeFromString(qrSize);
    const char* sizeStr = (size == QRSize::Small) ? "S" : (size == QRSize::Medium) ? "M" : "L";
    Serial.printf("Drucke Label: %s / %s (QR: %s)\n", name, id, sizeStr);

    LabelImage label(printer->getLabelWidth(), printer->getLabelHeight());
    if (!label.generate(link, name, id, size)) {
        Serial.printf("Label-Fehler: %s\n", label.getError());
        return label.getError();
    }

    char* dataUrl = label.toDataURL();
    if (dataUrl) {
        Serial.println("Label-Vorschau:");
        Serial.println(dataUrl);
        free(dataUrl);
    }

    printer->sendBitmap(label.getData());
    Serial.println("Label gesendet!");
    return nullptr;
}

void printFrame() {
    if (!printer || !printer->isConnected()) {
        Serial.println("Drucker nicht verbunden!");
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
    Serial.println("Rahmen gesendet!");
}

// ============================================================
// MQTT Functions
// ============================================================

void sendResult(const char* printId, bool success, const char* error = nullptr) {
    if (!mqttConnected) return;

    const ConfigManager::Config& config = configManager.getConfig();

    JsonDocument doc;
    if (printId && strlen(printId) > 0) {
        doc["printId"] = printId;
    }
    doc["success"] = success;
    if (!success && error) {
        doc["error"] = error;
    }

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(config.mqttTopicResult, buffer);
    Serial.printf("Result gesendet: %s\n", buffer);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("MQTT Nachricht auf %s\n", topic);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);

    if (err) {
        Serial.printf("JSON Fehler: %s\n", err.c_str());
        sendResult(nullptr, false, "invalid JSON");
        return;
    }

    const char* printId = doc["printId"] | "";
    const char* link = doc["link"] | "";
    const char* name = doc["name"] | "";
    const char* id = doc["id"] | "";
    const char* size = doc["size"] | "L";

    const char* error = printLabel(link, name, id, size);
    sendResult(printId, error == nullptr, error);
}

void connectMQTT() {
    if (!wifiManager.isConnected()) return;

    const ConfigManager::Config& config = configManager.getConfig();

    if (config.mqttUseSsl) {
        wifiClientSecure.setInsecure();
        mqttClient.setClient(wifiClientSecure);
        Serial.printf("Verbinde mit MQTT (SSL) %s:%d...\n", config.mqttServer, config.mqttPort);
    } else {
        mqttClient.setClient(wifiClient);
        Serial.printf("Verbinde mit MQTT %s:%d...\n", config.mqttServer, config.mqttPort);
    }

    mqttClient.setServer(config.mqttServer, config.mqttPort);
    mqttClient.setCallback(mqttCallback);

    if (mqttClient.connect(config.deviceName, config.mqttUser, config.mqttPassword)) {
        Serial.println("MQTT verbunden!");
        mqttClient.subscribe(config.mqttTopicPrint);
        Serial.printf("Subscribed: %s\n", config.mqttTopicPrint);
        mqttConnected = true;
    } else {
        Serial.printf("MQTT Fehler: %d\n", mqttClient.state());
        mqttConnected = false;
    }
}

unsigned long lastMqttRetry = 0;
const unsigned long MQTT_RETRY_INTERVAL = 10000;

void checkMQTT() {
    if (!wifiManager.isConnected()) return;

    const ConfigManager::Config& config = configManager.getConfig();

    if (!mqttClient.connected()) {
        mqttConnected = false;
        unsigned long now = millis();
        if (now - lastMqttRetry >= MQTT_RETRY_INTERVAL) {
            lastMqttRetry = now;
            if (wifiManager.checkDns(config.mqttServer)) {
                connectMQTT();
            }
        }
    } else {
        mqttClient.loop();
    }
}

unsigned long lastStatusTime = 0;
const unsigned long STATUS_INTERVAL = 30000;

void publishStatus() {
    if (!mqttConnected || !printer) return;

    const ConfigManager::Config& config = configManager.getConfig();
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
    mqttClient.publish(config.mqttTopicStatus, buffer);
    Serial.printf("Status gesendet: %s\n", buffer);
}

// ============================================================
// Configuration Portal
// ============================================================

void startConfigPortal() {
    Serial.println("\nStarting configuration portal...");

    if (configPortal == nullptr) {
        configPortal = new ConfigPortal(configManager);
    }

    if (configPortal->begin()) {
        portalActive = true;
    } else {
        Serial.println("Failed to start portal!");
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
    Serial.println("\n=== Nelko P21 MQTT Printer ===");
    Serial.println("Status:");
    Serial.printf("  WiFi: %s (%d dBm)\n", wifiManager.isConnected() ? "Verbunden" : "Getrennt", wifiManager.getRSSI());
    Serial.printf("  MQTT: %s\n", mqttConnected ? "Verbunden" : "Getrennt");
    Serial.printf("  Drucker: %s\n", (printer && printer->isConnected()) ? "Verbunden" : "Getrennt");
    if (printer) {
        int battery = printer->getBattery();
        if (battery >= 0) {
            Serial.printf("  Batterie: %d%%\n", battery);
        }
    }
    Serial.println("Befehle:");
    Serial.println("  scan        - Drucker suchen & verbinden");
    Serial.println("  disconnect  - Drucker trennen");
    Serial.println("  status      - Drucker-Status (Papier, etc.)");
    Serial.println("  config      - Drucker-Konfiguration");
    Serial.println("  battery     - Batterie abfragen");
    Serial.println("  frame       - Test: Rahmen drucken");
    Serial.println("  qrcode      - Test: QR-Code drucken");
    Serial.println("  wifi        - WiFi neu verbinden");
    Serial.println("  mqtt        - MQTT neu verbinden");
    Serial.println("  setup       - Konfigurations-Portal starten");
    Serial.println("  clearconfig - Konfiguration loeschen & neustarten");
    Serial.println("  help        - Diese Hilfe");
    Serial.println("==============================\n");
}

// ============================================================
// Setup & Loop
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  Nelko P21 MQTT Label Printer");
    Serial.println("========================================");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM: %d bytes\n", ESP.getPsramSize());

    // Load configuration from NVS
    configManager.load();

    // Check if configured
    if (!configManager.isConfigured()) {
        Serial.println("\nDevice not configured!");
        startConfigPortal();
        return;  // Portal will run in loop()
    }

    // Configuration exists - proceed with normal startup
    const ConfigManager::Config& config = configManager.getConfig();

    Serial.printf("\nConfiguration loaded:");
    Serial.printf("\n  Device: %s", config.deviceName);
    Serial.printf("\n  WiFi: %s", config.wifiSsid);
    Serial.printf("\n  MQTT: %s:%d\n", config.mqttServer, config.mqttPort);

    // Connect WiFi using stored credentials
    wifiManager.setCredentials(config.wifiSsid, config.wifiPassword);
    wifiManager.connect();

    // Connect MQTT (before Bluetooth - SSL needs RAM during handshake)
    connectMQTT();

    // Initialize printer
    printer = new NelkoP21Printer();
    if (printer->connect()) {
        Serial.println("Drucker bereit.");
    }
}

void loop() {
    // Handle configuration portal if active
    if (portalActive && configPortal != nullptr) {
        if (!configPortal->loop()) {
            // Timeout - restart device
            Serial.println("Portal timeout, restarting...");
            stopConfigPortal();
            delay(1000);
            ESP.restart();
        }

        if (configPortal->wasConfigSaved()) {
            // Config saved - restart to apply
            Serial.println("Configuration saved, restarting...");
            stopConfigPortal();
            delay(2000);
            ESP.restart();
        }

        return;  // Don't run normal loop while portal is active
    }

    // Normal operation
    wifiManager.loop();
    checkMQTT();

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

        if (cmdLower == "scan") {
            if (printer) printer->connect();
        }
        else if (cmdLower == "disconnect") {
            if (printer) printer->disconnect();
        }
        else if (cmdLower == "frame") {
            printFrame();
        }
        else if (cmdLower == "qrcode") {
            printLabel("https://zeug.makerspacebonn.de/i/1259", "Test Item", "1259", "L");
        }
        else if (cmdLower == "wifi") {
            wifiManager.connect();
        }
        else if (cmdLower == "mqtt") {
            connectMQTT();
        }
        else if (cmdLower == "help" || cmdLower == "?") {
            printHelp();
        }
        else if (cmdLower == "status") {
            printHelp();
        }
        else if (cmdLower == "battery") {
            if (printer) printer->getBattery();
        }
        else if (cmdLower == "config") {
            if (printer) printer->queryConfig();
        }
        else if (cmdLower == "ready") {
            if (printer) printer->queryStatus();
        }
        else if (cmdLower == "setup") {
            // Start configuration portal manually
            startConfigPortal();
        }
        else if (cmdLower == "clearconfig") {
            Serial.println("Clearing configuration and restarting...");
            configManager.clear();
            delay(1000);
            ESP.restart();
        }
        else if (cmdLower.startsWith("send ")) {
            if (printer) printer->sendCommand(cmd.substring(5).c_str());
        }
    }

    // Process printer messages
    if (printer) {
        printer->processIncoming();
    }

    delay(10);
}
