#include "NelkoP21Printer.h"
#include "TSPL2.h"
#include "Log.h"

// Static instance pointer for callback
NelkoP21Printer* NelkoP21Printer::_instance = nullptr;

NelkoP21Printer::NelkoP21Printer()
    : _initialized(false)
    , _connected(false)
    , _scanning(false)
    , _battery(-1)
    , _lastSeen(0)
    , _autoReconnectEnabled(false)
    , _lastReconnectAttempt(0)
    , _reconnectInterval(0)
    , _reconnectIntervalMin(10)
    , _reconnectIntervalMax(300)
    , _maxReconnectAttempts(0)
    , _reconnectAttempts(0)
    , _wasConnected(false)
    , _connectionStateCallback(nullptr)
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
        LOG_INFO(Printer, "Printer disconnected!");
        _instance->_connected = false;
    }
}

bool NelkoP21Printer::begin(const char* deviceName) {
    if (_initialized) {
        return true;
    }

    if (!_serialBT.begin(deviceName, true)) {
        LOG_ERROR(Printer, "Bluetooth initialization failed!");
        return false;
    }

    _serialBT.register_callback(btCallback);
    _initialized = true;
    LOG_INFO(Printer, "Bluetooth initialized");
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
    LOG_INFO(Printer, "Scanning for Bluetooth devices...");

    BTScanResults* scanResults = _serialBT.discover(5000);
    if (!scanResults) {
        LOG_ERROR(Printer, "Bluetooth scan failed! Trying restart...");

        // Restart Bluetooth
        _serialBT.disconnect();
        _serialBT.end();
        btStop();
        delay(500);
        btStart();

        if (!_serialBT.begin("ESP32_LabelPrinter", true)) {
            LOG_ERROR(Printer, "Bluetooth restart failed!");
            return false;
        }

        _serialBT.register_callback(btCallback);
        delay(500);

        // Retry scan
        scanResults = _serialBT.discover(5000);
        if (!scanResults) {
            LOG_ERROR(Printer, "Bluetooth scan failed again!");
            return false;
        }
    }

    int count = scanResults->getCount();
    LOG_INFOF(Printer, "%d devices found:", count);

    // List all devices
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.length() == 0) name = "(unknown)";

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);
        LOG_INFOF(Printer, "  [%d] %s (%02X:%02X:%02X:%02X:%02X:%02X)",
            i, name.c_str(),
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }

    // Search for printer
    for (int i = 0; i < count; i++) {
        BTAdvertisedDevice* device = scanResults->getDevice(i);
        if (!device) continue;

        String name = device->getName().c_str();
        if (name.indexOf("P21") < 0 && name.indexOf("Nelko") < 0) continue;

        LOG_INFOF(Printer, "Connecting to printer: %s", name.c_str());

        uint8_t addr[6];
        memcpy(addr, device->getAddress().getNative(), 6);

        if (_serialBT.connect(addr)) {
            LOG_INFO(Printer, "Printer connected!");
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

    LOG_ERROR(Printer, "No printer found");
    return false;
}

bool NelkoP21Printer::connect() {
    if (!_initialized) {
        if (!begin()) {
            return false;
        }
    }
    _scanning = true;
    bool result = scanAndConnect();
    _scanning = false;
    return result;
}

void NelkoP21Printer::disconnect() {
    if (_connected) {
        _serialBT.disconnect();
        _connected = false;
        LOG_INFO(Printer, "Printer disconnected");
    }
}

int NelkoP21Printer::getBattery() {
    if (!_connected) {
        _battery = -1;
        return _battery;
    }

    clearBuffer();

    // Send battery query
    _serialBT.print(TSPL2::Query::BATTERY);
    delay(TSPL2::Timeout::BATTERY_RESPONSE);

    // Skip echo
    skipEcho(TSPL2::Query::BATTERY_ECHO_LEN, TSPL2::Timeout::QUERY);

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
                LOG_INFOF(Printer, "Battery: %d%%", _battery);
            } else {
                LOG_DEBUGF(Printer, "Battery raw: 0x%02X (%d)", raw, level);
            }
            return _battery;
        }
    }

    LOG_DEBUG(Printer, "Battery: no response");
    return _battery;
}

