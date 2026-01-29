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
#include "JsonHelpers.h"
#include "LogManager.h"
#include "Log.h"
#include "LedStatusManager.h"

// ============================================================
// Global Objects
// ============================================================

ConfigManager configManager;
ConfigPortal* configPortal = nullptr;

Printer* printer = nullptr;
WiFiManager wifiManager;
MqttManager mqttManager;
LedStatusManager ledStatusManager;

bool portalActive = false;

// MQTT topics for logging commands
char mqttTopicCommand[65] = "";
char mqttTopicLog[65] = "";

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
        LOG_ERROR(Printer, "Printer not connected!");
        return PrintError::PrinterNotConnected;
    }

    PrintError statusError = printer->checkReady();
    if (statusError != PrintError::None) {
        LOG_ERRORF(Printer, "Printer error: %s", printErrorToString(statusError));
        return statusError;
    }

    QRSize size = QRCodeRenderer::sizeFromString(qrSize);
    const char* sizeStr = (size == QRSize::Small) ? "S" : (size == QRSize::Medium) ? "M" : "L";
    LOG_INFOF(Label, "Printing: %s / %s (QR: %s)", name, id, sizeStr);

    LabelImage label(printer->getLabelWidth(), printer->getLabelHeight());
    if (!label.generate(link, name, id, size)) {
        LOG_ERRORF(Label, "Generation error: %s", printErrorToString(label.getError()));
        return label.getError();
    }

    char* dataUrl = label.toDataURL();
    if (dataUrl) {
        LOG_DEBUG(Label, "Label preview generated");
        LOG_DEBUG(Label, dataUrl);
        free(dataUrl);
    }

    printer->sendBitmap(label.getData());
    LOG_INFO(Label, "Label sent to printer");
    return PrintError::None;
}

void printFrame() {
    if (!printer || !printer->isConnected()) {
        LOG_ERROR(Printer, "Printer not connected!");
        return;
    }

    LabelImage label(printer->getLabelWidth(), printer->getLabelHeight());
    if (!label.generateFrame()) {
        LOG_ERROR(Label, "Failed to generate frame!");
        return;
    }

    printer->sendBitmap(label.getData());
    LOG_INFO(Label, "Frame sent to printer");
}

// ============================================================
// MQTT Functions
// ============================================================

void sendResult(const char* printId, PrintError error) {
    if (!mqttManager.isConnected()) return;

    char buffer[256];
    JsonHelpers::buildResult(buffer, sizeof(buffer), printId, error);
    mqttManager.publishResult(buffer);
    LOG_INFOF(MQTT, "Result sent: %s", buffer);
}

