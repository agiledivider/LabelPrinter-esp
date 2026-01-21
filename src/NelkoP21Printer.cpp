#include "NelkoP21Printer.h"

// Static instance pointer for callback
NelkoP21Printer* NelkoP21Printer::_instance = nullptr;

NelkoP21Printer::NelkoP21Printer()
    : _initialized(false)
    , _connected(false)
    , _battery(-1)
    , _lastSeen(0)
{
    _instance = this;
}

NelkoP21Printer::~NelkoP21Printer() {
    if (_connected) {
        disconnect();
    }
    if (_initialized) {
        end();
    }
    if (_instance == this) {
        _instance = nullptr;
    }
}

void NelkoP21Printer::btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
    if (event == ESP_SPP_CLOSE_EVT && _instance) {
        Serial.println("Printer disconnected!");
        _instance->_connected = false;
    }
}

bool NelkoP21Printer::begin(const char* deviceName) {
    if (_initialized) {
        return true;
    }

    if (!_serialBT.begin(deviceName, true)) {
        Serial.println("Bluetooth error!");
        return false;
    }

    _serialBT.register_callback(btCallback);
    _initialized = true;
    Serial.println("Bluetooth initialized");
    return true;
}

void NelkoP21Printer::end() {
    if (_connected) {
        disconnect();
    }
    if (_initialized) {
        _serialBT.end();
        btStop();
        _initialized = false;
    }
}

void NelkoP21Printer::clearBuffer() {
    delay(100);
    while (_serialBT.available()) {
        _serialBT.read();
    }
}

int NelkoP21Printer::readWithTimeout(uint8_t* buffer, int maxLen, unsigned long timeoutMs) {
    int count = 0;
    unsigned long start = millis();
    while (millis() - start < timeoutMs && count < maxLen) {
        if (_serialBT.available()) {
            buffer[count++] = _serialBT.read();
        }
    }
    return count;
}

void NelkoP21Printer::skipEcho(int echoLen, unsigned long timeoutMs) {
    int count = 0;
    unsigned long start = millis();
    while (millis() - start < timeoutMs && count < echoLen) {
        if (_serialBT.available()) {
            _serialBT.read();
            count++;
        }
    }
}

bool NelkoP21Printer::scanAndConnect() {
    Serial.println("Scanning for Bluetooth devices...");

    BTScanResults* scanResults = _serialBT.discover(5000);
    if (!scanResults) {
        Serial.println("Bluetooth scan failed! Trying restart...");

        // Restart Bluetooth
        _serialBT.disconnect();
        _serialBT.end();
        btStop();
        delay(500);
        btStart();

        if (!_serialBT.begin("ESP32_LabelPrinter", true)) {
            Serial.println("Bluetooth restart failed!");
            return false;
        }

        _serialBT.register_callback(btCallback);
        delay(500);

        // Retry scan
        scanResults = _serialBT.discover(5000);
        if (!scanResults) {
            Serial.println("Bluetooth scan failed again!");
            return false;
        }
    }

    int count = scanResults->getCount();
    Serial.printf("%d devices found:\n", count);

    // List all devices
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.length() == 0) name = "(unknown)";

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);
        Serial.printf("  [%d] %s (%02X:%02X:%02X:%02X:%02X:%02X)\n",
            i, name.c_str(),
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }

    // Search for printer
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.indexOf("P21") < 0 && name.indexOf("Nelko") < 0) continue;

        Serial.printf("Connecting to printer: %s\n", name.c_str());

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);

        if (_serialBT.connect(addr)) {
            Serial.println("Printer connected!\n");
            _connected = true;
            _lastSeen = millis();

            // Query initial status
            delay(500);
            queryConfig();
            delay(300);
            getBattery();
            delay(300);
            queryStatus();
            return true;
        }
    }

    Serial.println("No printer found.");
    return false;
}

bool NelkoP21Printer::connect() {
    if (!_initialized) {
        if (!begin()) {
            return false;
        }
    }
    return scanAndConnect();
}

void NelkoP21Printer::disconnect() {
    if (_connected) {
        _serialBT.disconnect();
        _connected = false;
        Serial.println("Printer disconnected.");
    }
}

int NelkoP21Printer::getBattery() {
    if (!_connected) {
        _battery = -1;
        return _battery;
    }

    clearBuffer();

    // Send battery query (command: BATTERY?)
    _serialBT.print("BATTERY?\r\n");
    delay(300);

    // Skip echo (8 chars: "BATTERY?")
    skipEcho(8, 500);

    // Read 2 response bytes (first byte = BCD percentage)
    unsigned long start = millis();
    while (millis() - start < 300) {
        if (_serialBT.available()) {
            uint8_t raw = _serialBT.read();

            // Discard rest of response
            delay(200);
            while (_serialBT.available()) _serialBT.read();

            // Decode BCD: 0x99 = 99%, 0x66 = 66%
            int level = ((raw >> 4) & 0x0F) * 10 + (raw & 0x0F);

            if (level >= 0 && level <= 100) {
                _battery = level;
                Serial.printf("Battery: %d%%\n", _battery);
            } else {
                Serial.printf("Battery raw: 0x%02X (%d)\n", raw, level);
            }
            return _battery;
        }
    }

    Serial.println("Battery: no response");
    return _battery;
}

