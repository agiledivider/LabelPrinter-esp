#ifndef CONFIG_PORTAL_HTML_H
#define CONFIG_PORTAL_HTML_H

#include <Arduino.h>

/**
 * Static HTML content for ConfigPortal stored in PROGMEM (flash memory).
 * This saves ~4KB of RAM by keeping HTML strings in flash instead of RAM.
 */
namespace ConfigPortalHtml {

// Config page header (DOCTYPE through form start)
const char CONFIG_HEADER[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Label Printer Setup</title>
    <style>
        * { box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
               margin: 0; padding: 20px; background: #f5f5f5; }
        .container { max-width: 500px; margin: 0 auto; background: white;
                     padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { margin: 0 0 5px 0; color: #333; font-size: 24px; }
        .subtitle { color: #666; margin-bottom: 25px; font-size: 14px; }
        fieldset { border: 1px solid #ddd; border-radius: 6px; padding: 15px; margin-bottom: 20px; }
        legend { font-weight: 600; color: #333; padding: 0 8px; }
        label { display: block; margin-bottom: 5px; color: #555; font-size: 14px; }
        input[type="text"], input[type="password"], input[type="number"] {
            width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px;
            font-size: 16px; margin-bottom: 15px; }
        input:focus { outline: none; border-color: #007bff; }
        .checkbox-row { display: flex; align-items: center; margin-bottom: 15px; }
        .checkbox-row input { width: auto; margin-right: 8px; }
        .checkbox-row label { margin: 0; }
        .hint { font-size: 12px; color: #888; margin-top: -12px; margin-bottom: 15px; }
        button { width: 100%; padding: 12px; background: #007bff; color: white;
                 border: none; border-radius: 4px; font-size: 16px; cursor: pointer; }
        button:hover { background: #0056b3; }
        .required::after { content: " *"; color: #dc3545; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Label Printer Setup</h1>
        <p class="subtitle">Configure your device to connect to WiFi and MQTT</p>
        <form method="POST" action="/save">
            <fieldset>
                <legend>WiFi Network</legend>
                <label class="required">SSID</label>
                <input type="text" name="wifiSsid" value=")rawliteral";

// After WiFi SSID value
const char CONFIG_AFTER_WIFI_SSID[] PROGMEM = R"rawliteral(" required>
                <label>Password</label>
                <input type="password" name="wifiPassword" value=")rawliteral";

// After WiFi password value
const char CONFIG_AFTER_WIFI_PASS[] PROGMEM = R"rawliteral(">
            </fieldset>
            <fieldset>
                <legend>Device</legend>
                <label class="required">Device Name</label>
                <input type="text" name="deviceName" value=")rawliteral";

// After device name value
const char CONFIG_AFTER_DEVICE_NAME[] PROGMEM = R"rawliteral(" required>
                <p class="hint">Used for MQTT client ID and default topic generation</p>
            </fieldset>
            <fieldset>
                <legend>MQTT Broker</legend>
                <label class="required">Server</label>
                <input type="text" name="mqttServer" value=")rawliteral";

// After MQTT server value
const char CONFIG_AFTER_MQTT_SERVER[] PROGMEM = R"rawliteral(" required>
                <label>Port</label>
                <input type="number" name="mqttPort" value=")rawliteral";

// After MQTT port value
const char CONFIG_AFTER_MQTT_PORT[] PROGMEM = R"rawliteral(" min="1" max="65535">
                <label>Username</label>
                <input type="text" name="mqttUser" value=")rawliteral";

// After MQTT user value
const char CONFIG_AFTER_MQTT_USER[] PROGMEM = R"rawliteral(">
                <label>Password</label>
                <input type="password" name="mqttPassword" value=")rawliteral";

// After MQTT password value
const char CONFIG_AFTER_MQTT_PASS[] PROGMEM = R"rawliteral(">
                <div class="checkbox-row">
                    <input type="checkbox" name="mqttSsl" id="mqttSsl" )rawliteral";

// After SSL checkbox (checked or empty)
const char CONFIG_AFTER_SSL_CHECK[] PROGMEM = R"rawliteral(>
                    <label for="mqttSsl">Use SSL/TLS</label>
                </div>
            </fieldset>
            <fieldset>
                <legend>MQTT Topics</legend>
                <p class="hint" style="margin-top:0">Leave empty for auto-generated topics based on device name</p>
                <label>Print Topic</label>
                <input type="text" name="topicPrint" value=")rawliteral";

// After print topic value
const char CONFIG_AFTER_TOPIC_PRINT[] PROGMEM = R"rawliteral(" placeholder="labelprinter/print">
                <label>Status Topic</label>
                <input type="text" name="topicStatus" value=")rawliteral";

// After status topic value
const char CONFIG_AFTER_TOPIC_STATUS[] PROGMEM = R"rawliteral(" placeholder="labelprinter/status">
                <label>Result Topic</label>
                <input type="text" name="topicResult" value=")rawliteral";

// Config page footer (after result topic value to end)
const char CONFIG_FOOTER[] PROGMEM = R"rawliteral(" placeholder="labelprinter/result">
            </fieldset>
            <button type="submit">Save Configuration</button>
        </form>
    </div>
</body>
</html>)rawliteral";

// Success page (completely static)
const char SUCCESS_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuration Saved</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
               margin: 0; padding: 20px; background: #f5f5f5; text-align: center; }
        .container { max-width: 400px; margin: 50px auto; background: white;
                     padding: 40px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .icon { font-size: 60px; margin-bottom: 20px; }
        h1 { color: #28a745; margin: 0 0 10px 0; }
        p { color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <div class="icon">&#10004;</div>
        <h1>Configuration Saved!</h1>
        <p>The device will restart and connect to your WiFi network.</p>
        <p><small>This may take a few seconds...</small></p>
    </div>
</body>
</html>)rawliteral";

}  // namespace ConfigPortalHtml

#endif
