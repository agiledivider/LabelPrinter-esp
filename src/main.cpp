#include <Arduino.h>
#include "BluetoothSerial.h"

// Bluetooth Serial Objekt
BluetoothSerial SerialBT;

// Nelko P21 Drucker Konfiguration
const char* PRINTER_NAME = "P21";
const int LABEL_WIDTH = 96;    // Pixel
const int LABEL_HEIGHT = 284;  // Pixel
const int BYTES_PER_ROW = LABEL_WIDTH / 8;  // 12 Bytes pro Zeile

// Status
bool printerConnected = false;
bool scanning = false;

// Gefundene Geräte speichern
struct BTDevice {
    String name;
    uint8_t address[6];
    bool valid;
};
BTDevice foundDevices[20];
int deviceCount = 0;

// Gefundenes Gerät verarbeiten
void processFoundDevice(const char* name, esp_bd_addr_t address) {
    Serial.printf("Gefunden: %s", name);
    Serial.printf(" [%02X:%02X:%02X:%02X:%02X:%02X]",
                  address[0], address[1], address[2], address[3], address[4], address[5]);
    Serial.println();

    // Gerät speichern
    if (deviceCount < 20) {
        foundDevices[deviceCount].name = name;
        memcpy(foundDevices[deviceCount].address, address, 6);
        foundDevices[deviceCount].valid = true;
        deviceCount++;
    }

    // Prüfen ob es ein Nelko Drucker ist
    String deviceName = name;
    if (deviceName.indexOf("P21") >= 0 || deviceName.indexOf("Nelko") >= 0) {
        Serial.println(">>> Nelko P21 Drucker gefunden! <<<");
    }
}

// Bluetooth-Scan starten
void startScan() {
    Serial.println("\n=== Starte Bluetooth-Scan (10 Sek.) ===");
    deviceCount = 0;
    scanning = true;

    BTScanResults* scanResults = SerialBT.discover(10000);  // 10 Sekunden scannen

    if (scanResults) {
        int count = scanResults->getCount();
        Serial.printf("\nScan abgeschlossen. %d Geräte gefunden.\n", count);

        for (int i = 0; i < count; i++) {
            BTAdvertisedDevice* device = scanResults->getDevice(i);
            if (device) {
                String name = device->getName().c_str();
                if (name.length() == 0) {
                    name = "(kein Name)";
                }

                // Adresse holen
                uint8_t addr[6];
                memcpy(addr, device->getAddress().getNative(), 6);
                processFoundDevice(name.c_str(), addr);
            }
        }
    } else {
        Serial.println("Scan fehlgeschlagen!");
    }
    scanning = false;
}

// Mit Drucker verbinden (nach Name)
bool connectToPrinter(const char* name) {
    Serial.printf("Verbinde mit '%s'...\n", name);

    if (SerialBT.connect(name)) {
        Serial.println("Verbindung erfolgreich!");
        printerConnected = true;
        return true;
    } else {
        Serial.println("Verbindung fehlgeschlagen!");
        printerConnected = false;
        return false;
    }
}

// Mit Drucker verbinden (nach MAC-Adresse)
bool connectToPrinterByAddress(uint8_t* address) {
    Serial.printf("Verbinde mit MAC %02X:%02X:%02X:%02X:%02X:%02X...\n",
                  address[0], address[1], address[2], address[3], address[4], address[5]);

    if (SerialBT.connect(address)) {
        Serial.println("Verbindung erfolgreich!");
        printerConnected = true;
        return true;
    } else {
        Serial.println("Verbindung fehlgeschlagen!");
        printerConnected = false;
        return false;
    }
}

// Verbindung trennen
void disconnect() {
    if (printerConnected) {
        SerialBT.disconnect();
        printerConnected = false;
        Serial.println("Verbindung getrennt.");
    }
}

// Daten an Drucker senden
bool sendData(const uint8_t* data, size_t length) {
    if (!printerConnected) {
        Serial.println("Fehler: Nicht mit Drucker verbunden!");
        return false;
    }

    size_t written = SerialBT.write(data, length);
    return (written == length);
}

// Text-Befehl an Drucker senden
bool sendCommand(const char* command) {
    if (!printerConnected) {
        Serial.println("Fehler: Nicht mit Drucker verbunden!");
        return false;
    }

    SerialBT.println(command);
    Serial.printf("Gesendet: %s\n", command);
    return true;
}