void handleLogCommand(JsonDocument& doc) {
    LogManager& logMgr = LogManager::getInstance();

    const char* action = doc["action"] | "";

    if (strcmp(action, "setLevel") == 0) {
        const char* level = doc["level"] | "";
        if (logMgr.setLevelFromString(level)) {
            LOG_INFOF(System, "Log level changed to: %s", logMgr.getLevelString());
        } else {
            LOG_ERRORF(System, "Invalid log level: %s", level);
        }
    }
    else if (strcmp(action, "getLevel") == 0) {
        LOG_INFOF(System, "Current log level: %s", logMgr.getLevelString());
    }
    else if (strcmp(action, "getLogs") == 0) {
        char buffer[2048];
        logMgr.getRecentLogsJson(buffer, sizeof(buffer));
        mqttManager.publish(mqttTopicLog, buffer);
    }
    else if (strcmp(action, "saveLogs") == 0) {
        logMgr.saveToPersistent();
        LOG_INFO(System, "Logs saved to persistent storage");
    }
    else if (strcmp(action, "clearLogs") == 0) {
        logMgr.clearPersistent();
        LOG_INFO(System, "Persistent logs cleared");
    }
    else {
        LOG_WARNF(MQTT, "Unknown log action: %s", action);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // IMPORTANT: Copy payload BEFORE any logging!
    // MQTT logging can corrupt the PubSubClient's internal buffer.
    static char payloadCopy[512];
    size_t copyLen = length < sizeof(payloadCopy) - 1 ? length : sizeof(payloadCopy) - 1;
    memcpy(payloadCopy, payload, copyLen);
    payloadCopy[copyLen] = '\0';

    LOG_INFOF(MQTT, "Message on %s (%d bytes)", topic, length);
    LOG_INFOF(MQTT, "Raw payload: %s", payloadCopy);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payloadCopy, copyLen);

    if (err) {
        LOG_ERRORF(MQTT, "JSON parse error: %s", err.c_str());
        char buffer[256];
        JsonHelpers::buildErrorResult(buffer, sizeof(buffer), "invalid JSON");
        mqttManager.publishResult(buffer);
        return;
    }

    // Check if this is a command message
    if (strlen(mqttTopicCommand) > 0 && strcmp(topic, mqttTopicCommand) == 0) {
        const char* cmd = doc["cmd"] | "";
        if (strcmp(cmd, "log") == 0) {
            handleLogCommand(doc);
            return;
        }
        LOG_WARNF(MQTT, "Unknown command: %s", cmd);
        return;
    }

    // Otherwise treat as print request
    const char* printId = doc["printId"] | "";
    const char* link = doc["link"] | "";
    const char* name = doc["name"] | "";
    const char* id = doc["id"] | "";
    const char* size = doc["size"] | "L";

    LOG_DEBUGF(MQTT, "Print request: link=%s name=%s id=%s size=%s", link, name, id, size);

    PrintError error = printLabel(link, name, id, size);
    sendResult(printId, error);
}

unsigned long lastStatusTime = 0;
const unsigned long STATUS_INTERVAL = 30000;

void publishStatus() {
    if (!mqttManager.isConnected() || !printer) return;

    unsigned long lastSeenSec = 0;
    if (printer->getLastSeenMs() > 0) {
        lastSeenSec = (millis() - printer->getLastSeenMs()) / 1000;
    }

    char buffer[256];
    JsonHelpers::buildStatus(buffer, sizeof(buffer),
        printer->isConnected(),
        printer->getBattery(),
        lastSeenSec,
        wifiManager.getRSSI(),
        ESP.getFreeHeap(),
        millis() / 1000);

    mqttManager.publishStatus(buffer);
    LOG_DEBUGF(MQTT, "Status published");
}

// ============================================================
// Configuration Portal
// ============================================================

