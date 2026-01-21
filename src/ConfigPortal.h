#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ConfigManager.h"

/**
 * Captive portal for device configuration.
 *
 * Features:
 * - Creates "LabelPrinter-Setup" WiFi hotspot
 * - Serves configuration form at 192.168.4.1
 * - Captive portal redirects all DNS to config page
 * - 5-minute timeout for safety
 *
 * Usage:
 *   ConfigPortal portal(configManager);
 *   portal.begin();
 *   while (!portal.wasConfigSaved()) {
 *       if (!portal.loop()) break;  // timeout
 *   }
 *   portal.end();
 *   if (portal.wasConfigSaved()) ESP.restart();
 */
class ConfigPortal {
public:
    ConfigPortal(ConfigManager& configManager);

    /**
     * Starts AP mode and web server.
     * @return true if started successfully
     */
    bool begin();

    /**
     * Stops AP mode and web server.
     */
    void end();

    /**
     * Handles web requests and DNS.
     * @return false if timeout exceeded
     */
    bool loop();

    /**
     * Checks if configuration was saved.
     * @return true if user submitted valid config
     */
    bool wasConfigSaved() const { return _configSaved; }

    /**
     * Checks if portal is currently active.
     * @return true if AP and web server are running
     */
    bool isActive() const { return _active; }

private:
    ConfigManager& _configManager;
    WebServer _webServer;
    DNSServer _dnsServer;

    bool _active;
    bool _configSaved;
    unsigned long _startTime;

    static const unsigned long TIMEOUT_MS = 5 * 60 * 1000;  // 5 minutes
    static const char* AP_SSID;
    static const byte DNS_PORT = 53;

    // HTTP handlers
    void handleRoot();
    void handleSave();
    void handleNotFound();

    // HTML generation
    String getConfigPage();
    String getSuccessPage();
    String urlDecode(const String& str);
};

#endif
