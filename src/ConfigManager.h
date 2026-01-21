#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

/**
 * Manages persistent device configuration in ESP32 NVS.
 *
 * Features:
 * - Stores WiFi, MQTT, and device settings
 * - Survives firmware updates and power cycles
 * - Auto-generates MQTT topics from device name
 *
 * Usage:
 *   ConfigManager config;
 *   if (!config.load() || !config.isConfigured()) {
 *       // Start configuration portal
 *   }
 */
class ConfigManager {
public:
    struct Config {
        char wifiSsid[33];
        char wifiPassword[65];
        char deviceName[33];
        char mqttServer[65];
        uint16_t mqttPort;
        char mqttUser[33];
        char mqttPassword[65];
        bool mqttUseSsl;
        char mqttTopicPrint[65];
        char mqttTopicStatus[65];
        char mqttTopicResult[65];
        bool configured;
    };

    ConfigManager();

    /**
     * Loads configuration from NVS.
     * @return true if loaded successfully (may still be unconfigured)
     */
    bool load();

    /**
     * Saves current configuration to NVS.
     * @return true if saved successfully
     */
    bool save();

    /**
     * Clears all configuration from NVS.
     */
    void clear();

    /**
     * Checks if device has been configured.
     * @return true if configuration exists
     */
    bool isConfigured() const { return _config.configured; }

    /**
     * Returns read-only reference to current configuration.
     */
    const Config& getConfig() const { return _config; }

    /**
     * Returns mutable reference for modifying configuration.
     */
    Config& getConfigMutable() { return _config; }

    /**
     * Generates default MQTT topics based on device name.
     * Called automatically if topics are empty when saving.
     */
    void generateDefaultTopics();

    /**
     * Sets default values for unconfigured fields.
     */
    void setDefaults();

private:
    Config _config;
    Preferences _prefs;

    static const char* PREFS_NAMESPACE;
};

#endif