void startConfigPortal() {
    LOG_INFO(Portal, "Starting configuration portal...");
    ledStatusManager.setPortalMode();

    if (configPortal == nullptr) {
        configPortal = new ConfigPortal(configManager);
    }

    if (configPortal->begin()) {
        portalActive = true;
        LOG_INFO(Portal, "Portal active at 192.168.4.1");
    } else {
        LOG_ERROR(Portal, "Failed to start portal!");
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
    LogManager& logMgr = LogManager::getInstance();

    LOG_INFO(System, "\n=== Nelko P21 MQTT Printer ===");
    LOG_INFO(System, "Status:");
    LOG_INFOF(System, "  WiFi: %s (%d dBm)", wifiManager.isConnected() ? "Connected" : "Disconnected", wifiManager.getRSSI());
    LOG_INFOF(System, "  MQTT: %s", mqttManager.isConnected() ? "Connected" : "Disconnected");
    LOG_INFOF(System, "  Printer: %s", (printer && printer->isConnected()) ? "Connected" : "Disconnected");
    if (printer) {
        int battery = printer->getBattery();
        if (battery >= 0) {
            LOG_INFOF(System, "  Battery: %d%%", battery);
        }
    }
    LOG_INFOF(System, "  Log level: %s (buffer: %d%%)", logMgr.getLevelString(), logMgr.getBufferUsage());
    LOG_INFO(System, "Commands:");
    LOG_INFO(System, "  scan        - Scan & connect printer");
    LOG_INFO(System, "  disconnect  - Disconnect printer");
    LOG_INFO(System, "  status      - Printer status (paper, etc.)");
    LOG_INFO(System, "  config      - Printer configuration");
    LOG_INFO(System, "  battery     - Query battery level");
    LOG_INFO(System, "  frame       - Test: Print frame");
    LOG_INFO(System, "  qrcode      - Test: Print QR code");
    LOG_INFO(System, "  wifi        - Reconnect WiFi");
    LOG_INFO(System, "  mqtt        - Reconnect MQTT");
    LOG_INFO(System, "  setup       - Start configuration portal");
    LOG_INFO(System, "  clearconfig - Clear config & restart");
    LOG_INFO(System, "  log <level> - Set log level (DEBUG/INFO/WARN/ERROR/NONE)");
    LOG_INFO(System, "  loglevel    - Show current log level");
    LOG_INFO(System, "  logsave     - Save logs to flash");
    LOG_INFO(System, "  logshow     - Show saved crash log");
    LOG_INFO(System, "  logclear    - Clear saved crash log");
    LOG_INFO(System, "  help        - Show this help");
    LOG_INFO(System, "==============================\n");
}

// ============================================================
// Command Handlers
// ============================================================

void cmdScan() {
    if (printer) {
        ledStatusManager.setPrinterState(LedStatusManager::ConnectionState::Connecting);
        printer->connect();
    }
}
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
    LOG_INFO(Config, "Clearing configuration and restarting...");
    configManager.clear();
    delay(1000);
    ESP.restart();
}

void cmdLogLevel() {
    LogManager& logMgr = LogManager::getInstance();
    LOG_INFOF(System, "Current log level: %s", logMgr.getLevelString());
}

void cmdLogSave() {
    LogManager& logMgr = LogManager::getInstance();
    logMgr.saveToPersistent();
    LOG_INFO(System, "Logs saved to persistent storage");
}

void cmdLogShow() {
    LogManager& logMgr = LogManager::getInstance();
    char buffer[2048];
    size_t len = logMgr.loadFromPersistent(buffer, sizeof(buffer));
    if (len > 0) {
        LOG_INFO(System, "=== Saved Crash Log ===");
        Serial.println(buffer);
        LOG_INFO(System, "=== End Crash Log ===");
    } else {
        LOG_INFO(System, "No saved crash log found");
    }
}

void cmdLogClear() {
    LogManager& logMgr = LogManager::getInstance();
    logMgr.clearPersistent();
    LOG_INFO(System, "Persistent logs cleared");
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
    {"loglevel", cmdLogLevel},
    {"logsave", cmdLogSave},
    {"logshow", cmdLogShow},
    {"logclear", cmdLogClear},
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
// Hardware Button Reset (F005)
// ============================================================

/**
 * Checks if BOOT button is held during startup.
 * If held for CONFIG_RESET_HOLD_MS, clears config and starts portal.
 * @return true if config was reset, false otherwise
 */
bool checkBootButtonReset() {
    pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);

    // Check if button is pressed (active LOW)
    if (digitalRead(BOOT_BUTTON_PIN) == HIGH) {
        return false;  // Button not pressed
    }

    LOG_INFO(System, "BOOT button pressed - hold for config reset...");

    unsigned long startTime = millis();
    unsigned long lastDotTime = 0;
    const unsigned long dotInterval = 500;  // Print dot every 500ms

    while (digitalRead(BOOT_BUTTON_PIN) == LOW) {
        unsigned long elapsed = millis() - startTime;

        // Print progress dots
        if (millis() - lastDotTime >= dotInterval) {
            Serial.print(".");
            lastDotTime = millis();
        }

        // Check if held long enough
        if (elapsed >= CONFIG_RESET_HOLD_MS) {
            Serial.println(" RESET!");
            LOG_INFO(System, "Config reset triggered by button hold");
            configManager.clear();
            return true;
        }

        delay(10);
    }

    // Button released too early
    Serial.println(" released");
    LOG_INFO(System, "Button released - normal boot");
    return false;
}