// Antwort vom Drucker lesen
String readResponse(unsigned long timeout = 1000) {
    String response = "";
    unsigned long startTime = millis();

    while (millis() - startTime < timeout) {
        if (SerialBT.available()) {
            char c = SerialBT.read();
            response += c;
        }
        delay(10);
    }

    return response;
}

// Mit Gerät aus Liste verbinden (per Index)
bool connectToDevice(int index) {
    if (index < 1 || index > deviceCount) {
        Serial.printf("Ungültiger Index. Wähle 1-%d\n", deviceCount);
        return false;
    }

    BTDevice& dev = foundDevices[index - 1];
    Serial.printf("Verbinde mit '%s' [%02X:%02X:%02X:%02X:%02X:%02X]...\n",
                  dev.name.c_str(),
                  dev.address[0], dev.address[1], dev.address[2],
                  dev.address[3], dev.address[4], dev.address[5]);

    // Verbindung per MAC-Adresse (zuverlässiger)
    if (SerialBT.connect(dev.address)) {
        Serial.println("Verbindung erfolgreich!");
        printerConnected = true;
        return true;
    } else {
        Serial.println("Verbindung fehlgeschlagen!");
        printerConnected = false;
        return false;
    }
}

// ============================================================
// TSPL2 Drucker-Befehle
// ============================================================

// TSPL2-Befehl senden (mit CRLF)
void sendTSPL(const char* cmd) {
    SerialBT.print(cmd);
    SerialBT.print("\r\n");
    Serial.printf("TSPL> %s\n", cmd);
    delay(50);
}

// Drucker initialisieren
void initPrinter() {
    Serial.println("Initialisiere Drucker...");
    sendTSPL("SIZE 14.0 mm,40.0 mm");
    sendTSPL("GAP 3.0 mm,0 mm");
    sendTSPL("DIRECTION 0,0");
    sendTSPL("DENSITY 8");
    sendTSPL("CLS");
    Serial.println("Drucker bereit.");
}

// Batteriestatus abfragen
void queryBattery() {
    sendTSPL("BATTERY?");
    delay(200);
    String resp = readResponse(500);
    if (resp.length() > 0) {
        Serial.printf("Batterie-Antwort: ");
        for (int i = 0; i < resp.length(); i++) {
            Serial.printf("%02X ", (uint8_t)resp[i]);
        }
        Serial.println();
    }
}

// Config abfragen
void queryConfig() {
    sendTSPL("CONFIG?");
    delay(200);
    String resp = readResponse(500);
    if (resp.length() > 0) {
        Serial.printf("Config-Antwort: ");
        for (int i = 0; i < resp.length(); i++) {
            Serial.printf("%02X ", (uint8_t)resp[i]);
        }
        Serial.println();
    }
}

// Selftest ausführen
void selfTest() {
    Serial.println("Starte Selftest...");
    sendTSPL("SELFTEST");
}

// Bitmap drucken (96x284 Pixel, 1-Bit)
// TSPL2 BITMAP Format: BITMAP X,Y,width(bytes),height,mode,data
void printBitmap(const uint8_t* data, int width, int height) {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    int widthBytes = width / 8;
    int totalBytes = widthBytes * height;

    Serial.printf("Drucke Bitmap %dx%d (%d Bytes)...\n", width, height, totalBytes);

    // BITMAP Befehl senden (ohne CRLF am Ende - Daten folgen direkt)
    char bitmapCmd[64];
    snprintf(bitmapCmd, sizeof(bitmapCmd), "BITMAP 0,0,%d,%d,0,", widthBytes, height);
    SerialBT.print(bitmapCmd);
    Serial.printf("TSPL> %s[%d bytes]\n", bitmapCmd, totalBytes);

    // Bitmap-Daten in kleinen Chunks senden (verhindert Buffer-Überlauf)
    const int chunkSize = 64;
    for (int i = 0; i < totalBytes; i += chunkSize) {
        int len = min(chunkSize, totalBytes - i);
        SerialBT.write(data + i, len);
        delay(5);  // Kurze Pause zwischen Chunks
    }

    SerialBT.print("\r\n");
    delay(200);

    // Drucken
    sendTSPL("PRINT 1");
    Serial.println("Druck gesendet.");
}

