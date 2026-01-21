#ifndef PRINTER_H
#define PRINTER_H

#include <Arduino.h>
#include "PrintError.h"

/**
 * Abstract printer interface for label printers.
 *
 * This abstraction allows different printer implementations (Bluetooth, WiFi,
 * USB, etc.) to be used interchangeably through the adapter pattern.
 *
 * Implementations must provide:
 * - Label specifications (dimensions, bitmap format)
 * - Connection management
 * - Status queries (battery, config, readiness)
 * - Bitmap printing
 */
class Printer {
public:
    virtual ~Printer() = default;

    // =========================================================
    // Label Specifications
    // =========================================================

    /** Label width in pixels */
    virtual int getLabelWidth() const = 0;

    /** Label height in pixels */
    virtual int getLabelHeight() const = 0;

    /** Bytes per row in bitmap (width / 8) */
    virtual int getBytesPerRow() const = 0;

    /** Total bitmap size in bytes */
    virtual int getBitmapSize() const = 0;

    /** Label width in mm */
    virtual float getLabelWidthMm() const = 0;

    /** Label height in mm */
    virtual float getLabelHeightMm() const = 0;

    /** Gap between labels in mm */
    virtual float getLabelGapMm() const = 0;

    // =========================================================
    // Connection Management
    // =========================================================

    /**
     * Auto-discover and connect to printer.
     * @return true if connection successful
     */
    virtual bool connect() = 0;

    /** Disconnect from printer */
    virtual void disconnect() = 0;

    /** Check if currently connected */
    virtual bool isConnected() const = 0;

    /** Milliseconds since last successful connection (0 if never) */
    virtual unsigned long getLastSeenMs() const = 0;

    // =========================================================
    // Status Queries
    // =========================================================

    /**
     * Check if printer is ready to print.
     * @return PrintError::None if ready, error code otherwise
     */
    virtual PrintError checkReady() = 0;

    /**
     * Query and return battery level.
     * @return 0-100 percentage, or -1 if unknown
     */
    virtual int getBattery() = 0;

    /** Query and print configuration to Serial */
    virtual void queryConfig() = 0;

    /** Query and print status to Serial */
    virtual void queryStatus() = 0;

    // =========================================================
    // Printing
    // =========================================================

    /**
     * Send bitmap to printer for printing.
     * Bitmap must be getLabelWidth() x getLabelHeight() pixels,
     * 1-bit monochrome, getBitmapSize() bytes total.
     *
     * @param bitmap Pointer to bitmap data
     */
    virtual void sendBitmap(const uint8_t* bitmap) = 0;

    // =========================================================
    // Debug
    // =========================================================

    /**
     * Send raw command to printer (for debugging).
     * @param cmd Command string to send
     */
    virtual void sendCommand(const char* cmd) = 0;

    /**
     * Process any incoming data from printer.
     * Should be called periodically in loop().
     */
    virtual void processIncoming() = 0;
};

#endif
