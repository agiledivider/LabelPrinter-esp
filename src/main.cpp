#include <Arduino.h>
#include "BluetoothSerial.h"

// Bluetooth Serial Objekt
BluetoothSerial SerialBT;

// Nelko P21 Drucker Konfiguration
const char* PRINTER_NAME = "P21";  // Nelko P21 Bluetooth-Name (kann auch "Nelko_P21" sein)
const uint8_t PRINTER_ADDR[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // MAC-Adresse (wird beim Scan ermittelt)

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

// Hilfe anzeigen
void printHelp() {
    Serial.println("\n=== Nelko P21 Bluetooth Controller ===");
    Serial.println("Befehle:");
    Serial.println("  scan     - Nach Bluetooth-Geräten suchen");
    Serial.println("  list     - Gefundene Geräte auflisten");
    Serial.println("  c <nr>   - Mit Gerät Nr. verbinden (per MAC)");
    Serial.println("  disc     - Verbindung trennen");
    Serial.println("  status   - Verbindungsstatus anzeigen");
    Serial.println("  test     - Test-Befehl senden");
    Serial.println("  help     - Diese Hilfe anzeigen");
    Serial.println("=====================================\n");
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
    Serial.printf("ESP32 MAC: %s\n", SerialBT.getBtAddressString().c_str());

    // Callback für Device-Discovery
    SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
        if (event == ESP_SPP_CLOSE_EVT) {
            Serial.println("Verbindung vom Drucker getrennt!");
            printerConnected = false;
        }
    });

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
        else if (cmd.length() > 0) {
            // Unbekannter Befehl - direkt an Drucker senden wenn verbunden
            if (printerConnected) {
                sendCommand(cmd.c_str());
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