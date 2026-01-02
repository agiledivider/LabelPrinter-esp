#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include "qrcode.h"
#include "config.h"
#include "font5x7.h"

// Label-Konfiguration
const int LABEL_WIDTH = 96;
const int LABEL_HEIGHT = 284;
const int BYTES_PER_ROW = LABEL_WIDTH / 8;
const int BITMAP_SIZE = LABEL_HEIGHT * BYTES_PER_ROW;

// ============================================================
// Globale Objekte
// ============================================================

BluetoothSerial SerialBT;
WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient;

bool printerConnected = false;
bool wifiConnected = false;
bool mqttConnected = false;
int printerBattery = -1;  // -1 = unbekannt
unsigned long printerLastSeen = 0;  // millis() wenn zuletzt verbunden

// Bluetooth-Geräte
struct BTDevice {
    String name;
    uint8_t address[6];
};
BTDevice foundDevices[20];
int deviceCount = 0;

// ============================================================
// Forward Declarations
// ============================================================

void queryBattery();
void queryConfig();
void queryStatus();

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
// Bluetooth-Funktionen
// ============================================================

void autoConnectPrinter() {
    Serial.println("Suche Bluetooth-Geraete...");

    BTScanResults* scanResults = SerialBT.discover(5000);
    if (!scanResults) {
        Serial.println("Bluetooth-Scan fehlgeschlagen! Versuche Neustart...");

        // Bluetooth neu initialisieren
        SerialBT.disconnect();
        SerialBT.end();
        btStop();
        delay(500);
        btStart();
        if (!SerialBT.begin("ESP32_LabelPrinter", true)) {
            Serial.println("Bluetooth-Neustart fehlgeschlagen!");
            return;
        }

        // Callback neu registrieren
        SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
            if (event == ESP_SPP_CLOSE_EVT) {
                Serial.println("Drucker getrennt!");
                printerConnected = false;
            }
        });
        delay(500);

        // Erneut versuchen
        scanResults = SerialBT.discover(5000);
        if (!scanResults) {
            Serial.println("Bluetooth-Scan erneut fehlgeschlagen!");
            return;
        }
    }

    int count = scanResults->getCount();
    Serial.printf("%d Geraete gefunden:\n", count);

    // Alle Geräte anzeigen
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.length() == 0) name = "(unbekannt)";

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);
        Serial.printf("  [%d] %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
            i, name.c_str(),
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }

    // Nach Drucker suchen
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.indexOf("P21") < 0 && name.indexOf("Nelko") < 0) continue;

        Serial.printf("Verbinde mit Drucker: %s\n", name.c_str());

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);

        if (SerialBT.connect(addr)) {
            Serial.println("Drucker verbunden!\n");
            printerConnected = true;
            printerLastSeen = millis();
            foundDevices[0].name = name;
            memcpy(foundDevices[0].address, addr, 6);
            deviceCount = 1;

            // Config und Status abfragen
            delay(500);  // Kurz warten nach Verbindung
            queryConfig();
            delay(300);
            queryBattery();
            delay(300);
            queryStatus();
            return;
        }
    }

    Serial.println("Kein Drucker gefunden.");
}

void disconnect() {
    if (printerConnected) {
        SerialBT.disconnect();
        printerConnected = false;
        Serial.println("Drucker getrennt.");
    }
}

