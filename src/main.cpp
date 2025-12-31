#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"
#include "qrcode.h"
#include "config.h"

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

// Bluetooth-Geräte
struct BTDevice {
    String name;
    uint8_t address[6];
};
BTDevice foundDevices[20];
int deviceCount = 0;

// ============================================================
// 5x7 Pixel Font (ASCII 32-126)
// ============================================================

const uint8_t font5x7[][5] PROGMEM = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 Space
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // 42 *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x00, 0x08, 0x14, 0x22, 0x41}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x41, 0x22, 0x14, 0x08, 0x00}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // 70 F
    {0x3E, 0x41, 0x41, 0x51, 0x32}, // 71 G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // 77 M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x00, 0x7F, 0x41, 0x41}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 backslash
    {0x41, 0x41, 0x7F, 0x00, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 f
    {0x08, 0x14, 0x54, 0x54, 0x3C}, // 103 g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 j
    {0x00, 0x7F, 0x10, 0x28, 0x44}, // 107 k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // 124 |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 }
    {0x08, 0x08, 0x2A, 0x1C, 0x08}, // 126 ~
};

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
    Serial.println("Suche Drucker...");

    BTScanResults* scanResults = SerialBT.discover(5000);
    if (!scanResults) {
        Serial.println("Bluetooth-Scan fehlgeschlagen!");
        return;
    }

    int count = scanResults->getCount();
    Serial.printf("%d Geräte gefunden.\n", count);

    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.indexOf("P21") < 0 && name.indexOf("Nelko") < 0) continue;

        Serial.printf("Drucker gefunden: %s\n", name.c_str());

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);

        if (SerialBT.connect(addr)) {
            Serial.println("Drucker verbunden!");
            printerConnected = true;
            foundDevices[0].name = name;
            memcpy(foundDevices[0].address, addr, 6);
            deviceCount = 1;
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
    int len = strlen(text);
    int textWidth = len * 6 - 1;  // 5 Pixel + 1 Pixel Abstand
    int startX = (LABEL_WIDTH - textWidth) / 2;
    if (startX < 0) startX = 0;

    for (int i = 0; i < len && startX + i * 6 < LABEL_WIDTH; i++) {
        char c = text[i];
        if (c < 32 || c > 126) c = '?';

        for (int col = 0; col < 5; col++) {
            uint8_t colData = pgm_read_byte(&font5x7[c - 32][col]);
            for (int row = 0; row < 7; row++) {
                if (colData & (1 << row)) {
                    setPixel(bitmap, startX + i * 6 + col, y + row);
                }
            }
        }
    }
}

// ============================================================
// Druck-Funktionen
// ============================================================

void printLabel(const char* link, const char* name, const char* id) {
    if (!printerConnected) {
        Serial.println("Drucker nicht verbunden!");
        return;
    }

    Serial.printf("Drucke Label: %s / %s\n", name, id);

    // QR-Code generieren
    QRCode qrcode;
    int bestVersion = 0, bestScale = 0, bestSize = 0;

    for (int version = 3; version <= 12; version++) {
        uint8_t tempData[qrcode_getBufferSize(version)];
        if (qrcode_initText(&qrcode, tempData, version, ECC_MEDIUM, link) != 0) continue;

        for (int scale = 3; scale >= 1; scale--) {
            int size = qrcode.size * scale;
            if (size <= LABEL_WIDTH && size > bestSize) {
                bestVersion = version;
                bestScale = scale;
                bestSize = size;
            }
        }
    }

    if (bestVersion == 0) {
        Serial.println("QR-Code Fehler!");
        return;
    }

    uint8_t qrcodeData[qrcode_getBufferSize(bestVersion)];
    qrcode_initText(&qrcode, qrcodeData, bestVersion, ECC_MEDIUM, link);

    int qrSize = qrcode.size;
    int qrPixels = qrSize * bestScale;

    // Layout berechnen (Portrait: QR oben, Text unten)
    int qrY = 10;  // QR-Code beginnt bei Y=10
    int qrX = (LABEL_WIDTH - qrPixels) / 2;
    int textY1 = qrY + qrPixels + 15;  // Name
    int textY2 = textY1 + 12;          // ID

    // Bitmap erstellen
    uint8_t* bitmap = (uint8_t*)malloc(BITMAP_SIZE);
    if (!bitmap) {
        Serial.println("Speicherfehler!");
        return;
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

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    Serial.printf("MQTT Nachricht auf %s\n", topic);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.printf("JSON Fehler: %s\n", error.c_str());
        return;
    }

    const char* link = doc["link"] | "";
    const char* name = doc["name"] | "";
    const char* id = doc["id"] | "";

    if (strlen(link) > 0) {
        printLabel(link, name, id);
    }
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
        mqttClient.subscribe(mqttTopic);
        Serial.printf("Subscribed: %s\n", mqttTopic);
        mqttConnected = true;
    } else {
        Serial.printf("MQTT Fehler: %d\n", mqttClient.state());
        mqttConnected = false;
    }
}

void checkMQTT() {
    if (!wifiConnected) return;

    if (!mqttClient.connected()) {
        mqttConnected = false;
        connectMQTT();
    }
    mqttClient.loop();
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
    Serial.println("Befehle:");
    Serial.println("  scan    - Drucker suchen");
    Serial.println("  frame   - Rahmen drucken");
    Serial.println("  qrcode  - Test QR-Code");
    Serial.println("  wifi    - WiFi neu verbinden");
    Serial.println("  mqtt    - MQTT neu verbinden");
    Serial.println("  help    - Diese Hilfe");
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

    printHelp();
}

void loop() {
    // MQTT verarbeiten
    checkMQTT();

    // Serielle Befehle
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toLowerCase();

        if (cmd == "scan") {
            autoConnectPrinter();
        }
        else if (cmd == "frame") {
            printFrame();
        }
        else if (cmd == "qrcode") {
            printLabel("https://zeug.makerspacebonn.de/i/1259", "Test Item", "1259");
        }
        else if (cmd == "wifi") {
            connectWiFi();
        }
        else if (cmd == "mqtt") {
            connectMQTT();
        }
        else if (cmd == "help" || cmd == "?") {
            printHelp();
        }
        else if (cmd == "status") {
            printHelp();
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
