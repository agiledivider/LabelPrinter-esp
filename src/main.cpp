#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "LabelImage.h"
#include "QRCodeRenderer.h"
#include "Printer.h"
#include "NelkoP21Printer.h"

// ============================================================
// Globale Objekte
// ============================================================

// Printer abstraction - can be swapped for different implementations
Printer* printer = nullptr;

WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient;

bool wifiConnected = false;
bool mqttConnected = false;

// ============================================================
// WiFi-Funktionen
// ============================================================

void connectWiFi() {
    Serial.println("Verbinde mit WiFi...");

    for (int i = 0; i < wifiNetworkCount; i++) {
        Serial.printf("Versuche: %s\n", wifiNetworks[i][0]);
        WiFi.begin(wifiNetworks[i][0], wifiNetworks[i][1]);

        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nVerbunden mit %s\n", wifiNetworks[i][0]);
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
            wifiConnected = true;
            return;
        }
        Serial.println(" fehlgeschlagen");
    }

    Serial.println("Kein WiFi-Netzwerk erreichbar!");
    wifiConnected = false;
}

// ============================================================
// Druck-Funktionen
// ============================================================

// Gibt nullptr bei Erfolg zurück, sonst Fehlermeldung
// qrSize: "S", "M" oder "L" (default: "L")
const char* printLabel(const char* link, const char* name, const char* id, const char* qrSize = nullptr) {
    if (!printer || !printer->isConnected()) {
        Serial.println("Drucker nicht verbunden!");
        return "printer not connected";
    }

    // Drucker-Status prüfen (Papier, etc.)
    const char* statusError = printer->checkReady();
    if (statusError) {
        Serial.printf("Drucker-Fehler: %s\n", statusError);
        return statusError;
    }

    // QR-Code Groesse parsen (Default: Large)
    QRSize size = QRCodeRenderer::sizeFromString(qrSize);
    const char* sizeStr = (size == QRSize::Small) ? "S" : (size == QRSize::Medium) ? "M" : "L";
    Serial.printf("Drucke Label: %s / %s (QR: %s)\n", name, id, sizeStr);

    // Label-Bild generieren (use printer's label dimensions)
    LabelImage label(printer->getLabelWidth(), printer->getLabelHeight());
    if (!label.generate(link, name, id, size)) {
        Serial.printf("Label-Fehler: %s\n", label.getError());
        return label.getError();
    }

    // Debug: Base64 Data-URL ausgeben (klickbar im Browser)
    char* dataUrl = label.toDataURL();
    if (dataUrl) {
        Serial.println("Label-Vorschau:");
        Serial.println(dataUrl);
        free(dataUrl);
    }

    // Senden
    printer->sendBitmap(label.getData());
    Serial.println("Label gesendet!");
    return nullptr;  // Erfolg
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
// MQTT-Funktionen
// ============================================================

void sendResult(const char* printId, bool success, const char* error = nullptr) {
    if (!mqttConnected) return;

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
    mqttClient.publish(mqttTopicResult, buffer);
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
    const char* size = doc["size"] | "L";  // QR-Code Groesse: S, M, L (Default: L)

    const char* error = printLabel(link, name, id, size);
    sendResult(printId, error == nullptr, error);
}

void connectMQTT() {
    if (!wifiConnected) return;

    if (mqttUseSsl) {
        wifiClientSecure.setInsecure();  // Skip certificate verification
        mqttClient.setClient(wifiClientSecure);
        Serial.printf("Verbinde mit MQTT (SSL) %s:%d...\n", mqttServer, mqttPort);
    } else {
        mqttClient.setClient(wifiClient);
        Serial.printf("Verbinde mit MQTT %s:%d...\n", mqttServer, mqttPort);
    }

    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);

    if (mqttClient.connect(mqttClientId, mqttUser, mqttPassword)) {
        Serial.println("MQTT verbunden!");
        mqttClient.subscribe(mqttTopicPrint);
        Serial.printf("Subscribed: %s\n", mqttTopicPrint);
        mqttConnected = true;
    } else {
        Serial.printf("MQTT Fehler: %d\n", mqttClient.state());
        mqttConnected = false;
    }
}

unsigned long lastMqttRetry = 0;
const unsigned long MQTT_RETRY_INTERVAL = 10000;  // 10 Sekunden zwischen Verbindungsversuchen
int dnsFailCount = 0;
const int DNS_FAIL_RECONNECT_THRESHOLD = 3;  // Nach 3 DNS-Fehlern WiFi neu verbinden

