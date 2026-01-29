#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <Arduino.h>
#include <functional>

/**
 * Log levels for the enhanced logging system.
 */
enum class LogLevel : uint8_t {
    None  = 0,
    Error = 1,
    Warn  = 2,
    Info  = 3,
    Debug = 4
};

/**
 * Log components for filtering.
 */
enum class LogComponent : uint8_t {
    System  = 0,
    WiFi    = 1,
    MQTT    = 2,
    Printer = 3,
    Config  = 4,
    Label   = 5,
    Portal  = 6,
    LED     = 7
};

/**
 * Circular buffer entry for log storage.
 */
struct LogEntry {
    uint32_t timestamp;      // millis() at log time
    LogLevel level;
    LogComponent component;
    char message[128];       // Truncated message
};

// Callback type for MQTT publishing
using MqttPublishCallback = std::function<bool(const char* topic, const char* payload)>;

/**
 * Enhanced Logging System for ESP32.
 *
 * Features:
 * - Configurable log levels (runtime adjustable)
 * - Multiple outputs: Serial, MQTT
 * - Circular buffer for recent logs
 * - LittleFS for persistent crash logs
 * - Component-based filtering
 * - Timestamped entries
 *
 * Usage:
 *   LogManager& log = LogManager::getInstance();
 *   log.begin();
 *   log.setLevel(LogLevel::Debug);
 *   log.info(LogComponent::System, "System initialized");
 */
class LogManager {
public:
    /**
     * Gets the singleton instance.
     */
    static LogManager& getInstance();

    /**
     * Initializes the logging system.
     * Mounts LittleFS for persistent storage.
     */
    void begin();

    /**
     * Sets the current log level.
     * Messages below this level are ignored.
     * @param level The minimum level to log
     */
    void setLevel(LogLevel level);

    /**
     * Gets the current log level.
     */
    LogLevel getLevel() const { return _level; }

    /**
     * Sets log level from string (for MQTT commands).
     * @param levelStr "DEBUG", "INFO", "WARN", "ERROR", "NONE"
     * @return true if valid level string
     */
    bool setLevelFromString(const char* levelStr);

    /**
     * Gets current log level as string.
     */
    const char* getLevelString() const;

    /**
     * Enables/disables Serial output.
     */
    void setSerialEnabled(bool enabled) { _serialEnabled = enabled; }

    /**
     * Enables/disables MQTT output.
     * @param enabled Enable MQTT logging
     * @param topic Topic to publish logs to
     * @param callback Function to call for publishing
     */
    void setMqttEnabled(bool enabled, const char* topic = nullptr, MqttPublishCallback callback = nullptr);

    /**
     * Logs a message at ERROR level.
     */
    void error(LogComponent component, const char* format, ...);

    /**
     * Logs a message at WARN level.
     */
    void warn(LogComponent component, const char* format, ...);

    /**
     * Logs a message at INFO level.
     */
    void info(LogComponent component, const char* format, ...);

    /**
     * Logs a message at DEBUG level.
     */
    void debug(LogComponent component, const char* format, ...);

    /**
     * Logs a raw message (no prefix/newline).
     */
    void raw(const char* format, ...);

    /**
     * Gets recent log entries from circular buffer.
     * @param buffer Array to fill with entries
     * @param maxEntries Maximum entries to return
     * @return Number of entries copied
     */
    size_t getRecentLogs(LogEntry* buffer, size_t maxEntries);

    /**
     * Gets recent logs as JSON string.
     * @param buffer Output buffer
     * @param bufferSize Buffer size
     * @return Number of bytes written
     */
    size_t getRecentLogsJson(char* buffer, size_t bufferSize);

    /**
     * Saves current buffer to persistent storage.
     * Call this on crash or before restart.
     */
    void saveToPersistent();

    /**
     * Loads logs from persistent storage.
     * @param buffer Output buffer for crash log content
     * @param bufferSize Buffer size
     * @return Number of bytes read
     */
    size_t loadFromPersistent(char* buffer, size_t bufferSize);

    /**
     * Clears persistent log storage.
     */
    void clearPersistent();

    /**
     * Gets buffer fill percentage.
     */
    uint8_t getBufferUsage() const;

    // Delete copy constructor and assignment
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

private:
    LogManager();

    void log(LogLevel level, LogComponent component, const char* format, va_list args);
    void addToBuffer(LogLevel level, LogComponent component, const char* message);
    void outputToSerial(LogLevel level, LogComponent component, const char* message, uint32_t timestamp);
    void outputToMqtt(LogLevel level, LogComponent component, const char* message, uint32_t timestamp);
    const char* levelToString(LogLevel level) const;
    const char* componentToString(LogComponent component) const;

    // Configuration
    LogLevel _level;
    bool _serialEnabled;
    bool _mqttEnabled;
    char _mqttTopic[65];
    MqttPublishCallback _mqttCallback;

    // Circular buffer
    static const size_t BUFFER_SIZE = 32;  // ~4KB total
    LogEntry _buffer[BUFFER_SIZE];
    size_t _bufferHead;
    size_t _bufferCount;

    // State
    bool _initialized;
};

#endif // LOG_MANAGER_H