void queryBattery() {
    if (!printerConnected) {
        printerBattery = -1;
        return;
    }

    // Buffer leeren - warten auf spaete Daten
    delay(100);
    while (SerialBT.available()) SerialBT.read();

    // Batterie abfragen (korrekter Befehl: BATTERY?)
    SerialBT.print("BATTERY?\r\n");
    delay(300);

    // Drucker sendet Echo zurück: "BATTERY?" + 2 Bytes Antwort
    // Echo überspringen (8 Zeichen "BATTERY?")
    unsigned long start = millis();
    int echoCount = 0;
    while (millis() - start < 500 && echoCount < 8) {
        if (SerialBT.available()) {
            SerialBT.read();  // Echo-Byte verwerfen
            echoCount++;
        }
    }

    // Jetzt die 2 Antwort-Bytes lesen (erstes Byte = BCD Prozent)
    start = millis();
    while (millis() - start < 300) {
        if (SerialBT.available()) {
            uint8_t raw = SerialBT.read();
            // Rest der Antwort verwerfen (CRLF etc.)
            delay(200);
            while (SerialBT.available()) SerialBT.read();

            // BCD dekodieren: 0x99 = 99%, 0x66 = 66%
            int level = ((raw >> 4) & 0x0F) * 10 + (raw & 0x0F);

            if (level >= 0 && level <= 100) {
                printerBattery = level;
                Serial.printf("Batterie: %d%%\n", printerBattery);
            } else {
                Serial.printf("Batterie raw: 0x%02X (%d)\n", raw, level);
            }
            return;
        }
    }
    Serial.println("Batterie: keine Antwort");
}

// Status abfragen und anzeigen (16 Bytes)
void queryStatus() {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    // Buffer leeren - warten auf spaete Daten
    delay(100);
    Serial.println("Buffer leeren...");
    while (SerialBT.available()) {
        Serial.print(SerialBT.read());
    }
    Serial.println("...fertig.\r\n");
    // ESC!o senden
    SerialBT.print("\x1B!o\r\n");

    // 16 Byte Antwort lesen
    uint8_t response[16];
    int count = 0;
    unsigned long start = millis();
    Serial.println("Response...");
    while (millis() - start < 1000 && count < 16) {
        if (SerialBT.available()) {
            response[count] = SerialBT.read();
            Serial.print(response[count]);
            count++;
        }
    }
    Serial.println("\r\n...fertig.\r\n");

    // Rest verwerfen
    Serial.println("Rest verwerfen...");
    while (SerialBT.available()) {
        Serial.print(SerialBT.read());
    }
    Serial.println("...fertig.\r\n");

    if (count < 1) {
        Serial.println("Status: (keine Antwort)");
        return;
    }

    Serial.println("=== Drucker-Status ===");

    // Status interpretieren
    if (response[0] == 0x00) {
        Serial.println("  Status:       OK");
        if (count >= 14) {
            Serial.printf("  Papier:       %d x %d mm\n", response[13], response[11]);
        }
    } else if (response[0] == 0x04) {
        Serial.println("  Status:       FEHLER - Kein Papier!");
    } else {
        Serial.printf("  Status:       Unbekannt (0x%02X)\n", response[0]);
    }

    // Raw hex dump
    Serial.print("  Raw:          ");
    for (int i = 0; i < count; i++) {
        Serial.printf("%02X ", response[i]);
    }
    Serial.println();
    Serial.println("======================");
}

// Config abfragen (10 Bytes: Protokoll, DPI, HW-Version, FW-Version, Timeout, Beep)
void queryConfig() {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    // Buffer leeren
    while (SerialBT.available()) SerialBT.read();

    SerialBT.print("CONFIG?\r\n");
    delay(200);

    // Echo überspringen (7 Zeichen "CONFIG?")
    unsigned long start = millis();
    int echoCount = 0;
    while (millis() - start < 500 && echoCount < 7) {
        if (SerialBT.available()) {
            SerialBT.read();
            echoCount++;
        }
    }

    // 10 Bytes Config lesen
    uint8_t config[10];
    start = millis();
    int count = 0;
    while (millis() - start < 500 && count < 10) {
        if (SerialBT.available()) {
            config[count++] = SerialBT.read();
        }
    }

    // Rest verwerfen
    while (SerialBT.available()) SerialBT.read();

    if (count < 10) {
        Serial.println("Config: (unvollstaendige Antwort)");
        return;
    }

    // Human-readable output
    Serial.println("=== Drucker-Konfiguration ===");
    Serial.printf("  Protokoll:    %s\n", config[0] == 0 ? "TSPL2" : "Unbekannt");
    Serial.printf("  DPI:          %d\n", config[1]);
    Serial.printf("  Hardware:     v%d.%d.%d\n", config[2], config[3], config[4]);
    Serial.printf("  Firmware:     v%d.%d.%d\n", config[5], config[6], config[7]);

    const char* timeouts[] = {"Nie", "15 Min", "30 Min", "60 Min"};
    int timeoutIdx = config[8] < 4 ? config[8] : 0;
    Serial.printf("  Auto-Off:     %s\n", timeouts[timeoutIdx]);
    Serial.printf("  Beep:         %s\n", config[9] ? "An" : "Aus");

    // Raw hex dump
    Serial.print("  Raw:          ");
    for (int i = 0; i < 10; i++) {
        Serial.printf("%02X ", config[i]);
    }
    Serial.println();
    Serial.println("==============================");
}