bool checkDns(const char* hostname) {
    IPAddress ip;
    if (WiFi.hostByName(hostname, ip)) {
        dnsFailCount = 0;
        return true;
    }

    dnsFailCount++;
    Serial.printf("DNS fehlgeschlagen fuer %s (%d/%d)\n", hostname, dnsFailCount, DNS_FAIL_RECONNECT_THRESHOLD);

    if (dnsFailCount >= DNS_FAIL_RECONNECT_THRESHOLD) {
        Serial.println("Zu viele DNS-Fehler, WiFi wird neu verbunden...");
        dnsFailCount = 0;
        WiFi.disconnect();
        wifiConnected = false;
        delay(1000);
        while (!wifiConnected) {
            connectWiFi();
            if (!wifiConnected) {
                Serial.println("WiFi fehlgeschlagen, neuer Versuch in 5 Sekunden...");
                delay(5000);
            }
        }
    }
    return false;
}

void checkMQTT() {
    if (!wifiConnected) return;

    if (!mqttClient.connected()) {
        mqttConnected = false;
        unsigned long now = millis();
        if (now - lastMqttRetry >= MQTT_RETRY_INTERVAL) {
            lastMqttRetry = now;
            if (checkDns(mqttServer)) {
                connectMQTT();
            }
        }
    } else {
        mqttClient.loop();
    }
}

unsigned long lastStatusTime = 0;
const unsigned long STATUS_INTERVAL = 30000;  // 30 Sekunden

void publishStatus() {
    if (!mqttConnected || !printer) return;

    int battery = printer->getBattery();

    JsonDocument doc;
    doc["printer"] = printer->isConnected() ? "connected" : "disconnected";
    if (battery >= 0) {
        doc["battery"] = battery;
    }
    if (printer->getLastSeenMs() > 0) {
        doc["lastSeen"] = (millis() - printer->getLastSeenMs()) / 1000;
    }
    doc["wifi"] = WiFi.RSSI();
    doc["heap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(mqttTopicStatus, buffer);
    Serial.printf("Status gesendet: %s\n", buffer);
}

// ============================================================
// Benutzeroberfläche
// ============================================================

void printHelp() {
    Serial.println("\n=== Nelko P21 MQTT Printer ===");
    Serial.println("Status:");
    Serial.printf("  WiFi: %s\n", wifiConnected ? "Verbunden" : "Getrennt");
    Serial.printf("  MQTT: %s\n", mqttConnected ? "Verbunden" : "Getrennt");
    Serial.printf("  Drucker: %s\n", (printer && printer->isConnected()) ? "Verbunden" : "Getrennt");
    if (printer) {
        int battery = printer->getBattery();
        if (battery >= 0) {
            Serial.printf("  Batterie: %d%%\n", battery);
        }
    }
    Serial.println("Befehle:");
    Serial.println("  scan       - Drucker suchen & verbinden");
    Serial.println("  disconnect - Drucker trennen");
    Serial.println("  status     - Drucker-Status (Papier, etc.)");
    Serial.println("  config     - Drucker-Konfiguration");
    Serial.println("  battery    - Batterie abfragen");
    Serial.println("  frame      - Test: Rahmen drucken");
    Serial.println("  qrcode     - Test: QR-Code drucken");
    Serial.println("  wifi       - WiFi neu verbinden");
    Serial.println("  mqtt       - MQTT neu verbinden");
    Serial.println("  help       - Diese Hilfe");
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

    // WiFi verbinden
    connectWiFi();

    // MQTT verbinden (vor Bluetooth - SSL braucht viel RAM beim Handshake)
    connectMQTT();

    // Initialize printer (using Nelko P21 adapter)
    printer = new NelkoP21Printer();
    if (printer->connect()) {
        Serial.println("Drucker bereit.");
    }

    //printHelp();
}

void loop() {
    // MQTT verarbeiten
    checkMQTT();

    // Periodisch Status senden
    if (millis() - lastStatusTime >= STATUS_INTERVAL) {
        lastStatusTime = millis();
        publishStatus();
    }

    // Serielle Befehle
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
            connectWiFi();
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
        else if (cmdLower == "ready" || cmdLower == "status") {
            if (printer) printer->queryStatus();
        }
        else if (cmdLower.startsWith("send ")) {
            // Sende beliebigen Befehl: "send BEFEHL" (für Debugging)
            if (printer) printer->sendCommand(cmd.substring(5).c_str());
        }
    }

    // Drucker-Nachrichten verarbeiten
    if (printer) {
        printer->processIncoming();
    }

    delay(10);
}