void NelkoP21Printer::queryConfig() {
    if (!_connected) {
        LOG_ERROR(Printer, "Not connected!");
        return;
    }

    clearBuffer();

    _serialBT.print(TSPL2::Query::CONFIG);
    delay(200);

    // Skip echo
    skipEcho(TSPL2::Query::CONFIG_ECHO_LEN, TSPL2::Timeout::QUERY);

    // Read config bytes
    uint8_t config[TSPL2::Response::CONFIG_SIZE];
    int count = readWithTimeout(config, TSPL2::Response::CONFIG_SIZE, TSPL2::Timeout::QUERY);

    // Discard rest
    while (_serialBT.available()) _serialBT.read();

    if (count < TSPL2::Response::CONFIG_SIZE) {
        LOG_DEBUG(Printer, "Config: incomplete response");
        return;
    }

    using namespace TSPL2::ConfigIndex;
    LOG_INFO(Printer, "=== Printer Configuration ===");
    LOG_INFOF(Printer, "  Protocol:     %s", config[PROTOCOL] == 0 ? "TSPL2" : "Unknown");
    LOG_INFOF(Printer, "  DPI:          %d", config[DPI]);
    LOG_INFOF(Printer, "  Hardware:     v%d.%d.%d", config[HW_MAJOR], config[HW_MINOR], config[HW_PATCH]);
    LOG_INFOF(Printer, "  Firmware:     v%d.%d.%d", config[FW_MAJOR], config[FW_MINOR], config[FW_PATCH]);

    const char* timeouts[] = {"Never", "15 min", "30 min", "60 min"};
    int timeoutIdx = config[AUTO_OFF] < 4 ? config[AUTO_OFF] : 0;
    LOG_INFOF(Printer, "  Auto-Off:     %s", timeouts[timeoutIdx]);
    LOG_INFOF(Printer, "  Beep:         %s", config[BEEP] ? "On" : "Off");

    // Raw hex dump
    LOG_RAW("  Raw:          ");
    for (int i = 0; i < TSPL2::Response::CONFIG_SIZE; i++) {
        Serial.printf("%02X ", config[i]);
    }
    Serial.println();
    LOG_INFO(Printer, "=============================");
}

void NelkoP21Printer::queryStatus() {
    if (!_connected) {
        LOG_ERROR(Printer, "Not connected!");
        return;
    }

    clearBuffer();
    LOG_DEBUG(Printer, "Querying status...");

    // Send status query (ESC!o)
    _serialBT.print(TSPL2::Query::STATUS);

    // Read response
    uint8_t response[TSPL2::Response::STATUS_SIZE];
    int count = readWithTimeout(response, TSPL2::Response::STATUS_SIZE, TSPL2::Timeout::STATUS);

    // Discard rest
    while (_serialBT.available()) _serialBT.read();

    if (count < 1) {
        LOG_DEBUG(Printer, "Status: no response");
        return;
    }

    LOG_INFO(Printer, "=== Printer Status ===");

    if (response[0] == TSPL2::Status::OK) {
        LOG_INFO(Printer, "  Status:       OK");
        if (count >= 14) {
            LOG_INFOF(Printer, "  Paper:        %d x %d mm", response[13], response[11]);
        }
    } else if (response[0] == TSPL2::Status::NO_PAPER) {
        LOG_ERROR(Printer, "  Status:       ERROR - No paper!");
    } else {
        LOG_INFOF(Printer, "  Status:       Unknown (0x%02X)", response[0]);
    }

    // Raw hex dump
    LOG_RAW("  Raw:          ");
    for (int i = 0; i < count; i++) {
        Serial.printf("%02X ", response[i]);
    }
    Serial.println();
    LOG_INFO(Printer, "======================");
}

PrintError NelkoP21Printer::checkReady() {
    queryStatus();

    if (!_connected || !_serialBT.connected()) {
        _connected = false;
        return PrintError::PrinterNotConnected;
    }

    clearBuffer();

    // Query status (ESC!o)
    _serialBT.print(TSPL2::Query::STATUS);
    delay(TSPL2::Timeout::BATTERY_RESPONSE);

    // Read response
    uint8_t response[TSPL2::Response::STATUS_SIZE];
    int count = readWithTimeout(response, TSPL2::Response::STATUS_SIZE, TSPL2::Timeout::QUERY);

    LOG_RAW("Response (Hex): ");
    for (int i = 0; i < TSPL2::Response::STATUS_SIZE; i++) {
        Serial.printf("%02X ", response[i]);
    }
    Serial.println();

    // Discard rest
    while (_serialBT.available()) _serialBT.read();

    if (count >= 1) {
        if (response[0] == TSPL2::Status::OK) {
            return PrintError::None;
        } else if (response[0] == TSPL2::Status::NO_PAPER) {
            return PrintError::NoPaper;
        } else {
            LOG_DEBUGF(Printer, "Unknown status: 0x%02X", response[0]);
            return PrintError::None;  // Unknown but continue
        }
    }

    // No response - maybe SPP doesn't support this command
    return PrintError::None;
}