void NelkoP21Printer::queryConfig() {
    if (!_connected) {
        Serial.println("Not connected!");
        return;
    }

    clearBuffer();

    _serialBT.print("CONFIG?\r\n");
    delay(200);

    // Skip echo (7 chars: "CONFIG?")
    skipEcho(7, 500);

    // Read 10 config bytes
    uint8_t config[10];
    int count = readWithTimeout(config, 10, 500);

    // Discard rest
    while (_serialBT.available()) _serialBT.read();

    if (count < 10) {
        Serial.println("Config: (incomplete response)");
        return;
    }

    Serial.println("=== Printer Configuration ===");
    Serial.printf("  Protocol:     %s\n", config[0] == 0 ? "TSPL2" : "Unknown");
    Serial.printf("  DPI:          %d\n", config[1]);
    Serial.printf("  Hardware:     v%d.%d.%d\n", config[2], config[3], config[4]);
    Serial.printf("  Firmware:     v%d.%d.%d\n", config[5], config[6], config[7]);

    const char* timeouts[] = {"Never", "15 min", "30 min", "60 min"};
    int timeoutIdx = config[8] < 4 ? config[8] : 0;
    Serial.printf("  Auto-Off:     %s\n", timeouts[timeoutIdx]);
    Serial.printf("  Beep:         %s\n", config[9] ? "On" : "Off");

    // Raw hex dump
    Serial.print("  Raw:          ");
    for (int i = 0; i < 10; i++) {
        Serial.printf("%02X ", config[i]);
    }
    Serial.println();
    Serial.println("=============================");
}

void NelkoP21Printer::queryStatus() {
    if (!_connected) {
        Serial.println("Not connected!");
        return;
    }

    clearBuffer();
    Serial.println("Clearing buffer...done.\r\n");

    // Send ESC!o
    _serialBT.print("\x1B!o\r\n");

    // Read 16 byte response
    uint8_t response[16];
    Serial.println("Response...");
    int count = readWithTimeout(response, 16, 1000);
    for (int i = 0; i < count; i++) {
        Serial.print(response[i]);
    }
    Serial.println("\r\n...done.\r\n");

    // Discard rest
    Serial.println("Discarding rest...");
    while (_serialBT.available()) {
        Serial.print(_serialBT.read());
    }
    Serial.println("...done.\r\n");

    if (count < 1) {
        Serial.println("Status: (no response)");
        return;
    }

    Serial.println("=== Printer Status ===");

    if (response[0] == 0x00) {
        Serial.println("  Status:       OK");
        if (count >= 14) {
            Serial.printf("  Paper:        %d x %d mm\n", response[13], response[11]);
        }
    } else if (response[0] == 0x04) {
        Serial.println("  Status:       ERROR - No paper!");
    } else {
        Serial.printf("  Status:       Unknown (0x%02X)\n", response[0]);
    }

    // Raw hex dump
    Serial.print("  Raw:          ");
    for (int i = 0; i < count; i++) {
        Serial.printf("%02X ", response[i]);
    }
    Serial.println();
    Serial.println("======================");
}

const char* NelkoP21Printer::checkReady() {
    queryStatus();

    if (!_connected || !_serialBT.connected()) {
        _connected = false;
        return "printer not connected";
    }

    clearBuffer();

    // Query status with ESC!o (0x1B 0x21 0x6F)
    _serialBT.write(0x1B);
    _serialBT.write('!');
    _serialBT.write('o');
    _serialBT.print("\r\n");
    delay(300);

    // Read 16 byte response
    uint8_t response[16];
    int count = readWithTimeout(response, 16, 500);

    Serial.print("Response (Hex): ");
    for (int i = 0; i < 16; i++) {
        Serial.printf("%02X ", response[i]);
    }
    Serial.println();

    // Discard rest
    while (_serialBT.available()) _serialBT.read();

    if (count >= 1) {
        // Byte 0 = Status: 0x00 = OK, 0x04 = Paper error
        if (response[0] == 0x00) {
            return nullptr;  // OK
        } else if (response[0] == 0x04) {
            return "no paper";
        } else {
            Serial.printf("Unknown status: 0x%02X\n", response[0]);
            return nullptr;  // Unknown but continue
        }
    }

    // No response - maybe SPP doesn't support this command
    return nullptr;
}

void NelkoP21Printer::sendBitmap(const uint8_t* bitmap) {
    if (!_connected) {
        Serial.println("Printer not connected!");
        return;
    }

    // TSPL2 command sequence
    char header[128];
    snprintf(header, sizeof(header),
        "SIZE %.1f mm,%.1f mm\r\n"
        "GAP %.1f mm,0 mm\r\n"
        "DIRECTION 0,0\r\n"
        "DENSITY 15\r\n"
        "CLS\r\n"
        "BITMAP 0,0,%d,%d,1,",
        LABEL_WIDTH_MM, LABEL_HEIGHT_MM,
        LABEL_GAP_MM,
        BYTES_PER_ROW, LABEL_HEIGHT);

    _serialBT.print(header);
    _serialBT.write(bitmap, BITMAP_SIZE);
    _serialBT.print("\r\nPRINT 1\r\n");

    _lastSeen = millis();
}

void NelkoP21Printer::sendCommand(const char* cmd) {
    if (!_connected) {
        Serial.println("Not connected!");
        return;
    }

    clearBuffer();

    Serial.printf("Sending: %s\n", cmd);
    _serialBT.print(cmd);
    _serialBT.print("\r\n");
    delay(500);

    Serial.print("Response: ");
    bool gotData = false;
    unsigned long start = millis();
    while (millis() - start < 1000) {
        if (_serialBT.available()) {
            uint8_t c = _serialBT.read();
            Serial.printf("[0x%02X '%c'] ", c, (c >= 32 && c < 127) ? c : '.');
            gotData = true;
        }
    }
    if (!gotData) {
        Serial.print("(none)");
    }
    Serial.println();
}

void NelkoP21Printer::processIncoming() {
    if (_connected && _serialBT.available()) {
        while (_serialBT.available()) {
            Serial.print((char)_serialBT.read());
        }
    }
}
