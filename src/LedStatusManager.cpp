#include "LedStatusManager.h"
#include "Log.h"

LedStatusManager::LedStatusManager()
    : _enabled(false)
    , _initialized(false)
    , _lastBlinkToggle(0)
    , _blinkOn(false)
{
    memset(_leds, 0, sizeof(_leds));
    for (int i = 0; i < NUM_LEDS; i++) {
        _states[i] = ConnectionState::Off;
    }
}

void LedStatusManager::begin(uint8_t dataPin, uint8_t brightness, bool enabled) {
    _enabled = enabled;

    FastLED.addLeds<WS2812B, 13, GRB>(_leds, NUM_LEDS);
    FastLED.setBrightness(brightness);

    _initialized = true;

    if (_enabled) {
        // All LEDs red on startup (nothing connected yet)
        for (int i = 0; i < NUM_LEDS; i++) {
            _states[i] = ConnectionState::Disconnected;
        }
        updateLeds();
    } else {
        FastLED.clear(true);
    }

    LOG_INFOF(LED, "Status LEDs initialized: pin=%d, brightness=%d, enabled=%s",
              dataPin, brightness, enabled ? "yes" : "no");
}

void LedStatusManager::loop() {
    if (!_initialized || !_enabled) return;

    unsigned long now = millis();
    if (now - _lastBlinkToggle >= BLINK_INTERVAL_MS) {
        _lastBlinkToggle = now;
        _blinkOn = !_blinkOn;
        updateLeds();
    }
}

void LedStatusManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!_enabled && _initialized) {
        FastLED.clear(true);
    }
}

void LedStatusManager::setBrightness(uint8_t brightness) {
    if (_initialized) {
        FastLED.setBrightness(brightness);
        FastLED.show();
    }
}

void LedStatusManager::setWlanState(ConnectionState state) {
    if (_states[0] != state) {
        _states[0] = state;
        if (_initialized && _enabled) updateLeds();
    }
}

void LedStatusManager::setMqttState(ConnectionState state) {
    if (_states[1] != state) {
        _states[1] = state;
        if (_initialized && _enabled) updateLeds();
    }
}

void LedStatusManager::setPrinterState(ConnectionState state) {
    if (_states[2] != state) {
        _states[2] = state;
        if (_initialized && _enabled) updateLeds();
    }
}

void LedStatusManager::setPortalMode() {
    if (!_initialized || !_enabled) return;
    for (int i = 0; i < NUM_LEDS; i++) {
        _leds[i] = CRGB::Yellow;
    }
    FastLED.show();
}

void LedStatusManager::updateLeds() {
    for (int i = 0; i < NUM_LEDS; i++) {
        _leds[i] = colorForState(_states[i]);
    }
    FastLED.show();
}

CRGB LedStatusManager::colorForState(ConnectionState state) const {
    switch (state) {
        case ConnectionState::Connected:    return CRGB::Green;
        case ConnectionState::Disconnected: return CRGB::Red;
        case ConnectionState::Connecting:   return _blinkOn ? CRGB::Blue : CRGB::Black;
        case ConnectionState::Off:
        default:                            return CRGB::Black;
    }
}
