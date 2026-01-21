#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

/**
 * WiFi-Verbindungsmanager fuer ESP32.
 *
 * Funktionen:
 * - Multi-Netzwerk-Unterstuetzung mit geordnetem Fallback
 * - Single-Credential-Modus fuer dynamische Konfiguration
 * - DNS-Validierung mit Auto-Reconnect nach Schwellwert-Fehlern
 * - Signalstaerke-Abfrage (RSSI)
 * - Automatische Verbindungswiederherstellung
 *
 * Verwendung (Multi-Netzwerk):
 *   WiFiManager wifi;
 *   wifi.begin(wifiNetworks, wifiNetworkCount);
 *   wifi.connect();
 *
 * Verwendung (Single-Credential):
 *   WiFiManager wifi;
 *   wifi.setCredentials("SSID", "password");
 *   wifi.connect();
 */
class WiFiManager {
public:
    WiFiManager();

    /**
     * Initialisiert den Manager mit Netzwerkliste.
     * @param networks Array von [SSID, Passwort] Paaren
     * @param networkCount Anzahl der Netzwerke
     */
    void begin(const char* networks[][2], int networkCount);

    /**
     * Setzt einzelne WiFi-Credentials fuer dynamische Konfiguration.
     * @param ssid SSID des Netzwerks
     * @param password Passwort des Netzwerks
     */
    void setCredentials(const char* ssid, const char* password);

    /**
     * Prueft ob Credentials gesetzt sind.
     * @return true wenn Credentials verfuegbar
     */
    bool hasCredentials() const;

    /**
     * Verbindet mit dem ersten erreichbaren Netzwerk.
     * Versucht alle konfigurierten Netzwerke der Reihe nach.
     * @return true wenn Verbindung erfolgreich
     */
    bool connect();

    /**
     * Trennt die WiFi-Verbindung.
     */
    void disconnect();

    /**
     * Prueft ob WiFi verbunden ist.
     * @return true wenn verbunden
     */
    bool isConnected() const { return _connected; }

    /**
     * Prueft DNS-Aufloesung fuer einen Hostnamen.
     * Bei wiederholten Fehlern wird WiFi automatisch neu verbunden.
     * @param hostname zu pruefender Hostname
     * @return true wenn DNS-Aufloesung erfolgreich
     */
    bool checkDns(const char* hostname);

    /**
     * Gibt die aktuelle Signalstaerke zurueck.
     * @return RSSI in dBm (typisch -30 bis -90)
     */
    int getRSSI() const;

    /**
     * Gibt die aktuelle IP-Adresse zurueck.
     * @return IP-Adresse als String
     */
    String getIP() const;

    /**
     * Muss in loop() aufgerufen werden.
     * Ueberwacht Verbindung und stellt bei Bedarf wieder her.
     */
    void loop();

private:
    // Netzwerk-Konfiguration (Multi-Netzwerk-Modus)
    const char* (*_networks)[2];
    int _networkCount;

    // Einzelne Credentials (Single-Credential-Modus)
    char _singleSsid[33];
    char _singlePassword[65];
    bool _useSingleCredentials;

    // Zustand
    bool _connected;
    int _dnsFailCount;

    // Verbindungsueberwachung
    unsigned long _lastConnectionCheck;
    static const unsigned long CONNECTION_CHECK_INTERVAL = 10000;  // 10 Sekunden

    // DNS-Fehlerschwellwert
    static const int DNS_FAIL_THRESHOLD = 3;

    // Interne Helfer
    bool tryConnect(const char* ssid, const char* password);
    void reconnect();
};

#endif
