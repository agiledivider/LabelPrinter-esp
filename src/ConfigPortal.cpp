#include "ConfigPortal.h"
#include "ConfigPortalHtml.h"
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
    LOG_INFO(Portal, "\n========================================");
    LOG_INFO(Portal, "  Starting Configuration Portal");
    LOG_INFO(Portal, "========================================");

    // Disconnect from any existing WiFi
    WiFi.disconnect(true);
    delay(100);

    // Start AP mode
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(AP_SSID)) {
        LOG_ERROR(Portal, "Failed to start AP!");
        return false;
    }

    IPAddress apIP = WiFi.softAPIP();
    LOG_INFOF(Portal, "AP SSID: %s", AP_SSID);
    LOG_INFOF(Portal, "AP IP: %s", apIP.toString().c_str());

    // Start DNS server for captive portal
    _dnsServer.start(DNS_PORT, "*", apIP);

    // Setup web server routes
    _webServer.on("/", HTTP_GET, [this]() { handleRoot(); });
    _webServer.on("/save", HTTP_POST, [this]() { handleSave(); });
    _webServer.onNotFound([this]() { handleNotFound(); });

    _webServer.begin();
    LOG_INFO(Portal, "Web server started on port 80");

    _active = true;
    _configSaved = false;
    _startTime = millis();

    LOG_INFO(Portal, "Connect to WiFi: LabelPrinter-Setup");
    LOG_INFO(Portal, "Then open http://192.168.4.1");
    LOG_INFO(Portal, "Portal will timeout in 5 minutes");

    return true;
}

void ConfigPortal::end() {
    if (!_active) return;

    _webServer.stop();
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    _active = false;
    LOG_INFO(Portal, "Configuration portal stopped");
}

bool ConfigPortal::loop() {
    if (!_active) return false;

    // Check timeout
    if (millis() - _startTime >= TIMEOUT_MS) {
        LOG_ERROR(Portal, "Configuration portal timeout!");
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
        LOG_INFO(Portal, "Configuration saved successfully!");
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
    using namespace ConfigPortalHtml;
    const ConfigManager::Config& config = _configManager.getConfig();

    String html;
    html.reserve(4096);  // Pre-allocate to reduce reallocations

    html += FPSTR(CONFIG_HEADER);
    html += config.wifiSsid;
    html += FPSTR(CONFIG_AFTER_WIFI_SSID);
    html += config.wifiPassword;
    html += FPSTR(CONFIG_AFTER_WIFI_PASS);
    html += strlen(config.deviceName) > 0 ? config.deviceName : "LabelPrinter";
    html += FPSTR(CONFIG_AFTER_DEVICE_NAME);
    html += strlen(config.mqttServer) > 0 ? config.mqttServer : "status.makerspacebonn.de";
    html += FPSTR(CONFIG_AFTER_MQTT_SERVER);
    html += String(config.mqttPort > 0 ? config.mqttPort : 1883);
    html += FPSTR(CONFIG_AFTER_MQTT_PORT);
    html += config.mqttUser;
    html += FPSTR(CONFIG_AFTER_MQTT_USER);
    html += config.mqttPassword;
    html += FPSTR(CONFIG_AFTER_MQTT_PASS);
    html += config.mqttUseSsl ? "checked" : "";
    html += FPSTR(CONFIG_AFTER_SSL_CHECK);
    html += config.mqttTopicPrint;
    html += FPSTR(CONFIG_AFTER_TOPIC_PRINT);
    html += config.mqttTopicStatus;
    html += FPSTR(CONFIG_AFTER_TOPIC_STATUS);
    html += config.mqttTopicResult;
    html += FPSTR(CONFIG_FOOTER);

    return html;
}

String ConfigPortal::getSuccessPage() {
    return FPSTR(ConfigPortalHtml::SUCCESS_PAGE);
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
