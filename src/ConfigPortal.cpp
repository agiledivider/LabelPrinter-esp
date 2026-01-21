#include "ConfigPortal.h"
#include "StringUtils.h"
#include "Log.h"

const char* ConfigPortal::AP_SSID = "LabelPrinter-Setup";

ConfigPortal::ConfigPortal(ConfigManager& configManager)
    : _configManager(configManager)
    , _webServer(80)
    , _active(false)
    , _configSaved(false)
    , _startTime(0)
{
}

bool ConfigPortal::begin() {
    LOG_INFO("\n========================================");
    LOG_INFO("  Starting Configuration Portal");
    LOG_INFO("========================================");

    // Disconnect from any existing WiFi
    WiFi.disconnect(true);
    delay(100);

    // Start AP mode
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(AP_SSID)) {
        LOG_ERROR("Failed to start AP!");
        return false;
    }

    IPAddress apIP = WiFi.softAPIP();
    LOG_INFOF("AP SSID: %s", AP_SSID);
    LOG_INFOF("AP IP: %s", apIP.toString().c_str());

    // Start DNS server for captive portal
    _dnsServer.start(DNS_PORT, "*", apIP);

    // Setup web server routes
    _webServer.on("/", HTTP_GET, [this]() { handleRoot(); });
    _webServer.on("/save", HTTP_POST, [this]() { handleSave(); });
    _webServer.onNotFound([this]() { handleNotFound(); });

    _webServer.begin();
    LOG_INFO("Web server started on port 80");

    _active = true;
    _configSaved = false;
    _startTime = millis();

    LOG_INFO("\nConnect to WiFi network: LabelPrinter-Setup");
    LOG_INFO("Then open http://192.168.4.1 in your browser");
    LOG_INFO("Portal will timeout in 5 minutes\n");

    return true;
}

void ConfigPortal::end() {
    if (!_active) return;

    _webServer.stop();
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    _active = false;
    LOG_INFO("Configuration portal stopped");
}

bool ConfigPortal::loop() {
    if (!_active) return false;

    // Check timeout
    if (millis() - _startTime >= TIMEOUT_MS) {
        LOG_ERROR("Configuration portal timeout!");
        return false;
    }

    _dnsServer.processNextRequest();
    _webServer.handleClient();

    return true;
}

void ConfigPortal::handleRoot() {
    _webServer.send(200, "text/html", getConfigPage());
}

void ConfigPortal::handleSave() {
    // Get form values
    String wifiSsid = urlDecode(_webServer.arg("wifiSsid"));
    String wifiPassword = urlDecode(_webServer.arg("wifiPassword"));
    String deviceName = urlDecode(_webServer.arg("deviceName"));
    String mqttServer = urlDecode(_webServer.arg("mqttServer"));
    String mqttPort = _webServer.arg("mqttPort");
    String mqttUser = urlDecode(_webServer.arg("mqttUser"));
    String mqttPassword = urlDecode(_webServer.arg("mqttPassword"));
    String mqttSsl = _webServer.arg("mqttSsl");
    String topicPrint = urlDecode(_webServer.arg("topicPrint"));
    String topicStatus = urlDecode(_webServer.arg("topicStatus"));
    String topicResult = urlDecode(_webServer.arg("topicResult"));

    // Validate required fields
    if (wifiSsid.isEmpty() || deviceName.isEmpty() || mqttServer.isEmpty()) {
        _webServer.send(400, "text/html",
            "<html><body><h1>Error</h1>"
            "<p>WiFi SSID, Device Name, and MQTT Server are required.</p>"
            "<a href='/'>Back</a></body></html>");
        return;
    }

    // Update configuration
    ConfigManager::Config& config = _configManager.getConfigMutable();

    safeCopy(config.wifiSsid, wifiSsid.c_str(), sizeof(config.wifiSsid));
    safeCopy(config.wifiPassword, wifiPassword.c_str(), sizeof(config.wifiPassword));
    safeCopy(config.deviceName, deviceName.c_str(), sizeof(config.deviceName));
    safeCopy(config.mqttServer, mqttServer.c_str(), sizeof(config.mqttServer));
    config.mqttPort = mqttPort.isEmpty() ? 1883 : mqttPort.toInt();
    safeCopy(config.mqttUser, mqttUser.c_str(), sizeof(config.mqttUser));
    safeCopy(config.mqttPassword, mqttPassword.c_str(), sizeof(config.mqttPassword));
    config.mqttUseSsl = (mqttSsl == "on" || mqttSsl == "1");
    safeCopy(config.mqttTopicPrint, topicPrint.c_str(), sizeof(config.mqttTopicPrint));
    safeCopy(config.mqttTopicStatus, topicStatus.c_str(), sizeof(config.mqttTopicStatus));
    safeCopy(config.mqttTopicResult, topicResult.c_str(), sizeof(config.mqttTopicResult));

    // Save to NVS
    if (_configManager.save()) {
        _configSaved = true;
        _webServer.send(200, "text/html", getSuccessPage());
        LOG_INFO("Configuration saved successfully!");
    } else {
        _webServer.send(500, "text/html",
            "<html><body><h1>Error</h1>"
            "<p>Failed to save configuration.</p>"
            "<a href='/'>Back</a></body></html>");
    }
}