// ============================================================
// Setup & Loop
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize logging system first
    LogManager& logMgr = LogManager::getInstance();
    logMgr.begin();

    LOG_INFO(System, "\n========================================");
    LOG_INFO(System, "  Nelko P21 MQTT Label Printer");
    LOG_INFO(System, "========================================");
    LOG_INFOF(System, "Free heap: %d bytes", ESP.getFreeHeap());
    LOG_INFOF(System, "PSRAM: %d bytes", ESP.getPsramSize());
    LOG_INFOF(System, "Log level: %s", logMgr.getLevelString());

    // Load configuration from NVS (needed for LED settings)
    configManager.load();

    // Initialize RGB LED status indicators early (F009)
    {
        const ConfigManager::Config& cfg = configManager.getConfig();
        ledStatusManager.begin(cfg.ledDataPin, cfg.ledBrightness, cfg.ledEnabled);
    }

    // Check for hardware button reset (F005)
    if (checkBootButtonReset()) {
        LOG_INFO(System, "Starting config portal after button reset...");
        startConfigPortal();
        return;
    }

    // Check if configured
    if (!configManager.isConfigured()) {
        LOG_INFO(Config, "Device not configured!");
        startConfigPortal();
        return;  // Portal will run in loop()
    }

    // Configuration exists - proceed with normal startup
    const ConfigManager::Config& config = configManager.getConfig();

    LOG_INFO(Config, "Configuration loaded:");
    LOG_INFOF(Config, "  Device: %s", config.deviceName);
    LOG_INFOF(Config, "  WiFi: %s", config.wifiSsid);
    LOG_INFOF(Config, "  MQTT: %s:%d", config.mqttServer, config.mqttPort);

    // Generate command and log topics based on device name
    snprintf(mqttTopicCommand, sizeof(mqttTopicCommand), "labelprinter/%s/cmd", config.deviceName);
    snprintf(mqttTopicLog, sizeof(mqttTopicLog), "labelprinter/%s/log", config.deviceName);

    // Connect WiFi using stored credentials
    using CS = LedStatusManager::ConnectionState;
    wifiManager.setCredentials(config.wifiSsid, config.wifiPassword);
    ledStatusManager.setWlanState(CS::Connecting);
    wifiManager.connect();
    ledStatusManager.setWlanState(wifiManager.isConnected() ? CS::Connected : CS::Disconnected);

    // Initialize and connect MQTT (before Bluetooth - SSL needs RAM during handshake)
    mqttManager.begin(wifiManager);
    mqttManager.setConfig(config.mqttServer, config.mqttPort, config.mqttUseSsl,
                          config.mqttUser, config.mqttPassword, config.deviceName);
    mqttManager.setTopics(config.mqttTopicPrint, config.mqttTopicStatus, config.mqttTopicResult);
    mqttManager.setCallback(mqttCallback);

    // Subscribe to command topic for log level adjustment and other commands
    mqttManager.subscribe(mqttTopicCommand);
    LOG_INFOF(MQTT, "Command topic: %s", mqttTopicCommand);
    LOG_INFOF(MQTT, "Log topic: %s", mqttTopicLog);

    ledStatusManager.setMqttState(CS::Connecting);
    mqttManager.connect();
    ledStatusManager.setMqttState(mqttManager.isConnected() ? CS::Connected : CS::Disconnected);

    // Setup MQTT log output (publish logs to MQTT)
    logMgr.setMqttEnabled(true, mqttTopicLog,
        [](const char* topic, const char* payload) -> bool {
            return mqttManager.publish(topic, payload);
        });

    // Initialize printer
    printer = new NelkoP21Printer();

    // Setup printer auto-reconnect (F006)
    NelkoP21Printer* nelkoPrinter = static_cast<NelkoP21Printer*>(printer);
    if (config.printerAutoReconnect) {
        nelkoPrinter->enableAutoReconnect(
            config.printerReconnectMin,
            config.printerReconnectMax,
            config.printerMaxAttempts
        );
    }

    // Set connection state callback for MQTT notifications (F006)
    nelkoPrinter->setConnectionStateCallback([](bool connected) {
        if (!mqttManager.isConnected()) return;

        JsonDocument doc;
        doc["event"] = connected ? "printer_connected" : "printer_disconnected";
        doc["timestamp"] = millis();

        char buffer[128];
        serializeJson(doc, buffer, sizeof(buffer));

        const ConfigManager::Config& cfg = configManager.getConfig();
        mqttManager.publish(cfg.mqttTopicStatus, buffer);
        LOG_INFOF(Printer, "Connection state changed: %s", connected ? "connected" : "disconnected");
    });

    // Set printer LED to blue before blocking BT scan (F009)
    ledStatusManager.setPrinterState(LedStatusManager::ConnectionState::Connecting);

    if (printer->connect()) {
        LOG_INFO(Printer, "Printer ready");
    }
}