// Debug: Sendet Befehl und zeigt Antwort
void debugPrinterCommand(const char* cmd) {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    // Buffer leeren
    while (SerialBT.available()) SerialBT.read();

    Serial.printf("Sende: %s\n", cmd);
    SerialBT.print(cmd);
    SerialBT.print("\r\n");
    delay(500);

    Serial.print("Antwort: ");
    bool gotData = false;
    unsigned long start = millis();
    while (millis() - start < 1000) {
        if (SerialBT.available()) {
            uint8_t c = SerialBT.read();
            Serial.printf("[0x%02X '%c'] ", c, (c >= 32 && c < 127) ? c : '.');
            gotData = true;
        }
    }
    if (!gotData) {
        Serial.print("(keine)");
    }
    Serial.println();
}

// Prüft Drucker-Status mit ESC!o Befehl
// Gibt Fehlermeldung zurück oder nullptr wenn OK
const char* checkPrinterStatus() {
    queryStatus();

    if (!printerConnected || !SerialBT.connected()) {
        printerConnected = false;
        return "printer not connected";
    }

    // Buffer leeren
    while (SerialBT.available()) SerialBT.read();

    // Status abfragen mit ESC!o (0x1B 0x21 0x6F)
    SerialBT.write(0x1B);
    SerialBT.write('!');
    SerialBT.write('o');
    SerialBT.print("\r\n");
    delay(300);

    // 16 Byte Antwort lesen
    uint8_t response[16];
    int count = 0;
    unsigned long start = millis();
    while (millis() - start < 500 && count < 16) {
        if (SerialBT.available()) {
            response[count++] = SerialBT.read();
        }
    }

    Serial.print("Response (Hex): ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X ", response[i]);
    }
    Serial.println();

    // Rest verwerfen
    while (SerialBT.available()) SerialBT.read();

    if (count >= 1) {
        // Byte 0 = Status: 0x00 = OK, 0x04 = Papier-Fehler
        if (response[0] == 0x00) {
            return nullptr;  // OK
        } else if (response[0] == 0x04) {
            return "no paper";
        } else {
            Serial.printf("Unbekannter Status: 0x%02X\n", response[0]);
            return nullptr;  // Unbekannt, aber weitermachen
        }
    }

    // Keine Antwort - evtl. SPP unterstützt den Befehl nicht
    return nullptr;
}

// ============================================================
// Bitmap-Funktionen
// ============================================================

inline void setPixel(uint8_t* bitmap, int x, int y) {
    if (x >= 0 && x < LABEL_WIDTH && y >= 0 && y < LABEL_HEIGHT) {
        int byteIdx = y * BYTES_PER_ROW + (x / 8);
        int bitIdx = 7 - (x % 8);
        bitmap[byteIdx] &= ~(1 << bitIdx);
    }
}

