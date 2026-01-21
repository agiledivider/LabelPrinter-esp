#ifndef LABEL_IMAGE_H
#define LABEL_IMAGE_H

#include <Arduino.h>
#include "font5x7.h"
#include "msb_logo.h"
#include "QRCodeRenderer.h"
#include "PrintError.h"

/**
 * Layout constants for label generation.
 * All values in pixels unless noted otherwise.
 */
namespace LabelLayout {
    // Font dimensions (5x7 pixel font)
    constexpr int GLYPH_WIDTH = 5;
    constexpr int GLYPH_HEIGHT = 7;
    constexpr int CHAR_SPACING = 1;
    constexpr int CHAR_WIDTH = GLYPH_WIDTH + CHAR_SPACING;  // 6

    // Text layout
    constexpr int LINE_HEIGHT = 9;          // Line spacing for wrapped text
    constexpr int MAX_TEXT_LINES = 10;      // Maximum lines for name text
    constexpr int ID_SCALE = 2;             // Scale factor for ID text
    constexpr int ID_HEIGHT = GLYPH_HEIGHT * ID_SCALE;  // 14

    // Vertical margins and spacing
    constexpr int QR_TOP_MARGIN = 10;       // Space above QR code
    constexpr int QR_BOTTOM_MARGIN = 10;    // Space below QR code
    constexpr int ID_BOTTOM_MARGIN = 8;     // Space below ID (to name)
    constexpr int LOGO_TOP_MARGIN = 10;     // Space above logo
}

class LabelImage {
public:
    // Constructor with label size in pixels
    LabelImage(int width, int height);
    ~LabelImage();

    // Getters
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getBytesPerRow() const { return _bytesPerRow; }
    int getBitmapSize() const { return _bitmapSize; }

    /**
     * Generate label image with QR code and text.
     * @param link URL to encode in QR code
     * @param name Item name (may wrap to multiple lines)
     * @param id Item ID (displayed larger below QR)
     * @param qrSize QR code scale (Small=1x, Medium=2x, Large=3x)
     * @return true on success, false on error
     */
    bool generate(const char* link, const char* name, const char* id,
                  QRSize qrSize = QRSize::Large);

    // Access bitmap data (after generate())
    const uint8_t* getData() const { return _bitmap; }

    // Error code after failed generate()
    PrintError getError() const { return _error; }

    /**
     * Generate Base64-encoded data URL (BMP format) for browser display.
     * @return Data URL string (caller must free with free())
     */
    char* toDataURL() const;

private:
    int _width;
    int _height;
    int _bytesPerRow;
    int _bitmapSize;
    uint8_t* _bitmap;
    PrintError _error;

    // Clear bitmap (white)
    void clear();

    // Set pixel (black)
    void setPixel(int x, int y);

    // Text rendering
    void drawTextLine(const char* start, int charCount, int y);
    void drawTextLineScaled(const char* start, int charCount, int y, int scale);
    int drawTextCentered(const char* text, int y);  // Returns number of lines
    void drawTextScaled(const char* text, int y, int scale);

    // Draw QR code (uses QRCodeRenderer)
    bool drawQRCode(const char* data, int y, QRSize size, int* outHeight);

    // Draw logo
    void drawLogo(int y);
};

#endif