void loop() {
    // Handle configuration portal if active
    if (portalActive && configPortal != nullptr) {
        if (!configPortal->loop()) {
            // Timeout - restart device
            LOG_INFO(Portal, "Portal timeout, restarting...");
            stopConfigPortal();
            delay(1000);
            ESP.restart();
        }

        if (configPortal->wasConfigSaved()) {
            // Config saved - restart to apply
            LOG_INFO(Portal, "Configuration saved, restarting...");
            stopConfigPortal();
            delay(2000);
            ESP.restart();
        }

        return;  // Don't run normal loop while portal is active
    }

    // Normal operation
    wifiManager.loop();
    mqttManager.loop();

    // Printer auto-reconnect loop (F006)
    NelkoP21Printer* nelkoPrinter = nullptr;
    if (printer) {
        nelkoPrinter = static_cast<NelkoP21Printer*>(printer);
        nelkoPrinter->loop();
    }

    // Update RGB LED status indicators (F009)
    {
        using CS = LedStatusManager::ConnectionState;

        // WLAN: connected, disconnected, or connecting (WiFi configured but not yet connected)
        if (wifiManager.isConnected()) {
            ledStatusManager.setWlanState(CS::Connected);
        } else if (wifiManager.hasCredentials()) {
            ledStatusManager.setWlanState(CS::Connecting);
        } else {
            ledStatusManager.setWlanState(CS::Disconnected);
        }

        // MQTT: connected, disconnected, or connecting (WiFi up but MQTT not yet)
        if (mqttManager.isConnected()) {
            ledStatusManager.setMqttState(CS::Connected);
        } else if (wifiManager.isConnected()) {
            ledStatusManager.setMqttState(CS::Connecting);
        } else {
            ledStatusManager.setMqttState(CS::Disconnected);
        }

        // Printer: connected, disconnected, or scanning (actively scanning for devices)
        if (printer && printer->isConnected()) {
            ledStatusManager.setPrinterState(CS::Connected);
        } else if (nelkoPrinter && nelkoPrinter->isScanning()) {
            ledStatusManager.setPrinterState(CS::Connecting);
        } else {
            ledStatusManager.setPrinterState(CS::Disconnected);
        }

        ledStatusManager.loop();
    }

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
            else if (cmdLower.startsWith("log ")) {
                // Set log level: "log DEBUG", "log INFO", etc.
                String level = cmd.substring(4);
                level.trim();
                level.toUpperCase();
                LogManager& logMgr = LogManager::getInstance();
                if (logMgr.setLevelFromString(level.c_str())) {
                    LOG_INFOF(System, "Log level set to: %s", logMgr.getLevelString());
                } else {
                    LOG_ERRORF(System, "Invalid log level: %s (use DEBUG/INFO/WARN/ERROR/NONE)", level.c_str());
                }
            }
        }
    }

    // Process printer messages
    if (printer) {
        printer->processIncoming();
    }

    delay(10);
}
