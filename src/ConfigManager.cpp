#include "ConfigManager.h"
#include "StringUtils.h"
#include "Log.h"

const char* ConfigManager::PREFS_NAMESPACE = "labelprinter";

ConfigManager::ConfigManager() {
    memset(&_config, 0, sizeof(_config));
}

void ConfigManager::setDefaults() {
    // Default device name
    if (strlen(_config.deviceName) == 0) {
        safeCopy(_config.deviceName, "LabelPrinter", sizeof(_config.deviceName));
    }

    // Default MQTT settings
    if (strlen(_config.mqttServer) == 0) {
        safeCopy(_config.mqttServer, "status.makerspacebonn.de", sizeof(_config.mqttServer));
    }
    if (_config.mqttPort == 0) {
        _config.mqttPort = 1883;
    }

    // Default printer auto-reconnect settings (F006)
    // These are set explicitly since 0 is a valid value for some fields
    _config.printerAutoReconnect = true;
    _config.printerReconnectMin = 10;    // 10 seconds initial
    _config.printerReconnectMax = 300;   // 5 minutes max
    _config.printerMaxAttempts = 0;      // Infinite retries
}

void ConfigManager::generateDefaultTopics() {
    // Generate topics based on device name (lowercase, no spaces)
    char baseName[33];
    safeCopy(baseName, _config.deviceName, sizeof(baseName));

    // Convert to lowercase and replace spaces with underscores
    for (int i = 0; baseName[i]; i++) {
        if (baseName[i] == ' ') {
            baseName[i] = '_';
        } else {
            baseName[i] = tolower(baseName[i]);
        }
    }

    // Generate default topics if empty
    if (strlen(_config.mqttTopicPrint) == 0) {
        snprintf(_config.mqttTopicPrint, sizeof(_config.mqttTopicPrint),
                 "%s/print", baseName);
    }
    if (strlen(_config.mqttTopicStatus) == 0) {
        snprintf(_config.mqttTopicStatus, sizeof(_config.mqttTopicStatus),
                 "%s/status", baseName);
    }
    if (strlen(_config.mqttTopicResult) == 0) {
        snprintf(_config.mqttTopicResult, sizeof(_config.mqttTopicResult),
                 "%s/result", baseName);
    }
}

bool ConfigManager::load() {
    if (!_prefs.begin(PREFS_NAMESPACE, true)) {  // Read-only mode
        LOG_ERROR(Config, "Failed to open preferences for reading");
        return false;
    }

    _config.configured = _prefs.getBool("configured", false);

    if (_config.configured) {
        // Load WiFi settings
        _prefs.getString("wifiSsid", _config.wifiSsid, sizeof(_config.wifiSsid));
        _prefs.getString("wifiPass", _config.wifiPassword, sizeof(_config.wifiPassword));

        // Load device name
        _prefs.getString("deviceName", _config.deviceName, sizeof(_config.deviceName));

        // Load MQTT settings
        _prefs.getString("mqttServer", _config.mqttServer, sizeof(_config.mqttServer));
        _config.mqttPort = _prefs.getUShort("mqttPort", 1883);
        _prefs.getString("mqttUser", _config.mqttUser, sizeof(_config.mqttUser));
        _prefs.getString("mqttPass", _config.mqttPassword, sizeof(_config.mqttPassword));
        _config.mqttUseSsl = _prefs.getBool("mqttSsl", false);

        // Load MQTT topics
        _prefs.getString("topicPrint", _config.mqttTopicPrint, sizeof(_config.mqttTopicPrint));
        _prefs.getString("topicStatus", _config.mqttTopicStatus, sizeof(_config.mqttTopicStatus));
        _prefs.getString("topicResult", _config.mqttTopicResult, sizeof(_config.mqttTopicResult));

        // Load printer auto-reconnect settings (F006)
        _config.printerAutoReconnect = _prefs.getBool("prtAutoRecon", true);
        _config.printerReconnectMin = _prefs.getUShort("prtReconMin", 10);
        _config.printerReconnectMax = _prefs.getUShort("prtReconMax", 300);
        _config.printerMaxAttempts = _prefs.getUChar("prtMaxAttempt", 0);

        LOG_INFOF(Config, "Loaded: WiFi=%s, MQTT=%s:%d",
                      _config.wifiSsid, _config.mqttServer, _config.mqttPort);
    } else {
        LOG_INFO(Config, "No configuration found");
        setDefaults();
    }

    _prefs.end();
    return true;
}

bool ConfigManager::save() {
    // Apply defaults for empty fields
    setDefaults();

    // Generate topics if empty
    generateDefaultTopics();

    if (!_prefs.begin(PREFS_NAMESPACE, false)) {  // Read-write mode
        LOG_ERROR(Config, "Failed to open preferences for writing");
        return false;
    }

    // Mark as configured
    _config.configured = true;
    _prefs.putBool("configured", true);

    // Save WiFi settings
    _prefs.putString("wifiSsid", _config.wifiSsid);
    _prefs.putString("wifiPass", _config.wifiPassword);

    // Save device name
    _prefs.putString("deviceName", _config.deviceName);

    // Save MQTT settings
    _prefs.putString("mqttServer", _config.mqttServer);
    _prefs.putUShort("mqttPort", _config.mqttPort);
    _prefs.putString("mqttUser", _config.mqttUser);
    _prefs.putString("mqttPass", _config.mqttPassword);
    _prefs.putBool("mqttSsl", _config.mqttUseSsl);

    // Save MQTT topics
    _prefs.putString("topicPrint", _config.mqttTopicPrint);
    _prefs.putString("topicStatus", _config.mqttTopicStatus);
    _prefs.putString("topicResult", _config.mqttTopicResult);

    // Save printer auto-reconnect settings (F006)
    _prefs.putBool("prtAutoRecon", _config.printerAutoReconnect);
    _prefs.putUShort("prtReconMin", _config.printerReconnectMin);
    _prefs.putUShort("prtReconMax", _config.printerReconnectMax);
    _prefs.putUChar("prtMaxAttempt", _config.printerMaxAttempts);

    _prefs.end();

    LOG_INFOF(Config, "Saved: WiFi=%s, MQTT=%s:%d",
                  _config.wifiSsid, _config.mqttServer, _config.mqttPort);
    return true;
}

void ConfigManager::clear() {
    if (!_prefs.begin(PREFS_NAMESPACE, false)) {
        LOG_ERROR(Config, "Failed to open preferences for clearing");
        return;
    }

    _prefs.clear();
    _prefs.end();

    memset(&_config, 0, sizeof(_config));
    LOG_INFO(Config, "Configuration cleared");
}