void NelkoP21Printer::sendBitmap(const uint8_t* bitmap) {
    if (!_connected) {
        LOG_ERROR(Printer, "Printer not connected!");
        return;
    }

    // TSPL2 command sequence
    char header[128];
    snprintf(header, sizeof(header), TSPL2::BITMAP_HEADER_FMT,
        LABEL_WIDTH_MM, LABEL_HEIGHT_MM,
        LABEL_GAP_MM,
        BYTES_PER_ROW, LABEL_HEIGHT);

    _serialBT.print(header);
    _serialBT.write(bitmap, BITMAP_SIZE);
    _serialBT.print(TSPL2::PRINT_COMMAND);

    _lastSeen = millis();
}

void NelkoP21Printer::sendCommand(const char* cmd) {
    if (!_connected) {
        LOG_ERROR(Printer, "Not connected!");
        return;
    }

    clearBuffer();

    LOG_DEBUGF(Printer, "Sending: %s", cmd);
    _serialBT.print(cmd);
    _serialBT.print("\r\n");
    delay(TSPL2::Timeout::QUERY);

    LOG_RAW("Response: ");
    bool gotData = false;
    unsigned long start = millis();
    while (millis() - start < TSPL2::Timeout::COMMAND) {
        if (_serialBT.available()) {
            uint8_t c = _serialBT.read();
            Serial.printf("[0x%02X '%c'] ", c, (c >= 32 && c < 127) ? c : '.');
            gotData = true;
        }
    }
    if (!gotData) {
        LOG_RAW("(none)");
    }
    Serial.println();
}

void NelkoP21Printer::processIncoming() {
    if (_connected && _serialBT.available()) {
        while (_serialBT.available()) {
            Serial.printf("%c", (char)_serialBT.read());
        }
    }
}

// ============================================================
// Auto-Reconnect (F006)
// ============================================================

void NelkoP21Printer::enableAutoReconnect(uint16_t minIntervalSec, uint16_t maxIntervalSec, uint8_t maxAttempts) {
    _autoReconnectEnabled = true;
    _reconnectIntervalMin = minIntervalSec;
    _reconnectIntervalMax = maxIntervalSec;
    _maxReconnectAttempts = maxAttempts;
    _reconnectInterval = (unsigned long)minIntervalSec * 1000;
    _reconnectAttempts = 0;
    LOG_INFOF(Printer, "Auto-reconnect enabled: %d-%ds, max attempts: %d",
              minIntervalSec, maxIntervalSec, maxAttempts == 0 ? -1 : maxAttempts);
}

void NelkoP21Printer::disableAutoReconnect() {
    _autoReconnectEnabled = false;
    LOG_INFO(Printer, "Auto-reconnect disabled");
}

void NelkoP21Printer::setConnectionStateCallback(std::function<void(bool)> callback) {
    _connectionStateCallback = callback;
}

void NelkoP21Printer::resetReconnectBackoff() {
    _reconnectInterval = (unsigned long)_reconnectIntervalMin * 1000;
    _reconnectAttempts = 0;
    _lastReconnectAttempt = 0;
}

void NelkoP21Printer::loop() {
    // Detect state change for callback
    if (_wasConnected != _connected) {
        _wasConnected = _connected;
        if (_connectionStateCallback) {
            _connectionStateCallback(_connected);
        }
        if (_connected) {
            resetReconnectBackoff();
        }
    }

    // Auto-reconnect logic
    if (!_autoReconnectEnabled || _connected) {
        return;
    }

    // Check if max attempts reached (0 = infinite)
    if (_maxReconnectAttempts > 0 && _reconnectAttempts >= _maxReconnectAttempts) {
        return;
    }

    unsigned long now = millis();
    if (now - _lastReconnectAttempt < _reconnectInterval) {
        return;
    }

    _lastReconnectAttempt = now;
    _reconnectAttempts++;

    LOG_INFOF(Printer, "Reconnect attempt %d/%d (interval: %lus)",
              _reconnectAttempts,
              _maxReconnectAttempts == 0 ? -1 : (int)_maxReconnectAttempts,
              _reconnectInterval / 1000);

    if (connect()) {
        LOG_INFO(Printer, "Reconnected successfully");
    } else {
        // First 10 attempts use minimum interval, then exponential backoff
        if (_reconnectAttempts >= 10) {
            unsigned long maxMs = (unsigned long)_reconnectIntervalMax * 1000;
            _reconnectInterval = (_reconnectInterval * 2 > maxMs) ? maxMs : _reconnectInterval * 2;
        }
        LOG_INFOF(Printer, "Reconnect failed, next attempt in %lus", _reconnectInterval / 1000);
    }
}
