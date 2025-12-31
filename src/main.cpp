#include <Arduino.h>
#include "BluetoothSerial.h"
#include "qrcode.h"

BluetoothSerial SerialBT;

// Nelko P21 Label-Konfiguration
const int LABEL_WIDTH = 96;
const int LABEL_HEIGHT = 284;
const int BYTES_PER_ROW = LABEL_WIDTH / 8;
const int BITMAP_SIZE = LABEL_HEIGHT * BYTES_PER_ROW;

bool printerConnected = false;

// Bluetooth-Geräte
struct BTDevice {
    String name;
    uint8_t address[6];
};
BTDevice foundDevices[20];
int deviceCount = 0;

// ============================================================
// Bluetooth-Funktionen
// ============================================================

void startScan() {
    Serial.println("\n=== Starte Bluetooth-Scan (10 Sek.) ===");
    deviceCount = 0;

    BTScanResults* scanResults = SerialBT.discover(10000);
    if (!scanResults) {
        Serial.println("Scan fehlgeschlagen!");
        return;
    }

    int count = scanResults->getCount();
    Serial.printf("\nScan abgeschlossen. %d Geräte gefunden.\n", count);

    for (int i = 0; i < count && deviceCount < 20; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.length() == 0) name = "(kein Name)";

        foundDevices[deviceCount].name = name;
        memcpy(foundDevices[deviceCount].address, device->getAddress().getNative(), 6);
        deviceCount++;

        Serial.printf("Gefunden: %s [%02X:%02X:%02X:%02X:%02X:%02X]",
            name.c_str(),
            foundDevices[deviceCount-1].address[0], foundDevices[deviceCount-1].address[1],
            foundDevices[deviceCount-1].address[2], foundDevices[deviceCount-1].address[3],
            foundDevices[deviceCount-1].address[4], foundDevices[deviceCount-1].address[5]);

        if (name.indexOf("P21") >= 0 || name.indexOf("Nelko") >= 0) {
            Serial.print(" <<< Drucker");
        }
        Serial.println();
    }
}

void listDevices() {
    Serial.printf("\n=== Gefundene Geräte (%d) ===\n", deviceCount);
    for (int i = 0; i < deviceCount; i++) {
        Serial.printf("%d: %s [%02X:%02X:%02X:%02X:%02X:%02X]\n",
            i + 1, foundDevices[i].name.c_str(),
            foundDevices[i].address[0], foundDevices[i].address[1],
            foundDevices[i].address[2], foundDevices[i].address[3],
            foundDevices[i].address[4], foundDevices[i].address[5]);
    }
    Serial.println("=============================\n");
}

bool connectToDevice(int index) {
    if (index < 1 || index > deviceCount) {
        Serial.printf("Ungültiger Index. Wähle 1-%d\n", deviceCount);
        return false;
    }

    BTDevice& dev = foundDevices[index - 1];
    Serial.printf("Verbinde mit '%s'...\n", dev.name.c_str());

    if (SerialBT.connect(dev.address)) {
        Serial.println("Verbunden!");
        printerConnected = true;
        return true;
    }
    Serial.println("Verbindung fehlgeschlagen!");
    return false;
}

void autoConnect() {
    Serial.println("\nSuche Drucker...");

    BTScanResults* scanResults = SerialBT.discover(5000);
    if (!scanResults) {
        Serial.println("Scan fehlgeschlagen!");
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

        Serial.println("Verbinde...");
        if (SerialBT.connect(addr)) {
            Serial.println("Verbunden!");
            printerConnected = true;
            foundDevices[0].name = name;
            memcpy(foundDevices[0].address, addr, 6);
            deviceCount = 1;
            return;
        }
        Serial.println("Verbindung fehlgeschlagen!");
    }

    Serial.println("Kein Drucker gefunden. Nutze 'scan' zum manuellen Suchen.");
}

