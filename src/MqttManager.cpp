#include "MqttManager.h"
#include "WiFiManager.h"
#include "StringUtils.h"
#include "Log.h"

MqttManager::MqttManager()
    : _wifiManager(nullptr)
    , _mqttClient()
    , _port(1883)
    , _useSsl(false)
    , _callback(nullptr)
    , _connected(false)
    , _lastRetry(0)
    , _extraTopicCount(0)
{
    memset(_server, 0, sizeof(_server));
    memset(_user, 0, sizeof(_user));
    memset(_password, 0, sizeof(_password));
    memset(_clientId, 0, sizeof(_clientId));
    memset(_topicPrint, 0, sizeof(_topicPrint));
    memset(_topicStatus, 0, sizeof(_topicStatus));
    memset(_topicResult, 0, sizeof(_topicResult));
    memset(_extraTopics, 0, sizeof(_extraTopics));
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
        LOG_INFOF(MQTT, "Connecting (SSL) %s:%d...", _server, _port);
    } else {
        _mqttClient.setClient(_wifiClient);
        LOG_INFOF(MQTT, "Connecting %s:%d...", _server, _port);
    }

    _mqttClient.setServer(_server, _port);

    // Re-set callback after setting client
    if (_callback) {
        _mqttClient.setCallback(_callback);
    }

    // Connect with credentials
    if (_mqttClient.connect(_clientId, _user, _password)) {
        LOG_INFO(MQTT, "Connected!");
        _mqttClient.subscribe(_topicPrint);
        LOG_INFOF(MQTT, "Subscribed: %s", _topicPrint);

        // Re-subscribe to extra topics
        for (size_t i = 0; i < _extraTopicCount; i++) {
            if (strlen(_extraTopics[i]) > 0) {
                _mqttClient.subscribe(_extraTopics[i]);
                LOG_INFOF(MQTT, "Subscribed: %s", _extraTopics[i]);
            }
        }

        _connected = true;
        return true;
    }

    LOG_ERRORF(MQTT, "Connection failed, error: %d", _mqttClient.state());
    _connected = false;
    return false;
}

void MqttManager::disconnect() {
    _mqttClient.disconnect();
    _connected = false;
    LOG_INFO(MQTT, "Disconnected");
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

bool MqttManager::subscribe(const char* topic) {
    if (!topic || strlen(topic) == 0) {
        return false;
    }

    // Store topic for re-subscription on reconnect
    if (_extraTopicCount < MAX_EXTRA_TOPICS) {
        safeCopy(_extraTopics[_extraTopicCount], topic, sizeof(_extraTopics[0]));
        _extraTopicCount++;
    }

    // Subscribe if connected
    if (_connected) {
        _mqttClient.subscribe(topic);
        LOG_INFOF(MQTT, "Subscribed: %s", topic);
    }

    return true;
}

void MqttManager::loop() {
    if (!_wifiManager || !_wifiManager->isConnected()) {
        return;
    }

    if (!_mqttClient.connected()) {
        if (_connected) {
            LOG_WARN(MQTT, "Connection lost");
        }
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