void sendLabelBitmap(uint8_t* bitmap) {
    String header = "SIZE 14.0 mm,40.0 mm\r\n"
                    "GAP 5.0 mm,0 mm\r\n"
                    "DIRECTION 0,0\r\n"
                    "DENSITY 15\r\n"
                    "CLS\r\n"
                    "BITMAP 0,0,12,284,1,";

    SerialBT.print(header);
    SerialBT.write(bitmap, BITMAP_SIZE);
    SerialBT.print("\r\nPRINT 1\r\n");
}

// Text auf Bitmap zeichnen (zentriert)
void drawTextCentered(uint8_t* bitmap, const char* text, int y) {
    int len = countDisplayChars(text);  // UTF-8 aware count
    int textWidth = len * 6 - 1;  // 5 Pixel + 1 Pixel Abstand
    int startX = (LABEL_WIDTH - textWidth) / 2;
    if (startX < 0) startX = 0;

    const char* ptr = text;
    int charIndex = 0;
    while (*ptr && startX + charIndex * 6 < LABEL_WIDTH) {
        const uint8_t* glyph = getGlyph(&ptr);

        for (int col = 0; col < 5; col++) {
            uint8_t colData = pgm_read_byte(&glyph[col]);
            for (int row = 0; row < 7; row++) {
                if (colData & (1 << row)) {
                    setPixel(bitmap, startX + charIndex * 6 + col, y + row);
                }
            }
        }
        charIndex++;
    }
}

// ============================================================
// Druck-Funktionen
// ============================================================

// Gibt nullptr bei Erfolg zurück, sonst Fehlermeldung
const char* printLabel(const char* link, const char* name, const char* id) {
    if (!printerConnected) {
        Serial.println("Drucker nicht verbunden!");
        return "printer not connected";
    }

    // Drucker-Status prüfen (Papier, etc.)
    const char* statusError = checkPrinterStatus();
    if (statusError) {
        Serial.printf("Drucker-Fehler: %s\n", statusError);
        return statusError;
    }

    if (!link || strlen(link) == 0) {
        return "missing link";
    }

    Serial.printf("Drucke Label: %s / %s\n", name, id);

    // QR-Code generieren - kleinste Version die passt
    QRCode qrcode;
    int bestVersion = 0, bestScale = 1;

    // Finde kleinste Version die den Text kodieren kann (min. Version 3 für URLs)
    for (int version = 3; version <= 12; version++) {
        uint8_t tempData[qrcode_getBufferSize(version)];
        if (qrcode_initText(&qrcode, tempData, version, ECC_MEDIUM, link) == 0) {
            bestVersion = version;
            break;  // Erste passende Version nehmen
        }
    }

    if (bestVersion == 0) {
        Serial.println("QR-Code Fehler!");
        return "QR code generation failed";
    }

    uint8_t qrcodeData[qrcode_getBufferSize(bestVersion)];
    qrcode_initText(&qrcode, qrcodeData, bestVersion, ECC_MEDIUM, link);

    int qrSize = qrcode.size;

    // Maximale Skalierung finden die noch passt
    bestScale = LABEL_WIDTH / qrSize;
    if (bestScale < 1) bestScale = 1;

    int qrPixels = qrSize * bestScale;
    Serial.printf("QR: Version %d, %dx%d, Scale %dx = %d Pixel\n",
        bestVersion, qrSize, qrSize, bestScale, qrPixels);

    // Layout berechnen (Portrait: QR oben, Text unten)
    int qrY = 10;  // QR-Code beginnt bei Y=10
    int qrX = (LABEL_WIDTH - qrPixels) / 2;
    int textY1 = qrY + qrPixels + 15;  // Name
    int textY2 = textY1 + 12;          // ID

    // Bitmap erstellen
    uint8_t* bitmap = (uint8_t*)malloc(BITMAP_SIZE);
    if (!bitmap) {
        Serial.println("Speicherfehler!");
        return "out of memory";
    }
    memset(bitmap, 0xFF, BITMAP_SIZE);

    // QR-Code zeichnen
    for (int qy = 0; qy < qrSize; qy++) {
        for (int qx = 0; qx < qrSize; qx++) {
            if (qrcode_getModule(&qrcode, qx, qy)) {
                for (int sy = 0; sy < bestScale; sy++) {
                    for (int sx = 0; sx < bestScale; sx++) {
                        setPixel(bitmap, qrX + qx * bestScale + sx, qrY + qy * bestScale + sy);
                    }
                }
            }
        }
    }

    // Text zeichnen
    drawTextCentered(bitmap, name, textY1);

    char idLine[32];
    snprintf(idLine, sizeof(idLine), "ID: %s", id);
    drawTextCentered(bitmap, idLine, textY2);

    // Senden
    sendLabelBitmap(bitmap);
    free(bitmap);
    Serial.println("Label gesendet!");
    return nullptr;  // Erfolg
}

