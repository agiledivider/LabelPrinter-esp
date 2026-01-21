#include "LogManager.h"
#include <LittleFS.h>
#include <cstdarg>
#include <cstring>

static const char* CRASH_LOG_PATH = "/crash.log";
static const size_t MAX_LOG_MESSAGE = 256;

LogManager& LogManager::getInstance() {
    static LogManager instance;
    return instance;
}

LogManager::LogManager()
    : _level(LogLevel::Info)
    , _serialEnabled(true)
    , _mqttEnabled(false)
    , _mqttCallback(nullptr)
    , _bufferHead(0)
    , _bufferCount(0)
    , _initialized(false)
{
    memset(_mqttTopic, 0, sizeof(_mqttTopic));
    memset(_buffer, 0, sizeof(_buffer));
}

void LogManager::begin() {
    if (_initialized) return;

    // Initialize LittleFS for persistent storage
    if (!LittleFS.begin(true)) {  // true = format on failure
        Serial.println("[LOG] LittleFS mount failed");
    }

    _initialized = true;
}

void LogManager::setLevel(LogLevel level) {
    _level = level;
}

bool LogManager::setLevelFromString(const char* levelStr) {
    if (!levelStr) return false;

    if (strcasecmp(levelStr, "DEBUG") == 0) {
        _level = LogLevel::Debug;
        return true;
    }
    if (strcasecmp(levelStr, "INFO") == 0) {
        _level = LogLevel::Info;
        return true;
    }
    if (strcasecmp(levelStr, "WARN") == 0 || strcasecmp(levelStr, "WARNING") == 0) {
        _level = LogLevel::Warn;
        return true;
    }
    if (strcasecmp(levelStr, "ERROR") == 0) {
        _level = LogLevel::Error;
        return true;
    }
    if (strcasecmp(levelStr, "NONE") == 0 || strcasecmp(levelStr, "OFF") == 0) {
        _level = LogLevel::None;
        return true;
    }

    return false;
}

const char* LogManager::getLevelString() const {
    return levelToString(_level);
}

void LogManager::setMqttEnabled(bool enabled, const char* topic, MqttPublishCallback callback) {
    _mqttEnabled = enabled;
    if (topic) {
        strncpy(_mqttTopic, topic, sizeof(_mqttTopic) - 1);
        _mqttTopic[sizeof(_mqttTopic) - 1] = '\0';
    }
    _mqttCallback = callback;
}

void LogManager::error(LogComponent component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::Error, component, format, args);
    va_end(args);
}

void LogManager::warn(LogComponent component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::Warn, component, format, args);
    va_end(args);
}

void LogManager::info(LogComponent component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::Info, component, format, args);
    va_end(args);
}

void LogManager::debug(LogComponent component, const char* format, ...) {
    va_list args;
    va_start(args, format);
    log(LogLevel::Debug, component, format, args);
    va_end(args);
}

void LogManager::raw(const char* format, ...) {
    if (!_serialEnabled) return;

    char buffer[MAX_LOG_MESSAGE];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Serial.print(buffer);
}

void LogManager::log(LogLevel level, LogComponent component, const char* format, va_list args) {
    // Check log level
    if (level == LogLevel::None || static_cast<uint8_t>(level) > static_cast<uint8_t>(_level)) {
        return;
    }

    // Format message
    char message[MAX_LOG_MESSAGE];
    vsnprintf(message, sizeof(message), format, args);

    uint32_t timestamp = millis();

    // Output to enabled destinations
    if (_serialEnabled) {
        outputToSerial(level, component, message, timestamp);
    }

    if (_mqttEnabled && _mqttCallback) {
        outputToMqtt(level, component, message, timestamp);
    }

    // Add to circular buffer
    addToBuffer(level, component, message);
}

void LogManager::addToBuffer(LogLevel level, LogComponent component, const char* message) {
    LogEntry& entry = _buffer[_bufferHead];
    entry.timestamp = millis();
    entry.level = level;
    entry.component = component;
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    entry.message[sizeof(entry.message) - 1] = '\0';

    _bufferHead = (_bufferHead + 1) % BUFFER_SIZE;
    if (_bufferCount < BUFFER_SIZE) {
        _bufferCount++;
    }
}

