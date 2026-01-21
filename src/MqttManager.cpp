#include "MqttManager.h"
#include "WiFiManager.h"
#include "StringUtils.h"

MqttManager::MqttManager()
    : _wifiManager(nullptr)
    , _mqttClient()
    , _port(1883)
    , _useSsl(false)
    , _callback(nullptr)
    , _connected(false)
    , _lastRetry(0)
{
    memset(_server, 0, sizeof(_server));
    memset(_user, 0, sizeof(_user));
    memset(_password, 0, sizeof(_password));
    memset(_clientId, 0, sizeof(_clientId));
    memset(_topicPrint, 0, sizeof(_topicPrint));
    memset(_topicStatus, 0, sizeof(_topicStatus));
    memset(_topicResult, 0, sizeof(_topicResult));
}

void MqttManager::begin(WiFiManager& wifiManager) {
    _wifiManager = &wifiManager;
}

void MqttManager::setConfig(const char* server, uint16_t port, bool useSsl,
                            const char* user, const char* password, const char* clientId) {
    safeCopy(_server, server, sizeof(_server));
    _port = port;
    _useSsl = useSsl;
    safeCopy(_user, user, sizeof(_user));
    safeCopy(_password, password, sizeof(_password));
    safeCopy(_clientId, clientId, sizeof(_clientId));
}

void MqttManager::setTopics(const char* printTopic, const char* statusTopic, const char* resultTopic) {
    safeCopy(_topicPrint, printTopic, sizeof(_topicPrint));
    safeCopy(_topicStatus, statusTopic, sizeof(_topicStatus));
    safeCopy(_topicResult, resultTopic, sizeof(_topicResult));
}

void MqttManager::setCallback(MqttMessageCallback callback) {
    _callback = callback;
    _mqttClient.setCallback(callback);
}

bool MqttManager::connect() {
    if (!_wifiManager || !_wifiManager->isConnected()) {
        return false;
    }

    // Configure client based on SSL setting
    if (_useSsl) {
        _wifiClientSecure.setInsecure();
        _mqttClient.setClient(_wifiClientSecure);
        Serial.printf("Connecting to MQTT (SSL) %s:%d...\n", _server, _port);
    } else {
        _mqttClient.setClient(_wifiClient);
        Serial.printf("Connecting to MQTT %s:%d...\n", _server, _port);
    }

    _mqttClient.setServer(_server, _port);

    // Re-set callback after setting client
    if (_callback) {
        _mqttClient.setCallback(_callback);
    }

    // Connect with credentials
    if (_mqttClient.connect(_clientId, _user, _password)) {
        Serial.println("MQTT connected!");
        _mqttClient.subscribe(_topicPrint);
        Serial.printf("Subscribed: %s\n", _topicPrint);
        _connected = true;
        return true;
    }

    Serial.printf("MQTT error: %d\n", _mqttClient.state());
    _connected = false;
    return false;
}

void MqttManager::disconnect() {
    _mqttClient.disconnect();
    _connected = false;
    Serial.println("MQTT disconnected.");
}

bool MqttManager::publish(const char* topic, const char* payload) {
    if (!_connected) {
        return false;
    }
    return _mqttClient.publish(topic, payload);
}

bool MqttManager::publishStatus(const char* payload) {
    if (!_connected || strlen(_topicStatus) == 0) {
        return false;
    }
    return _mqttClient.publish(_topicStatus, payload);
}

bool MqttManager::publishResult(const char* payload) {
    if (!_connected || strlen(_topicResult) == 0) {
        return false;
    }
    return _mqttClient.publish(_topicResult, payload);
}

void MqttManager::loop() {
    if (!_wifiManager || !_wifiManager->isConnected()) {
        return;
    }

    if (!_mqttClient.connected()) {
        _connected = false;
        reconnect();
    } else {
        _mqttClient.loop();
    }
}

void MqttManager::reconnect() {
    unsigned long now = millis();

    if (now - _lastRetry >= RETRY_INTERVAL) {
        _lastRetry = now;

        // Check DNS before attempting connection
        if (_wifiManager->checkDns(_server)) {
            connect();
        }
    }
}