void printFrame() {
    if (!printerConnected) {
        Serial.println("Drucker nicht verbunden!");
        return;
    }

    uint8_t* bitmap = (uint8_t*)malloc(BITMAP_SIZE);
    if (!bitmap) return;

    memset(bitmap, 0xFF, BITMAP_SIZE);
    memset(bitmap, 0x00, BYTES_PER_ROW);
    memset(bitmap + (LABEL_HEIGHT - 1) * BYTES_PER_ROW, 0x00, BYTES_PER_ROW);

    for (int y = 0; y < LABEL_HEIGHT; y++) {
        bitmap[y * BYTES_PER_ROW] &= 0x7F;
        bitmap[y * BYTES_PER_ROW + BYTES_PER_ROW - 1] &= 0xFE;
    }

    sendLabelBitmap(bitmap);
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

    const char* error = printLabel(link, name, id);
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
    if (!mqttConnected) return;

    queryBattery();

    // Update last seen wenn verbunden
    if (printerConnected) {
        printerLastSeen = millis();
    }

    JsonDocument doc;
    doc["printer"] = printerConnected ? "connected" : "disconnected";
    if (printerBattery >= 0) {
        doc["battery"] = printerBattery;
    }
    if (printerLastSeen > 0) {
        doc["lastSeen"] = (millis() - printerLastSeen) / 1000;  // Sekunden seit letzter Verbindung
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
    Serial.printf("  Drucker: %s\n", printerConnected ? "Verbunden" : "Getrennt");
    if (printerBattery >= 0) {
        Serial.printf("  Batterie: %d%%\n", printerBattery);
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

    // Bluetooth initialisieren
    if (!SerialBT.begin("ESP32_LabelPrinter", true)) {
        Serial.println("Bluetooth-Fehler!");
    } else {
        Serial.println("Bluetooth initialisiert");

        SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
            if (event == ESP_SPP_CLOSE_EVT) {
                Serial.println("Drucker getrennt!");
                printerConnected = false;
            }
        });

        autoConnectPrinter();
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
            autoConnectPrinter();
        }
        else if (cmdLower == "disconnect") {
            disconnect();
        }
        else if (cmdLower == "frame") {
            printFrame();
        }
        else if (cmdLower == "qrcode") {
            printLabel("https://zeug.makerspacebonn.de/i/1259", "Test Item", "1259");
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
            queryBattery();
        }
        else if (cmdLower == "config") {
            queryConfig();
        }
        else if (cmdLower == "ready" || cmdLower == "status") {
            queryStatus();
        }
        else if (cmdLower.startsWith("send ")) {
            // Sende beliebigen Befehl: "send BEFEHL" (für Debugging)
            debugPrinterCommand(cmd.substring(5).c_str());
        }
    }

    // Drucker-Nachrichten
    if (printerConnected && SerialBT.available()) {
        while (SerialBT.available()) {
            Serial.print((char)SerialBT.read());
        }
    }

    delay(10);
}