// Test-Muster drucken - basierend auf Wireshark-Capture
void printTestPattern() {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    Serial.println("Drucke basierend auf Capture-Analyse...");

    // Bitmap-Daten vorbereiten: 12 bytes * 284 rows = 3408 bytes
    const int rows = 284;
    const int cols = 12;
    const int bitmapSize = rows * cols;

    uint8_t* bitmap = (uint8_t*)malloc(bitmapSize);
    if (!bitmap) {
        Serial.println("Speicherfehler!");
        return;
    }

    // Streifen-Muster erzeugen
    for (int row = 0; row < rows; row++) {
        uint8_t val = ((row / 40) % 2 == 0) ? 0xFF : 0x00;
        for (int col = 0; col < cols; col++) {
            bitmap[row * cols + col] = val;
        }
    }

    // Header als String
    String header = "";
    header += "SIZE 14.0 mm,40.0 mm\r\n";
    header += "GAP 5.0 mm,0 mm\r\n";
    header += "DIRECTION 0,0\r\n";
    header += "DENSITY 15\r\n";
    header += "CLS\r\n";
    header += "BITMAP 0,0,12,284,1,";

    // Alles senden
    Serial.println("Sende Header...");
    SerialBT.print(header);

    Serial.printf("Sende %d Bytes Bitmap...\n", bitmapSize);
    SerialBT.write(bitmap, bitmapSize);

    Serial.println("Sende PRINT...");
    SerialBT.print("\r\nPRINT 1\r\n");

    free(bitmap);
    Serial.println("Druck gesendet!");
}

// Streifen-Muster als Bitmap drucken
void printStripes() {
    if (!printerConnected) {
        Serial.println("Nicht verbunden!");
        return;
    }

    Serial.println("Erstelle Streifen-Muster...");

    // Kleines Test-Bild: 96x100 Pixel
    const int testHeight = 100;
    uint8_t* bitmap = (uint8_t*)malloc(BYTES_PER_ROW * testHeight);

    if (!bitmap) {
        Serial.println("Speicher-Fehler!");
        return;
    }

    // Horizontale Streifen erzeugen
    for (int y = 0; y < testHeight; y++) {
        uint8_t val = (y / 10) % 2 == 0 ? 0xFF : 0x00;
        for (int x = 0; x < BYTES_PER_ROW; x++) {
            bitmap[y * BYTES_PER_ROW + x] = val;
        }
    }

    initPrinter();
    printBitmap(bitmap, LABEL_WIDTH, testHeight);

    free(bitmap);
}

// Hilfe anzeigen
void printHelp() {
    Serial.println("\n=== Nelko P21 Bluetooth Controller ===");
    Serial.println("Verbindung:");
    Serial.println("  scan      - Bluetooth-Geräte suchen");
    Serial.println("  list      - Gefundene Geräte auflisten");
    Serial.println("  c <nr>    - Mit Gerät Nr. verbinden");
    Serial.println("  disc      - Verbindung trennen");
    Serial.println("  status    - Verbindungsstatus");
    Serial.println("Drucker:");
    Serial.println("  init      - Drucker initialisieren");
    Serial.println("  battery   - Batteriestatus abfragen");
    Serial.println("  config    - Konfiguration abfragen");
    Serial.println("  selftest  - Selftest drucken");
    Serial.println("  bar       - Test-Balken drucken");
    Serial.println("  stripes   - Streifen-Muster drucken");
    Serial.println("  help      - Diese Hilfe");
    Serial.println("======================================\n");
}

// Gefundene Geräte auflisten
void listDevices() {
    Serial.printf("\n=== Gefundene Geräte (%d) ===\n", deviceCount);
    for (int i = 0; i < deviceCount; i++) {
        if (foundDevices[i].valid) {
            Serial.printf("%d: %s [%02X:%02X:%02X:%02X:%02X:%02X]\n",
                          i + 1,
                          foundDevices[i].name.c_str(),
                          foundDevices[i].address[0], foundDevices[i].address[1],
                          foundDevices[i].address[2], foundDevices[i].address[3],
                          foundDevices[i].address[4], foundDevices[i].address[5]);
        }
    }
    Serial.println("=============================\n");
}

