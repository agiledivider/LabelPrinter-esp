#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

class WiFiManager;  // Forward declaration

typedef void (*MqttMessageCallback)(char* topic, byte* payload, unsigned int length);

/**
 * MQTT-Verbindungsmanager fuer ESP32.
 *
 * Funktionen:
 * - SSL/TLS-Unterstuetzung
 * - Automatische Wiederverbindung bei Verbindungsverlust
 * - DNS-Pruefung vor Verbindungsversuchen
 * - Topic-basiertes Publishing (Status, Ergebnisse)
 *
 * Verwendung:
 *   MqttManager mqtt;
 *   mqtt.begin(wifiManager);
 *   mqtt.setConfig(server, port, useSsl, user, password, clientId);
 *   mqtt.setTopics(printTopic, statusTopic, resultTopic);
 *   mqtt.setCallback(mqttCallback);
 *   mqtt.connect();
 *
 *   // In loop():
 *   mqtt.loop();
 */
class MqttManager {
public:
    MqttManager();

    /**
     * Initialisiert den Manager mit Referenz zum WiFiManager.
     * @param wifiManager Referenz fuer Verbindungspruefung und DNS-Check
     */
    void begin(WiFiManager& wifiManager);

    /**
     * Konfiguriert die MQTT-Verbindungsparameter.
     * @param server MQTT-Broker-Hostname
     * @param port MQTT-Port (typisch 1883 oder 8883 fuer SSL)
     * @param useSsl SSL/TLS verwenden
     * @param user Benutzername (kann leer sein)
     * @param password Passwort (kann leer sein)
     * @param clientId Client-ID fuer MQTT-Verbindung
     */
    void setConfig(const char* server, uint16_t port, bool useSsl,
                   const char* user, const char* password, const char* clientId);

    /**
     * Konfiguriert die MQTT-Topics.
     * @param printTopic Topic fuer eingehende Druckauftraege
     * @param statusTopic Topic fuer Status-Veroeffentlichungen
     * @param resultTopic Topic fuer Druckergebnis-Veroeffentlichungen
     */
    void setTopics(const char* printTopic, const char* statusTopic, const char* resultTopic);

    /**
     * Setzt den Callback fuer eingehende Nachrichten.
     * @param callback Funktion die bei Nachrichten aufgerufen wird
     */
    void setCallback(MqttMessageCallback callback);

    /**
     * Verbindet mit dem MQTT-Broker.
     * @return true wenn Verbindung erfolgreich
     */
    bool connect();

    /**
     * Trennt die MQTT-Verbindung.
     */
    void disconnect();

    /**
     * Prueft ob MQTT verbunden ist.
     * @return true wenn verbunden
     */
    bool isConnected() const { return _connected; }

    /**
     * Veroeffentlicht eine Nachricht auf einem Topic.
     * @param topic Ziel-Topic
     * @param payload Nachrichteninhalt
     * @return true wenn erfolgreich
     */
    bool publish(const char* topic, const char* payload);

    /**
     * Veroeffentlicht Status auf dem Status-Topic.
     * @param payload JSON-Status-String
     * @return true wenn erfolgreich
     */
    bool publishStatus(const char* payload);

    /**
     * Veroeffentlicht Ergebnis auf dem Result-Topic.
     * @param payload JSON-Ergebnis-String
     * @return true wenn erfolgreich
     */
    bool publishResult(const char* payload);

    /**
     * Muss in loop() aufgerufen werden.
     * Verarbeitet eingehende Nachrichten und handhabt Wiederverbindung.
     */
    void loop();

private:
    WiFiManager* _wifiManager;
    WiFiClient _wifiClient;
    WiFiClientSecure _wifiClientSecure;
    PubSubClient _mqttClient;

    // Konfiguration
    char _server[65];
    uint16_t _port;
    bool _useSsl;
    char _user[33];
    char _password[65];
    char _clientId[33];

    // Topics
    char _topicPrint[65];
    char _topicStatus[65];
    char _topicResult[65];

    // Callback
    MqttMessageCallback _callback;

    // Zustand
    bool _connected;
    unsigned long _lastRetry;
    static const unsigned long RETRY_INTERVAL = 10000;  // 10 Sekunden

    // Interne Helfer
    void reconnect();
};

#endif