void disconnect() {
    if (printerConnected) {
        SerialBT.disconnect();
        printerConnected = false;
        Serial.println("Verbindung getrennt.");
    }
}

// ============================================================
// TSPL2 Drucker-Befehle
// ============================================================

void sendTSPL(const char* cmd) {
    SerialBT.print(cmd);
    SerialBT.print("\r\n");
    Serial.printf("TSPL> %s\n", cmd);
    delay(50);
}

String readResponse(unsigned long timeout = 1000) {
    String response = "";
    unsigned long start = millis();
    while (millis() - start < timeout) {
        if (SerialBT.available()) {
            response += (char)SerialBT.read();
        }
        delay(10);
    }
    return response;
}

void queryBattery() {
    sendTSPL("BATTERY?");
    delay(200);
    String resp = readResponse(500);
    if (resp.length() > 0) {
        Serial.print("Batterie: ");
        for (size_t i = 0; i < resp.length(); i++) {
            Serial.printf("%02X ", (uint8_t)resp[i]);
        }
        Serial.println();
    }
}

void queryConfig() {
    sendTSPL("CONFIG?");
    delay(200);
    String resp = readResponse(500);
    if (resp.length() > 0) {
        Serial.print("Config: ");
        for (size_t i = 0; i < resp.length(); i++) {
            Serial.printf("%02X ", (uint8_t)resp[i]);
        }
        Serial.println();
    }
}

// ============================================================
// Bitmap-Druckfunktionen
// ============================================================

// Sendet vollständiges Label-Bitmap (96x284) an Drucker
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

// Hilfsfunktion: Pixel im Bitmap setzen (schwarz)
inline void setPixel(uint8_t* bitmap, int x, int y) {
    if (x >= 0 && x < LABEL_WIDTH && y >= 0 && y < LABEL_HEIGHT) {
        int byteIdx = y * BYTES_PER_ROW + (x / 8);
        int bitIdx = 7 - (x % 8);
        bitmap[byteIdx] &= ~(1 << bitIdx);
    }
}

void printFrame() {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    Serial.println("Drucke Rahmen...");

    uint8_t* bitmap = (uint8_t*)malloc(BITMAP_SIZE);
    if (!bitmap) {
        Serial.println("Speicherfehler!");
        return;
    }

    memset(bitmap, 0xFF, BITMAP_SIZE);

    // Obere und untere Kante
    memset(bitmap, 0x00, BYTES_PER_ROW);
    memset(bitmap + (LABEL_HEIGHT - 1) * BYTES_PER_ROW, 0x00, BYTES_PER_ROW);

    // Linke und rechte Kante
    for (int y = 0; y < LABEL_HEIGHT; y++) {
        bitmap[y * BYTES_PER_ROW] &= 0x7F;
        bitmap[y * BYTES_PER_ROW + BYTES_PER_ROW - 1] &= 0xFE;
    }

    sendLabelBitmap(bitmap);
    free(bitmap);
    Serial.println("Gesendet!");
}