// Auto-connect to printer
void autoConnect() {
    Serial.println("\nSuche Drucker...");

    BTScanResults* scanResults = SerialBT.discover(5000);  // 5 Sekunden scannen

    if (!scanResults) {
        Serial.println("Scan fehlgeschlagen!");
        return;
    }

    int count = scanResults->getCount();
    Serial.printf("%d Geräte gefunden.\n", count);

    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (device) {
            String name = device->getName().c_str();

            // Prüfen ob es ein Nelko Drucker ist
            if (name.indexOf("P21") >= 0 || name.indexOf("Nelko") >= 0) {
                Serial.printf("Drucker gefunden: %s\n", name.c_str());

                // Adresse speichern
                uint8_t addr[6];
                memcpy(addr, device->getAddress().getNative(), 6);

                // Verbinden
                Serial.println("Verbinde...");
                if (SerialBT.connect(addr)) {
                    Serial.println("Verbunden!");
                    printerConnected = true;

                    // Gerät in Liste speichern
                    foundDevices[0].name = name;
                    memcpy(foundDevices[0].address, addr, 6);
                    foundDevices[0].valid = true;
                    deviceCount = 1;
                    return;
                } else {
                    Serial.println("Verbindung fehlgeschlagen!");
                }
            }
        }
    }

    Serial.println("Kein Drucker gefunden. Nutze 'scan' zum manuellen Suchen.");
}

void setup() {
    // Serial für Debug-Ausgabe
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("  Nelko P21 Label Printer - ESP32");
    Serial.println("========================================");

    // Bluetooth initialisieren
    if (!SerialBT.begin("ESP32_LabelPrinter", true)) {  // true = Master-Modus
        Serial.println("Bluetooth-Initialisierung fehlgeschlagen!");
        while(1);
    }

    Serial.println("Bluetooth initialisiert (Master-Modus)");

    // Callback für Verbindungstrennung
    SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
        if (event == ESP_SPP_CLOSE_EVT) {
            Serial.println("Verbindung getrennt!");
            printerConnected = false;
        }
    });

    // Automatisch nach Drucker suchen und verbinden
    autoConnect();

    printHelp();
}

void loop() {
    // Serielle Befehle verarbeiten
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        // Echo: Eingegebenen Befehl anzeigen
        Serial.printf("> %s\n", cmd.c_str());

        cmd.toLowerCase();

        if (cmd == "scan") {
            startScan();
        }
        else if (cmd == "connect") {
            connectToPrinter(PRINTER_NAME);
        }
        else if (cmd.startsWith("connect ")) {
            String name = cmd.substring(8);
            connectToPrinter(name.c_str());
        }
        else if (cmd == "disc" || cmd == "disconnect") {
            disconnect();
        }
        else if (cmd == "status") {
            Serial.printf("Verbindungsstatus: %s\n",
                         printerConnected ? "Verbunden" : "Nicht verbunden");
        }
        else if (cmd == "test") {
            if (printerConnected) {
                sendCommand("TEST");
                delay(100);
                String response = readResponse();
                if (response.length() > 0) {
                    Serial.printf("Antwort: %s\n", response.c_str());
                }
            } else {
                Serial.println("Nicht verbunden!");
            }
        }
        else if (cmd == "list") {
            listDevices();
        }
        else if (cmd.startsWith("c ")) {
            int index = cmd.substring(2).toInt();
            connectToDevice(index);
        }
        else if (cmd == "help" || cmd == "?") {
            printHelp();
        }
        // Drucker-Befehle
        else if (cmd == "init") {
            if (printerConnected) initPrinter();
            else Serial.println("Nicht verbunden!");
        }
        else if (cmd == "battery") {
            if (printerConnected) queryBattery();
            else Serial.println("Nicht verbunden!");
        }
        else if (cmd == "config") {
            if (printerConnected) queryConfig();
            else Serial.println("Nicht verbunden!");
        }
        else if (cmd == "selftest") {
            if (printerConnected) selfTest();
            else Serial.println("Nicht verbunden!");
        }
        else if (cmd == "bar") {
            printTestPattern();
        }
        else if (cmd == "stripes") {
            printStripes();
        }
        else if (cmd.length() > 0) {
            // Unbekannter Befehl - direkt an Drucker senden (TSPL)
            if (printerConnected) {
                sendTSPL(cmd.c_str());
                delay(100);
                String response = readResponse();
                if (response.length() > 0) {
                    Serial.printf("Antwort: %s\n", response.c_str());
                }
            } else {
                Serial.printf("Unbekannter Befehl: %s\n", cmd.c_str());
            }
        }
    }

    // Daten vom Drucker empfangen
    if (printerConnected && SerialBT.available()) {
        Serial.print("Drucker: ");
        while (SerialBT.available()) {
            char c = SerialBT.read();
            Serial.print(c);
        }
        Serial.println();
    }

    delay(10);
}