void LogManager::outputToSerial(LogLevel level, LogComponent component, const char* message, uint32_t timestamp) {
    // Format: [LEVEL] [COMPONENT] message
    Serial.printf("[%s] [%s] %s\n",
        levelToString(level),
        componentToString(component),
        message);
}

void LogManager::outputToMqtt(LogLevel level, LogComponent component, const char* message, uint32_t timestamp) {
    if (!_mqttCallback || strlen(_mqttTopic) == 0) return;

    // Build JSON payload
    char payload[384];
    snprintf(payload, sizeof(payload),
        "{\"ts\":%lu,\"level\":\"%s\",\"component\":\"%s\",\"msg\":\"%s\"}",
        timestamp,
        levelToString(level),
        componentToString(component),
        message);

    _mqttCallback(_mqttTopic, payload);
}

const char* LogManager::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Error: return "ERROR";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Debug: return "DEBUG";
        default:              return "NONE";
    }
}

const char* LogManager::componentToString(LogComponent component) const {
    switch (component) {
        case LogComponent::System:  return "SYS";
        case LogComponent::WiFi:    return "WIFI";
        case LogComponent::MQTT:    return "MQTT";
        case LogComponent::Printer: return "PRINT";
        case LogComponent::Config:  return "CFG";
        case LogComponent::Label:   return "LABEL";
        case LogComponent::Portal:  return "PORTAL";
        default:                    return "???";
    }
}

size_t LogManager::getRecentLogs(LogEntry* buffer, size_t maxEntries) {
    if (!buffer || maxEntries == 0 || _bufferCount == 0) return 0;

    size_t count = min(maxEntries, _bufferCount);
    size_t start = (_bufferHead + BUFFER_SIZE - _bufferCount) % BUFFER_SIZE;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % BUFFER_SIZE;
        buffer[i] = _buffer[idx];
    }

    return count;
}

size_t LogManager::getRecentLogsJson(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < 10) return 0;

    size_t written = 0;
    written += snprintf(buffer + written, bufferSize - written, "[");

    size_t count = _bufferCount;
    size_t start = (_bufferHead + BUFFER_SIZE - _bufferCount) % BUFFER_SIZE;

    for (size_t i = 0; i < count && written < bufferSize - 50; i++) {
        size_t idx = (start + i) % BUFFER_SIZE;
        const LogEntry& entry = _buffer[idx];

        if (i > 0) {
            written += snprintf(buffer + written, bufferSize - written, ",");
        }

        written += snprintf(buffer + written, bufferSize - written,
            "{\"ts\":%lu,\"level\":\"%s\",\"component\":\"%s\",\"msg\":\"%s\"}",
            entry.timestamp,
            levelToString(entry.level),
            componentToString(entry.component),
            entry.message);
    }

    written += snprintf(buffer + written, bufferSize - written, "]");
    return written;
}

void LogManager::saveToPersistent() {
    if (!_initialized) return;

    File file = LittleFS.open(CRASH_LOG_PATH, "w");
    if (!file) {
        Serial.println("[LOG] Failed to open crash log for writing");
        return;
    }

    // Write header with timestamp
    file.printf("=== Crash Log @ %lu ms ===\n", millis());

    // Write recent entries
    size_t count = _bufferCount;
    size_t start = (_bufferHead + BUFFER_SIZE - _bufferCount) % BUFFER_SIZE;

    for (size_t i = 0; i < count; i++) {
        size_t idx = (start + i) % BUFFER_SIZE;
        const LogEntry& entry = _buffer[idx];

        file.printf("[%lu] [%s] [%s] %s\n",
            entry.timestamp,
            levelToString(entry.level),
            componentToString(entry.component),
            entry.message);
    }

    file.close();
}

size_t LogManager::loadFromPersistent(char* buffer, size_t bufferSize) {
    if (!_initialized || !buffer || bufferSize == 0) return 0;

    File file = LittleFS.open(CRASH_LOG_PATH, "r");
    if (!file) {
        return 0;
    }

    size_t bytesRead = file.readBytes(buffer, bufferSize - 1);
    buffer[bytesRead] = '\0';
    file.close();

    return bytesRead;
}

void LogManager::clearPersistent() {
    if (!_initialized) return;
    LittleFS.remove(CRASH_LOG_PATH);
}

uint8_t LogManager::getBufferUsage() const {
    return static_cast<uint8_t>((_bufferCount * 100) / BUFFER_SIZE);
}