void printQRCode(const char* text) {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    Serial.printf("Generiere QR-Code: %s\n", text);

    // Finde optimale Version und Skalierung (1x, 2x oder 3x)
    QRCode qrcode;
    int bestVersion = 0, bestScale = 0, bestSize = 0;

    for (int version = 3; version <= 12; version++) {
        uint8_t tempData[qrcode_getBufferSize(version)];
        if (qrcode_initText(&qrcode, tempData, version, ECC_MEDIUM, text) != 0) continue;

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
        Serial.println("QR-Code kann nicht generiert werden!");
        return;
    }

    uint8_t qrcodeData[qrcode_getBufferSize(bestVersion)];
    qrcode_initText(&qrcode, qrcodeData, bestVersion, ECC_MEDIUM, text);

    int qrSize = qrcode.size;
    int offsetX = (LABEL_WIDTH - qrSize * bestScale) / 2;
    int offsetY = (LABEL_HEIGHT - qrSize * bestScale) / 2;

    Serial.printf("Version %d, %dx%d @ %dx = %d Pixel\n",
        bestVersion, qrSize, qrSize, bestScale, qrSize * bestScale);

    uint8_t* bitmap = (uint8_t*)malloc(BITMAP_SIZE);
    if (!bitmap) {
        Serial.println("Speicherfehler!");
        return;
    }

    memset(bitmap, 0xFF, BITMAP_SIZE);

    for (int qy = 0; qy < qrSize; qy++) {
        for (int qx = 0; qx < qrSize; qx++) {
            if (qrcode_getModule(&qrcode, qx, qy)) {
                for (int sy = 0; sy < bestScale; sy++) {
                    for (int sx = 0; sx < bestScale; sx++) {
                        setPixel(bitmap,
                            offsetX + qx * bestScale + sx,
                            offsetY + qy * bestScale + sy);
                    }
                }
            }
        }
    }

    sendLabelBitmap(bitmap);
    free(bitmap);
    Serial.println("Gesendet!");
}

// ============================================================
// Benutzeroberfläche
// ============================================================

void printHelp() {
    Serial.println("\n=== Nelko P21 Controller ===");
    Serial.println("Verbindung:");
    Serial.println("  scan     - Bluetooth-Geräte suchen");
    Serial.println("  list     - Gefundene Geräte anzeigen");
    Serial.println("  c <nr>   - Mit Gerät verbinden");
    Serial.println("  disc     - Verbindung trennen");
    Serial.println("  status   - Verbindungsstatus");
    Serial.println("Drucken:");
    Serial.println("  battery  - Batteriestatus");
    Serial.println("  config   - Konfiguration");
    Serial.println("  frame    - Rahmen");
    Serial.println("  qrcode   - QR-Code (Makerspace)");
    Serial.println("  help     - Diese Hilfe");
    Serial.println("============================\n");
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n========================================");
    Serial.println("  Nelko P21 Label Printer - ESP32");
    Serial.println("========================================");

    if (!SerialBT.begin("ESP32_LabelPrinter", true)) {
        Serial.println("Bluetooth-Fehler!");
        while(1);
    }

    Serial.println("Bluetooth initialisiert");

    SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
        if (event == ESP_SPP_CLOSE_EVT) {
            Serial.println("Verbindung getrennt!");
            printerConnected = false;
        }
    });

    autoConnect();
    printHelp();
}

void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        Serial.printf("> %s\n", cmd.c_str());
        cmd.toLowerCase();

        if (cmd == "scan") {
            startScan();
        }
        else if (cmd == "list") {
            listDevices();
        }
        else if (cmd.startsWith("c ")) {
            connectToDevice(cmd.substring(2).toInt());
        }
        else if (cmd == "disc") {
            disconnect();
        }
        else if (cmd == "status") {
            Serial.printf("Status: %s\n", printerConnected ? "Verbunden" : "Nicht verbunden");
        }
        else if (cmd == "help" || cmd == "?") {
            printHelp();
        }
        else if (cmd == "battery") {
            if (printerConnected) queryBattery();
            else Serial.println("Nicht verbunden!");
        }
        else if (cmd == "config") {
            if (printerConnected) queryConfig();
            else Serial.println("Nicht verbunden!");
        }
        else if (cmd == "frame") {
            printFrame();
        }
        else if (cmd == "qrcode") {
            printQRCode("https://zeug.makerspacebonn.de/i/1259");
        }
        else if (cmd.length() > 0 && printerConnected) {
            sendTSPL(cmd.c_str());
            delay(100);
            String resp = readResponse();
            if (resp.length() > 0) {
                Serial.printf("Antwort: %s\n", resp.c_str());
            }
        }
    }

    if (printerConnected && SerialBT.available()) {
        Serial.print("Drucker: ");
        while (SerialBT.available()) {
            Serial.print((char)SerialBT.read());
        }
        Serial.println();
    }

    delay(10);
}