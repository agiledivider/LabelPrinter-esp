#ifndef NELKO_P21_PRINTER_H
#define NELKO_P21_PRINTER_H

#include "Printer.h"
#include "BluetoothSerial.h"

/**
 * Nelko P21 thermal label printer implementation.
 *
 * Hardware specifications:
 * - Label size: 96 x 284 pixels (14.0mm x 40.0mm)
 * - Resolution: 203 DPI
 * - Protocol: TSPL2 (TSC Printer Language subset)
 * - Connection: Bluetooth SPP (Serial Port Profile)
 *
 * The Nelko P21 uses a subset of TSPL2 commands for bitmap printing.
 * Device discovery searches for Bluetooth devices with "P21" or "Nelko"
 * in their name.
 */
class NelkoP21Printer : public Printer {
public:
    NelkoP21Printer();
    ~NelkoP21Printer() override;

    // Label Specifications
    int getLabelWidth() const override { return LABEL_WIDTH; }
    int getLabelHeight() const override { return LABEL_HEIGHT; }
    int getBytesPerRow() const override { return BYTES_PER_ROW; }
    int getBitmapSize() const override { return BITMAP_SIZE; }
    float getLabelWidthMm() const override { return LABEL_WIDTH_MM; }
    float getLabelHeightMm() const override { return LABEL_HEIGHT_MM; }
    float getLabelGapMm() const override { return LABEL_GAP_MM; }

    // Connection Management
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override { return _connected; }
    unsigned long getLastSeenMs() const override { return _lastSeen; }

    // Status Queries
    const char* checkReady() override;
    int getBattery() override;
    void queryConfig() override;
    void queryStatus() override;

    // Printing
    void sendBitmap(const uint8_t* bitmap) override;

    // Debug
    void sendCommand(const char* cmd) override;
    void processIncoming() override;

    /**
     * Initialize Bluetooth.
     * Must be called before connect().
     * @param deviceName Name for this ESP32's Bluetooth
     * @return true if initialization successful
     */
    bool begin(const char* deviceName = "ESP32_LabelPrinter");

    /**
     * Stop Bluetooth completely.
     */
    void end();

private:
    // Label specifications for Nelko P21
    static constexpr int LABEL_WIDTH = 96;
    static constexpr int LABEL_HEIGHT = 284;
    static constexpr int BYTES_PER_ROW = LABEL_WIDTH / 8;  // 12
    static constexpr int BITMAP_SIZE = LABEL_HEIGHT * BYTES_PER_ROW;  // 3408
    static constexpr float LABEL_WIDTH_MM = 14.0f;
    static constexpr float LABEL_HEIGHT_MM = 40.0f;
    static constexpr float LABEL_GAP_MM = 5.0f;

    BluetoothSerial _serialBT;
    bool _initialized;
    bool _connected;
    int _battery;
    unsigned long _lastSeen;

    // Bluetooth callback for disconnect detection
    static NelkoP21Printer* _instance;
    static void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param);

    // Internal helpers
    bool scanAndConnect();
    void clearBuffer();
    int readWithTimeout(uint8_t* buffer, int maxLen, unsigned long timeoutMs);
    void skipEcho(int echoLen, unsigned long timeoutMs);
};

#endif
