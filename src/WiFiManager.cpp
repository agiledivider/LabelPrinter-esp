#include "WiFiManager.h"
#include "StringUtils.h"
#include "Log.h"

WiFiManager::WiFiManager()
    : _networks(nullptr)
    , _networkCount(0)
    , _useSingleCredentials(false)
    , _connected(false)
    , _dnsFailCount(0)
    , _lastConnectionCheck(0)
{
    memset(_singleSsid, 0, sizeof(_singleSsid));
    memset(_singlePassword, 0, sizeof(_singlePassword));
}

void WiFiManager::begin(const char* networks[][2], int networkCount) {
    _networks = networks;
    _networkCount = networkCount;
    _useSingleCredentials = false;
}

void WiFiManager::setCredentials(const char* ssid, const char* password) {
    safeCopy(_singleSsid, ssid, sizeof(_singleSsid));
    safeCopy(_singlePassword, password, sizeof(_singlePassword));
    _useSingleCredentials = true;
    _networks = nullptr;
    _networkCount = 0;
}

bool WiFiManager::hasCredentials() const {
    if (_useSingleCredentials) {
        return strlen(_singleSsid) > 0;
    }
    return _networks != nullptr && _networkCount > 0;
}

bool WiFiManager::tryConnect(const char* ssid, const char* password) {
    LOG_INFOF(WiFi, "Connecting to: %s", ssid);
    ::WiFi.begin(ssid, password);

    int attempts = 0;
    while (::WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        LOG_RAW(".");
        attempts++;
    }

    if (::WiFi.status() == WL_CONNECTED) {
        LOG_INFOF(WiFi, "Connected to %s", ssid);
        LOG_INFOF(WiFi, "IP: %s", ::WiFi.localIP().toString().c_str());
        return true;
    }

    LOG_WARNF(WiFi, "Failed to connect to %s", ssid);
    return false;
}

bool WiFiManager::connect() {
    if (!hasCredentials()) {
        LOG_ERROR(WiFi, "No networks configured!");
        _connected = false;
        return false;
    }

    LOG_INFO(WiFi, "Starting connection...");

    // Single credential mode
    if (_useSingleCredentials) {
        if (tryConnect(_singleSsid, _singlePassword)) {
            _connected = true;
            _dnsFailCount = 0;
            return true;
        }
    } else {
        // Multi-network mode
        for (int i = 0; i < _networkCount; i++) {
            if (tryConnect(_networks[i][0], _networks[i][1])) {
                _connected = true;
                _dnsFailCount = 0;
                return true;
            }
        }
    }

    LOG_ERROR(WiFi, "No network reachable!");
    _connected = false;
    return false;
}

void WiFiManager::disconnect() {
    ::WiFi.disconnect();
    _connected = false;
    LOG_INFO(WiFi, "Disconnected");
}

bool WiFiManager::checkDns(const char* hostname) {
    if (!_connected) {
        return false;
    }

    IPAddress ip;
    if (::WiFi.hostByName(hostname, ip)) {
        _dnsFailCount = 0;
        return true;
    }

    _dnsFailCount++;
    LOG_ERRORF(WiFi, "DNS failed for %s (%d/%d)", hostname, _dnsFailCount, DNS_FAIL_THRESHOLD);

    if (_dnsFailCount >= DNS_FAIL_THRESHOLD) {
        LOG_ERROR(WiFi, "Too many DNS failures, reconnecting...");
        reconnect();
    }

    return false;
}

void WiFiManager::reconnect() {
    _dnsFailCount = 0;
    ::WiFi.disconnect();
    _connected = false;
    delay(1000);

    while (!_connected) {
        connect();
        if (!_connected) {
            LOG_WARN(WiFi, "Connection failed, retrying in 5s...");
            delay(5000);
        }
    }
}

int WiFiManager::getRSSI() const {
    if (!_connected) {
        return 0;
    }
    return ::WiFi.RSSI();
}

String WiFiManager::getIP() const {
    if (!_connected) {
        return "0.0.0.0";
    }
    return ::WiFi.localIP().toString();
}

void WiFiManager::loop() {
    unsigned long now = millis();

    // Periodic connection check
    if (now - _lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
        _lastConnectionCheck = now;

        if (_connected && ::WiFi.status() != WL_CONNECTED) {
            LOG_ERROR(WiFi, "Connection lost!");
            _connected = false;
        }

        if (!_connected) {
            LOG_INFO(WiFi, "Reconnecting...");
            connect();
        }
    }
}