void ConfigPortal::handleNotFound() {
    // Captive portal redirect
    _webServer.sendHeader("Location", "http://192.168.4.1/", true);
    _webServer.send(302, "text/plain", "");
}

String ConfigPortal::getConfigPage() {
    const ConfigManager::Config& config = _configManager.getConfig();

    String html = R"rawliteral(
<!DOCTYPE html>
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
    html += config.wifiSsid;
    html += R"rawliteral(" required>

                <label>Password</label>
                <input type="password" name="wifiPassword" value=")rawliteral";
    html += config.wifiPassword;
    html += R"rawliteral(">
            </fieldset>

            <fieldset>
                <legend>Device</legend>
                <label class="required">Device Name</label>
                <input type="text" name="deviceName" value=")rawliteral";
    html += strlen(config.deviceName) > 0 ? config.deviceName : "LabelPrinter";
    html += R"rawliteral(" required>
                <p class="hint">Used for MQTT client ID and default topic generation</p>
            </fieldset>

            <fieldset>
                <legend>MQTT Broker</legend>
                <label class="required">Server</label>
                <input type="text" name="mqttServer" value=")rawliteral";
    html += strlen(config.mqttServer) > 0 ? config.mqttServer : "status.makerspacebonn.de";
    html += R"rawliteral(" required>

                <label>Port</label>
                <input type="number" name="mqttPort" value=")rawliteral";
    html += String(config.mqttPort > 0 ? config.mqttPort : 1883);
    html += R"rawliteral(" min="1" max="65535">

                <label>Username</label>
                <input type="text" name="mqttUser" value=")rawliteral";
    html += config.mqttUser;
    html += R"rawliteral(">

                <label>Password</label>
                <input type="password" name="mqttPassword" value=")rawliteral";
    html += config.mqttPassword;
    html += R"rawliteral(">

                <div class="checkbox-row">
                    <input type="checkbox" name="mqttSsl" id="mqttSsl" )rawliteral";
    html += config.mqttUseSsl ? "checked" : "";
    html += R"rawliteral(>
                    <label for="mqttSsl">Use SSL/TLS</label>
                </div>
            </fieldset>

            <fieldset>
                <legend>MQTT Topics</legend>
                <p class="hint" style="margin-top:0">Leave empty for auto-generated topics based on device name</p>

                <label>Print Topic</label>
                <input type="text" name="topicPrint" value=")rawliteral";
    html += config.mqttTopicPrint;
    html += R"rawliteral(" placeholder="labelprinter/print">

                <label>Status Topic</label>
                <input type="text" name="topicStatus" value=")rawliteral";
    html += config.mqttTopicStatus;
    html += R"rawliteral(" placeholder="labelprinter/status">

                <label>Result Topic</label>
                <input type="text" name="topicResult" value=")rawliteral";
    html += config.mqttTopicResult;
    html += R"rawliteral(" placeholder="labelprinter/result">
            </fieldset>

            <button type="submit">Save Configuration</button>
        </form>
    </div>
</body>
</html>
)rawliteral";

    return html;
}

String ConfigPortal::getSuccessPage() {
    return R"rawliteral(
<!DOCTYPE html>
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
</html>
)rawliteral";
}

String ConfigPortal::urlDecode(const String& str) {
    String decoded = "";
    char temp[3];
    temp[2] = '\0';

    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c == '+') {
            decoded += ' ';
        } else if (c == '%' && i + 2 < str.length()) {
            temp[0] = str.charAt(i + 1);
            temp[1] = str.charAt(i + 2);
            decoded += (char)strtol(temp, nullptr, 16);
            i += 2;
        } else {
            decoded += c;
        }
    }
    return decoded;
}
