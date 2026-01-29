#ifndef LED_STATUS_MANAGER_H
#define LED_STATUS_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>

/**
 * Manages WS2812B RGB LEDs for connection status display (F009).
 *
 * LED layout:
 *   LED 0: WLAN status
 *   LED 1: MQTT status
 *   LED 2: Printer status
 *
 * Colors:
 *   Green  = Connected
 *   Red    = Disconnected
 *   Blue (blinking) = Connecting/Reconnecting
 *   Off    = Disabled
 */
class LedStatusManager {
public:
    enum class ConnectionState { Off, Connected, Disconnected, Connecting };

    static const uint8_t NUM_LEDS = 3;

    LedStatusManager();

    /**
     * Initializes FastLED with the given pin and brightness.
     * @param dataPin GPIO pin for WS2812B data line
     * @param brightness LED brightness (0-255)
     * @param enabled Whether LEDs are active
     */
    void begin(uint8_t dataPin, uint8_t brightness, bool enabled);

    /**
     * Must be called from main loop. Updates blink animation.
     */
    void loop();

    void setEnabled(bool enabled);
    void setBrightness(uint8_t brightness);

    void setWlanState(ConnectionState state);
    void setMqttState(ConnectionState state);
    void setPrinterState(ConnectionState state);

    /**
     * Sets all LEDs to yellow (config portal / AP mode).
     */
    void setPortalMode();

private:
    CRGB _leds[NUM_LEDS];
    ConnectionState _states[NUM_LEDS];
    bool _enabled;
    bool _initialized;
    unsigned long _lastBlinkToggle;
    bool _blinkOn;

    static const unsigned long BLINK_INTERVAL_MS = 500;

    void updateLeds();
    CRGB colorForState(ConnectionState state) const;
};

#endif
