#include "WiFiManager.h"
#include <cstring>

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
    strncpy(_singleSsid, ssid, sizeof(_singleSsid) - 1);
    _singleSsid[sizeof(_singleSsid) - 1] = '\0';

    strncpy(_singlePassword, password, sizeof(_singlePassword) - 1);
    _singlePassword[sizeof(_singlePassword) - 1] = '\0';

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
    Serial.printf("Versuche: %s\n", ssid);
    WiFi.begin(ssid, password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nVerbunden mit %s\n", ssid);
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println(" fehlgeschlagen");
    return false;
}

bool WiFiManager::connect() {
    if (!hasCredentials()) {
        Serial.println("Keine Netzwerke konfiguriert!");
        _connected = false;
        return false;
    }

    Serial.println("Verbinde mit WiFi...");

    // Single-Credential-Modus
    if (_useSingleCredentials) {
        if (tryConnect(_singleSsid, _singlePassword)) {
            _connected = true;
            _dnsFailCount = 0;
            return true;
        }
    } else {
        // Multi-Netzwerk-Modus
        for (int i = 0; i < _networkCount; i++) {
            if (tryConnect(_networks[i][0], _networks[i][1])) {
                _connected = true;
                _dnsFailCount = 0;
                return true;
            }
        }
    }

    Serial.println("Kein WiFi-Netzwerk erreichbar!");
    _connected = false;
    return false;
}

void WiFiManager::disconnect() {
    WiFi.disconnect();
    _connected = false;
    Serial.println("WiFi getrennt.");
}

bool WiFiManager::checkDns(const char* hostname) {
    if (!_connected) {
        return false;
    }

    IPAddress ip;
    if (WiFi.hostByName(hostname, ip)) {
        _dnsFailCount = 0;
        return true;
    }

    _dnsFailCount++;
    Serial.printf("DNS fehlgeschlagen fuer %s (%d/%d)\n",
        hostname, _dnsFailCount, DNS_FAIL_THRESHOLD);

    if (_dnsFailCount >= DNS_FAIL_THRESHOLD) {
        Serial.println("Zu viele DNS-Fehler, WiFi wird neu verbunden...");
        reconnect();
    }

    return false;
}

void WiFiManager::reconnect() {
    _dnsFailCount = 0;
    WiFi.disconnect();
    _connected = false;
    delay(1000);

    while (!_connected) {
        connect();
        if (!_connected) {
            Serial.println("WiFi fehlgeschlagen, neuer Versuch in 5 Sekunden...");
            delay(5000);
        }
    }
}

int WiFiManager::getRSSI() const {
    if (!_connected) {
        return 0;
    }
    return WiFi.RSSI();
}

String WiFiManager::getIP() const {
    if (!_connected) {
        return "0.0.0.0";
    }
    return WiFi.localIP().toString();
}

void WiFiManager::loop() {
    unsigned long now = millis();

    // Periodische Verbindungspruefung
    if (now - _lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
        _lastConnectionCheck = now;

        if (_connected && WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi-Verbindung verloren!");
            _connected = false;
        }

        if (!_connected) {
            Serial.println("WiFi-Neuverbindung...");
            connect();
        }
    }
}
